// Sessions bound to canonical players (S04-T04), on the fake and on GNS.
#include "coop/session/client.h"
#include "coop/session/server.h"
#include "rig.h"

#include <algorithm>
#include <memory>
#include <optional>

namespace coop::test
{
using namespace coop::session;
using proto::Uuid128;

namespace
{
struct Cast
{
    Roster roster;
    std::vector<Uuid128> ids;
    std::vector<PublicKey> pubs;
    std::vector<std::shared_ptr<PrivateKey>> keys;
};

Cast MakeCast(int n)
{
    Cast c;
    for (int i = 0; i < n; ++i)
    {
        auto k = std::make_shared<PrivateKey>();
        RosterEntry e;
        REQUIRE(generate_key(*k, e.key));
        e.player_id = random_uuid();
        e.display_name = "Player " + std::to_string(i + 1);
        REQUIRE(c.roster.add(e));
        c.ids.push_back(e.player_id);
        c.pubs.push_back(e.key);
        c.keys.push_back(k);
    }
    return c;
}

proto::LocalBuild Build(std::uint8_t content = 2)
{
    proto::LocalBuild b;
    b.build_id.fill(1);
    b.content_digest.fill(content);
    return b;
}

ServerConfig SConfig()
{
    ServerConfig c;
    c.build = Build();
    c.campaign_id = {10, 10};
    c.party_id = {11, 11};
    c.authority_epoch = 5;
    return c;
}

PrivateKey Copy(const PrivateKey& k)
{
    // Keys cannot be copied; round-trip through a key file for a second holder.
    const std::string path = "copy-" + std::to_string(reinterpret_cast<std::uintptr_t>(&k)) + ".key";
    Uuid128 who = {1, 1};
    PrivateKey out;
    PublicKey pub;
    REQUIRE(save_key_file(path, who, k, nullptr) && load_key_file(path, who, out, pub, nullptr));
    std::remove(path.c_str());
    return out;
}

struct Srv
{
    std::unique_ptr<net::Transport> t;
    std::unique_ptr<Server> s;
    std::vector<SessionEvent> ev;
    std::string address;
    // max_connections: the transport's own cap on open connections, which
    // refuses (1001) before the session layer sees a peer.
    Srv(Rig& rig, const Cast& cast, const ServerConfig& cfg = SConfig(), std::uint32_t max_connections = 4)
    {
        net::TransportConfig tc = proto::transport_config();
        tc.max_connections = max_connections;
        t = rig.make(tc);
        address = rig.address();
        REQUIRE(t->listen(address.c_str()));
        s = std::make_unique<Server>(*t, cfg, cast.roster);
    }
    void update() { s->update(ev); }
    int count(SessionEventType type) const
    {
        return int(std::count_if(ev.begin(), ev.end(), [&](const SessionEvent& e) { return e.type == type; }));
    }
};

struct Cli
{
    std::unique_ptr<net::Transport> t;
    std::unique_ptr<Client> c;
    std::vector<ClientEvent> ev;
    Cli(Rig& rig, const Uuid128& player, PrivateKey key, const proto::LocalBuild& build = Build())
    {
        t = rig.make(proto::transport_config());
        ClientConfig cfg;
        cfg.build = build;
        cfg.player_id = player;
        cfg.display_name = "p";
        cfg.expected_campaign = {10, 10};
        c = std::make_unique<Client>(*t, cfg, std::move(key));
    }
    void update() { c->update(ev); }
    const ClientEvent* find(ClientEventType type) const
    {
        for (const ClientEvent& e : ev)
            if (e.type == type)
                return &e;
        return nullptr;
    }
};

// A client that speaks the codec directly, for attacks and broken peers.
struct Raw
{
    Node node;
    proto::SequenceCounter seq;
    Raw(Rig& rig) : node{rig.make(proto::transport_config())} {}
    net::ConnectionId conn() const
    {
        const net::Event* e = node.Find(net::EventType::Connected);
        return e ? e->connection : net::kNoConnection;
    }
    bool send(const proto::Message& m, std::uint64_t epoch = 0)
    {
        std::vector<std::uint8_t> b;
        const proto::MessageType t = proto::type_of(m);
        if (!proto::encode(m, epoch, seq.next(proto::channel_of(t)), b))
            return false;
        return node.t->send(conn(), proto::channel_of(t), b.data(), std::uint32_t(b.size())) == net::SendResult::Sent;
    }
    template <class T>
    std::optional<T> first() const
    {
        for (const net::Event& e : node.events)
            if (e.type == net::EventType::Message)
            {
                proto::Decoded d;
                if (proto::decode(e.channel, proto::Role::Client, e.payload.data(), e.payload.size(), d).ok())
                    if (auto* p = std::get_if<T>(&d.body))
                        return *p;
            }
        return std::nullopt;
    }
    const net::Event* ended() const { return node.Find(net::EventType::Disconnected); }
};

bool Run(Rig& rig, const std::function<bool()>& step, std::int64_t ms) { return Pump(rig, {}, step, ms); }

proto::Hello HelloFor(const Uuid128& player, const proto::LocalBuild& b = Build())
{
    proto::Hello h;
    h.build_id = b.build_id;
    h.content_digest = b.content_digest;
    h.player_id = player;
    h.display_name = "raw";
    random_bytes(h.client_nonce.data(), 32);
    return h;
}

const SessionEvent* Joined(const Srv& s, const Uuid128& player)
{
    for (const SessionEvent& e : s.ev)
        if (e.type == SessionEventType::Joined && e.player == player)
            return &e;
    return nullptr;
}
} // namespace

SCENARIO("session/two-players-join")
{
    Cast cast = MakeCast(2);
    Srv srv(rig, cast);
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0])), b(rig, cast.ids[1], Copy(*cast.keys[1]));
    REQUIRE(a.c->connect(srv.address) && b.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        b.update();
        return a.c->state() == ClientState::Active && b.c->state() == ClientState::Active;
    }, 8000));
    const proto::Welcome& wa = a.c->welcome();
    const proto::Welcome& wb = b.c->welcome();
    CHECK(wa.player_id == cast.ids[0] && wb.player_id == cast.ids[1]);
    CHECK(wa.slot != wb.slot);
    CHECK(wa.session_id != wb.session_id);
    CHECK(wa.campaign_id == (Uuid128{10, 10}) && wa.party_id == (Uuid128{11, 11}) && wa.authority_epoch == 5);
    CHECK(srv.count(SessionEventType::Joined) == 2);
    REQUIRE(Joined(srv, cast.ids[0]) && Joined(srv, cast.ids[1]));
    CHECK(Joined(srv, cast.ids[0])->slot == wa.slot && Joined(srv, cast.ids[0])->slot_generation == wa.slot_generation);
    // Each message is attributed to the player its connection is bound to.
    proto::ActionIntent ai;
    ai.operation_id = 7;
    ai.kind = 2;
    CHECK(a.c->send(ai));
    proto::InputBatch ib;
    ib.inputs.push_back(proto::Input{});
    CHECK(b.c->send(ib));
    proto::Spawn sp;
    sp.entity = {9, 1};
    sp.archetype = "actor";
    CHECK(srv.s->send(cast.ids[0], sp));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        b.update();
        return srv.count(SessionEventType::Message) == 2 && a.find(ClientEventType::Message);
    }, 5000));
    for (const SessionEvent& e : srv.ev)
        if (e.type == SessionEventType::Message)
        {
            if (std::holds_alternative<proto::ActionIntent>(e.message.body))
                CHECK(e.player == cast.ids[0] && e.slot == wa.slot);
            else
                CHECK(std::holds_alternative<proto::InputBatch>(e.message.body) && e.player == cast.ids[1]);
        }
    PumpFor(rig, {}, 50);
    b.update();
    CHECK(b.find(ClientEventType::Message) == nullptr); // the spawn went to A only
    const auto players = srv.s->players();
    CHECK(players.size() == 2 && players[0].connected && players[1].connected);
    CHECK(srv.s->counters().joins == 2 && srv.s->counters().violations == 0);
}

SCENARIO("session/impersonation-refused")
{
    Cast cast = MakeCast(2);
    Cast stranger = MakeCast(1);
    Srv srv(rig, cast);
    // B's key claiming to be A; a key nobody listed claiming A; a stranger.
    Cli fakeA(rig, cast.ids[0], Copy(*cast.keys[1]));
    Cli noKey(rig, cast.ids[0], Copy(*stranger.keys[0]));
    Cli unknown(rig, stranger.ids[0], Copy(*stranger.keys[0]));
    for (Cli* c : {&fakeA, &noKey, &unknown})
        REQUIRE(c->c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        for (Cli* c : {&fakeA, &noKey, &unknown})
            c->update();
        return fakeA.find(ClientEventType::Ended) && noKey.find(ClientEventType::Ended) && unknown.find(ClientEventType::Ended);
    }, 8000));
    for (Cli* c : {&fakeA, &noKey, &unknown})
    {
        REQUIRE(c->find(ClientEventType::Rejected));
        CHECK(c->find(ClientEventType::Rejected)->reject.reason == std::uint16_t(proto::RejectReason::AuthFailed));
        CHECK(c->find(ClientEventType::Ended)->code == session::close_code::kRejected);
    }
    CHECK(srv.count(SessionEventType::Joined) == 0);
    CHECK(srv.s->counters().rejected_auth == 3);
    CHECK(srv.s->players().empty());
    // The real A still gets in, to the first slot.
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(a.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        return a.c->state() == ClientState::Active;
    }, 5000));
    CHECK(a.c->welcome().slot == 0 && a.c->welcome().slot_generation == 1);
}

SCENARIO("session/replayed-proof-refused")
{
    Cast cast = MakeCast(1);
    Srv srv(rig, cast);
    // A legitimate exchange, recorded by an eavesdropper.
    Raw honest(rig);
    REQUIRE(honest.node.t->connect(srv.address.c_str()) != net::kNoConnection);
    REQUIRE(Run(rig, [&] {
        srv.update();
        honest.node.Poll();
        return honest.conn() != net::kNoConnection;
    }, 5000));
    const proto::Hello hello = HelloFor(cast.ids[0]);
    REQUIRE(honest.send(hello));
    REQUIRE(Run(rig, [&] {
        srv.update();
        honest.node.Poll();
        return honest.first<proto::AuthChallenge>().has_value();
    }, 5000));
    proto::AuthProof proof;
    REQUIRE(make_proof(hello, *honest.first<proto::AuthChallenge>(), *cast.keys[0], proof));
    REQUIRE(honest.send(proof));
    REQUIRE(Run(rig, [&] {
        srv.update();
        honest.node.Poll();
        return honest.first<proto::Welcome>().has_value();
    }, 5000));
    // The attacker replays the recorded Hello and proof on a new connection.
    Raw attacker(rig);
    REQUIRE(attacker.node.t->connect(srv.address.c_str()) != net::kNoConnection);
    REQUIRE(Run(rig, [&] {
        srv.update();
        honest.node.Poll();
        attacker.node.Poll();
        return attacker.conn() != net::kNoConnection;
    }, 5000));
    REQUIRE(attacker.send(hello));
    REQUIRE(Run(rig, [&] {
        srv.update();
        attacker.node.Poll();
        return attacker.first<proto::AuthChallenge>().has_value();
    }, 5000));
    CHECK(attacker.first<proto::AuthChallenge>()->server_nonce != honest.first<proto::AuthChallenge>()->server_nonce);
    REQUIRE(attacker.send(proof));
    REQUIRE(Run(rig, [&] {
        srv.update();
        honest.node.Poll();
        attacker.node.Poll();
        return attacker.ended() != nullptr;
    }, 5000));
    REQUIRE(attacker.first<proto::Reject>());
    CHECK(attacker.first<proto::Reject>()->reason == std::uint16_t(proto::RejectReason::AuthFailed));
    CHECK(srv.s->counters().rejected_auth == 1 && srv.s->counters().joins == 1);
    CHECK(honest.ended() == nullptr); // the real session is untouched
}

SCENARIO("session/third-player-and-slots")
{
    Cast cast = MakeCast(3);
    Srv srv(rig, cast);
    auto join = [&](Cli& c) {
        REQUIRE(c.c->connect(srv.address));
        return Run(rig, [&] {
            srv.update();
            c.update();
            return c.c->state() == ClientState::Active || c.c->state() == ClientState::Ended;
        }, 5000);
    };
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0])), b(rig, cast.ids[1], Copy(*cast.keys[1]));
    REQUIRE(join(a) && join(b));
    Cli c(rig, cast.ids[2], Copy(*cast.keys[2]));
    REQUIRE(join(c));
    REQUIRE(c.find(ClientEventType::Rejected));
    CHECK(c.find(ClientEventType::Rejected)->reject.reason == std::uint16_t(proto::RejectReason::ServerFull));
    // A leaves: the slot stays A's, so C is still refused, and A comes back to it.
    const std::uint8_t slotA = a.c->welcome().slot;
    const std::uint32_t genA = a.c->welcome().slot_generation;
    a.c->close(net::close_code::kNormal, "bye");
    REQUIRE(Run(rig, [&] {
        srv.update();
        return srv.count(SessionEventType::Left) == 1;
    }, 5000));
    Cli c2(rig, cast.ids[2], Copy(*cast.keys[2]));
    REQUIRE(join(c2));
    CHECK(c2.find(ClientEventType::Rejected) &&
          c2.find(ClientEventType::Rejected)->reject.reason == std::uint16_t(proto::RejectReason::ServerFull));
    Cli a2(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(join(a2));
    CHECK(a2.c->state() == ClientState::Active && a2.c->welcome().slot == slotA && a2.c->welcome().slot_generation == genA);
    // The server releases B: B is told, C takes the slot under a new generation.
    const std::uint8_t slotB = b.c->welcome().slot;
    const std::uint32_t genB = b.c->welcome().slot_generation;
    CHECK(srv.s->release(cast.ids[1]));
    REQUIRE(Run(rig, [&] {
        srv.update();
        b.update();
        return b.c->state() == ClientState::Ended;
    }, 5000));
    CHECK(b.find(ClientEventType::Ended)->code == session::close_code::kReleased);
    Cli c3(rig, cast.ids[2], Copy(*cast.keys[2]));
    REQUIRE(join(c3));
    CHECK(c3.c->state() == ClientState::Active && c3.c->welcome().slot == slotB &&
          c3.c->welcome().slot_generation == genB + 1);
    CHECK(srv.s->counters().rejected_full == 2);
}

SCENARIO("session/same-player-replaces-session")
{
    Cast cast = MakeCast(2);
    Srv srv(rig, cast);
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(a.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        return a.c->state() == ClientState::Active;
    }, 5000));
    // A connects again (e.g. after a crash, while the old session still looks alive).
    Cli again(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(again.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        again.update();
        return again.c->state() == ClientState::Active && a.c->state() == ClientState::Ended;
    }, 5000));
    CHECK(a.find(ClientEventType::Ended)->code == session::close_code::kReplaced);
    CHECK(again.c->welcome().slot == a.c->welcome().slot);
    CHECK(again.c->welcome().slot_generation == a.c->welcome().slot_generation);
    CHECK(again.c->welcome().session_id != a.c->welcome().session_id);
    int left = 0;
    for (const SessionEvent& e : srv.ev)
        if (e.type == SessionEventType::Left)
        {
            ++left;
            CHECK(e.replaced && e.player == cast.ids[0]);
        }
    CHECK(left == 1 && srv.count(SessionEventType::Joined) == 2);
    CHECK(srv.s->players().size() == 1 && srv.s->players()[0].connected);
    // Messages now come from the new connection, as the same player.
    proto::ActionIntent ai;
    ai.operation_id = 1;
    ai.kind = 1;
    CHECK(again.c->send(ai));
    REQUIRE(Run(rig, [&] {
        srv.update();
        again.update();
        return srv.count(SessionEventType::Message) == 1;
    }, 5000));
}

SCENARIO("session/build-mismatch-refused")
{
    Cast cast = MakeCast(1);
    Srv srv(rig, cast);
    proto::LocalBuild otherBuild = Build();
    otherBuild.build_id[0] ^= 1;
    proto::LocalBuild otherMajor = Build();
    otherMajor.major = 2;
    struct Case
    {
        proto::LocalBuild build;
        proto::RejectReason expect;
    };
    const Case cases[] = {{otherBuild, proto::RejectReason::BuildMismatch}, {Build(9), proto::RejectReason::ContentMismatch},
        {otherMajor, proto::RejectReason::VersionMismatch}};
    for (const Case& k : cases)
    {
        Cli c(rig, cast.ids[0], Copy(*cast.keys[0]), k.build);
        REQUIRE(c.c->connect(srv.address));
        REQUIRE(Run(rig, [&] {
            srv.update();
            c.update();
            return c.c->state() == ClientState::Ended;
        }, 5000));
        REQUIRE(c.find(ClientEventType::Rejected));
        CHECK_MSG(c.find(ClientEventType::Rejected)->reject.reason == std::uint16_t(k.expect), "got %u",
            c.find(ClientEventType::Rejected)->reject.reason);
    }
    CHECK(srv.s->counters().rejected_build == 1 && srv.s->counters().rejected_content == 1 &&
          srv.s->counters().rejected_version == 1);
    CHECK(srv.count(SessionEventType::Joined) == 0);
}

SCENARIO("session/rate-limits")
{
    Cast cast = MakeCast(2);
    ServerConfig cfg = SConfig();
    cfg.max_handshakes_per_address = 4;
    Srv srv(rig, cast, cfg, 10);
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0])), b(rig, cast.ids[1], Copy(*cast.keys[1]));
    REQUIRE(a.c->connect(srv.address) && b.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        b.update();
        return a.c->state() == ClientState::Active && b.c->state() == ClientState::Active;
    }, 5000));
    // B sends inputs at about 1,000/s for a second (transient; the server
    // allows 120/s): the excess is dropped and B stays connected.
    proto::InputBatch ib;
    ib.inputs.push_back(proto::Input{});
    int sent = 0;
    const std::int64_t f0 = rig.now_ms();
    Run(rig, [&] {
        sent += b.c->send(ib) ? 1 : 0;
        srv.update();
        a.update();
        b.update();
        return rig.now_ms() - f0 >= 1000;
    }, 3000);
    // A floods actions (reliable): over the burst, its connection is closed.
    proto::ActionIntent ai;
    ai.kind = 1;
    for (int i = 0; i < 100; ++i)
    {
        ai.operation_id = std::uint64_t(i + 1);
        a.c->send(ai);
    }
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        b.update();
        return a.c->state() == ClientState::Ended;
    }, 5000));
    PumpFor(rig, {}, 200);
    srv.update();
    b.update();
    CHECK(a.find(ClientEventType::Ended)->code == session::close_code::kRateLimited);
    CHECK(srv.s->counters().rate_limited_closes == 1);
    CHECK(b.c->state() == ClientState::Active);
    metric("inputs_sent", sent);
    metric("inputs_dropped_by_rate", double(srv.s->counters().rate_limited_drops));
    CHECK(srv.s->counters().rate_limited_drops > 0);
    int actions = 0;
    for (const SessionEvent& e : srv.ev)
        actions += e.type == SessionEventType::Message && std::holds_alternative<proto::ActionIntent>(e.message.body);
    metric("actions_delivered_before_close", actions);
    CHECK(actions <= 60); // the burst
    // Connection attempts from one address beyond 4 per window are refused at once.
    std::vector<std::unique_ptr<Raw>> flood;
    for (int i = 0; i < 6; ++i)
    {
        flood.push_back(std::make_unique<Raw>(rig));
        flood.back()->node.t->connect(srv.address.c_str());
    }
    Run(rig, [&] {
        srv.update();
        int ended = 0;
        for (auto& r : flood)
        {
            r->node.Poll();
            ended += r->ended() ? 1 : 0;
        }
        return ended >= 4;
    }, 5000);
    // 2 earlier handshakes (A, B) + 2 of these fit the limit of 4.
    CHECK(srv.s->counters().rejected_address_rate == 4);
    int limited = 0;
    for (auto& r : flood)
        limited += r->ended() && r->ended()->code == session::close_code::kRateLimited;
    CHECK(limited == 4);
}

SCENARIO("session/handshake-timeout-and-order")
{
    Cast cast = MakeCast(1);
    ServerConfig cfg = SConfig();
    cfg.handshake_timeout_ms = 800;
    Srv srv(rig, cast, cfg, 10);
    Raw silent(rig), noProof(rig), proofFirst(rig), actionFirst(rig), sendsWelcome(rig), twoHellos(rig);
    std::vector<Raw*> all = {&silent, &noProof, &proofFirst, &actionFirst, &sendsWelcome, &twoHellos};
    for (Raw* r : all)
        REQUIRE(r->node.t->connect(srv.address.c_str()) != net::kNoConnection);
    REQUIRE(Run(rig, [&] {
        srv.update();
        bool up = true;
        for (Raw* r : all)
        {
            r->node.Poll();
            up &= r->conn() != net::kNoConnection;
        }
        return up;
    }, 5000));
    CHECK(noProof.send(HelloFor(cast.ids[0])));
    CHECK(proofFirst.send(proto::AuthProof{}));
    proto::ActionIntent ai;
    ai.operation_id = 1;
    ai.kind = 1;
    CHECK(actionFirst.send(ai, 5));
    proto::Welcome w;
    w.campaign_id = w.session_id = w.player_id = w.party_id = {1, 1};
    w.slot_generation = 1;
    w.authority_epoch = 1;
    w.resume_token.assign(16, 1);
    CHECK(sendsWelcome.send(w));
    CHECK(twoHellos.send(HelloFor(cast.ids[0])));
    CHECK(twoHellos.send(HelloFor(cast.ids[0])));
    const std::int64_t t0 = rig.now_ms();
    REQUIRE(Run(rig, [&] {
        srv.update();
        bool ended = true;
        for (Raw* r : all)
        {
            r->node.Poll();
            ended &= r->ended() != nullptr;
        }
        return ended;
    }, 5000));
    CHECK(silent.ended()->code == session::close_code::kHandshakeTimeout);
    CHECK(noProof.ended()->code == session::close_code::kHandshakeTimeout);
    metric("timeout_after_ms", double(rig.now_ms() - t0));
    for (Raw* r : {&proofFirst, &actionFirst, &sendsWelcome, &twoHellos})
        CHECK(r->ended()->code == net::close_code::kProtocolViolation);
    CHECK(srv.s->counters().handshake_timeouts == 2 && srv.s->counters().violations == 4);
    CHECK(srv.count(SessionEventType::Joined) == 0);
    for (const auto& line : srv.s->log())
        note(line);
}

SCENARIO("session/no-credentials-in-diagnostics")
{
    Cast cast = MakeCast(2);
    Srv srv(rig, cast);
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]));
    Cli wrong(rig, cast.ids[1], Copy(*cast.keys[0]));
    REQUIRE(a.c->connect(srv.address) && wrong.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        wrong.update();
        return a.c->state() == ClientState::Active && wrong.c->state() == ClientState::Ended;
    }, 5000));
    a.c->close(net::close_code::kNormal, "bye");
    Run(rig, [&] {
        srv.update();
        return srv.count(SessionEventType::Left) == 1;
    }, 5000);
    // Nothing the server says about sessions contains a token, a key or a
    // signature (it only ever writes addresses, slots and reasons).
    const auto& token = a.c->welcome().resume_token;
    std::vector<std::string> secrets = {to_hex(token.data(), token.size()), to_hex(cast.pubs[0].data(), 8)};
    std::string all;
    for (const auto& l : srv.s->log())
        all += l + "\n";
    for (const SessionEvent& e : srv.ev)
        all += e.detail + "\n";
    for (const auto& s : secrets)
        CHECK(all.find(s) == std::string::npos);
    CHECK(all.find(std::string(token.begin(), token.end())) == std::string::npos);
    metric("diagnostic_lines", double(srv.s->log().size()));
}
} // namespace coop::test
