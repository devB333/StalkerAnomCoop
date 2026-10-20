// S04-T01 transport probe: an isolated GameNetworkingSockets listener and peer.
//
// One executable, two roles:
//   gns-probe server --listen 127.0.0.1:27100 --scenario <s> --summary <file> [options]
//   gns-probe client --connect 127.0.0.1:27100 --scenario <s> --name <n> --summary <file> [options]
//
// Each process writes JSON-lines trace events to stdout and one JSON summary
// file with its own checks. tools/run-network-probe.py runs the scenarios and
// combines the summaries. Nothing here is engine code; the S04-T02 adapter is
// the place where the engine meets the library.

#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#include <windows.h>
#include <tlhelp32.h>
#include <timeapi.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace
{
// ---------------------------------------------------------------- utilities

std::mutex g_outMutex;
int64_t g_startUs = 0;
DWORD g_mainThread = 0;
bool g_callbackOffMainThread = false;

int64_t NowUs()
{
    // MSVC's steady_clock is QueryPerformanceCounter, which is one clock for
    // every process on the machine, so one-way ages between the peer and the
    // listener on the same host are meaningful.
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

double ElapsedMs() { return (NowUs() - g_startUs) / 1000.0; }

std::string JsonEscape(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        switch (c)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            }
            else
                out += c;
        }
    }
    return out;
}

// Trace line: {"t_ms":..,"event":"..", <fields>}
void Trace(const char* event, const char* fmt = nullptr, ...)
{
    char fields[8192] = "";
    if (fmt)
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(fields, sizeof(fields), fmt, args);
        va_end(args);
    }
    std::lock_guard<std::mutex> lock(g_outMutex);
    // "us" is the machine-wide clock, so traces of different processes line up.
    printf("{\"t_ms\":%.3f,\"us\":%lld,\"event\":\"%s\"%s%s}\n", ElapsedMs(), static_cast<long long>(NowUs()), event,
        fields[0] ? "," : "", fields);
    fflush(stdout);
}

void DebugOutput(ESteamNetworkingSocketsDebugOutputType type, const char* msg)
{
    // May be called on the library's service thread.
    std::string text = msg;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.pop_back();
    Trace("gns_log", "\"level\":%d,\"text\":\"%s\"", static_cast<int>(type), JsonEscape(text).c_str());
}

int ProcessThreadCount()
{
    const DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return -1;
    THREADENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    int count = 0;
    if (Thread32First(snap, &entry))
    {
        do
        {
            if (entry.th32OwnerProcessID == pid)
                ++count;
        } while (Thread32Next(snap, &entry));
    }
    CloseHandle(snap);
    return count;
}

struct Percentiles
{
    size_t count = 0;
    double p50 = 0, p95 = 0, p99 = 0, max = 0;
};

Percentiles Summarise(std::vector<double> values)
{
    Percentiles p;
    p.count = values.size();
    if (values.empty())
        return p;
    std::sort(values.begin(), values.end());
    auto at = [&](double q) {
        size_t i = static_cast<size_t>(q * (values.size() - 1) + 0.5);
        return values[std::min(i, values.size() - 1)];
    };
    p.p50 = at(0.50);
    p.p95 = at(0.95);
    p.p99 = at(0.99);
    p.max = values.back();
    return p;
}

std::string PercentilesJson(const Percentiles& p)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"count\":%zu,\"p50\":%.3f,\"p95\":%.3f,\"p99\":%.3f,\"max\":%.3f}", p.count, p.p50,
        p.p95, p.p99, p.max);
    return buf;
}

// ------------------------------------------------------------ wire messages
// Probe-only framing. The real protocol is S04-T03.

enum MsgType : uint8_t
{
    kHello = 1,   // u8 name length, name
    kWelcome = 2, // u8 slot, u32 server-side connection handle
    kRel = 3,     // u32 seq, u32 payload length, payload, u32 checksum
    kSnap = 4,    // u32 seq, u64 send time us, u8 phase, padding
    kPing = 5,    // u32 id, u64 send time us, u8 phase
    kPong = 6,    // echo of a ping
    kDone = 7,    // u32 reliable messages received, u32 ok flag, u32 snapshots sent
    kFin = 8,     // u32 reliable messages received by the listener
    kBeat = 9,    // listener heartbeat, unreliable, ignored by peers
};

struct Writer
{
    std::vector<uint8_t> b;
    void U8(uint8_t v) { b.push_back(v); }
    void U32(uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            b.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    void U64(uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
            b.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    void Bytes(const void* p, size_t n)
    {
        const uint8_t* c = static_cast<const uint8_t*>(p);
        b.insert(b.end(), c, c + n);
    }
};

struct Reader
{
    const uint8_t* p;
    size_t n;
    size_t at = 0;
    bool ok = true;
    Reader(const void* data, size_t size) : p(static_cast<const uint8_t*>(data)), n(size) {}
    bool Need(size_t k)
    {
        if (n - at < k)
            ok = false;
        return ok;
    }
    uint8_t U8() { return Need(1) ? p[at++] : 0; }
    uint32_t U32()
    {
        if (!Need(4))
            return 0;
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<uint32_t>(p[at++]) << (8 * i);
        return v;
    }
    uint64_t U64()
    {
        if (!Need(8))
            return 0;
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<uint64_t>(p[at++]) << (8 * i);
        return v;
    }
};

uint8_t PatternByte(uint32_t seq, uint32_t i) { return static_cast<uint8_t>(seq * 31u + i * 7u + (i >> 8)); }

uint32_t Fnv1a(const uint8_t* p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

// Deterministic sizes: mostly small, every 100th 20 KB (fragmented), every
// 500th 100 KB (many fragments, one sliding window).
uint32_t RelSize(uint32_t seq)
{
    if (seq % 500 == 499)
        return 100 * 1024;
    if (seq % 100 == 99)
        return 20 * 1024;
    return 16 + (seq * 7919u) % 1200u;
}

std::vector<uint8_t> MakeRel(uint32_t seq)
{
    const uint32_t size = RelSize(seq);
    Writer w;
    w.U8(kRel);
    w.U32(seq);
    w.U32(size);
    const size_t start = w.b.size();
    w.b.resize(start + size);
    for (uint32_t i = 0; i < size; ++i)
        w.b[start + i] = PatternByte(seq, i);
    w.U32(Fnv1a(w.b.data() + start, size));
    return w.b;
}

// Checks a kRel message against the expected next sequence number.
// Returns "" if correct, otherwise the problem.
std::string CheckRel(const uint8_t* data, size_t size, uint32_t expectSeq, uint32_t* outSeq)
{
    Reader r(data, size);
    r.U8();
    const uint32_t seq = r.U32();
    const uint32_t len = r.U32();
    *outSeq = seq;
    if (!r.ok || !r.Need(static_cast<size_t>(len) + 4))
        return "truncated";
    const uint8_t* payload = data + r.at;
    for (uint32_t i = 0; i < len; ++i)
        if (payload[i] != PatternByte(seq, i))
            return "payload";
    r.at += len;
    if (r.U32() != Fnv1a(payload, len))
        return "checksum";
    if (len != RelSize(seq))
        return "length";
    if (seq != expectSeq)
        return seq < expectSeq ? "duplicate_or_old" : "gap_or_reorder";
    return "";
}

// ------------------------------------------------------------------ options

struct Options
{
    std::string role;
    std::string scenario;
    std::string address = "127.0.0.1:27100";
    std::string name = "peer";
    std::string summary;
    float lossPct = 0;       // FakePacketLoss_Send, each direction
    float reorderPct = 0;    // FakePacketReorder_Send
    int reorderMs = 0;       // FakePacketReorder_Time
    float dupPct = 0;        // FakePacketDup_Send
    int dupMs = 0;           // FakePacketDup_TimeMax
    int lagMs = 0;           // FakePacketLag_Send
    int timeoutConnectedMs = 0; // 0 keeps the library default (10000)
    int maxActive = 2;
    int sessions = 2;        // server: accepted sessions that must end before it exits
    int minRejects = 0;      // server: refused connection attempts expected
    int relCount = 3000;
    int snapRate = 500;
    int snapBytes = 200;
    int snapMs = 6000;
    int sendRateBytes = 0;   // 0 keeps the library default (256 KB/s)
    int holdMs = 8000;
    int crashAfterMs = 1000;
    int retryLimit = 40;
    int deadlineS = 90;
    int heartbeatMs = 0;     // server: unreliable heartbeat to every session; 0 = silent
    bool expectReject = false;
    bool allowPowerThrottling = false;
};

bool ParseOptions(int argc, char** argv, Options& o)
{
    if (argc < 2)
        return false;
    o.role = argv[1];
    for (int i = 2; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--scenario") o.scenario = next();
        else if (a == "--listen" || a == "--connect") o.address = next();
        else if (a == "--name") o.name = next();
        else if (a == "--summary") o.summary = next();
        else if (a == "--loss") o.lossPct = static_cast<float>(atof(next()));
        else if (a == "--reorder") o.reorderPct = static_cast<float>(atof(next()));
        else if (a == "--reorder-ms") o.reorderMs = atoi(next());
        else if (a == "--dup") o.dupPct = static_cast<float>(atof(next()));
        else if (a == "--dup-ms") o.dupMs = atoi(next());
        else if (a == "--lag") o.lagMs = atoi(next());
        else if (a == "--timeout-ms") o.timeoutConnectedMs = atoi(next());
        else if (a == "--max-active") o.maxActive = atoi(next());
        else if (a == "--sessions") o.sessions = atoi(next());
        else if (a == "--min-rejects") o.minRejects = atoi(next());
        else if (a == "--rel-count") o.relCount = atoi(next());
        else if (a == "--snap-rate") o.snapRate = atoi(next());
        else if (a == "--snap-bytes") o.snapBytes = atoi(next());
        else if (a == "--snap-ms") o.snapMs = atoi(next());
        else if (a == "--send-rate") o.sendRateBytes = atoi(next());
        else if (a == "--hold-ms") o.holdMs = atoi(next());
        else if (a == "--crash-after-ms") o.crashAfterMs = atoi(next());
        else if (a == "--retry-limit") o.retryLimit = atoi(next());
        else if (a == "--deadline-s") o.deadlineS = atoi(next());
        else if (a == "--heartbeat-ms") o.heartbeatMs = atoi(next());
        else if (a == "--expect-reject") o.expectReject = true;
        else if (a == "--allow-power-throttling") o.allowPowerThrottling = true;
        else
        {
            fprintf(stderr, "unknown option %s\n", a.c_str());
            return false;
        }
    }
    return (o.role == "server" || o.role == "client") && !o.scenario.empty();
}

// ------------------------------------------------------------ check record

struct Checks
{
    std::vector<std::pair<std::string, bool>> items;
    std::string detail;
    void Add(const std::string& name, bool pass) { items.emplace_back(name, pass); }
    bool AllPass() const
    {
        for (auto& c : items)
            if (!c.second)
                return false;
        return !items.empty();
    }
    std::string Json() const
    {
        std::string s = "[";
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (i)
                s += ",";
            s += "{\"name\":\"" + JsonEscape(items[i].first) + "\",\"pass\":" + (items[i].second ? "true" : "false") + "}";
        }
        return s + "]";
    }
};

ISteamNetworkingSockets* g_sock = nullptr;
ISteamNetworkingUtils* g_utils = nullptr;

void ApplyGlobalConfig(const Options& o)
{
    g_utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, o.lossPct);
    g_utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketReorder_Send, o.reorderPct);
    g_utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketReorder_Time, o.reorderMs);
    g_utils->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketDup_Send, o.dupPct);
    g_utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketDup_TimeMax, o.dupMs);
    g_utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, o.lagMs);
    if (o.timeoutConnectedMs > 0)
        g_utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_TimeoutConnected, o.timeoutConnectedMs);
    if (o.sendRateBytes > 0)
    {
        g_utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendRateMin, o.sendRateBytes);
        g_utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_SendRateMax, o.sendRateBytes);
    }
}

const char* StateName(ESteamNetworkingConnectionState s)
{
    switch (s)
    {
    case k_ESteamNetworkingConnectionState_None: return "None";
    case k_ESteamNetworkingConnectionState_Connecting: return "Connecting";
    case k_ESteamNetworkingConnectionState_FindingRoute: return "FindingRoute";
    case k_ESteamNetworkingConnectionState_Connected: return "Connected";
    case k_ESteamNetworkingConnectionState_ClosedByPeer: return "ClosedByPeer";
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: return "ProblemDetectedLocally";
    default: return "Other";
    }
}

EResult SendOnLane(HSteamNetConnection conn, const std::vector<uint8_t>& bytes, int flags, uint16_t lane)
{
    SteamNetworkingMessage_t* msg = g_utils->AllocateMessage(static_cast<int>(bytes.size()));
    memcpy(msg->m_pData, bytes.data(), bytes.size());
    msg->m_conn = conn;
    msg->m_nFlags = flags;
    msg->m_idxLane = lane;
    int64 result = 0;
    g_sock->SendMessages(1, &msg, &result, true);
    return result < 0 ? static_cast<EResult>(-result) : k_EResultOK;
}

EResult Send(HSteamNetConnection conn, const std::vector<uint8_t>& bytes, int flags)
{
    return g_sock->SendMessageToConnection(conn, bytes.data(), static_cast<uint32>(bytes.size()), flags, nullptr);
}

std::string ConnectionInfoJson(HSteamNetConnection conn)
{
    SteamNetConnectionInfo_t info;
    if (!g_sock->GetConnectionInfo(conn, &info))
        return "null";
    char addr[SteamNetworkingIPAddr::k_cchMaxString];
    info.m_addrRemote.ToString(addr, sizeof(addr), true);
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"remote\":\"%s\",\"flags\":%d,\"encrypted\":%s,\"authenticated\":%s,\"description\":\"%s\"}", addr,
        info.m_nFlags, (info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Unencrypted) ? "false" : "true",
        (info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Unauthenticated) ? "false" : "true",
        JsonEscape(info.m_szConnectionDescription).c_str());
    return buf;
}

std::string RealTimeJson(HSteamNetConnection conn)
{
    SteamNetConnectionRealTimeStatus_t st;
    if (g_sock->GetConnectionRealTimeStatus(conn, &st, 0, nullptr) != k_EResultOK)
        return "null";
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"ping_ms\":%d,\"quality_local\":%.4f,\"quality_remote\":%.4f,\"out_bytes_per_s\":%.0f,"
        "\"in_bytes_per_s\":%.0f,\"send_rate\":%d}",
        st.m_nPing, st.m_flConnectionQualityLocal, st.m_flConnectionQualityRemote, st.m_flOutBytesPerSec,
        st.m_flInBytesPerSec, st.m_nSendRateBytesPerSecond);
    return buf;
}

int PendingReliable(HSteamNetConnection conn)
{
    SteamNetConnectionRealTimeStatus_t st;
    if (g_sock->GetConnectionRealTimeStatus(conn, &st, 0, nullptr) != k_EResultOK)
        return 0;
    return st.m_cbPendingReliable + st.m_cbSentUnackedReliable;
}

void WriteSummary(const Options& o, const std::string& body, const Checks& checks, int threadsBefore, int threadsAfter)
{
    if (o.summary.empty())
        return;
    FILE* f = fopen(o.summary.c_str(), "wb");
    if (!f)
        return;
    fprintf(f,
        "{\n\"role\":\"%s\",\"scenario\":\"%s\",\"name\":\"%s\",\"pass\":%s,\n"
        "\"config\":{\"loss_pct\":%.2f,\"reorder_pct\":%.2f,\"reorder_ms\":%d,\"dup_pct\":%.2f,\"dup_ms\":%d,"
        "\"lag_ms\":%d,\"timeout_connected_ms\":%d,\"send_rate\":%d},\n"
        "\"threads\":{\"before_init\":%d,\"after_init\":%d,\"callbacks_off_main_thread\":%s},\n"
        "\"checks\":%s,\n\"detail\":\"%s\",\n%s\n}\n",
        o.role.c_str(), o.scenario.c_str(), JsonEscape(o.name).c_str(), checks.AllPass() ? "true" : "false",
        o.lossPct, o.reorderPct, o.reorderMs, o.dupPct, o.dupMs, o.lagMs, o.timeoutConnectedMs, o.sendRateBytes,
        threadsBefore, threadsAfter, g_callbackOffMainThread ? "true" : "false", checks.Json().c_str(),
        JsonEscape(checks.detail).c_str(), body.c_str());
    fclose(f);
}

// =================================================================== server

struct Session
{
    HSteamNetConnection conn = k_HSteamNetConnection_Invalid;
    std::string name;
    int slot = 0;
    double connectMs = 0, endMs = 0, lastRecvMs = 0;
    std::string endState;
    int endReason = 0;
    std::string endDebug;
    bool ended = false;
    std::string info = "null";
    // reliable
    uint32_t relExpect = 0;
    uint32_t relOk = 0;
    std::string relError;
    uint32_t relSent = 0;
    bool doneReceived = false;
    uint32_t peerRelRx = 0, peerRelOk = 0, peerSnapsSent = 0;
    bool finSent = false;
    std::string detailedStatus;
    std::string realTime = "null";
    // snapshots, per phase
    std::map<int, uint32_t> snapRx, snapStale;
    std::map<int, std::vector<double>> snapAgeMs;
    uint32_t snapNewest = 0;
    bool snapAny = false;
    uint32_t snapDuplicates = 0;
    std::vector<uint8_t> snapSeen;
    uint32_t pings = 0;
    double maxPingGapMs = 0, lastPingMs = 0;
    std::map<int, std::vector<double>> pingAgeMs; // one-way, peer -> listener, per phase
    double nextBeatMs = 0;
};

struct Server
{
    Options o;
    HSteamListenSocket listen = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup group = k_HSteamNetPollGroup_Invalid;
    std::map<HSteamNetConnection, Session> live;
    std::vector<Session> finished;
    int rejects = 0;
    int nextSlot = 1;
    std::vector<std::string> timeline;
    bool reliableScenario() const { return o.scenario == "reliable"; }

    int ActiveCount() const
    {
        int n = 0;
        for (auto& kv : live)
            if (!kv.second.ended)
                ++n;
        return n;
    }

    void Event(const std::string& line) { timeline.push_back(line); }

    void OnStatus(SteamNetConnectionStatusChangedCallback_t* info)
    {
        const HSteamNetConnection conn = info->m_hConn;
        const auto state = info->m_info.m_eState;
        Trace("status", "\"conn\":%u,\"old\":\"%s\",\"new\":\"%s\",\"reason\":%d,\"debug\":\"%s\"", conn,
            StateName(info->m_eOldState), StateName(state), info->m_info.m_eEndReason,
            JsonEscape(info->m_info.m_szEndDebug).c_str());
        if (state == k_ESteamNetworkingConnectionState_Connecting && info->m_info.m_hListenSocket == listen)
        {
            if (ActiveCount() >= o.maxActive)
            {
                // Refuse before accepting: the peer sees ClosedByPeer with this reason.
                g_sock->CloseConnection(conn, k_ESteamNetConnectionEnd_App_Min + 1, "server full", false);
                ++rejects;
                Trace("reject", "\"conn\":%u,\"active\":%d", conn, ActiveCount());
                char buf[128];
                snprintf(buf, sizeof(buf), "%.1f reject conn=%u active=%d", ElapsedMs(), conn, ActiveCount());
                Event(buf);
                return;
            }
            if (g_sock->AcceptConnection(conn) != k_EResultOK)
            {
                g_sock->CloseConnection(conn, 0, nullptr, false);
                Trace("accept_failed", "\"conn\":%u", conn);
                return;
            }
            g_sock->SetConnectionPollGroup(conn, group);
            Session s;
            s.conn = conn;
            s.connectMs = ElapsedMs();
            s.lastRecvMs = s.connectMs;
            live[conn] = s;
            char buf[128];
            snprintf(buf, sizeof(buf), "%.1f accept conn=%u", ElapsedMs(), conn);
            Event(buf);
            return;
        }
        auto it = live.find(conn);
        if (it == live.end())
            return;
        Session& s = it->second;
        if (state == k_ESteamNetworkingConnectionState_Connected)
        {
            s.info = ConnectionInfoJson(conn);
            return;
        }
        if (state == k_ESteamNetworkingConnectionState_ClosedByPeer ||
            state == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
        {
            s.ended = true;
            s.endMs = ElapsedMs();
            s.endState = StateName(state);
            s.endReason = info->m_info.m_eEndReason;
            s.endDebug = info->m_info.m_szEndDebug;
            char buf[256];
            snprintf(buf, sizeof(buf), "%.1f end conn=%u name=%s state=%s reason=%d since_last_recv_ms=%.1f",
                s.endMs, conn, s.name.c_str(), s.endState.c_str(), s.endReason, s.endMs - s.lastRecvMs);
            Event(buf);
            g_sock->CloseConnection(conn, 0, nullptr, false);
            finished.push_back(s);
            live.erase(it);
        }
    }

    void OnMessage(Session& s, SteamNetworkingMessage_t* m)
    {
        s.lastRecvMs = ElapsedMs();
        const uint8_t* d = static_cast<const uint8_t*>(m->m_pData);
        if (m->m_cbSize < 1)
            return;
        Reader r(d, m->m_cbSize);
        switch (r.U8())
        {
        case kHello:
        {
            const uint8_t len = r.U8();
            if (r.Need(len))
                s.name.assign(reinterpret_cast<const char*>(d + r.at), len);
            s.slot = nextSlot++;
            Writer w;
            w.U8(kWelcome);
            w.U8(static_cast<uint8_t>(s.slot));
            w.U32(s.conn);
            Send(s.conn, w.b, k_nSteamNetworkingSend_ReliableNoNagle);
            g_sock->SetConnectionName(s.conn, s.name.c_str());
            char buf[160];
            snprintf(buf, sizeof(buf), "%.1f hello conn=%u name=%s slot=%d", ElapsedMs(), s.conn, s.name.c_str(), s.slot);
            Event(buf);
            Trace("hello", "\"conn\":%u,\"name\":\"%s\",\"slot\":%d", s.conn, JsonEscape(s.name).c_str(), s.slot);
            break;
        }
        case kRel:
        {
            uint32_t seq = 0;
            const std::string err = CheckRel(d, m->m_cbSize, s.relExpect, &seq);
            if (err.empty())
            {
                ++s.relOk;
                ++s.relExpect;
            }
            else if (s.relError.empty())
            {
                s.relError = err + " at seq " + std::to_string(seq) + " expected " + std::to_string(s.relExpect);
                Trace("rel_error", "\"conn\":%u,\"error\":\"%s\"", s.conn, JsonEscape(s.relError).c_str());
            }
            break;
        }
        case kSnap:
        {
            const uint32_t seq = r.U32();
            const uint64_t sentUs = r.U64();
            const int phase = r.U8();
            if (seq >= s.snapSeen.size())
                s.snapSeen.resize(seq + 1024, 0);
            if (s.snapSeen[seq])
            {
                ++s.snapDuplicates;
                break;
            }
            s.snapSeen[seq] = 1;
            ++s.snapRx[phase];
            s.snapAgeMs[phase].push_back((NowUs() - static_cast<int64_t>(sentUs)) / 1000.0);
            // Application policy: a snapshot older than the newest one seen is stale.
            if (s.snapAny && seq < s.snapNewest)
                ++s.snapStale[phase];
            else
            {
                s.snapNewest = seq;
                s.snapAny = true;
            }
            break;
        }
        case kPing:
        {
            r.U32();
            const uint64_t sentUs = r.U64();
            const int phase = r.U8();
            if (r.ok)
                s.pingAgeMs[phase].push_back((NowUs() - static_cast<int64_t>(sentUs)) / 1000.0);
            std::vector<uint8_t> echo(d, d + m->m_cbSize);
            echo[0] = kPong;
            Send(s.conn, echo, k_nSteamNetworkingSend_ReliableNoNagle);
            const double now = ElapsedMs();
            if (s.pings)
                s.maxPingGapMs = std::max(s.maxPingGapMs, now - s.lastPingMs);
            s.lastPingMs = now;
            ++s.pings;
            break;
        }
        case kDone:
        {
            s.peerRelRx = r.U32();
            s.peerRelOk = r.U32();
            s.peerSnapsSent = r.U32();
            s.doneReceived = true;
            std::vector<char> text(16384);
            const int len = g_sock->GetDetailedConnectionStatus(s.conn, text.data(), static_cast<int>(text.size()));
            if (len >= 0)
                s.detailedStatus = text.data();
            s.realTime = RealTimeJson(s.conn);
            Trace("done", "\"conn\":%u,\"name\":\"%s\",\"peer_rel_rx\":%u,\"peer_rel_ok\":%u", s.conn,
                JsonEscape(s.name).c_str(), s.peerRelRx, s.peerRelOk);
            break;
        }
        default:
            break;
        }
    }

    void Tick()
    {
        for (auto& kv : live)
        {
            Session& s = kv.second;
            if (s.ended || s.slot == 0)
                continue;
            if (o.heartbeatMs > 0 && ElapsedMs() >= s.nextBeatMs)
            {
                Writer w;
                w.U8(kBeat);
                // Reliable: unreliable data asks for no ack, so it would not start
                // the library's reply timeout (net-3: 12.5 s to detect a dead peer).
                Send(s.conn, w.b, k_nSteamNetworkingSend_ReliableNoNagle);
                s.nextBeatMs = ElapsedMs() + o.heartbeatMs;
            }
            // reliable scenario: stream relCount messages back, throttled on the send buffer
            if (reliableScenario())
            {
                while (s.relSent < static_cast<uint32_t>(o.relCount) && PendingReliable(s.conn) < 256 * 1024)
                {
                    if (Send(s.conn, MakeRel(s.relSent), k_nSteamNetworkingSend_Reliable) != k_EResultOK)
                        break;
                    ++s.relSent;
                }
            }
            const bool sentAll = !reliableScenario() || s.relSent == static_cast<uint32_t>(o.relCount);
            if (s.doneReceived && sentAll && !s.finSent)
            {
                Writer w;
                w.U8(kFin);
                w.U32(s.relOk);
                Send(s.conn, w.b, k_nSteamNetworkingSend_ReliableNoNagle);
                s.finSent = true;
            }
        }
    }

    std::string SessionJson(const Session& s) const
    {
        std::string out;
        char buf[1024];
        snprintf(buf, sizeof(buf),
            "{\"conn\":%u,\"name\":\"%s\",\"slot\":%d,\"connect_ms\":%.1f,\"end_ms\":%.1f,\"last_recv_ms\":%.1f,"
            "\"end_state\":\"%s\",\"end_reason\":%d,\"end_debug\":\"%s\",\"detect_after_last_recv_ms\":%.1f,"
            "\"rel_ok\":%u,\"rel_error\":\"%s\",\"rel_sent\":%u,\"peer_rel_rx\":%u,\"peer_rel_ok\":%u,"
            "\"peer_snaps_sent\":%u,\"snap_duplicates\":%u,\"pings\":%u,\"max_ping_gap_ms\":%.1f,\"done\":%s,",
            s.conn, JsonEscape(s.name).c_str(), s.slot, s.connectMs, s.endMs, s.lastRecvMs, s.endState.c_str(),
            s.endReason, JsonEscape(s.endDebug).c_str(), s.endMs - s.lastRecvMs, s.relOk, JsonEscape(s.relError).c_str(),
            s.relSent, s.peerRelRx, s.peerRelOk, s.peerSnapsSent, s.snapDuplicates, s.pings, s.maxPingGapMs,
            s.doneReceived ? "true" : "false");
        out += buf;
        out += "\"info\":" + s.info + ",\"realtime\":" + s.realTime + ",\"snap_phases\":{";
        bool first = true;
        for (auto& kv : s.snapRx)
        {
            if (!first)
                out += ",";
            first = false;
            auto stale = s.snapStale.find(kv.first);
            auto ages = s.snapAgeMs.find(kv.first);
            snprintf(buf, sizeof(buf), "\"%d\":{\"received\":%u,\"stale_discarded\":%u,\"age_ms\":", kv.first,
                kv.second, stale == s.snapStale.end() ? 0u : stale->second);
            out += buf;
            out += PercentilesJson(Summarise(ages == s.snapAgeMs.end() ? std::vector<double>() : ages->second)) + "}";
        }
        out += "},\"ping_age_ms\":{";
        first = true;
        for (auto& kv : s.pingAgeMs)
        {
            out += (first ? "\"" : ",\"") + std::to_string(kv.first) + "\":" + PercentilesJson(Summarise(kv.second));
            first = false;
        }
        out += "},\"detailed_status\":\"" + JsonEscape(s.detailedStatus) + "\"}";
        return out;
    }

    void Evaluate(Checks& c, bool timedOut) const
    {
        c.Add("finished_before_deadline", !timedOut);
        c.Add("expected_sessions_ended", static_cast<int>(finished.size()) >= o.sessions);
        c.Add("expected_rejects", rejects >= o.minRejects);
        std::map<HSteamNetConnection, int> handles;
        for (auto& s : finished)
            ++handles[s.conn];
        c.Add("distinct_connection_handles", handles.size() == finished.size());
        bool encrypted = !finished.empty();
        for (auto& s : finished)
            if (s.info.find("\"encrypted\":true") == std::string::npos)
                encrypted = false;
        c.Add("all_sessions_encrypted", encrypted);

        if (o.scenario == "basic")
        {
            bool clean = true;
            for (auto& s : finished)
                clean = clean && s.endState == "ClosedByPeer" && s.endReason == k_ESteamNetConnectionEnd_App_Min &&
                        s.doneReceived && s.slot > 0;
            c.Add("peers_welcomed_and_closed_cleanly", clean);
        }
        else if (o.scenario == "reliable")
        {
            bool ok = true;
            for (auto& s : finished)
            {
                ok = ok && s.relOk == static_cast<uint32_t>(o.relCount) && s.relError.empty();
                ok = ok && s.peerRelRx == static_cast<uint32_t>(o.relCount) && s.peerRelOk == 1;
            }
            c.Add("reliable_complete_in_order_both_directions", ok);
        }
        else if (o.scenario == "unreliable")
        {
            bool ok = true;
            std::string detail;
            for (auto& s : finished)
            {
                uint32_t rx = 0;
                for (auto& kv : s.snapRx)
                    rx += kv.second;
                const double frac = s.peerSnapsSent ? static_cast<double>(rx) / s.peerSnapsSent : 0;
                const double expect = (1.0 - o.lossPct / 100.0);
                ok = ok && s.peerSnapsSent > 0 && frac > expect - 0.05 && frac <= 1.0 && s.snapDuplicates == 0;
                char buf[160];
                snprintf(buf, sizeof(buf), "%s delivered %.4f expected ~%.4f dups %u; ", s.name.c_str(), frac, expect,
                    s.snapDuplicates);
                detail += buf;
            }
            c.Add("unreliable_delivery_matches_loss_without_duplicates", ok);
            c.detail += detail;
        }
        else if (o.scenario == "priority")
        {
            bool ok = !finished.empty();
            for (auto& s : finished)
            {
                auto ages = s.snapAgeMs.find(1);
                const Percentiles p = Summarise(ages == s.snapAgeMs.end() ? std::vector<double>() : ages->second);
                // With the send-side discard policy, delivered snapshots stay fresh.
                ok = ok && p.count > 0 && p.p95 < 150.0;
            }
            c.Add("policy_phase_snapshots_fresh_p95_under_150ms", ok);
        }
        else if (o.scenario == "crash-idle")
        {
            // Silent listener: GNS sends its first keepalive 10 s after the last
            // receive (statsutils.h:57), the reply times out, then TimeoutConnected runs.
            bool ok = false;
            for (auto& s : finished)
            {
                const double detect = s.endMs - s.lastRecvMs;
                ok = s.endState == "ProblemDetectedLocally" && detect >= 10000.0 + o.timeoutConnectedMs &&
                     detect <= 10000.0 + o.timeoutConnectedMs + 2000.0;
            }
            c.Add("idle_crash_detected_after_keepalive_plus_timeout", ok && finished.size() == 1);
        }
        else if (o.scenario == "reconnect")
        {
            int clean = 0, problem = 0;
            bool healthyUnaffected = true;
            for (auto& s : finished)
            {
                if (s.endState == "ClosedByPeer" && s.doneReceived)
                    ++clean;
                if (s.endState == "ProblemDetectedLocally")
                {
                    ++problem;
                    // With traffic to the peer, the reply timeout starts about one
                    // reply interval (0.75 s) after the last receive, then the
                    // configured TimeoutConnected runs (connections.cpp:3600-3605).
                    const double detect = s.endMs - s.lastRecvMs;
                    c.Add("crash_detected_within_timeout_plus_1500ms",
                        o.timeoutConnectedMs > 0 && detect >= o.timeoutConnectedMs &&
                            detect <= o.timeoutConnectedMs + 1500.0);
                }
                if (s.pings > 0)
                    healthyUnaffected = healthyUnaffected && s.maxPingGapMs < 1000.0;
            }
            c.Add("three_clean_sessions_one_crash", clean == 3 && problem == 1);
            // The replacement for the dead session is admitted after the dead one
            // times out and while the held session is still connected.
            const Session* dead = nullptr;
            for (auto& s : finished)
                if (s.endState == "ProblemDetectedLocally")
                    dead = &s;
            bool admittedInOrder = false;
            if (dead)
                for (auto& r2 : finished)
                    if (r2.name == dead->name && r2.connectMs >= dead->endMs)
                        for (auto& held : finished)
                            if (held.name != dead->name && held.pings > 0 && held.endMs > r2.connectMs)
                                admittedInOrder = true;
            c.Add("replacement_admitted_after_timeout_while_other_peer_connected", admittedInOrder);
            c.Add("held_session_pings_unaffected_by_crash", healthyUnaffected);
        }
    }
};

Server* g_server = nullptr;
void ServerStatusThunk(SteamNetConnectionStatusChangedCallback_t* info)
{
    if (GetCurrentThreadId() != g_mainThread)
        g_callbackOffMainThread = true;
    g_server->OnStatus(info);
}

int RunServer(const Options& o, int threadsBefore, int threadsAfter)
{
    Server server;
    server.o = o;
    g_server = &server;
    g_utils->SetGlobalCallback_SteamNetConnectionStatusChanged(ServerStatusThunk);

    SteamNetworkingIPAddr addr;
    addr.Clear();
    if (!addr.ParseString(o.address.c_str()))
    {
        fprintf(stderr, "bad listen address %s\n", o.address.c_str());
        return 2;
    }
    server.listen = g_sock->CreateListenSocketIP(addr, 0, nullptr);
    server.group = g_sock->CreatePollGroup();
    if (server.listen == k_HSteamListenSocket_Invalid || server.group == k_HSteamNetPollGroup_Invalid)
    {
        Trace("listen_failed", "\"address\":\"%s\"", o.address.c_str());
        return 2;
    }
    Trace("listening", "\"address\":\"%s\"", o.address.c_str());

    const double deadline = o.deadlineS * 1000.0;
    bool timedOut = false;
    while (true)
    {
        g_sock->RunCallbacks();
        SteamNetworkingMessage_t* msgs[256];
        int n;
        while ((n = g_sock->ReceiveMessagesOnPollGroup(server.group, msgs, 256)) > 0)
        {
            for (int i = 0; i < n; ++i)
            {
                auto it = server.live.find(msgs[i]->m_conn);
                if (it != server.live.end())
                    server.OnMessage(it->second, msgs[i]);
                msgs[i]->Release();
            }
        }
        server.Tick();
        if (static_cast<int>(server.finished.size()) >= o.sessions && server.rejects >= o.minRejects &&
            server.live.empty())
            break;
        if (ElapsedMs() > deadline)
        {
            timedOut = true;
            break;
        }
        Sleep(1);
    }
    // Sessions still live at the deadline are reported too.
    for (auto& kv : server.live)
        server.finished.push_back(kv.second);

    Checks checks;
    server.Evaluate(checks, timedOut);
    std::string body = "\"rejects\":" + std::to_string(server.rejects) + ",\n\"sessions\":[";
    for (size_t i = 0; i < server.finished.size(); ++i)
        body += (i ? ",\n" : "\n") + server.SessionJson(server.finished[i]);
    body += "],\n\"timeline\":[";
    for (size_t i = 0; i < server.timeline.size(); ++i)
        body += (i ? "," : "") + std::string("\"") + JsonEscape(server.timeline[i]) + "\"";
    body += "]";
    WriteSummary(o, body, checks, threadsBefore, threadsAfter);
    g_sock->CloseListenSocket(server.listen);
    g_sock->DestroyPollGroup(server.group);
    Trace("server_exit", "\"pass\":%s", checks.AllPass() ? "true" : "false");
    return checks.AllPass() ? 0 : 1;
}

// =================================================================== client

struct PhaseStats
{
    std::vector<double> rttMs;
    uint32_t pingsSent = 0, pingsRefused = 0;
    uint32_t snapsOffered = 0, snapsSent = 0, snapsDiscardedByPolicy = 0, snapsLimitExceeded = 0;
    int64_t maxLaneQueueUs[2] = {0, 0};
    std::vector<double> pongQueueMs; // reply waited in the library's receive queue
    std::vector<double> pumpGapMs;   // time between two polls of the peer's loop
    std::vector<double> sendSnapUs;  // cost of one SendSnap (status query + send)
};

struct Client
{
    Options o;
    HSteamNetConnection conn = k_HSteamNetConnection_Invalid;
    ESteamNetworkingConnectionState state = k_ESteamNetworkingConnectionState_None;
    int endReason = 0;
    std::string endDebug;
    int slot = 0;
    bool fin = false;
    uint32_t relExpect = 0, relOk = 0;
    std::string relError;
    std::map<int, PhaseStats> phases;
    std::map<uint32_t, std::pair<int64_t, int>> pingsInFlight; // id -> send time, phase
    uint32_t nextPing = 0;
    uint32_t nextSnap = 0;
    int lanes = 1;
    int phase = -1;
    int64_t lastPumpUs = 0;
    std::vector<std::string> sessionsLog;

    void OnStatus(SteamNetConnectionStatusChangedCallback_t* info)
    {
        if (info->m_hConn != conn)
            return;
        state = info->m_info.m_eState;
        endReason = info->m_info.m_eEndReason;
        endDebug = info->m_info.m_szEndDebug;
        Trace("status", "\"conn\":%u,\"old\":\"%s\",\"new\":\"%s\",\"reason\":%d,\"debug\":\"%s\"", info->m_hConn,
            StateName(info->m_eOldState), StateName(state), endReason, JsonEscape(endDebug).c_str());
    }

    void Pump()
    {
        const int64_t now = NowUs();
        if (phase >= 0 && lastPumpUs)
            phases[phase].pumpGapMs.push_back((now - lastPumpUs) / 1000.0);
        lastPumpUs = now;
        g_sock->RunCallbacks();
        if (conn == k_HSteamNetConnection_Invalid)
            return;
        SteamNetworkingMessage_t* msgs[256];
        int n;
        while ((n = g_sock->ReceiveMessagesOnConnection(conn, msgs, 256)) > 0)
        {
            for (int i = 0; i < n; ++i)
            {
                OnMessage(msgs[i]);
                msgs[i]->Release();
            }
        }
    }

    void OnMessage(SteamNetworkingMessage_t* m)
    {
        const uint8_t* d = static_cast<const uint8_t*>(m->m_pData);
        if (m->m_cbSize < 1)
            return;
        Reader r(d, m->m_cbSize);
        switch (r.U8())
        {
        case kWelcome:
            slot = r.U8();
            Trace("welcome", "\"slot\":%d,\"server_conn\":%u", slot, r.U32());
            break;
        case kRel:
        {
            uint32_t seq = 0;
            const std::string err = CheckRel(d, m->m_cbSize, relExpect, &seq);
            if (err.empty())
            {
                ++relOk;
                ++relExpect;
            }
            else if (relError.empty())
                relError = err + " at seq " + std::to_string(seq);
            break;
        }
        case kPong:
        {
            const uint32_t id = r.U32();
            auto it = pingsInFlight.find(id);
            if (it != pingsInFlight.end())
            {
                phases[it->second.second].pongQueueMs.push_back(
                    (g_utils->GetLocalTimestamp() - m->m_usecTimeReceived) / 1000.0);
                phases[it->second.second].rttMs.push_back((NowUs() - it->second.first) / 1000.0);
                pingsInFlight.erase(it);
            }
            break;
        }
        case kFin:
            fin = true;
            break;
        default:
            break;
        }
    }

    bool Connected() const { return state == k_ESteamNetworkingConnectionState_Connected; }
    bool Ended() const
    {
        return state == k_ESteamNetworkingConnectionState_ClosedByPeer ||
               state == k_ESteamNetworkingConnectionState_ProblemDetectedLocally;
    }

    // Connects and waits for the welcome. Returns false if refused or failed.
    bool Open(double timeoutMs)
    {
        SteamNetworkingIPAddr addr;
        addr.Clear();
        addr.ParseString(o.address.c_str());
        state = k_ESteamNetworkingConnectionState_None;
        slot = 0;
        fin = false;
        conn = g_sock->ConnectByIPAddress(addr, 0, nullptr);
        if (conn == k_HSteamNetConnection_Invalid)
            return false;
        state = k_ESteamNetworkingConnectionState_Connecting;
        const double until = ElapsedMs() + timeoutMs;
        bool helloSent = false;
        while (ElapsedMs() < until)
        {
            Pump();
            if (Connected() && !helloSent)
            {
                Writer w;
                w.U8(kHello);
                w.U8(static_cast<uint8_t>(o.name.size()));
                w.Bytes(o.name.data(), o.name.size());
                Send(conn, w.b, k_nSteamNetworkingSend_ReliableNoNagle);
                helloSent = true;
            }
            if (slot > 0)
                return true;
            if (Ended())
                return false;
            Sleep(1);
        }
        return false;
    }

    void SendDone(uint32_t snapsSent)
    {
        Writer w;
        w.U8(kDone);
        w.U32(relOk);
        w.U32(relError.empty() && relOk == relExpect ? 1u : 0u);
        w.U32(snapsSent);
        Send(conn, w.b, k_nSteamNetworkingSend_ReliableNoNagle);
    }

    bool WaitFin(double timeoutMs)
    {
        const double until = ElapsedMs() + timeoutMs;
        while (ElapsedMs() < until && !fin && !Ended())
        {
            Pump();
            Sleep(1);
        }
        return fin;
    }

    void Close(const char* why)
    {
        g_sock->CloseConnection(conn, k_ESteamNetConnectionEnd_App_Min, why, true);
        char buf[160];
        snprintf(buf, sizeof(buf), "%.1f closed conn=%u slot=%d (%s)", ElapsedMs(), conn, slot, why);
        sessionsLog.push_back(buf);
        conn = k_HSteamNetConnection_Invalid;
        state = k_ESteamNetworkingConnectionState_None;
    }

    void SendPing(int pingPhase)
    {
        Writer w;
        w.U8(kPing);
        w.U32(nextPing);
        const int64_t now = NowUs();
        w.U64(static_cast<uint64_t>(now));
        w.U8(static_cast<uint8_t>(pingPhase));
        if (Send(conn, w.b, k_nSteamNetworkingSend_ReliableNoNagle) == k_EResultOK)
        {
            pingsInFlight[nextPing] = {now, pingPhase};
            ++phases[pingPhase].pingsSent;
        }
        else
            ++phases[pingPhase].pingsRefused; // e.g. k_EResultLimitExceeded: the connection's send buffer is full
        ++nextPing;
    }

    // Sends a snapshot on the given lane. With the policy on, a snapshot that
    // would wait more than 50 ms in its lane queue is dropped before sending.
    void SendSnap(int snapPhase, uint16_t lane, bool policy, int flags)
    {
        PhaseStats& ps = phases[snapPhase];
        ++ps.snapsOffered;
        struct Timer
        {
            PhaseStats& ps;
            int64_t t0 = NowUs();
            ~Timer() { ps.sendSnapUs.push_back(static_cast<double>(NowUs() - t0)); }
        } timer{ps};
        SteamNetConnectionRealTimeStatus_t st;
        SteamNetConnectionRealTimeLaneStatus_t laneStatus[2] = {};
        if (g_sock->GetConnectionRealTimeStatus(conn, &st, this->lanes, laneStatus) == k_EResultOK)
        {
            for (int i = 0; i < this->lanes; ++i)
                ps.maxLaneQueueUs[i] = std::max<int64_t>(ps.maxLaneQueueUs[i], laneStatus[i].m_usecQueueTime);
            if (policy && laneStatus[lane].m_usecQueueTime > 50000)
            {
                ++ps.snapsDiscardedByPolicy;
                return;
            }
        }
        Writer w;
        w.U8(kSnap);
        w.U32(nextSnap);
        w.U64(static_cast<uint64_t>(NowUs()));
        w.U8(static_cast<uint8_t>(snapPhase));
        w.b.resize(std::max<size_t>(w.b.size(), static_cast<size_t>(o.snapBytes)), 0xA5);
        const EResult res = SendOnLane(conn, w.b, flags, lane);
        if (res == k_EResultOK)
        {
            ++ps.snapsSent;
            ++nextSnap;
        }
        else if (res == k_EResultLimitExceeded)
            ++ps.snapsLimitExceeded;
    }

    std::string PhasesJson() const
    {
        std::string out = "{";
        bool first = true;
        for (auto& kv : phases)
        {
            const PhaseStats& p = kv.second;
            char buf[512];
            snprintf(buf, sizeof(buf),
                "%s\"%d\":{\"pings_sent\":%u,\"pings_refused\":%u,\"snaps_offered\":%u,\"snaps_sent\":%u,\"snaps_discarded_by_policy\":%u,"
                "\"snaps_limit_exceeded\":%u,\"max_lane_queue_ms\":[%.1f,%.1f],\"rtt_ms\":",
                first ? "" : ",", kv.first, p.pingsSent, p.pingsRefused, p.snapsOffered, p.snapsSent, p.snapsDiscardedByPolicy,
                p.snapsLimitExceeded, p.maxLaneQueueUs[0] / 1000.0, p.maxLaneQueueUs[1] / 1000.0);
            out += buf;
            out += PercentilesJson(Summarise(p.rttMs));
            out += ",\"pong_queue_ms\":" + PercentilesJson(Summarise(p.pongQueueMs));
            out += ",\"pump_gap_ms\":" + PercentilesJson(Summarise(p.pumpGapMs));
            out += ",\"send_snap_us\":" + PercentilesJson(Summarise(p.sendSnapUs)) + "}";
            first = false;
        }
        return out + "}";
    }
};

Client* g_client = nullptr;
void ClientStatusThunk(SteamNetConnectionStatusChangedCallback_t* info)
{
    if (GetCurrentThreadId() != g_mainThread)
        g_callbackOffMainThread = true;
    g_client->OnStatus(info);
}

void PumpFor(Client& c, double ms)
{
    const double until = ElapsedMs() + ms;
    while (ElapsedMs() < until)
    {
        c.Pump();
        Sleep(1);
    }
}

// Waits until this connection has no queued or unacknowledged data.
void Drain(Client& c, double maxMs)
{
    const double until = ElapsedMs() + maxMs;
    while (ElapsedMs() < until)
    {
        c.Pump();
        SteamNetConnectionRealTimeStatus_t st;
        if (g_sock->GetConnectionRealTimeStatus(c.conn, &st, 0, nullptr) == k_EResultOK &&
            st.m_cbPendingReliable + st.m_cbPendingUnreliable + st.m_cbSentUnackedReliable == 0 &&
            c.pingsInFlight.empty())
            return;
        Sleep(1);
    }
}

int RunClient(const Options& o, int threadsBefore, int threadsAfter)
{
    Client c;
    c.o = o;
    g_client = &c;
    g_utils->SetGlobalCallback_SteamNetConnectionStatusChanged(ClientStatusThunk);
    Checks checks;
    std::string body;
    char buf[512];

    if (o.scenario == "basic")
    {
        const bool opened = c.Open(10000);
        if (o.expectReject)
        {
            checks.Add("refused_by_listener", !opened && c.state == k_ESteamNetworkingConnectionState_ClosedByPeer &&
                                                  c.endReason == k_ESteamNetConnectionEnd_App_Min + 1);
            snprintf(buf, sizeof(buf), "\"end_state\":\"%s\",\"end_reason\":%d,\"end_debug\":\"%s\"", StateName(c.state),
                c.endReason, JsonEscape(c.endDebug).c_str());
            body = buf;
            if (c.conn != k_HSteamNetConnection_Invalid)
                g_sock->CloseConnection(c.conn, 0, nullptr, false);
        }
        else
        {
            checks.Add("welcomed", opened && c.slot > 0);
            const std::string info = opened ? ConnectionInfoJson(c.conn) : "null";
            // Stay connected long enough for the third peer to be refused.
            PumpFor(c, 2000);
            c.SendDone(0);
            checks.Add("fin_received", c.WaitFin(10000));
            checks.Add("still_connected_before_close", c.Connected());
            snprintf(buf, sizeof(buf), "\"slot\":%d,", c.slot);
            body = buf + std::string("\"info\":") + info;
            c.Close("done");
        }
    }
    else if (o.scenario == "reliable")
    {
        const bool opened = c.Open(10000);
        checks.Add("welcomed", opened);
        uint32_t sent = 0;
        const double until = ElapsedMs() + o.deadlineS * 1000.0 - 5000.0;
        while (opened && ElapsedMs() < until && !c.Ended())
        {
            c.Pump();
            while (sent < static_cast<uint32_t>(o.relCount) && PendingReliable(c.conn) < 256 * 1024)
            {
                if (Send(c.conn, MakeRel(sent), k_nSteamNetworkingSend_Reliable) != k_EResultOK)
                    break;
                ++sent;
            }
            if (sent == static_cast<uint32_t>(o.relCount) && c.relExpect == static_cast<uint32_t>(o.relCount))
                break;
            if (!c.relError.empty())
                break;
            Sleep(1);
        }
        c.SendDone(0);
        const bool fin = c.WaitFin(20000);
        checks.Add("sent_all", sent == static_cast<uint32_t>(o.relCount));
        checks.Add("received_all_in_order", c.relOk == static_cast<uint32_t>(o.relCount) && c.relError.empty());
        checks.Add("fin_received", fin);
        snprintf(buf, sizeof(buf), "\"sent\":%u,\"received_ok\":%u,\"rel_error\":\"%s\",\"realtime\":", sent, c.relOk,
            JsonEscape(c.relError).c_str());
        body = buf + RealTimeJson(c.conn);
        std::vector<char> text(16384);
        if (g_sock->GetDetailedConnectionStatus(c.conn, text.data(), static_cast<int>(text.size())) >= 0)
            body += ",\"detailed_status\":\"" + JsonEscape(text.data()) + "\"";
        c.Close("done");
    }
    else if (o.scenario == "unreliable")
    {
        const bool opened = c.Open(10000);
        checks.Add("welcomed", opened);
        const double start = ElapsedMs();
        while (opened && ElapsedMs() - start < o.snapMs && !c.Ended())
        {
            c.Pump();
            const uint32_t target = static_cast<uint32_t>((ElapsedMs() - start) * o.snapRate / 1000.0);
            while (c.phases[0].snapsOffered < target)
                c.SendSnap(0, 0, false, k_nSteamNetworkingSend_UnreliableNoNagle);
            Sleep(1);
        }
        const PhaseStats& p = c.phases[0];
        c.SendDone(p.snapsSent);
        checks.Add("fin_received", c.WaitFin(15000));
        checks.Add("all_offered_sent", p.snapsSent == p.snapsOffered && p.snapsOffered > 0);
        body = "\"phases\":" + c.PhasesJson();
        c.Close("done");
    }
    else if (o.scenario == "priority")
    {
        // Phase 0: critical pings only.
        // Phase 1: snapshots on lane 1 (lower priority) with the send-side discard policy.
        // Phase 2: snapshots on lane 1, no policy (the queue fills to the send buffer).
        // Phase 3: snapshots on lane 0 with the critical pings, no policy.
        const bool opened = c.Open(10000);
        checks.Add("welcomed", opened);
        const int priorities[2] = {0, 1};
        const EResult lanesResult = opened ? g_sock->ConfigureConnectionLanes(c.conn, 2, priorities, nullptr)
                                           : k_EResultFail;
        checks.Add("lanes_configured", lanesResult == k_EResultOK);
        if (lanesResult == k_EResultOK)
            c.lanes = 2;
        struct Phase
        {
            int id;
            double ms;
            bool snaps;
            uint16_t lane;
            bool policy;
        };
        // Every phase has the same length, so loss hits each about equally.
        const Phase plan[] = {{0, double(o.snapMs), false, 1, false}, {1, double(o.snapMs), true, 1, true},
            {2, double(o.snapMs), true, 1, false}, {3, double(o.snapMs), true, 0, false}};
        for (const Phase& ph : plan)
        {
            if (!opened || c.Ended())
                break;
            Trace("phase", "\"phase\":%d", ph.id);
            c.phase = ph.id;
            const double start = ElapsedMs();
            double nextPingAt = start;
            uint32_t offeredBase = c.phases[ph.id].snapsOffered;
            while (ElapsedMs() - start < ph.ms && !c.Ended())
            {
                c.Pump();
                if (ElapsedMs() >= nextPingAt)
                {
                    c.SendPing(ph.id);
                    nextPingAt += 20.0;
                }
                if (ph.snaps)
                {
                    const uint32_t target =
                        offeredBase + static_cast<uint32_t>((ElapsedMs() - start) * o.snapRate / 1000.0);
                    while (c.phases[ph.id].snapsOffered < target)
                        c.SendSnap(ph.id, ph.lane, ph.policy, k_nSteamNetworkingSend_UnreliableNoNagle);
                }
                Sleep(1);
            }
            c.phase = -1;
            Drain(c, 20000);
        }
        c.SendDone(c.nextSnap);
        checks.Add("fin_received", c.WaitFin(15000));
        const PhaseStats& base = c.phases[0];
        const PhaseStats& pol = c.phases[1];
        const PhaseStats& nopol = c.phases[2];
        const Percentiles pb = Summarise(base.rttMs), p1 = Summarise(pol.rttMs), p2 = Summarise(nopol.rttMs);
        const Percentiles p3 = Summarise(c.phases[3].rttMs);
        checks.Add("all_critical_pings_answered", c.pingsInFlight.empty() && pb.count == base.pingsSent &&
                                                      p1.count == pol.pingsSent && p2.count == nopol.pingsSent &&
                                                      p3.count == c.phases[3].pingsSent);
        // Under random loss a lost ping delays the later pings on its lane until
        // the retransmit (reliable order is per lane), so only the median is
        // compared; the tail is compared on a loss-free link.
        checks.Add("critical_rtt_p50_with_policy_within_15ms_of_baseline", p1.count > 0 && p1.p50 <= pb.p50 + 15.0);
        if (o.lossPct == 0)
        {
            checks.Add("loss_free_critical_rtt_p95_with_policy_within_25ms_of_baseline", p1.p95 <= pb.p95 + 25.0);
            checks.Add("loss_free_critical_rtt_p99_with_policy_within_25ms_of_baseline", p1.p99 <= pb.p99 + 25.0);
        }
        checks.Add("no_critical_ping_refused_with_policy", pol.pingsRefused == 0 && base.pingsRefused == 0);
        checks.Add("policy_discarded_snapshots", pol.snapsDiscardedByPolicy > 0);
        checks.Add("policy_lane_queue_bounded_under_100ms", pol.maxLaneQueueUs[1] < 100000);
        body = "\"phases\":" + c.PhasesJson();
        c.Close("done");
    }
    else if (o.scenario == "reconnect-clean")
    {
        // Session 1: short, clean close. Session 2: reconnect from the same
        // process and hold, pinging, while another peer crashes.
        bool ok1 = c.Open(10000);
        const int slot1 = c.slot;
        if (ok1)
        {
            c.SendDone(0);
            ok1 = c.WaitFin(10000);
            c.Close("session 1 done");
        }
        PumpFor(c, 500);
        bool ok2 = c.Open(10000);
        const int slot2 = c.slot;
        if (ok2)
        {
            const double start = ElapsedMs();
            double nextPingAt = start;
            while (ElapsedMs() - start < o.holdMs && !c.Ended())
            {
                c.Pump();
                if (ElapsedMs() >= nextPingAt)
                {
                    c.SendPing(0);
                    nextPingAt += 100.0;
                }
                Sleep(1);
            }
            Drain(c, 10000);
            c.SendDone(0);
            ok2 = c.WaitFin(10000);
            c.Close("session 2 done");
        }
        checks.Add("session1_clean", ok1);
        checks.Add("session2_clean", ok2);
        checks.Add("reconnect_got_new_slot", slot2 > 0 && slot2 != slot1);
        const Percentiles p = Summarise(c.phases[0].rttMs);
        checks.Add("held_pings_answered", c.pingsInFlight.empty() && p.count == c.phases[0].pingsSent && p.count > 0);
        snprintf(buf, sizeof(buf), "\"slot1\":%d,\"slot2\":%d,\"rtt_ms\":", slot1, slot2);
        body = buf + PercentilesJson(p);
    }
    else if (o.scenario == "reconnect-crash")
    {
        const bool opened = c.Open(10000);
        Trace("crash_armed", "\"opened\":%s,\"after_ms\":%d", opened ? "true" : "false", o.crashAfterMs);
        // Ping until the crash so the listener's last receive time is the crash time.
        const double start = ElapsedMs();
        double nextPingAt = start;
        while (opened && ElapsedMs() - start < o.crashAfterMs)
        {
            c.Pump();
            if (ElapsedMs() >= nextPingAt)
            {
                c.SendPing(0);
                nextPingAt += 100.0;
            }
            Sleep(1);
        }
        // Abrupt end: no close packet, no library shutdown, no summary.
        Trace("crash_now");
        TerminateProcess(GetCurrentProcess(), 99);
    }
    else if (o.scenario == "reconnect-retry")
    {
        int attempts = 0, refused = 0;
        bool opened = false;
        double firstAttempt = ElapsedMs();
        while (attempts < o.retryLimit)
        {
            ++attempts;
            opened = c.Open(10000);
            if (opened)
                break;
            if (c.state == k_ESteamNetworkingConnectionState_ClosedByPeer &&
                c.endReason == k_ESteamNetConnectionEnd_App_Min + 1)
                ++refused;
            if (c.conn != k_HSteamNetConnection_Invalid)
                g_sock->CloseConnection(c.conn, 0, nullptr, false);
            c.conn = k_HSteamNetConnection_Invalid;
            PumpFor(c, 500);
        }
        const double acceptedAfter = ElapsedMs() - firstAttempt;
        bool fin = false;
        if (opened)
        {
            c.SendDone(0);
            fin = c.WaitFin(10000);
            c.Close("done");
        }
        checks.Add("eventually_accepted", opened);
        checks.Add("refused_while_stale_session_held_slot", refused >= 1);
        checks.Add("fin_received", fin);
        snprintf(buf, sizeof(buf), "\"attempts\":%d,\"refused\":%d,\"accepted_after_ms\":%.1f", attempts, refused,
            acceptedAfter);
        body = buf;
    }
    else
    {
        fprintf(stderr, "unknown client scenario %s\n", o.scenario.c_str());
        return 2;
    }

    // Let the close packets leave before shutting the library down.
    PumpFor(c, 300);
    std::string log = ",\"sessions_log\":[";
    for (size_t i = 0; i < c.sessionsLog.size(); ++i)
        log += (i ? "," : "") + std::string("\"") + JsonEscape(c.sessionsLog[i]) + "\"";
    body += log + "]";
    WriteSummary(o, body, checks, threadsBefore, threadsAfter);
    Trace("client_exit", "\"pass\":%s", checks.AllPass() ? "true" : "false");
    return checks.AllPass() ? 0 : 1;
}
} // namespace

int main(int argc, char** argv)
{
    Options o;
    if (!ParseOptions(argc, argv, o))
    {
        fprintf(stderr, "usage: gns-probe server|client --scenario <name> [options]\n");
        return 2;
    }
    // A windowless background process may be power-throttled by Windows 11,
    // which then ignores timeBeginPeriod and Sleep(1) lasts 15.6 ms (net-4).
    // Opt out unless asked to reproduce that.
    BOOL throttleOptOut = FALSE;
    if (!o.allowPowerThrottling)
    {
        PROCESS_POWER_THROTTLING_STATE pt = {};
        pt.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        pt.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
        pt.StateMask = 0;
        throttleOptOut = SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &pt, sizeof(pt));
    }
    timeBeginPeriod(1); // 1 ms Sleep granularity for the poll loops
    g_startUs = NowUs();
    g_mainThread = GetCurrentThreadId();

    const int threadsBefore = ProcessThreadCount();
    SteamNetworkingErrMsg err;
    if (!GameNetworkingSockets_Init(nullptr, err))
    {
        Trace("init_failed", "\"error\":\"%s\"", JsonEscape(err).c_str());
        return 2;
    }
    const int threadsAfter = ProcessThreadCount();
    g_sock = SteamNetworkingSockets();
    g_utils = SteamNetworkingUtils();
    g_utils->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, DebugOutput);
    ApplyGlobalConfig(o);
    Trace("init",
        "\"role\":\"%s\",\"scenario\":\"%s\",\"name\":\"%s\",\"threads_before\":%d,\"threads_after\":%d,"
        "\"power_throttling_opt_out\":%s",
        o.role.c_str(), o.scenario.c_str(), JsonEscape(o.name).c_str(), threadsBefore, threadsAfter,
        throttleOptOut ? "true" : "false");

    const int rc = o.role == "server" ? RunServer(o, threadsBefore, threadsAfter) : RunClient(o, threadsBefore, threadsAfter);
    GameNetworkingSockets_Kill();
    timeEndPeriod(1);
    return rc;
}
