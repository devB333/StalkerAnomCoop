// The protocol over the S04-T02 transport (fake and GNS loopback), with the
// v1 channel layout: handshake, traffic in both directions through decode()
// and the guard, a refused hello, and a peer sending a server-only message.
#include "fixtures.h"
#include "rig.h"
#include "coop/proto/snapshot.h"

#include <random>

namespace coop::test
{
namespace
{
struct Peer
{
    Peer(Rig& rig, Role r) : node{rig.make(transport_config())}, guard(r), role(r) {}
    Node node;
    StreamGuard guard;
    SequenceCounter seq;
    Role role;
    std::vector<Decoded> got;
    std::vector<std::string> problems;
    std::size_t seen = 0;

    bool send(net::ConnectionId c, const Message& m)
    {
        std::vector<std::uint8_t> b;
        std::string error;
        const MessageType t = type_of(m);
        const std::uint64_t epoch = handshake_type(std::uint16_t(t)) ? 0 : guard.epoch();
        if (!encode(m, epoch, seq.next(channel_of(t)), b, &error))
        {
            problems.push_back("encode: " + error);
            return false;
        }
        return node.t->send(c, channel_of(t), b.data(), std::uint32_t(b.size())) == net::SendResult::Sent;
    }

    // Decodes and guards every new message; closes a peer that breaks the protocol.
    void process()
    {
        for (; seen < node.events.size(); ++seen)
        {
            const net::Event& e = node.events[seen];
            if (e.type != net::EventType::Message)
                continue;
            Decoded d;
            const DecodeResult r = decode(e.channel, role, e.payload.data(), e.payload.size(), d);
            if (r.ignorable)
                continue;
            if (!r.ok())
            {
                problems.push_back(std::string("decode: ") + to_string(r.error) + " at " + r.field);
                node.t->close(e.connection, net::close_code::kProtocolViolation, "malformed", false);
                continue;
            }
            const GuardResult g = guard.check(e.channel, d.header);
            if (g.verdict == Verdict::Violation)
            {
                problems.push_back(std::string("guard: ") + g.why);
                node.t->close(e.connection, net::close_code::kProtocolViolation, g.why, false);
                continue;
            }
            if (g.verdict == Verdict::Accept)
                got.push_back(std::move(d));
        }
    }
    template <class T>
    int count() const
    {
        int n = 0;
        for (const Decoded& d : got)
            n += std::holds_alternative<T>(d.body) ? 1 : 0;
        return n;
    }
    template <class T>
    const T* first() const
    {
        for (const Decoded& d : got)
            if (auto* p = std::get_if<T>(&d.body))
                return p;
        return nullptr;
    }
};

Hello MakeHello(std::uint64_t player, std::uint8_t content)
{
    Hello h;
    h.build_id = MakeDigest(1);
    h.content_digest = MakeDigest(content);
    h.player_id = MakeUuid(player);
    h.display_name = "p" + std::to_string(player);
    return h;
}

net::ConnectionId ConnOf(const Node& n)
{
    const net::Event* e = n.Find(net::EventType::Connected);
    return e ? e->connection : net::kNoConnection;
}
} // namespace

SCENARIO("wire/handshake-and-traffic")
{
    Peer server(rig, Role::Server), client(rig, Role::Client);
    REQUIRE(ConnectClients(rig, server.node, {&client.node}));
    const net::ConnectionId c = ConnOf(client.node), s = ConnOf(server.node);
    LocalBuild local;
    local.build_id = MakeDigest(1);
    local.content_digest = MakeDigest(2);
    REQUIRE(client.send(c, MakeHello(1, 2)));
    std::vector<Node*> nodes = {&server.node, &client.node};
    REQUIRE(Pump(rig, nodes, [&] {
        server.process();
        return server.count<Hello>() == 1;
    }, 5000));
    std::uint16_t minor = 0;
    CHECK(check_hello(*server.first<Hello>(), local, &minor) == 0);
    Welcome w;
    w.minor = minor;
    w.campaign_id = MakeUuid(10);
    w.session_id = MakeUuid(11);
    w.player_id = server.first<Hello>()->player_id;
    w.party_id = MakeUuid(12);
    w.slot = 0;
    w.slot_generation = 1;
    w.authority_epoch = 5;
    w.resume_token.assign(32, 7);
    REQUIRE(server.send(s, w));
    server.guard.begin_session(5);
    REQUIRE(Pump(rig, nodes, [&] {
        client.process();
        return client.count<Welcome>() == 1;
    }, 5000));
    client.guard.begin_session(client.first<Welcome>()->authority_epoch);
    CHECK(client.first<Welcome>()->player_id == MakeUuid(1));

    // Client: one action, three input batches (each repeating the last).
    ActionIntent ai;
    ai.operation_id = 1;
    ai.kind = 3;
    ai.target = {500, 1};
    CHECK(client.send(c, ai));
    InputBatch batch;
    for (int i = 0; i < 3; ++i)
    {
        Input in;
        in.tick = 100 + std::uint64_t(i);
        in.move_y = 1.f;
        batch.inputs.insert(batch.inputs.begin(), in);
        CHECK(client.send(c, batch));
    }
    // Server: a spawn, a snapshot of it, the action's result.
    Spawn sp;
    sp.entity = {500, 1};
    sp.archetype = "m_boar_e";
    sp.transform = {1, 2, 3, 0};
    CHECK(server.send(s, sp));
    Snapshot sn;
    sn.server_tick = 102;
    sn.snapshot_id = 1;
    sn.last_input = 3;
    SnapshotEntity se;
    se.entity = {500, 1};
    se.components = kTransform | kHealth;
    se.health = 10000;
    sn.entities.push_back(se);
    CHECK(server.send(s, sn));
    ActionResult ar;
    ar.operation_id = 1;
    ar.outcome = ActionOutcome::Accepted;
    CHECK(server.send(s, ar));
    REQUIRE(Pump(rig, nodes, [&] {
        server.process();
        client.process();
        bool lastBatch = false; // the third batch carries all three inputs
        for (const Decoded& d : server.got)
            lastBatch |= std::holds_alternative<InputBatch>(d.body) && d.header.sequence == 3;
        return server.count<ActionIntent>() == 1 && lastBatch &&
               client.count<Spawn>() == 1 && client.count<ActionResult>() == 1 && client.count<Snapshot>() == 1;
    }, 5000));
    // The server applies only fresh inputs; the client tracks the entity.
    std::uint64_t lastInput = 0;
    int applied = 0;
    for (const Decoded& d : server.got)
        if (auto* b = std::get_if<InputBatch>(&d.body))
            for (auto& [seqNo, in] : FreshInputs(*b, d.header.sequence, lastInput))
            {
                CHECK(in.tick == 99 + seqNo);
                lastInput = seqNo;
                ++applied;
            }
    CHECK(applied == 3 && lastInput == 3);
    EntityTable table(1024);
    CHECK(table.spawn(client.first<Spawn>()->entity) == EntityTable::Apply::Ok);
    CHECK(table.classify(client.first<Snapshot>()->entities[0].entity) == EntityTable::Ref::Current);
    CHECK(server.problems.empty() && client.problems.empty());
    for (const auto& p : server.problems)
        note("server: " + p);
}

SCENARIO("wire/refused-hello")
{
    Peer server(rig, Role::Server), client(rig, Role::Client);
    REQUIRE(ConnectClients(rig, server.node, {&client.node}));
    const net::ConnectionId c = ConnOf(client.node), s = ConnOf(server.node);
    LocalBuild local;
    local.build_id = MakeDigest(1);
    local.content_digest = MakeDigest(2);
    REQUIRE(client.send(c, MakeHello(1, 99))); // different content
    std::vector<Node*> nodes = {&server.node, &client.node};
    REQUIRE(Pump(rig, nodes, [&] {
        server.process();
        return server.count<Hello>() == 1;
    }, 5000));
    const std::uint16_t why = check_hello(*server.first<Hello>(), local, nullptr);
    CHECK(why == std::uint16_t(RejectReason::ContentMismatch));
    Reject rj;
    rj.reason = why;
    rj.detail = "content digest differs";
    REQUIRE(server.send(s, rj));
    CHECK(server.node.t->close(s, net::close_code::kSessionMin, "rejected", true));
    REQUIRE(Pump(rig, nodes, [&] {
        client.process();
        return client.node.Find(net::EventType::Disconnected) != nullptr;
    }, 5000));
    REQUIRE(client.count<Reject>() == 1);
    CHECK(client.first<Reject>()->reason == std::uint16_t(RejectReason::ContentMismatch));
    CHECK(client.node.Find(net::EventType::Disconnected)->code == net::close_code::kSessionMin);
    CHECK(!client.guard.in_session());
}

SCENARIO("wire/server-only-message-from-client")
{
    // A hostile client sends a Welcome (server-only) and then, on another
    // connection, an action before any handshake. Both are closed; nothing
    // reaches the server's accepted messages.
    Peer server(rig, Role::Server), a(rig, Role::Client), b(rig, Role::Client);
    REQUIRE(ConnectClients(rig, server.node, {&a.node, &b.node}));
    std::vector<std::uint8_t> bytes;
    Welcome w = std::get<Welcome>(AllFixtures()[2].message);
    REQUIRE(encode(w, 0, 1, bytes));
    CHECK(a.node.t->send(ConnOf(a.node), kControl, bytes.data(), std::uint32_t(bytes.size())) == net::SendResult::Sent);
    ActionIntent ai;
    ai.operation_id = 9;
    ai.kind = 1;
    REQUIRE(encode(ai, 5, 1, bytes));
    CHECK(b.node.t->send(ConnOf(b.node), kActions, bytes.data(), std::uint32_t(bytes.size())) == net::SendResult::Sent);
    std::vector<Node*> nodes = {&server.node, &a.node, &b.node};
    REQUIRE(Pump(rig, nodes, [&] {
        server.process();
        return a.node.Find(net::EventType::Disconnected) && b.node.Find(net::EventType::Disconnected);
    }, 5000));
    CHECK(server.got.empty());
    REQUIRE(server.problems.size() == 2);
    note("server: " + server.problems[0]);
    note("server: " + server.problems[1]);
    CHECK(a.node.Find(net::EventType::Disconnected)->code == net::close_code::kProtocolViolation);
    CHECK(b.node.Find(net::EventType::Disconnected)->code == net::close_code::kProtocolViolation);
}
// S06-T02: real snapshots (SnapshotSender / SnapshotReceiver) and lifecycle
// events over the v1 layout with 10 % loss and 25 ms lag. Phase A sends
// lifecycle only; phase B adds snapshots of 400 moving entities (8 parts,
// as the server sends them) at 40 Hz, more than the link's send rate. Reliable lifecycle latency must stay near
// phase A's (obsolete motion is dropped, never queued ahead of it), every
// lifecycle event arrives once and in order, and every snapshot the client
// completes equals the server's frame for it.
SCENARIO("wire/snapshots-beside-lifecycle-under-loss")
{
    Peer server(rig, Role::Server), client(rig, Role::Client);
    REQUIRE(ConnectClients(rig, server.node, {&client.node}));
    const net::ConnectionId c = ConnOf(client.node), s = ConnOf(server.node);
    server.guard.begin_session(5);
    client.guard.begin_session(5);
    GnsImpairment m;
    m.loss_pct = 10.f;
    m.lag_ms = 25;
    rig.impair(m);
    std::vector<Node*> nodes = {&server.node, &client.node};

    SnapshotSenderConfig sc;
    sc.max_parts = 8;
    SnapshotSender sender(sc);
    SnapshotReceiver receiver;
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    std::vector<EntityState> world;
    for (std::uint64_t i = 1; i <= 400; ++i)
    {
        EntityState e;
        e.entity = {i, 1};
        e.components = kTransform | kVelocity | kHealth | kStateBits;
        e.transform = {float(i), 0.f, 0.f, 0.f};
        e.health = 10000;
        world.push_back(e);
    }
    std::map<std::uint64_t, std::int64_t> spawn_sent; // commit -> send time (us)
    std::vector<double> base_ms, loaded_ms;
    std::uint64_t commit = 0, next_expected = 1, out_of_order = 0;
    std::uint64_t snapshots_sent = 0, parts_sent = 0, parts_dropped = 0, completed = 0, mismatches = 0;
    std::uint64_t tick = 0;
    const std::int64_t t0 = rig.now_ms(), phase_a = 4000, phase_b = 4000;
    std::int64_t next_spawn = t0, next_snapshot = t0 + phase_a;
    std::size_t sseen = 0, cseen = 0;
    while (rig.now_ms() - t0 < phase_a + phase_b + 1500)
    {
        const std::int64_t now = rig.now_ms();
        const bool sending = now - t0 < phase_a + phase_b;
        if (sending && now >= next_spawn)
        {
            next_spawn += 25;
            Spawn sp;
            sp.entity = {100000 + commit + 1, 1};
            sp.archetype = "medkit";
            sp.commit_id = ++commit;
            sp.state.assign(200, 0x33);
            spawn_sent[commit] = server.node.t->now_us();
            CHECK(server.send(s, sp));
        }
        if (sending && now >= next_snapshot)
        {
            next_snapshot += 25;
            ++tick;
            for (EntityState& e : world)
                e.transform.x += 0.05f + 0.01f * u01(rng);
            std::vector<Snapshot> parts;
            sender.build(tick, 0, world, parts);
            ++snapshots_sent;
            bool congested = false;
            for (const Snapshot& p : parts)
            {
                ++parts_sent;
                if (!server.send(s, p))
                {
                    ++parts_dropped;
                    congested = true;
                }
            }
            // As the server does: dropped parts or a snapshot queue older
            // than one 40 Hz interval halve the budget.
            net::ConnectionStatus st;
            if (server.node.t->status(s, st) && st.channels.size() > kSnapshot && st.channels[kSnapshot].queue_ms > 25)
                congested = true;
            sender.sent(congested);
        }
        rig.tick();
        server.node.Poll();
        client.node.Poll();
        client.process();
        server.process();
        for (; cseen < client.got.size(); ++cseen)
        {
            const Decoded& d = client.got[cseen];
            if (const auto* sp = std::get_if<Spawn>(&d.body))
            {
                if (sp->commit_id != next_expected)
                    ++out_of_order;
                next_expected = sp->commit_id + 1;
                const auto it = spawn_sent.find(sp->commit_id);
                if (it != spawn_sent.end())
                {
                    const double ms = double(client.node.t->now_us() - it->second) / 1000.0;
                    (sp->commit_id * 25 < std::uint64_t(phase_a) ? base_ms : loaded_ms).push_back(ms);
                }
            }
            else if (const auto* sn = std::get_if<Snapshot>(&d.body))
            {
                SnapshotUpdate u;
                if (receiver.offer(*sn, u) == SnapshotVerdict::Completed)
                {
                    ++completed;
                    const SnapshotFrame* mine = receiver.frame(sn->snapshot_id);
                    const SnapshotFrame* theirs = sender.frame(sn->snapshot_id);
                    if (mine && theirs && mine->size() != theirs->size())
                        ++mismatches;
                    else if (mine && theirs)
                        for (const auto& e : *mine)
                        {
                            const auto it = theirs->find(e.first);
                            if (it == theirs->end() || it->second.transform.x != e.second.transform.x ||
                                !(it->second.entity == e.second.entity))
                                ++mismatches;
                        }
                }
            }
        }
        SnapshotAck ack;
        if (receiver.take_ack(ack))
            client.send(c, ack);
        for (; sseen < server.got.size(); ++sseen)
            if (const auto* a = std::get_if<SnapshotAck>(&server.got[sseen].body))
                sender.on_ack(*a);
    }
    const Percentiles base = Summarise(base_ms), loaded = Summarise(loaded_ms);
    // With 10 % loss some lifecycle messages are retransmitted in both
    // phases; what must not change is how many are slow.
    auto slow = [](const std::vector<double>& v) {
        std::size_t n = 0;
        for (double x : v)
            n += x > 100.0 ? 1 : 0;
        return v.empty() ? 0.0 : double(n) / double(v.size());
    };
    metric("lifecycle_base_slow_fraction", slow(base_ms));
    metric("lifecycle_loaded_slow_fraction", slow(loaded_ms));
    metric("lifecycle_base_p50_ms", base.p50);
    metric("lifecycle_base_p95_ms", base.p95);
    metric("lifecycle_loaded_p50_ms", loaded.p50);
    metric("lifecycle_loaded_p95_ms", loaded.p95);
    metric("lifecycle_loaded_max_ms", loaded.max);
    metric("snapshots_sent", double(snapshots_sent));
    metric("snapshot_parts_sent", double(parts_sent));
    metric("snapshot_parts_dropped_at_send", double(parts_dropped));
    metric("snapshots_completed", double(completed));
    metric("snapshot_full", double(sender.counters().full));
    metric("snapshot_delta", double(sender.counters().delta));
    metric("snapshot_deferred", double(sender.counters().deferred));
    metric("snapshot_congestions", double(sender.counters().congestions));
    metric("snapshot_min_budget", double(sender.counters().min_budget));
    metric("snapshot_max_deferrals", double(sender.counters().max_deferrals));
    metric("snapshot_bytes_per_s", double(sender.counters().bytes) / (double(phase_b) / 1000.0));
    metric("receiver_missing_baseline", double(receiver.counters().missing_baseline));
    CHECK_MSG(next_expected == commit + 1, "lifecycle delivered up to %llu of %llu",
        (unsigned long long)(next_expected - 1), (unsigned long long)commit);
    CHECK(out_of_order == 0);
    CHECK_MSG(mismatches == 0, "%llu entities differ in completed snapshots", (unsigned long long)mismatches);
    CHECK(base.count >= 150 && loaded.count >= 150);
    CHECK_MSG(loaded.p50 <= base.p50 + 25.0, "lifecycle p50 loaded %.1f vs %.1f ms", loaded.p50, base.p50);
    CHECK_MSG(slow(loaded_ms) <= slow(base_ms) + 0.10, "lifecycle over 100 ms: loaded %.3f vs %.3f", slow(loaded_ms),
        slow(base_ms));
    CHECK_MSG(completed >= snapshots_sent / 3, "%llu of %llu snapshots completed", (unsigned long long)completed,
        (unsigned long long)snapshots_sent); // the budget adapts to the link
    CHECK(server.problems.empty() && client.problems.empty());
    GnsImpairment none;
    rig.impair(none);
}

} // namespace coop::test
