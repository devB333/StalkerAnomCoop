// GameNetworkingSockets backend of coop::net::Transport. The only file that
// includes the library's headers. Pinned library: tools/network-probe/pins.json
// (v1.6.1, d534c19a); behaviour notes cite that checkout.
#include "coop/net/gns_testing.h"
#include "coop/net/transport.h"
#include "policy.h"

#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace coop::net
{
namespace
{
class GnsTransport;

// ------------------------------------------------- process-wide library state
//
// GameNetworkingSockets_Init/Kill are process-global and not reference counted
// (csteamnetworkingsockets.cpp:2503-2533), so the adapter counts users and
// kills the library with the last transport. Status callbacks are queued by the
// library and run on whichever thread calls RunCallbacks()
// (csteamnetworkingsockets.cpp:1714-1757); with two transports on two threads,
// one thread can run the other's callbacks. So a callback only routes the
// notice, by the instance id stored as connection user data, into that
// transport's locked inbox. The owner handles it in its own poll().
struct Library
{
    std::mutex mutex; // refs, registry
    int refs = 0;
    std::int64_t nextInstance = 1;
    std::unordered_map<std::int64_t, GnsTransport*> registry;
};

Library& library()
{
    static Library lib;
    return lib;
}

std::atomic<LogSink> g_logSink{nullptr};
std::atomic<int> g_logLevel{int(LogLevel::Warning)};

// The library may call this on its service thread while holding its global
// lock, so it must never take Library::mutex.
void OnDebugOutput(ESteamNetworkingSocketsDebugOutputType type, const char* text)
{
    LogSink sink = g_logSink.load();
    if (!sink)
        return;
    LogLevel level = LogLevel::Debug;
    if (type <= k_ESteamNetworkingSocketsDebugOutputType_Error)
        level = LogLevel::Error;
    else if (type <= k_ESteamNetworkingSocketsDebugOutputType_Warning)
        level = LogLevel::Warning;
    else if (type <= k_ESteamNetworkingSocketsDebugOutputType_Msg)
        level = LogLevel::Info;
    if (int(level) <= g_logLevel.load())
        sink(level, text);
}

ESteamNetworkingSocketsDebugOutputType GnsLevel(int level)
{
    switch (LogLevel(level))
    {
    case LogLevel::Error: return k_ESteamNetworkingSocketsDebugOutputType_Error;
    case LogLevel::Warning: return k_ESteamNetworkingSocketsDebugOutputType_Warning;
    case LogLevel::Info: return k_ESteamNetworkingSocketsDebugOutputType_Msg;
    case LogLevel::Debug: return k_ESteamNetworkingSocketsDebugOutputType_Debug;
    }
    return k_ESteamNetworkingSocketsDebugOutputType_Warning;
}

void OnStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);

// RunCallbacks() swaps the library's queue and dispatches it without a lock
// (csteamnetworkingsockets.cpp:1714-1757). Two threads dispatching two swapped
// batches at once could reorder one connection's notices, so dispatch is
// serialised. Never taken together with Library::mutex held by the same thread
// in the other order.
std::mutex g_dispatchMutex;

void RunLibraryCallbacks()
{
    std::lock_guard<std::mutex> lock(g_dispatchMutex);
    SteamNetworkingSockets()->RunCallbacks();
}

// Close codes the library itself may report are outside our app range.
bool IsAppCode(int code) { return code >= k_ESteamNetConnectionEnd_App_Min && code <= k_ESteamNetConnectionEnd_AppException_Max; }

std::uint32_t UsToMs(SteamNetworkingMicroseconds us) { return us <= 0 ? 0u : std::uint32_t((us + 999) / 1000); }

// ------------------------------------------------------------- the transport
class GnsTransport final : public Transport
{
public:
    GnsTransport(const TransportConfig& config, std::int64_t instance)
        : config_(config), instance_(instance), sock_(SteamNetworkingSockets()), utils_(SteamNetworkingUtils())
    {
        counters_.channels.resize(config_.channels.size());
        std::uint32_t largest = 0;
        for (const ChannelSpec& ch : config_.channels)
        {
            priorities_.push_back(ch.priority);
            weights_.push_back(ch.weight);
            largest = std::max(largest, ch.max_message_bytes);
        }
        auto add = [this](ESteamNetworkingConfigValue key, std::int32_t v) {
            SteamNetworkingConfigValue_t o;
            o.SetInt32(key, v);
            options_.push_back(o);
        };
        SteamNetworkingConfigValue_t o;
        o.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, reinterpret_cast<void*>(&OnStatusChanged));
        options_.push_back(o);
        // Inherited by connections accepted from the listen socket, so every
        // notice names its transport from the start.
        o.SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, instance_);
        options_.push_back(o);
        add(k_ESteamNetworkingConfig_SendBufferSize, std::int32_t(config_.send_buffer_bytes));
        add(k_ESteamNetworkingConfig_RecvBufferSize, std::int32_t(config_.recv_buffer_bytes));
        add(k_ESteamNetworkingConfig_RecvBufferMessages, std::int32_t(config_.recv_buffer_messages));
        // A larger reliable message makes the library close the connection itself
        // (steamnetworkingsockets_snp.cpp:3815-3824); the adapter's per-channel
        // check below catches smaller oversize messages.
        add(k_ESteamNetworkingConfig_RecvMaxMessageSize, std::int32_t(largest));
        add(k_ESteamNetworkingConfig_SendRateMin, std::int32_t(config_.send_rate_bytes_per_s));
        add(k_ESteamNetworkingConfig_SendRateMax, std::int32_t(config_.send_rate_bytes_per_s));
        add(k_ESteamNetworkingConfig_TimeoutInitial, std::int32_t(config_.connect_timeout_ms));
        add(k_ESteamNetworkingConfig_TimeoutConnected, std::int32_t(config_.connected_timeout_ms));
    }

    ~GnsTransport() override { shutdownForDestroy(); }

    // The owner should have called shutdown(); otherwise close without
    // flushing. The owner-thread rule cannot be enforced in a destructor.
    void shutdownForDestroy()
    {
        if (!closed_)
            closeAll(0);
    }

    // Called from OnStatusChanged on any thread, with Library::mutex held.
    void Enqueue(const SteamNetConnectionStatusChangedCallback_t& notice)
    {
        std::lock_guard<std::mutex> lock(inboxMutex_);
        inbox_.push_back(notice);
    }

    bool listen(const char* address, std::string* error) override
    {
        if (!owner_.here())
            return wrongThread(error);
        if (closed_)
            return fail(error, "transport is shut down");
        if (listen_ != k_HSteamListenSocket_Invalid)
            return fail(error, "already listening");
        SteamNetworkingIPAddr addr;
        if (!address || !addr.ParseString(address))
            return fail(error, std::string("bad address: ") + (address ? address : "(null)"));
        listen_ = sock_->CreateListenSocketIP(addr, int(options_.size()), options_.data());
        if (listen_ == k_HSteamListenSocket_Invalid)
            return fail(error, std::string("cannot listen on ") + address);
        return true;
    }

    ConnectionId connect(const char* address, std::string* error) override
    {
        if (!owner_.here())
        {
            wrongThread(error);
            return kNoConnection;
        }
        if (closed_)
        {
            fail(error, "transport is shut down");
            return kNoConnection;
        }
        SteamNetworkingIPAddr addr;
        if (!address || !addr.ParseString(address))
        {
            fail(error, std::string("bad address: ") + (address ? address : "(null)"));
            return kNoConnection;
        }
        const HSteamNetConnection h = sock_->ConnectByIPAddress(addr, int(options_.size()), options_.data());
        if (h == k_HSteamNetConnection_Invalid)
        {
            ++counters_.connections_failed;
            fail(error, std::string("cannot connect to ") + address);
            return kNoConnection;
        }
        if (!configureLanes(h))
        {
            sock_->CloseConnection(h, 0, nullptr, false);
            ++counters_.connections_failed;
            fail(error, "cannot configure lanes");
            return kNoConnection;
        }
        return addConnection(h, false, address);
    }

    SendResult send(ConnectionId id, ChannelId channel, const void* data, std::uint32_t size) override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return SendResult::WrongThread;
        }
        if (closed_)
            return SendResult::NotConnected;
        if (channel >= config_.channels.size())
            return SendResult::InvalidChannel;
        auto it = conns_.find(id);
        if (it == conns_.end() || it->second.state != ConnectionState::Connected)
            return SendResult::NotConnected;
        Conn& c = it->second;
        const ChannelSpec& spec = config_.channels[channel];
        if (size > spec.max_message_bytes)
            return SendResult::TooLarge;

        detail::Admission a = detail::admit(spec, c.est[channel], size);
        if (a != detail::Admission::Send)
        {
            // The estimate only over-counts; ask the library before refusing.
            refresh(c);
            a = detail::admit(spec, c.est[channel], size);
        }
        if (a == detail::Admission::Send)
        {
            SteamNetworkingMessage_t* msg = utils_->AllocateMessage(int(size));
            if (!msg)
            {
                ++counters_.send_failures;
                return SendResult::Failed;
            }
            if (size)
                std::memcpy(msg->m_pData, data, size);
            msg->m_conn = c.handle;
            msg->m_idxLane = channel;
            msg->m_nFlags = SendFlags(spec.delivery);
            std::int64_t result = 0;
            sock_->SendMessages(1, &msg, &result, true); // takes ownership of msg, also on failure
            if (result < 0)
            {
                const EResult e = EResult(-result);
                if (e != k_EResultLimitExceeded)
                {
                    ++counters_.send_failures;
                    return e == k_EResultInvalidParam || e == k_EResultNoConnection ? SendResult::NotConnected
                                                                                     : SendResult::Failed;
                }
                // The library's shared send buffer is full. validate() keeps the
                // channel bounds inside it, so this means a misjudged estimate;
                // apply the channel's overflow rule.
                a = spec.delivery == Delivery::Critical ? detail::Admission::Overload
                    : spec.delivery == Delivery::Bulk   ? detail::Admission::Backpressure
                                                        : detail::Admission::DropBacklog;
            }
            else
                c.est[channel].queued_since_refresh += size;
        }
        detail::count_send(counters_.channels[channel], a, size);
        switch (a)
        {
        case detail::Admission::Send: return SendResult::Sent;
        case detail::Admission::DropStale:
        case detail::Admission::DropBacklog: return SendResult::Dropped;
        case detail::Admission::Backpressure: return SendResult::Backpressure;
        case detail::Admission::Overload:
            closeByAdapter(id, close_code::kOverloaded, CloseReason::Overloaded,
                "critical backlog on channel " + spec.name + " over bound", pending_);
            return SendResult::Overloaded;
        }
        return SendResult::Failed;
    }

    void flush(ConnectionId id) override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return;
        }
        auto it = conns_.find(id);
        if (!closed_ && it != conns_.end())
            sock_->FlushMessagesOnConnection(it->second.handle);
    }

    void poll(std::vector<Event>& out) override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return;
        }
        if (closed_)
            return;
        for (Event& e : pending_)
            out.push_back(std::move(e));
        pending_.clear();

        handleNotices(out);

        // Messages only for connections the owner has seen connect, so a
        // connection's events stay in order even when another thread ran its
        // Connected callback. Rotate the start for fairness.
        std::vector<ConnectionId> ids;
        for (auto& kv : conns_)
            if (kv.second.state == ConnectionState::Connected)
                ids.push_back(kv.first);
        if (!ids.empty())
        {
            const std::size_t start = rotation_++ % ids.size();
            for (std::size_t i = 0; i < ids.size(); ++i)
            {
                auto it = conns_.find(ids[(start + i) % ids.size()]);
                if (it != conns_.end())
                    receive(it->second, config_.max_messages_per_poll, out);
            }
        }
        for (auto& kv : conns_)
            if (kv.second.state == ConnectionState::Connected)
                refresh(kv.second);
    }

    bool close(ConnectionId id, int code, const char* detail, bool flush) override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return false;
        }
        if (!detail::valid_close_code(code))
            return false;
        auto it = conns_.find(id);
        if (closed_ || it == conns_.end())
            return false;
        sock_->CloseConnection(it->second.handle, code, detail, flush);
        byHandle_.erase(it->second.handle);
        conns_.erase(it);
        return true;
    }

    ShutdownReport shutdown(std::uint32_t flushTimeoutMs) override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return {};
        }
        if (closed_)
            return {};
        return closeAll(flushTimeoutMs);
    }

    bool status(ConnectionId id, ConnectionStatus& out) const override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            return false;
        }
        auto it = conns_.find(id);
        if (it == conns_.end())
            return false;
        const Conn& c = it->second;
        out.state = c.state;
        out.incoming = c.incoming;
        out.remote_address = c.address;
        out.ping_ms = c.ping;
        out.quality_local = c.qualityLocal;
        out.quality_remote = c.qualityRemote;
        out.out_bytes_per_s = c.outBytes;
        out.in_bytes_per_s = c.inBytes;
        out.channels.clear();
        for (const auto& e : c.est)
        {
            ChannelBacklog b = e.last; // plus what this side queued since, so producers can pace
            b.queued_bytes += std::uint32_t(e.queued_since_refresh);
            out.channels.push_back(b);
        }
        return true;
    }

    TransportCounters counters() const override
    {
        if (!owner_.here())
        {
            ++wrongThreadCalls_;
            TransportCounters only;
            only.wrong_thread_calls = wrongThreadCalls_.load();
            return only;
        }
        TransportCounters c = counters_;
        c.wrong_thread_calls = wrongThreadCalls_.load();
        return c;
    }
    const TransportConfig& config() const override { return config_; }
    std::int64_t now_us() const override { return utils_->GetLocalTimestamp(); }

private:
    struct Conn
    {
        ConnectionId id = kNoConnection;
        HSteamNetConnection handle = k_HSteamNetConnection_Invalid;
        bool incoming = false;
        ConnectionState state = ConnectionState::Connecting;
        std::string address;
        std::vector<detail::ChannelEstimate> est;
        int ping = -1;
        float qualityLocal = -1.f, qualityRemote = -1.f, outBytes = 0.f, inBytes = 0.f;
    };

    static int SendFlags(Delivery d)
    {
        switch (d)
        {
        case Delivery::Critical: return k_nSteamNetworkingSend_ReliableNoNagle;
        case Delivery::Bulk: return k_nSteamNetworkingSend_Reliable;
        case Delivery::Transient: return k_nSteamNetworkingSend_UnreliableNoNagle;
        }
        return k_nSteamNetworkingSend_Reliable;
    }

    bool wrongThread(std::string* error)
    {
        ++wrongThreadCalls_;
        return fail(error, "called off the owner thread");
    }

    static bool fail(std::string* error, const std::string& what)
    {
        if (error)
            *error = what;
        return false;
    }

    bool configureLanes(HSteamNetConnection h)
    {
        return sock_->ConfigureConnectionLanes(h, int(priorities_.size()), priorities_.data(), weights_.data()) ==
               k_EResultOK;
    }

    ConnectionId addConnection(HSteamNetConnection h, bool incoming, const std::string& address)
    {
        const ConnectionId id = nextId_++;
        Conn& c = conns_[id];
        c.id = id;
        c.handle = h;
        c.incoming = incoming;
        c.address = address;
        c.est.resize(config_.channels.size());
        byHandle_[h] = id;
        return id;
    }

    bool refresh(Conn& c)
    {
        SteamNetConnectionRealTimeStatus_t st;
        SteamNetConnectionRealTimeLaneStatus_t lanes[kMaxChannels];
        const int n = int(config_.channels.size());
        if (sock_->GetConnectionRealTimeStatus(c.handle, &st, n, lanes) != k_EResultOK)
            return false;
        for (int i = 0; i < n; ++i)
        {
            ChannelBacklog& b = c.est[i].last;
            b.queued_bytes = std::uint32_t(std::max(0, lanes[i].m_cbPendingReliable + lanes[i].m_cbPendingUnreliable));
            b.unacked_bytes = std::uint32_t(std::max(0, lanes[i].m_cbSentUnackedReliable));
            b.queue_ms = UsToMs(lanes[i].m_usecQueueTime);
            c.est[i].queued_since_refresh = 0;
        }
        c.ping = st.m_nPing;
        c.qualityLocal = st.m_flConnectionQualityLocal;
        c.qualityRemote = st.m_flConnectionQualityRemote;
        c.outBytes = st.m_flOutBytesPerSec;
        c.inBytes = st.m_flInBytesPerSec;
        return true;
    }

    void handleNotices(std::vector<Event>& out)
    {
        RunLibraryCallbacks();
        std::vector<SteamNetConnectionStatusChangedCallback_t> notices;
        {
            std::lock_guard<std::mutex> lock(inboxMutex_);
            notices.swap(inbox_);
        }
        for (const auto& n : notices)
            handleNotice(n, out);
    }

    void handleNotice(const SteamNetConnectionStatusChangedCallback_t& n, std::vector<Event>& out)
    {
        const ESteamNetworkingConnectionState state = n.m_info.m_eState;
        auto hit = byHandle_.find(n.m_hConn);
        if (hit == byHandle_.end())
        {
            if (state == k_ESteamNetworkingConnectionState_Connecting &&
                n.m_info.m_hListenSocket != k_HSteamListenSocket_Invalid && n.m_info.m_hListenSocket == listen_)
            {
                acceptIncoming(n);
                return;
            }
            // A notice for a connection already closed here (or one this
            // transport never adopted). Free it if the library still holds it.
            ++counters_.stale_notifications;
            if (state == k_ESteamNetworkingConnectionState_ClosedByPeer ||
                state == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
                sock_->CloseConnection(n.m_hConn, 0, nullptr, false);
            return;
        }
        const ConnectionId id = hit->second;
        Conn& c = conns_[id];
        switch (state)
        {
        case k_ESteamNetworkingConnectionState_Connected:
            if (c.state == ConnectionState::Connecting)
            {
                c.state = ConnectionState::Connected;
                if (c.incoming)
                    ++counters_.connections_accepted;
                Event e;
                e.type = EventType::Connected;
                e.connection = id;
                e.incoming = c.incoming;
                out.push_back(std::move(e));
                refresh(c);
            }
            break;
        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
        {
            const bool wasConnected = c.state == ConnectionState::Connected;
            // The peer's last messages come before its Disconnected.
            if (wasConnected)
                receive(c, 0xffffffffu, out);
            if (conns_.find(id) == conns_.end())
                return; // closed for a violation while draining
            const int code = n.m_info.m_eEndReason;
            CloseReason reason;
            if (state == k_ESteamNetworkingConnectionState_ClosedByPeer)
                reason = IsAppCode(code) ? detail::reason_from_peer_code(code) : CloseReason::RemoteProblem;
            else
                reason = code == k_ESteamNetConnectionEnd_Misc_Timeout ? CloseReason::Timeout : CloseReason::LocalProblem;
            if (!wasConnected && reason != CloseReason::Refused)
                reason = CloseReason::ConnectFailed;
            if (!wasConnected && !c.incoming)
                ++counters_.connections_failed;
            // An incoming connection the owner never saw connect ends silently.
            if (wasConnected || !c.incoming)
            {
                Event e;
                e.type = EventType::Disconnected;
                e.connection = id;
                e.incoming = c.incoming;
                e.reason = reason;
                e.code = code;
                e.detail = n.m_info.m_szEndDebug;
                out.push_back(std::move(e));
            }
            sock_->CloseConnection(c.handle, 0, nullptr, false);
            byHandle_.erase(c.handle);
            conns_.erase(id);
            break;
        }
        default:
            break; // Connecting / FindingRoute: nothing to report yet
        }
    }

    void acceptIncoming(const SteamNetConnectionStatusChangedCallback_t& n)
    {
        std::uint32_t open = 0;
        for (const auto& kv : conns_)
            open += kv.second.incoming ? 1u : 0u;
        if (closed_)
        {
            ++counters_.connections_refused;
            sock_->CloseConnection(n.m_hConn, close_code::kShutdown, "shutting down", false);
            return;
        }
        if (open >= config_.max_connections)
        {
            ++counters_.connections_refused;
            sock_->CloseConnection(n.m_hConn, close_code::kServerFull, "server full", false);
            return;
        }
        if (sock_->AcceptConnection(n.m_hConn) != k_EResultOK || !configureLanes(n.m_hConn))
        {
            // Gone before we got to it.
            ++counters_.stale_notifications;
            sock_->CloseConnection(n.m_hConn, 0, nullptr, false);
            return;
        }
        char addr[SteamNetworkingIPAddr::k_cchMaxString];
        n.m_info.m_addrRemote.ToString(addr, sizeof(addr), true);
        addConnection(n.m_hConn, true, addr);
    }

    // Takes up to `budget` messages from one connection, validating each.
    void receive(Conn& c, std::uint32_t budget, std::vector<Event>& out)
    {
        SteamNetworkingMessage_t* msgs[64];
        const ConnectionId id = c.id;
        const HSteamNetConnection h = c.handle;
        bool violated = false;
        std::string why;
        while (budget > 0)
        {
            const int want = int(std::min<std::uint32_t>(budget, 64));
            const int got = sock_->ReceiveMessagesOnConnection(h, msgs, want);
            if (got <= 0)
                break;
            for (int i = 0; i < got; ++i)
            {
                SteamNetworkingMessage_t* m = msgs[i];
                if (!violated)
                {
                    const bool reliable = (m->m_nFlags & k_nSteamNetworkingSend_Reliable) != 0;
                    const auto check = detail::check_inbound(config_, m->m_idxLane, reliable, std::uint32_t(m->m_cbSize));
                    if (check != detail::InboundCheck::Ok)
                    {
                        violated = true;
                        why = std::string(detail::to_string(check)) + " (lane " + std::to_string(m->m_idxLane) +
                              ", " + std::to_string(m->m_cbSize) + " bytes)";
                    }
                    else
                    {
                        Event e;
                        e.type = EventType::Message;
                        e.connection = id;
                        e.channel = ChannelId(m->m_idxLane);
                        const auto* p = static_cast<const std::uint8_t*>(m->m_pData);
                        e.payload.assign(p, p + m->m_cbSize);
                        e.received_us = m->m_usecTimeReceived;
                        ChannelCounters& cc = counters_.channels[m->m_idxLane];
                        ++cc.received_messages;
                        cc.received_bytes += std::uint32_t(m->m_cbSize);
                        out.push_back(std::move(e));
                    }
                }
                m->Release();
            }
            if (violated)
                break;
            budget -= std::uint32_t(got);
            if (got < want)
                break;
        }
        if (violated)
        {
            ++counters_.protocol_violations;
            closeByAdapter(id, close_code::kProtocolViolation, CloseReason::ProtocolViolation, why, out);
        }
    }

    // The adapter closes a connection the owner did not ask to close, and tells
    // the owner with a Disconnected event.
    void closeByAdapter(ConnectionId id, int code, CloseReason reason, const std::string& why, std::vector<Event>& out)
    {
        auto it = conns_.find(id);
        if (it == conns_.end())
            return;
        sock_->CloseConnection(it->second.handle, code, why.c_str(), false);
        Event e;
        e.type = EventType::Disconnected;
        e.connection = id;
        e.incoming = it->second.incoming;
        e.reason = reason;
        e.code = code;
        e.detail = why;
        out.push_back(std::move(e));
        byHandle_.erase(it->second.handle);
        conns_.erase(it);
    }

    ShutdownReport closeAll(std::uint32_t flushTimeoutMs)
    {
        ShutdownReport report;
        closed_ = true; // refuse incoming connections from here on
        const auto t0 = std::chrono::steady_clock::now();
        auto elapsedMs = [&] {
            return std::uint32_t(
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
        };
        // Reliable bytes the peer has not acknowledged; 0 once the library has
        // no status for it (closed by the peer meanwhile).
        auto backlog = [this](Conn& c) {
            if (!refresh(c))
                return std::uint64_t(0);
            std::uint64_t b = 0;
            for (std::size_t i = 0; i < c.est.size(); ++i)
                if (config_.channels[i].delivery != Delivery::Transient)
                    b += std::uint64_t(c.est[i].last.queued_bytes) + c.est[i].last.unacked_bytes;
            return b;
        };
        // Answer connection requests already waiting (refused with
        // kShutdown) and drop peers that closed. Nothing is delivered any more.
        const std::uint64_t refusedBefore = counters_.connections_refused;
        std::vector<Event> discarded;
        handleNotices(discarded);
        if (flushTimeoutMs > 0)
        {
            for (auto& kv : conns_)
                if (kv.second.state == ConnectionState::Connected)
                    sock_->FlushMessagesOnConnection(kv.second.handle);
            for (;;)
            {
                discarded.clear();
                handleNotices(discarded);
                std::uint64_t left = 0;
                for (auto& kv : conns_)
                    if (kv.second.state == ConnectionState::Connected)
                        left += backlog(kv.second);
                if (left == 0 || elapsedMs() >= flushTimeoutMs)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        for (auto& kv : conns_)
        {
            Conn& c = kv.second;
            if (c.state == ConnectionState::Connected)
            {
                const std::uint64_t left = backlog(c);
                report.unflushed_bytes += left;
                if (left)
                    report.flushed = false;
            }
            sock_->CloseConnection(c.handle, close_code::kShutdown, "shutdown", false);
            ++report.connections_closed;
        }
        // The library sends close packets from its own thread
        // (ConnectionState_FinWait, steamnetworkingsockets_connections.cpp:3188-3215),
        // and destroying the listen socket destroys its accepted connections
        // without one (:424-452). Give the close packets a moment to leave.
        const bool refusedAny = counters_.connections_refused != refusedBefore;
        if ((report.connections_closed || refusedAny) && listen_ != k_HSteamListenSocket_Invalid)
            std::this_thread::sleep_for(std::chrono::milliseconds(kCloseGraceMs));
        if (listen_ != k_HSteamListenSocket_Invalid)
            sock_->CloseListenSocket(listen_);
        listen_ = k_HSteamListenSocket_Invalid;
        conns_.clear();
        byHandle_.clear();
        pending_.clear();
        {
            std::lock_guard<std::mutex> lock(inboxMutex_);
            inbox_.clear();
        }
        report.waited_ms = elapsedMs();
        return report;
    }

    static constexpr int kCloseGraceMs = 30;

    detail::OwnerThread owner_;
    TransportConfig config_;
    std::int64_t instance_;
    ISteamNetworkingSockets* sock_;
    ISteamNetworkingUtils* utils_;
    std::vector<SteamNetworkingConfigValue_t> options_;
    std::vector<int> priorities_;
    std::vector<std::uint16_t> weights_;
    HSteamListenSocket listen_ = k_HSteamListenSocket_Invalid;
    bool closed_ = false;
    ConnectionId nextId_ = 1;
    std::map<ConnectionId, Conn> conns_;
    std::unordered_map<HSteamNetConnection, ConnectionId> byHandle_;
    std::mutex inboxMutex_;
    std::vector<SteamNetConnectionStatusChangedCallback_t> inbox_;
    std::vector<Event> pending_; // raised outside poll(), delivered by the next one
    std::size_t rotation_ = 0;
    TransportCounters counters_;
    // Bumped by calls from other threads, so atomic; everything else is owner-only.
    mutable std::atomic<std::uint64_t> wrongThreadCalls_{0};
};

void OnStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
{
    bool orphan = false;
    {
        Library& lib = library();
        std::lock_guard<std::mutex> lock(lib.mutex);
        auto it = lib.registry.find(info->m_info.m_nUserData);
        if (it != lib.registry.end())
            it->second->Enqueue(*info);
        else
            orphan = true;
    }
    // Its transport is gone: free the connection (outside our lock).
    if (orphan && info->m_info.m_eState != k_ESteamNetworkingConnectionState_None)
        SteamNetworkingSockets()->CloseConnection(info->m_hConn, 0, nullptr, false);
}

// Owns one library reference and the registry entry for a transport.
class RegisteredGnsTransport final : public Transport
{
public:
    RegisteredGnsTransport(std::unique_ptr<GnsTransport> impl, std::int64_t instance)
        : impl_(std::move(impl)), instance_(instance)
    {
    }
    ~RegisteredGnsTransport() override
    {
        impl_->shutdownForDestroy();
        Library& lib = library();
        std::lock_guard<std::mutex> lock(lib.mutex);
        lib.registry.erase(instance_);
        impl_.reset();
        if (--lib.refs == 0)
            GameNetworkingSockets_Kill();
    }
    bool listen(const char* a, std::string* e) override { return impl_->listen(a, e); }
    ConnectionId connect(const char* a, std::string* e) override { return impl_->connect(a, e); }
    SendResult send(ConnectionId c, ChannelId ch, const void* d, std::uint32_t s) override { return impl_->send(c, ch, d, s); }
    void flush(ConnectionId c) override { impl_->flush(c); }
    void poll(std::vector<Event>& out) override { impl_->poll(out); }
    bool close(ConnectionId c, int code, const char* d, bool f) override { return impl_->close(c, code, d, f); }
    ShutdownReport shutdown(std::uint32_t ms) override { return impl_->shutdown(ms); }
    bool status(ConnectionId c, ConnectionStatus& out) const override { return impl_->status(c, out); }
    TransportCounters counters() const override { return impl_->counters(); }
    const TransportConfig& config() const override { return impl_->config(); }
    std::int64_t now_us() const override { return impl_->now_us(); }

private:
    std::unique_ptr<GnsTransport> impl_;
    std::int64_t instance_;
};

} // namespace

std::unique_ptr<Transport> create_gns_transport(const TransportConfig& config, std::string* error)
{
    const std::string problem = validate(config);
    if (!problem.empty())
    {
        if (error)
            *error = problem;
        return nullptr;
    }
    Library& lib = library();
    std::lock_guard<std::mutex> lock(lib.mutex);
    if (lib.refs == 0)
    {
        SteamNetworkingErrMsg msg = {};
        if (!GameNetworkingSockets_Init(nullptr, msg))
        {
            if (error)
                *error = std::string("GameNetworkingSockets_Init failed: ") + msg;
            return nullptr;
        }
        SteamNetworkingUtils()->SetDebugOutputFunction(GnsLevel(g_logLevel.load()), &OnDebugOutput);
    }
    ++lib.refs;
    const std::int64_t instance = lib.nextInstance++;
    auto impl = std::make_unique<GnsTransport>(config, instance);
    lib.registry[instance] = impl.get();
    return std::make_unique<RegisteredGnsTransport>(std::move(impl), instance);
}

void set_log_sink(LogSink sink, LogLevel maxLevel)
{
    g_logLevel.store(int(maxLevel));
    g_logSink.store(sink);
    Library& lib = library();
    std::lock_guard<std::mutex> lock(lib.mutex);
    if (lib.refs > 0)
        SteamNetworkingUtils()->SetDebugOutputFunction(GnsLevel(int(maxLevel)), &OnDebugOutput);
}

bool set_gns_impairment(const GnsImpairment& m)
{
    Library& lib = library();
    std::lock_guard<std::mutex> lock(lib.mutex);
    if (lib.refs == 0)
        return false;
    ISteamNetworkingUtils* u = SteamNetworkingUtils();
    bool ok = true;
    ok &= u->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, m.loss_pct);
    ok &= u->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketReorder_Send, m.reorder_pct);
    ok &= u->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketReorder_Time, m.reorder_ms);
    ok &= u->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketDup_Send, m.dup_pct);
    ok &= u->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketDup_TimeMax, m.dup_ms);
    ok &= u->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, m.lag_ms);
    return ok;
}

int gns_library_users()
{
    Library& lib = library();
    std::lock_guard<std::mutex> lock(lib.mutex);
    return lib.refs;
}

} // namespace coop::net
