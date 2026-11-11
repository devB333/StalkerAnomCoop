// Sessions bound to canonical players (S04-T04), on the fake and on GNS.
#include "session_rig.h"
#include <cctype>

namespace coop::test
{
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

// B64: a message the encoder refuses (a CampaignEvent over kMaxEventPayload)
// is not sent and takes no sequence; the next one on its reliable channel
// arrives without a gap (before, the retries left one and the client closed).
SCENARIO("session/unencodable-send-leaves-no-gap")
{
    Cast cast = MakeCast(1);
    Srv srv(rig, cast);
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(a.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        return a.c->state() == ClientState::Active;
    }, 8000));
    proto::CampaignEvent big;
    big.event_id = 1;
    big.kind = 11;
    big.revision = 1;
    big.payload.assign(proto::kMaxEventPayload + 1, 7);
    for (int i = 0; i < 3; ++i)
        CHECK(!srv.s->send(cast.ids[0], big));
    proto::CampaignEvent ok = big;
    ok.event_id = 2;
    ok.payload.assign(proto::kMaxEventPayload, 7);
    CHECK(srv.s->send(cast.ids[0], ok));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        return a.find(ClientEventType::Message) != nullptr || a.find(ClientEventType::Ended) != nullptr;
    }, 5000));
    const ClientEvent* got = a.find(ClientEventType::Message);
    REQUIRE(got);
    const auto* event = std::get_if<proto::CampaignEvent>(&got->message.body);
    REQUIRE(event);
    CHECK(event->event_id == 2 && event->payload.size() == proto::kMaxEventPayload);
    CHECK(a.find(ClientEventType::Ended) == nullptr);
    CHECK(a.c->violations() == 0 && a.c->state() == ClientState::Active);
    // a transient channel keeps taking a sequence per attempt: a batch of 3
    // inputs needs sequence >= 3, reached on the third try (bugfix-85: with the
    // reliable rule there too, no input ever left a client)
    proto::InputBatch ib;
    for (std::uint64_t tick = 3; tick >= 1; --tick)
    {
        proto::Input in;
        in.tick = tick;
        ib.inputs.push_back(in);
    }
    int tries = 0;
    while (tries < 8 && !a.c->send(ib))
        ++tries;
    CHECK(tries == 2);
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        for (const SessionEvent& e : srv.ev)
            if (e.type == SessionEventType::Message && std::holds_alternative<proto::InputBatch>(e.message.body))
                return true;
        return false;
    }, 5000));
    CHECK(srv.s->counters().violations == 0);
}

// S06-T06: a player is confirmed only once its client has sent inside the
// session (it then has its Welcome); the host starts other channels after that.
SCENARIO("session/confirmed-after-client-speaks")
{
    Cast cast = MakeCast(1);
    Srv srv(rig, cast);
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(a.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        return srv.count(SessionEventType::Joined) == 1;
    }, 8000));
    // the server has sent the Welcome but processed nothing from the session yet
    REQUIRE(srv.s->players().size() == 1);
    CHECK(!srv.s->players()[0].confirmed);
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        return !srv.s->players().empty() && srv.s->players()[0].confirmed;
    }, 5000));
    CHECK(a.c->state() == ClientState::Active);
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

// S11-T00: a player not in the roster joins with the host's join code; a wrong
// code, no code on the server, or no free slot is refused; the new player is
// kept in the roster for its reconnects and handed to the host once.
SCENARIO("session/join-code")
{
    Cast cast = MakeCast(1); // the host
    ServerConfig cfg = SConfig();
    cfg.join_code = make_join_code();
    REQUIRE(cfg.join_code.size() == 11 && cfg.join_code[5] == '-');
    std::string norm;
    CHECK(normalize_join_code(cfg.join_code, norm) && norm.size() == 10);
    std::string lower = cfg.join_code;
    for (char& ch : lower)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    std::string norm2;
    CHECK(normalize_join_code(lower, norm2) && norm2 == norm);
    CHECK(!normalize_join_code("ABCDE-0OIL1", norm2)); // ambiguous symbols are not in the alphabet
    Srv srv(rig, cast, cfg);
    auto join = [&](Cli& c) {
        REQUIRE(c.c->connect(srv.address));
        return Run(rig, [&] {
            srv.update();
            c.update();
            return c.c->state() == ClientState::Active || c.c->state() == ClientState::Ended;
        }, 5000);
    };
    auto fresh = [](PrivateKey& k, PublicKey& pub, Uuid128& id) {
        REQUIRE(generate_key(k, pub));
        id = random_uuid();
    };
    // a wrong code
    PrivateKey kw;
    PublicKey pw;
    Uuid128 iw;
    fresh(kw, pw, iw);
    Cli wrong(rig, iw, std::move(kw), Build(), {}, std::nullopt, "22222-22222");
    REQUIRE(join(wrong));
    REQUIRE(wrong.find(ClientEventType::Rejected));
    CHECK(wrong.find(ClientEventType::Rejected)->reject.reason == std::uint16_t(proto::RejectReason::AuthFailed));
    CHECK(srv.s->counters().rejected_code == 1 && srv.s->take_added_by_code().empty());
    // the right code (any case, no separator)
    PrivateKey kn;
    PublicKey pn;
    Uuid128 in;
    fresh(kn, pn, in);
    PrivateKey kn2 = Copy(kn);
    Cli friend1(rig, in, std::move(kn), Build(), {}, std::nullopt, lower);
    REQUIRE(join(friend1));
    REQUIRE(friend1.c->state() == ClientState::Active);
    CHECK(friend1.c->welcome().player_id == in);
    const auto added = srv.s->take_added_by_code();
    REQUIRE(added.size() == 1);
    CHECK(added[0].player_id == in && added[0].key == pn && added[0].display_name == "p");
    CHECK(srv.s->take_added_by_code().empty()); // handed over once
    CHECK(srv.s->counters().joined_by_code == 1);
    // the host still joins by its roster key
    Cli host(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(join(host));
    CHECK(host.c->state() == ClientState::Active);
    // both slots taken: another new player with the right code is refused
    PrivateKey k3;
    PublicKey p3;
    Uuid128 i3;
    fresh(k3, p3, i3);
    Cli third(rig, i3, std::move(k3), Build(), {}, std::nullopt, cfg.join_code);
    REQUIRE(join(third));
    REQUIRE(third.find(ClientEventType::Rejected) != nullptr);
    // B56: a full party says so (not a wrong code) and adds no one
    CHECK(third.find(ClientEventType::Rejected)->reject.reason == std::uint16_t(proto::RejectReason::ServerFull));
    CHECK(srv.s->counters().rejected_code == 1 && srv.s->counters().rejected_full == 1);
    CHECK(srv.s->take_added_by_code().empty());
    // the friend reconnects with its key and no code: it is in the roster now
    friend1.c->close(net::close_code::kNormal, "bye");
    REQUIRE(Run(rig, [&] {
        srv.update();
        return srv.count(SessionEventType::Left) >= 1;
    }, 5000));
    Cli again(rig, in, std::move(kn2));
    REQUIRE(join(again));
    CHECK(again.c->state() == ClientState::Active && again.c->welcome().slot == friend1.c->welcome().slot);
}

// B56: a roster of two is a full party even while one of them is away (a
// fresh server: no session slot bound yet); a new player with the right code
// is refused as full and the roster is left as it was.
SCENARIO("session/join-code-full-roster")
{
    Cast cast = MakeCast(2);
    ServerConfig cfg = SConfig();
    cfg.join_code = make_join_code();
    Srv srv(rig, cast, cfg);
    Cli host(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(host.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        host.update();
        return host.c->state() == ClientState::Active;
    }, 5000));
    PrivateKey k;
    PublicKey pub;
    REQUIRE(generate_key(k, pub));
    Cli third(rig, random_uuid(), std::move(k), Build(), {}, std::nullopt, cfg.join_code);
    REQUIRE(third.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        host.update();
        third.update();
        return third.c->state() == ClientState::Ended;
    }, 5000));
    REQUIRE(third.find(ClientEventType::Rejected));
    CHECK(third.find(ClientEventType::Rejected)->reject.reason == std::uint16_t(proto::RejectReason::ServerFull));
    CHECK(srv.s->take_added_by_code().empty());
    CHECK(srv.s->counters().joined_by_code == 0 && srv.s->counters().rejected_full == 1);
    // the away player still gets its place back with its key
    Cli away(rig, cast.ids[1], Copy(*cast.keys[1]));
    REQUIRE(away.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        host.update();
        away.update();
        return away.c->state() == ClientState::Active;
    }, 5000));
}

// S11-T00: with no join code on the server a join proof is an unknown player.
SCENARIO("session/join-code-off")
{
    Cast cast = MakeCast(1);
    Srv srv(rig, cast);
    PrivateKey k;
    PublicKey pub;
    REQUIRE(generate_key(k, pub));
    Cli c(rig, random_uuid(), std::move(k), Build(), {}, std::nullopt, make_join_code());
    REQUIRE(c.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        c.update();
        return c.c->state() == ClientState::Ended;
    }, 5000));
    REQUIRE(c.find(ClientEventType::Rejected));
    CHECK(c.find(ClientEventType::Rejected)->reject.reason == std::uint16_t(proto::RejectReason::AuthFailed));
    CHECK(srv.s->counters().joined_by_code == 0);
}

// S11-T00: while the game replaces its world the server answers ServerBusy; afterwards the same player joins.
SCENARIO("session/busy-while-loading")
{
    Cast cast = MakeCast(1);
    Srv srv(rig, cast);
    srv.s->set_accepting(false);
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(a.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        return a.c->state() == ClientState::Ended;
    }, 5000));
    REQUIRE(a.find(ClientEventType::Rejected));
    CHECK(a.find(ClientEventType::Rejected)->reject.reason == std::uint16_t(proto::RejectReason::ServerBusy));
    CHECK(srv.s->counters().rejected_busy == 1 && srv.count(SessionEventType::Joined) == 0);
    srv.s->set_accepting(true);
    Cli b(rig, cast.ids[0], Copy(*cast.keys[0]));
    REQUIRE(b.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        b.update();
        return b.c->state() == ClientState::Active;
    }, 5000));
    CHECK(srv.count(SessionEventType::Joined) == 1);
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
