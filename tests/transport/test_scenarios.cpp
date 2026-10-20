// Scenario tests, each run as fake/<name> (virtual time) and gns/<name>
// (GameNetworkingSockets over 127.0.0.1, real time).
#include "rig.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <thread>

namespace coop::test
{
namespace
{
// A paced producer: sends `total` messages on one channel, keeping the
// connection's backlog on it below `pace` bytes, as a well-behaved game-side
// producer would. Bulk refusals are retried on a later step.
struct Stream
{
    Transport* t = nullptr;
    ConnectionId conn = kNoConnection;
    ChannelId ch = 0;
    std::uint32_t total = 0;
    std::uint32_t pace = 32 * 1024;
    std::uint32_t (*size)(std::uint32_t seq) = nullptr;
    std::uint32_t next = 0;
    int backpressure = 0, overloaded = 0, other = 0;

    bool done() const { return next >= total; }
    void step(int maxPerStep = 64)
    {
        ConnectionStatus st;
        for (int k = 0; k < maxPerStep && next < total; ++k)
        {
            if (!t->status(conn, st))
                return;
            const ChannelBacklog& b = st.channels[ch];
            const std::uint32_t sz = size(next);
            if (b.queued_bytes + b.unacked_bytes + sz > pace)
                return;
            const auto p = Payload(next, sz);
            const SendResult r = t->send(conn, ch, p.data(), sz);
            if (r == SendResult::Sent)
                ++next;
            else
            {
                (r == SendResult::Backpressure ? backpressure : r == SendResult::Overloaded ? overloaded : other)++;
                return;
            }
        }
    }
};

// Checks the messages one node received on (conn, channel): intact payloads
// with sequence numbers 0, 1, 2, ... (or, for transient, increasing).
struct SeqCheck
{
    std::uint32_t count = 0, corrupt = 0, outOfOrder = 0, gaps = 0;
    std::uint32_t last = 0;
    bool any = false;
};

SeqCheck CheckSeq(const Node& n, ConnectionId conn, ChannelId ch, bool contiguous)
{
    SeqCheck s;
    for (const Event* e : n.Messages(conn, ch))
    {
        std::uint32_t seq = 0;
        if (!PayloadOk(e->payload, &seq))
        {
            ++s.corrupt;
            continue;
        }
        if (contiguous && seq != (s.any ? s.last + 1 : 0u))
            ++s.gaps;
        if (s.any && seq <= s.last)
            ++s.outOfOrder;
        s.last = seq;
        s.any = true;
        ++s.count;
    }
    return s;
}

std::uint32_t SmallSize(std::uint32_t seq) { return 16 + (seq * 37u) % 1200u; }
std::uint32_t ControlSize(std::uint32_t seq) { return seq % 100 == 99 ? 4096u : SmallSize(seq); }
std::uint32_t BulkSize(std::uint32_t) { return 32 * 1024; }
std::uint32_t Kb1(std::uint32_t) { return 1000; }

ConnectionId Conn(const Node& n)
{
    const Event* e = n.Find(EventType::Connected);
    return e ? e->connection : kNoConnection;
}

// Server-side connection ids in Connected order.
std::vector<ConnectionId> Incoming(const Node& n)
{
    std::vector<ConnectionId> ids;
    for (const Event& e : n.events)
        if (e.type == EventType::Connected && e.incoming)
            ids.push_back(e.connection);
    return ids;
}

bool SendSeq(Transport& t, ConnectionId c, ChannelId ch, std::uint32_t seq, std::uint32_t size)
{
    const auto p = Payload(seq, size);
    return t.send(c, ch, p.data(), std::uint32_t(p.size())) == SendResult::Sent;
}
} // namespace

// --------------------------------------------------------------- connecting

SCENARIO("connect-exchange")
{
    Node server{rig.make(TestConfig())}, a{rig.make(TestConfig())}, b{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a, &b}));
    const ConnectionId ca = Conn(a), cb = Conn(b);
    CHECK(ca != kNoConnection && cb != kNoConnection);
    CHECK(!a.Find(EventType::Connected)->incoming);
    const auto in = Incoming(server);
    REQUIRE(in.size() == 2);
    CHECK(in[0] != in[1]);

    // One message on every channel from each client, distinct sequence numbers.
    const std::uint32_t sizes[5] = {64, 4096, 64 * 1024, 1200, 512};
    for (ChannelId ch = 0; ch < 5; ++ch)
    {
        CHECK(SendSeq(*a.t, ca, ch, 100 + ch, sizes[ch]));
        CHECK(SendSeq(*b.t, cb, ch, 200 + ch, sizes[ch]));
    }
    REQUIRE(Pump(rig, {&server, &a, &b}, [&] { return server.Count(EventType::Message) >= 10; }, 5000));
    std::map<std::uint32_t, ConnectionId> from; // client base -> server conn id
    for (const Event& e : server.events)
        if (e.type == EventType::Message)
        {
            std::uint32_t seq = 0;
            CHECK(PayloadOk(e.payload, &seq));
            CHECK_MSG(seq % 100 == e.channel, "seq %u on channel %u", seq, e.channel);
            CHECK(e.payload.size() == sizes[e.channel]);
            const std::uint32_t base = seq / 100 * 100;
            if (from.count(base))
                CHECK(from[base] == e.connection);
            from[base] = e.connection;
        }
    REQUIRE(from.size() == 2);
    CHECK(from[100] != from[200]);

    // Replies reach the right client.
    CHECK(SendSeq(*server.t, from[100], kControl, 1, 64));
    CHECK(SendSeq(*server.t, from[200], kControl, 2, 64));
    REQUIRE(Pump(rig, {&server, &a, &b},
        [&] { return a.Count(EventType::Message) == 1 && b.Count(EventType::Message) == 1; }, 5000));
    std::uint32_t sa = 0, sb = 0;
    CHECK(PayloadOk(a.Find(EventType::Message)->payload, &sa) && sa == 1);
    CHECK(PayloadOk(b.Find(EventType::Message)->payload, &sb) && sb == 2);

    ConnectionStatus st;
    REQUIRE(server.t->status(in[0], st));
    CHECK(st.state == ConnectionState::Connected && st.incoming && st.channels.size() == 5);
    CHECK(!st.remote_address.empty());
    for (const Node* n : {&server, &a, &b})
        CheckEventOrder(*n, rig.name());
    CHECK(server.t->counters().connections_accepted == 2);
}

SCENARIO("third-refused")
{
    TransportConfig sc = TestConfig();
    sc.max_connections = 2;
    Node server{rig.make(sc)}, a{rig.make(TestConfig())}, b{rig.make(TestConfig())}, c{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a, &b}));
    // c connects to the address a used.
    ConnectionStatus st;
    REQUIRE(a.t->status(Conn(a), st));
    const ConnectionId cc = c.t->connect(st.remote_address.c_str());
    REQUIRE(cc != kNoConnection);
    REQUIRE(Pump(rig, {&server, &a, &b, &c}, [&] { return c.Find(EventType::Disconnected) != nullptr; }, 8000));
    const Event* d = c.Find(EventType::Disconnected);
    CHECK(d->connection == cc);
    CHECK_MSG(d->reason == CloseReason::Refused, "reason %s", to_string(d->reason));
    CHECK(d->code == close_code::kServerFull);
    CHECK(!c.Find(EventType::Connected));
    CHECK(server.t->counters().connections_refused == 1);
    CHECK(server.Count(EventType::Connected) == 2);
    // The two admitted peers are unaffected.
    CHECK(SendSeq(*a.t, Conn(a), kControl, 7, 32));
    REQUIRE(Pump(rig, {&server, &a, &b, &c}, [&] { return server.Count(EventType::Message) == 1; }, 3000));
    CHECK(server.Count(EventType::Disconnected) == 0);
    for (const Node* n : {&server, &a, &b, &c})
        CheckEventOrder(*n, rig.name());
}

// ----------------------------------------------------------------- ordering

SCENARIO("reliable-order-impaired")
{
    // Two clients, both directions, two critical channels and bulk, under
    // loss/reordering/duplication/lag (GNS) or lag and transient loss (fake).
    Node server{rig.make(TestConfig())}, a{rig.make(TestConfig())}, b{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a, &b}));
    GnsImpairment m;
    m.loss_pct = 10;
    m.reorder_pct = 10;
    m.reorder_ms = 25;
    m.dup_pct = 5;
    m.dup_ms = 20;
    m.lag_ms = 20;
    rig.impair(m);
    const std::uint32_t n = rig.real() ? 800 : 2000;
    std::vector<Stream> streams;
    auto in = Incoming(server);
    for (Node* c : {&a, &b})
    {
        streams.push_back({c->t.get(), Conn(*c), kControl, n, 24 * 1024, &ControlSize});
        streams.push_back({c->t.get(), Conn(*c), kActions, n, 24 * 1024, &SmallSize});
        streams.push_back({c->t.get(), Conn(*c), kBulk, 12, 160 * 1024, &BulkSize});
    }
    for (ConnectionId s : in)
        streams.push_back({server.t.get(), s, kControl, n, 24 * 1024, &ControlSize});
    const std::int64_t t0 = rig.now_ms();
    auto received = [&] {
        for (ConnectionId s : in)
            if (server.Messages(s, kControl).size() < n || server.Messages(s, kActions).size() < n ||
                server.Messages(s, kBulk).size() < 12)
                return false;
        return a.Messages(Conn(a), kControl).size() >= n && b.Messages(Conn(b), kControl).size() >= n;
    };
    int ticks = 0;
    const bool ok = Pump(rig, {&server, &a, &b}, [&] {
        for (Stream& s : streams)
            s.step();
        return ++ticks % 50 == 0 && received(); // the full check scans every event
    }, 120000);
    metric("elapsed_ms", double(rig.now_ms() - t0));
    REQUIRE_MSG(ok, "not all messages arrived");
    for (const Stream& s : streams)
        CHECK_MSG(s.overloaded == 0 && s.other == 0, "stream ch%u: overloaded %d other %d", s.ch, s.overloaded, s.other);
    for (ConnectionId s : in)
        for (ChannelId ch : {kControl, kActions, kBulk})
        {
            const SeqCheck k = CheckSeq(server, s, ch, true);
            CHECK_MSG(k.corrupt == 0 && k.gaps == 0 && k.outOfOrder == 0, "server conn %u ch %u: corrupt %u gaps %u order %u",
                s, ch, k.corrupt, k.gaps, k.outOfOrder);
        }
    for (Node* c : {&a, &b})
    {
        const SeqCheck k = CheckSeq(*c, Conn(*c), kControl, true);
        CHECK(k.corrupt == 0 && k.gaps == 0 && k.outOfOrder == 0 && k.count == n);
    }
    if (rig.real())
    {
        ConnectionStatus st;
        if (server.t->status(in[0], st))
            metric("quality_local", st.quality_local);
    }
    for (const Node* x : {&server, &a, &b})
        CheckEventOrder(*x, rig.name());
}

// ---------------------------------------------------- overload: transient

SCENARIO("transient-discard-keeps-critical")
{
    // Probe priority scenario through the adapter: critical pings (50/s,
    // control, echoed) beside 1000-byte snapshots offered at 400/s on a
    // 128 KB/s link (about 3x capacity). The adapter's stale/backlog rule
    // drops snapshots; pings must keep their baseline latency.
    TransportConfig cfg = TestConfig();
    cfg.send_rate_bytes_per_s = 128 * 1024;
    Node server{rig.make(cfg)}, a{rig.make(cfg)};
    REQUIRE(ConnectClients(rig, server, {&a}));
    GnsImpairment m;
    m.lag_ms = 15;
    rig.impair(m);
    const ConnectionId c = Conn(a), s = Incoming(server)[0];
    struct Ping
    {
        std::int64_t sent_us;
        bool loaded;
    };
    std::map<std::uint32_t, Ping> pings;
    std::vector<double> rttBase, rttLoaded, snapAge;
    std::uint32_t pingSeq = 0, snapSeq = 0, offered = 0, dropped = 0, refusedCritical = 0;
    std::uint32_t maxSnapQueued = 0;
    const std::int64_t t0 = rig.now_ms();
    const std::int64_t baseMs = 2000, loadMs = 4000;
    std::int64_t nextPing = t0;
    std::size_t serverSeen = 0, clientSeen = 0;
    while (rig.now_ms() - t0 < baseMs + loadMs + 1000)
    {
        const std::int64_t now = rig.now_ms();
        const bool loaded = now - t0 >= baseMs && now - t0 < baseMs + loadMs;
        if (now >= nextPing && now - t0 < baseMs + loadMs)
        {
            nextPing += 20;
            std::vector<std::uint8_t> p(16);
            const std::int64_t us = a.t->now_us();
            std::memcpy(p.data(), &pingSeq, 4);
            std::memcpy(p.data() + 8, &us, 8);
            const SendResult r = a.t->send(c, kControl, p.data(), 16);
            if (r == SendResult::Sent)
                pings[pingSeq] = {us, loaded};
            else
                ++refusedCritical;
            ++pingSeq;
        }
        if (loaded)
        {
            // Offer snapshots at 400/s regardless of how often we get here.
            const std::uint32_t due = std::uint32_t((now - t0 - baseMs) * 400 / 1000);
            while (offered < due)
            {
                std::vector<std::uint8_t> p(1000, 0x5a);
                const std::int64_t us = a.t->now_us();
                std::memcpy(p.data(), &snapSeq, 4);
                std::memcpy(p.data() + 8, &us, 8);
                ++snapSeq;
                ++offered;
                const SendResult r = a.t->send(c, kSnapshot, p.data(), 1000);
                if (r == SendResult::Dropped)
                    ++dropped;
                else
                    CHECK_MSG(r == SendResult::Sent, "snapshot send %s", to_string(r));
            }
            ConnectionStatus st;
            if (a.t->status(c, st))
                maxSnapQueued = std::max(maxSnapQueued, st.channels[kSnapshot].queued_bytes);
        }
        a.Poll();
        server.Poll();
        for (; serverSeen < server.events.size(); ++serverSeen)
        {
            const Event& e = server.events[serverSeen];
            if (e.type != EventType::Message)
                continue;
            if (e.channel == kControl) // echo
                CHECK(server.t->send(s, kControl, e.payload.data(), std::uint32_t(e.payload.size())) == SendResult::Sent);
            else if (e.channel == kSnapshot)
            {
                std::int64_t us = 0;
                std::memcpy(&us, e.payload.data() + 8, 8);
                snapAge.push_back(double(e.received_us - us) / 1000.0);
            }
        }
        for (; clientSeen < a.events.size(); ++clientSeen)
        {
            const Event& e = a.events[clientSeen];
            if (e.type != EventType::Message || e.channel != kControl)
                continue;
            std::uint32_t seq = 0;
            std::memcpy(&seq, e.payload.data(), 4);
            auto it = pings.find(seq);
            if (it == pings.end())
                continue;
            const double rtt = double(e.received_us - it->second.sent_us) / 1000.0;
            (it->second.loaded ? rttLoaded : rttBase).push_back(rtt);
            pings.erase(it);
        }
        rig.tick();
    }
    const Percentiles base = Summarise(rttBase), load = Summarise(rttLoaded), age = Summarise(snapAge);
    metric("rtt_base_p50_ms", base.p50);
    metric("rtt_base_p95_ms", base.p95);
    metric("rtt_loaded_p50_ms", load.p50);
    metric("rtt_loaded_p95_ms", load.p95);
    metric("rtt_loaded_p99_ms", load.p99);
    metric("snapshots_offered", offered);
    metric("snapshots_dropped", dropped);
    metric("snapshots_received", double(snapAge.size()));
    metric("snapshot_age_p50_ms", age.p50);
    metric("snapshot_age_p95_ms", age.p95);
    metric("snapshot_queue_max_bytes", maxSnapQueued);
    CHECK(refusedCritical == 0);
    CHECK_MSG(pings.empty(), "%zu pings unanswered", pings.size());
    CHECK(base.count >= 90 && load.count >= 190);
    CHECK_MSG(dropped > offered / 3, "only %u of %u snapshots dropped on a 3x oversubscribed link", dropped, offered);
    CHECK(maxSnapQueued <= cfg.channels[kSnapshot].max_backlog_bytes);
    // Critical latency holds near baseline (probe: within 5-9 ms at p50/p95
    // on a loss-free link); snapshots stay fresh.
    CHECK_MSG(load.p50 <= base.p50 + 15.0, "loaded p50 %.1f vs base %.1f", load.p50, base.p50);
    CHECK_MSG(load.p95 <= base.p95 + 25.0, "loaded p95 %.1f vs base %.1f", load.p95, base.p95);
    CHECK_MSG(age.p95 <= 50.0 + 15.0 + 40.0, "snapshot age p95 %.1f ms", age.p95);
    const auto cc = a.t->counters().channels[kSnapshot];
    CHECK(cc.dropped_stale + cc.dropped_backlog == dropped);
    metric("dropped_stale", double(cc.dropped_stale));
    metric("dropped_backlog", double(cc.dropped_backlog));
    CHECK(a.t->counters().send_failures == 0);
}

// ------------------------------------------------ overload: stalled receiver

namespace
{
// A receiver that buffers little, to stand for a peer that stops polling
// (a long level load) without waiting for megabytes to pile up.
TransportConfig SmallReceiver()
{
    TransportConfig c = TestConfig();
    c.channels[kBulk].max_message_bytes = 8 * 1024;
    c.recv_buffer_bytes = 32 * 1024;
    return c;
}
std::uint32_t Kb8(std::uint32_t) { return 8 * 1024; }
} // namespace

SCENARIO("critical-overload-closes")
{
    // A producer paced below the link rate (1000 B every 8 ms, ~125 KB/s of
    // 256 KB/s). Phase 1, the server polls: no overload. Phase 2, the server's
    // owner stalls: its library buffer fills, nothing more is acknowledged, the
    // critical backlog reaches its 64 KB bound and the adapter closes the
    // connection rather than drop a critical message.
    Node server{rig.make(SmallReceiver())}, a{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a}));
    const ConnectionId c = Conn(a);
    std::uint32_t sent = 0;
    SendResult last = SendResult::Sent;
    std::int64_t nextSend = rig.now_ms();
    auto produce = [&] {
        if (rig.now_ms() >= nextSend && last == SendResult::Sent)
        {
            nextSend += 8;
            const auto p = Payload(sent, 1000);
            last = a.t->send(c, kControl, p.data(), 1000);
            if (last == SendResult::Sent)
                ++sent;
        }
    };
    const std::int64_t t0 = rig.now_ms();
    Pump(rig, {&server, &a}, [&] {
        produce();
        return rig.now_ms() - t0 >= 1000;
    }, 2000);
    CHECK_MSG(last == SendResult::Sent, "overloaded while the server was polling: %s", to_string(last));
    const std::uint32_t healthy = sent;
    metric("critical_sent_while_polling", healthy);
    server.polling = false; // the server's owner stalls
    const std::int64_t t1 = rig.now_ms();
    const bool overloaded = Pump(rig, {&a}, [&] {
        produce();
        return last != SendResult::Sent;
    }, 20000);
    REQUIRE_MSG(overloaded, "critical backlog never reached its bound (%u sent)", sent);
    CHECK_MSG(last == SendResult::Overloaded, "last send %s", to_string(last));
    metric("stall_to_overload_ms", double(rig.now_ms() - t1));
    metric("critical_sent_during_stall", sent - healthy);
    // 32 KB in the server's buffer plus the 64 KB bound, roughly.
    CHECK(sent - healthy >= 64);
    ConnectionStatus st;
    CHECK(!a.t->status(c, st));
    CHECK(a.t->send(c, kControl, "x", 1) == SendResult::NotConnected);
    REQUIRE(Pump(rig, {&a}, [&] { return a.Find(EventType::Disconnected) != nullptr; }, 1000));
    const Event* d = a.Find(EventType::Disconnected);
    CHECK(d->reason == CloseReason::Overloaded && d->code == close_code::kOverloaded);
    CHECK(a.t->counters().channels[kControl].overloads == 1);
    CHECK(a.t->counters().send_failures == 0); // the library itself never refused
    // The server resumes: a contiguous prefix, then the peer's close.
    server.polling = true;
    REQUIRE(Pump(rig, {&server, &a}, [&] { return server.Find(EventType::Disconnected) != nullptr; }, 15000));
    const SeqCheck k = CheckSeq(server, Incoming(server)[0], kControl, true);
    CHECK(k.corrupt == 0 && k.gaps == 0 && k.outOfOrder == 0);
    CHECK(k.count >= healthy && k.count <= sent);
    metric("critical_delivered", k.count);
    const Event* sd = server.Find(EventType::Disconnected);
    CHECK_MSG(sd->reason == CloseReason::PeerClosed && sd->code == close_code::kOverloaded, "server saw %s %d",
        to_string(sd->reason), sd->code);
    for (const Node* x : {&server, &a})
        CheckEventOrder(*x, rig.name());
}

SCENARIO("bulk-backpressure-loses-nothing")
{
    Node server{rig.make(SmallReceiver())}, a{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a}));
    const ConnectionId c = Conn(a);
    server.polling = false;
    Stream bulk{a.t.get(), c, kBulk, 64, 1u << 30, &Kb8}; // no pacing: run into the bound
    const bool refused = Pump(rig, {&a}, [&] {
        bulk.step(4);
        return bulk.backpressure > 0;
    }, 20000);
    REQUIRE_MSG(refused, "bulk never refused (%u sent)", bulk.next);
    metric("bulk_sent_before_backpressure", bulk.next);
    ConnectionStatus st;
    REQUIRE(a.t->status(c, st));
    CHECK(st.state == ConnectionState::Connected);
    CHECK(st.channels[kBulk].queued_bytes + st.channels[kBulk].unacked_bytes <= 192 * 1024);
    // Hold the stall a little: still refused, still connected.
    PumpFor(rig, {&a}, 300);
    bulk.step(4);
    CHECK(a.Count(EventType::Disconnected) == 0);
    // Resume; every chunk arrives once, in order.
    server.polling = true;
    const bool all = Pump(rig, {&server, &a}, [&] {
        bulk.step(4);
        return server.Messages(Incoming(server)[0], kBulk).size() >= 64;
    }, 30000);
    REQUIRE(all);
    const SeqCheck k = CheckSeq(server, Incoming(server)[0], kBulk, true);
    CHECK(k.count == 64 && k.gaps == 0 && k.corrupt == 0 && k.outOfOrder == 0);
    CHECK(bulk.backpressure > 0 && bulk.overloaded == 0 && bulk.other == 0);
    metric("bulk_backpressure_refusals", bulk.backpressure);
    CHECK(a.t->counters().channels[kBulk].backpressure == std::uint64_t(bulk.backpressure));
    CHECK(server.Count(EventType::Disconnected) == 0 && a.Count(EventType::Disconnected) == 0);
}

SCENARIO("transient-bounded-while-peer-stalls")
{
    Node server{rig.make(SmallReceiver())}, a{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a}));
    const ConnectionId c = Conn(a);
    server.polling = false;
    std::uint32_t seq = 0, dropped = 0, maxQueued = 0;
    const std::int64_t t0 = rig.now_ms();
    Pump(rig, {&a}, [&] {
        for (int k = 0; k < 2; ++k)
        {
            std::vector<std::uint8_t> p(1000, 1);
            std::memcpy(p.data(), &seq, 4);
            ++seq;
            const SendResult r = a.t->send(c, kSnapshot, p.data(), 1000);
            CHECK(r == SendResult::Sent || r == SendResult::Dropped);
            dropped += r == SendResult::Dropped ? 1u : 0u;
        }
        ConnectionStatus st;
        if (a.t->status(c, st))
            maxQueued = std::max(maxQueued, st.channels[kSnapshot].queued_bytes);
        return rig.now_ms() - t0 >= 2000;
    }, 5000);
    metric("snapshots_offered", seq);
    metric("snapshots_dropped", dropped);
    metric("snapshot_queue_max_bytes", maxQueued);
    CHECK(dropped > 0);
    CHECK(maxQueued <= 16 * 1024);
    CHECK(a.Count(EventType::Disconnected) == 0);
    // Critical traffic still gets through once the server polls again.
    CHECK(SendSeq(*a.t, c, kControl, 0, 100));
    server.polling = true;
    REQUIRE(Pump(rig, {&server, &a}, [&] { return !server.Messages(Incoming(server)[0], kControl).empty(); }, 10000));
    CHECK(server.Count(EventType::Disconnected) == 0);
}

// ------------------------------------------------------- inbound validation

namespace
{
void ExpectViolation(Rig& rig, const TransportConfig& clientCfg, ChannelId badChannel, std::uint32_t badSize,
    const TransportConfig& serverCfg = TestConfig())
{
    Node server{rig.make(serverCfg)}, a{rig.make(clientCfg)};
    REQUIRE(ConnectClients(rig, server, {&a}));
    const ConnectionId c = Conn(a);
    for (std::uint32_t i = 0; i < 3; ++i)
        CHECK(SendSeq(*a.t, c, kControl, i, 64));
    REQUIRE(Pump(rig, {&server, &a}, [&] { return server.Count(EventType::Message) == 3; }, 5000));
    const auto bad = Payload(99, badSize);
    REQUIRE(a.t->send(c, badChannel, bad.data(), badSize) == SendResult::Sent);
    REQUIRE(Pump(rig, {&server, &a},
        [&] { return server.Find(EventType::Disconnected) && a.Find(EventType::Disconnected); }, 8000));
    const Event* sd = server.Find(EventType::Disconnected);
    CHECK_MSG(sd->reason == CloseReason::ProtocolViolation, "server: %s", to_string(sd->reason));
    CHECK(sd->code == close_code::kProtocolViolation);
    note(std::string("server detail: ") + sd->detail);
    const Event* cd = a.Find(EventType::Disconnected);
    CHECK_MSG(cd->reason == CloseReason::PeerClosed && cd->code == close_code::kProtocolViolation, "client: %s %d",
        to_string(cd->reason), cd->code);
    CHECK(server.Count(EventType::Message) == 3); // the bad message never reached the owner
    CHECK(server.t->counters().protocol_violations == 1);
    for (const Node* x : {&server, &a})
        CheckEventOrder(*x, rig.name());
}
} // namespace

SCENARIO("violation-unknown-channel")
{
    TransportConfig c = TestConfig();
    c.channels.push_back({"extra", Delivery::Critical, 0, 1, 1024, 8 * 1024, 0});
    ExpectViolation(rig, c, 5, 64);
}

SCENARIO("violation-reliable-on-transient-channel")
{
    TransportConfig c = TestConfig();
    c.channels[kSnapshot].delivery = Delivery::Critical;
    ExpectViolation(rig, c, kSnapshot, 64);
}

SCENARIO("violation-unreliable-on-critical-channel")
{
    TransportConfig c = TestConfig();
    c.channels[kActions].delivery = Delivery::Transient;
    c.channels[kActions].max_queue_ms = 0;
    ExpectViolation(rig, c, kActions, 64);
}

SCENARIO("violation-oversize")
{
    TransportConfig server = TestConfig();
    server.channels[kActions].max_message_bytes = 1024;
    ExpectViolation(rig, TestConfig(), kActions, 2000, server);
}

// ---------------------------------------------------------- close and ids

SCENARIO("last-words-before-close")
{
    Node server{rig.make(TestConfig())}, a{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a}));
    const ConnectionId c = Conn(a);
    for (std::uint32_t i = 0; i < 50; ++i)
        CHECK(SendSeq(*a.t, c, kControl, i, 500));
    CHECK(a.t->close(c, close_code::kSessionMin, "bye", true));
    REQUIRE(Pump(rig, {&server, &a}, [&] { return server.Find(EventType::Disconnected) != nullptr; }, 8000));
    const ConnectionId s = Incoming(server)[0];
    const SeqCheck k = CheckSeq(server, s, kControl, true);
    CHECK_MSG(k.count == 50 && k.gaps == 0, "%u of 50 before the close", k.count);
    const Event* d = server.Find(EventType::Disconnected);
    CHECK(d->reason == CloseReason::PeerClosed && d->code == close_code::kSessionMin);
    CHECK(d->detail == "bye");
    CHECK(a.Count(EventType::Disconnected) == 0); // the owner closed it itself
    PumpFor(rig, {&server, &a}, 100);
    CheckEventOrder(server, rig.name());
}

SCENARIO("ids-never-reused")
{
    Node server{rig.make(TestConfig())}, a{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a}));
    const ConnectionId first = Conn(a);
    ConnectionStatus st;
    REQUIRE(a.t->status(first, st));
    const std::string address = st.remote_address;
    CHECK(!a.t->close(first, 999, "bad code", false)); // outside the app range
    CHECK(a.t->status(first, st));
    CHECK(a.t->close(first, close_code::kNormal, "again", false));
    CHECK(!a.t->close(first, close_code::kNormal, "twice", false));
    CHECK(SendSeq(*a.t, first, kControl, 0, 10) == false);
    CHECK(a.t->send(first, kControl, "x", 1) == SendResult::NotConnected);
    CHECK(!a.t->status(first, st));
    REQUIRE(Pump(rig, {&server, &a}, [&] { return server.Find(EventType::Disconnected) != nullptr; }, 5000));
    const ConnectionId second = a.t->connect(address.c_str());
    CHECK(second != kNoConnection && second != first);
    REQUIRE(Pump(rig, {&server, &a}, [&] { return a.Count(EventType::Connected) == 2 && server.Count(EventType::Connected) == 2; }, 5000));
    const auto in = Incoming(server);
    CHECK(in.size() == 2 && in[0] != in[1]);
    CHECK(a.t->send(second, 7, "x", 1) == SendResult::InvalidChannel);
    std::vector<std::uint8_t> big(5000);
    CHECK(a.t->send(second, kControl, big.data(), 5000) == SendResult::TooLarge);
    CHECK(SendSeq(*a.t, second, kControl, 1, 10));
    for (const Node* x : {&server, &a})
        CheckEventOrder(*x, rig.name());
}

SCENARIO("receive-budget-per-poll")
{
    TransportConfig sc = TestConfig();
    sc.max_messages_per_poll = 10;
    Node server{rig.make(sc)}, a{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a}));
    for (std::uint32_t i = 0; i < 100; ++i)
        CHECK(SendSeq(*a.t, Conn(a), kActions, i, 100));
    PumpFor(rig, {&a}, 300); // let it all arrive before the server polls
    int maxPerPoll = 0, polls = 0;
    while (server.Count(EventType::Message) < 100 && polls < 1000)
    {
        const std::size_t before = server.events.size();
        server.Poll();
        int msgs = 0;
        for (std::size_t i = before; i < server.events.size(); ++i)
            msgs += server.events[i].type == EventType::Message ? 1 : 0;
        maxPerPoll = std::max(maxPerPoll, msgs);
        ++polls;
        rig.tick();
    }
    metric("polls", polls);
    CHECK(maxPerPoll <= 10);
    CHECK(polls >= 10);
    const SeqCheck k = CheckSeq(server, Incoming(server)[0], kActions, true);
    CHECK(k.count == 100 && k.gaps == 0);
}

// ----------------------------------------------------------------- liveness

SCENARIO("silent-peer-times-out")
{
    // Fake: the client crashes. GNS: the link goes silent both ways (100%
    // simulated loss). Both sides keep a reliable heartbeat, which GNS needs
    // to start its timeout clock (S04-T01 finding 1).
    TransportConfig cfg = TestConfig();
    cfg.connected_timeout_ms = 2000;
    Node server{rig.make(cfg)}, a{rig.make(cfg)};
    REQUIRE(ConnectClients(rig, server, {&a}));
    const ConnectionId c = Conn(a), s = Incoming(server)[0];
    const std::int64_t t0 = rig.now_ms();
    if (rig.real())
    {
        GnsImpairment m;
        m.loss_pct = 100;
        rig.impair(m);
    }
    else
        rig.crash(*a.t);
    std::int64_t nextBeat = t0;
    const bool ended = Pump(rig, {&server, &a}, [&] {
        if (rig.now_ms() >= nextBeat)
        {
            nextBeat += 100;
            SendSeq(*server.t, s, kControl, 0, 16);
            SendSeq(*a.t, c, kControl, 0, 16);
        }
        return server.Find(EventType::Disconnected) && (!rig.real() || a.Find(EventType::Disconnected));
    }, 10000);
    REQUIRE(ended);
    const double detectMs = double(rig.now_ms() - t0);
    metric("detected_after_ms", detectMs);
    CHECK_MSG(server.Find(EventType::Disconnected)->reason == CloseReason::Timeout, "server: %s",
        to_string(server.Find(EventType::Disconnected)->reason));
    if (rig.real())
        CHECK(a.Find(EventType::Disconnected)->reason == CloseReason::Timeout);
    CHECK_MSG(detectMs >= 1900 && detectMs <= 2000 + 1500, "detected after %.0f ms (timeout 2000)", detectMs);
}

// ---------------------------------------------------------------- shutdown

SCENARIO("shutdown-flushes-then-closes")
{
    Node server{rig.make(TestConfig())}, a{rig.make(TestConfig())}, b{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a, &b}));
    const auto in = Incoming(server);
    std::vector<Stream> streams;
    for (ConnectionId s : in)
        streams.push_back({server.t.get(), s, kControl, 300, 48 * 1024, &Kb1});
    REQUIRE(Pump(rig, {&server, &a, &b}, [&] {
        for (Stream& s : streams)
            s.step();
        return streams[0].done() && streams[1].done();
    }, 20000));
    // Everything is queued; some is not yet acknowledged. Shut down now.
    ConnectionStatus st;
    REQUIRE(server.t->status(in[0], st));
    metric("backlog_at_shutdown_bytes", double(st.channels[kControl].queued_bytes + st.channels[kControl].unacked_bytes));
    // Blocks the server's owner while it waits (the fake drives its virtual
    // clock meanwhile). The clients do not poll during it.
    const ShutdownReport r = server.t->shutdown(3000);
    metric("shutdown_waited_ms", r.waited_ms);
    CHECK(r.flushed && r.unflushed_bytes == 0);
    CHECK(r.connections_closed == 2);
    REQUIRE(Pump(rig, {&a, &b},
        [&] { return a.Find(EventType::Disconnected) && b.Find(EventType::Disconnected); }, 5000));
    for (Node* c : {&a, &b})
    {
        const SeqCheck k = CheckSeq(*c, Conn(*c), kControl, true);
        CHECK_MSG(k.count == 300 && k.gaps == 0, "client got %u of 300", k.count);
        const Event* d = c->Find(EventType::Disconnected);
        CHECK_MSG(d->reason == CloseReason::PeerShutdown && d->code == close_code::kShutdown, "client: %s %d",
            to_string(d->reason), d->code);
        CheckEventOrder(*c, rig.name());
    }
}

SCENARIO("closed-after-shutdown")
{
    Node server{rig.make(TestConfig())}, a{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a}));
    ConnectionStatus st;
    REQUIRE(a.t->status(Conn(a), st));
    const std::string address = st.remote_address;
    const ConnectionId s = Incoming(server)[0];
    const ShutdownReport r = server.t->shutdown(0);
    CHECK(r.connections_closed == 1);
    std::string error;
    CHECK(!server.t->listen(rig.address().c_str(), &error) && !error.empty());
    CHECK(server.t->connect(address.c_str()) == kNoConnection);
    CHECK(server.t->send(s, kControl, "x", 1) == SendResult::NotConnected);
    CHECK(!server.t->close(s, close_code::kNormal, "", false));
    const std::size_t before = server.events.size();
    server.Poll();
    CHECK(server.events.size() == before);
    CHECK(server.t->shutdown(100).connections_closed == 0);
    // A new client finds nobody listening.
    TransportConfig quick = TestConfig();
    quick.connect_timeout_ms = 1500;
    Node late{rig.make(quick)};
    REQUIRE(late.t->connect(address.c_str()) != kNoConnection);
    REQUIRE(Pump(rig, {&a, &late},
        [&] { return a.Find(EventType::Disconnected) && late.Find(EventType::Disconnected); }, 5000));
    CHECK(a.Find(EventType::Disconnected)->reason == CloseReason::PeerShutdown);
    CHECK_MSG(late.Find(EventType::Disconnected)->reason == CloseReason::ConnectFailed, "late: %s",
        to_string(late.Find(EventType::Disconnected)->reason));
    CHECK(late.t->counters().connections_failed == 1);
}

SCENARIO("destroy-without-shutdown")
{
    Node server{rig.make(TestConfig())}, a{rig.make(TestConfig())}, b{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a, &b}));
    for (std::uint32_t i = 0; i < 20; ++i)
        SendSeq(*a.t, Conn(a), kControl, i, 1000);
    b.t.reset(); // a client goes away without shutdown()
    REQUIRE(Pump(rig, {&server, &a}, [&] { return server.Find(EventType::Disconnected) != nullptr; }, 5000));
    CHECK(server.Find(EventType::Disconnected)->reason == CloseReason::PeerShutdown);
    server.t.reset(); // the server too, with a's messages possibly unread
    REQUIRE(Pump(rig, {&a}, [&] { return a.Find(EventType::Disconnected) != nullptr; }, 5000));
    CHECK(a.Find(EventType::Disconnected)->reason == CloseReason::PeerShutdown);
    CheckEventOrder(a, rig.name());
}

// --------------------------------------------------------------- threading

SCENARIO("owner-thread-only")
{
    Node server{rig.make(TestConfig())}, a{rig.make(TestConfig())};
    REQUIRE(ConnectClients(rig, server, {&a}));
    const ConnectionId c = Conn(a);
    std::vector<Event> stolen;
    ConnectionStatus st;
    std::string error;
    std::thread([&] {
        CHECK(a.t->send(c, kControl, "x", 1) == SendResult::WrongThread);
        a.t->poll(stolen);
        CHECK(a.t->connect("127.0.0.1:1", &error) == kNoConnection);
        CHECK(!a.t->close(c, close_code::kNormal, "", false));
        CHECK(!a.t->status(c, st));
        CHECK(!a.t->listen("127.0.0.1:2", &error));
        a.t->flush(c);
        CHECK(a.t->shutdown(0).connections_closed == 0);
    }).join();
    CHECK(stolen.empty());
    CHECK(a.t->counters().wrong_thread_calls == 8);
    // The owner carries on as if nothing happened.
    CHECK(SendSeq(*a.t, c, kControl, 0, 32));
    REQUIRE(Pump(rig, {&server, &a}, [&] { return server.Count(EventType::Message) == 1; }, 3000));
    CHECK(a.t->status(c, st) && st.state == ConnectionState::Connected);
}

} // namespace coop::test
