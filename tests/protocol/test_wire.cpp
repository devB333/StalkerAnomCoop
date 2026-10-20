// The protocol over the S04-T02 transport (fake and GNS loopback), with the
// v1 channel layout: handshake, traffic in both directions through decode()
// and the guard, a refused hello, and a peer sending a server-only message.
#include "fixtures.h"
#include "rig.h"

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
        const std::uint64_t epoch =
            t == MessageType::Hello || t == MessageType::Welcome || t == MessageType::Reject ? 0 : guard.epoch();
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
} // namespace coop::test
