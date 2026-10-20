// Liveness and in-memory reconnect (S04-T05), on the fake and on GNS.
// Short timings so the tests run quickly; defaults are 1 s / 5 s / 15 s.
#include "session_rig.h"

namespace coop::test
{
namespace
{
LivenessConfig Fast()
{
    LivenessConfig l;
    l.heartbeat_interval_ms = 200;
    l.suspect_after_ms = 1000;
    l.timeout_ms = 2500;
    return l;
}

ServerConfig FastConfig()
{
    ServerConfig c = SConfig();
    c.liveness = Fast();
    c.max_handshakes_per_address = 100;
    return c;
}

const SessionEvent* Last(const Srv& s, SessionEventType type)
{
    for (auto it = s.ev.rbegin(); it != s.ev.rend(); ++it)
        if (it->type == type)
            return &*it;
    return nullptr;
}

bool JoinAll(Rig& rig, Srv& srv, std::vector<Cli*> clients)
{
    for (Cli* c : clients)
        if (!c->c->connect(srv.address))
            return false;
    return Run(rig, [&] {
        srv.update();
        bool all = true;
        for (Cli* c : clients)
        {
            c->update();
            all &= c->c->state() == ClientState::Active || c->c->state() == ClientState::Ended;
        }
        return all;
    }, 8000);
}
} // namespace

SCENARIO("liveness/heartbeat-rtt")
{
    Cast cast = MakeCast(2);
    Srv srv(rig, cast, FastConfig());
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast()), b(rig, cast.ids[1], Copy(*cast.keys[1]), Build(), Fast());
    REQUIRE(JoinAll(rig, srv, {&a, &b}));
    const std::int64_t t0 = rig.now_ms();
    Run(rig, [&] {
        srv.update();
        a.update();
        b.update();
        return rig.now_ms() - t0 >= 3000;
    }, 6000);
    const Percentiles ra = Summarise(a.c->rtts());
    metric("rtt_count_a", double(ra.count));
    metric("rtt_p50_ms", ra.p50);
    metric("rtt_max_ms", ra.max);
    CHECK(ra.count >= 12 && Summarise(b.c->rtts()).count >= 12);
    CHECK(srv.count(SessionEventType::Suspect) == 0 && a.count(ClientEventType::Suspect) == 0);
    CHECK(srv.s->counters().heartbeats >= 24);
    for (const PlayerView& p : srv.s->players())
        CHECK(p.connected && !p.suspect);
}

SCENARIO("liveness/dead-client-detected")
{
    // Fake: the client's process dies. GNS: the link goes silent both ways.
    Cast cast = MakeCast(1);
    Srv srv(rig, cast, FastConfig());
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast());
    REQUIRE(JoinAll(rig, srv, {&a}));
    Run(rig, [&] {
        srv.update();
        a.update();
        return false;
    }, 300);
    const std::int64_t t0 = rig.now_ms();
    if (rig.real())
    {
        GnsImpairment m;
        m.loss_pct = 100;
        rig.impair(m);
    }
    else
        rig.crash(*a.t);
    std::int64_t suspectAt = -1, leftAt = -1, clientEndAt = -1;
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        if (suspectAt < 0 && Last(srv, SessionEventType::Suspect))
            suspectAt = rig.now_ms() - t0;
        if (leftAt < 0 && Last(srv, SessionEventType::Left))
            leftAt = rig.now_ms() - t0;
        if (clientEndAt < 0 && a.find(ClientEventType::Ended))
            clientEndAt = rig.now_ms() - t0;
        return leftAt >= 0 && (!rig.real() || clientEndAt >= 0);
    }, 8000));
    metric("server_suspect_after_ms", double(suspectAt));
    metric("server_left_after_ms", double(leftAt));
    // Silence started at most one heartbeat before t0.
    CHECK_MSG(suspectAt >= 1000 - 220 && suspectAt <= 1000 + 150, "suspect after %lld ms", (long long)suspectAt);
    CHECK_MSG(leftAt >= 2500 - 220 && leftAt <= 2500 + 150, "left after %lld ms", (long long)leftAt);
    const SessionEvent* left = Last(srv, SessionEventType::Left);
    CHECK(left->reason == LeftReason::Timeout && left->player == cast.ids[0]);
    CHECK(srv.s->counters().timeouts == 1);
    REQUIRE(srv.s->players().size() == 1);
    CHECK(!srv.s->players()[0].connected); // the slot stays the player's
    if (rig.real())
    {
        metric("client_ended_after_ms", double(clientEndAt));
        CHECK(a.find(ClientEventType::Suspect) && a.find(ClientEventType::Ended)->reason == net::CloseReason::Timeout);
        CHECK(clientEndAt <= 2500 + 300);
    }
}

SCENARIO("liveness/dead-server-detected")
{
    Cast cast = MakeCast(1);
    Srv srv(rig, cast, FastConfig());
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast());
    REQUIRE(JoinAll(rig, srv, {&a}));
    const std::int64_t t0 = rig.now_ms();
    bool serverAlive = true;
    if (rig.real())
    {
        GnsImpairment m;
        m.loss_pct = 100;
        rig.impair(m);
    }
    else
    {
        rig.crash(*srv.t);
        serverAlive = false;
    }
    REQUIRE(Run(rig, [&] {
        if (serverAlive)
            srv.update();
        a.update();
        return a.find(ClientEventType::Ended) != nullptr;
    }, 8000));
    const double after = double(rig.now_ms() - t0);
    metric("client_ended_after_ms", after);
    CHECK(a.find(ClientEventType::Suspect) != nullptr);
    CHECK(a.find(ClientEventType::Ended)->reason == net::CloseReason::Timeout);
    CHECK(a.find(ClientEventType::Ended)->code == session::close_code::kTimeout);
    CHECK(after >= 2500 - 220 && after <= 2500 + 300);
}

SCENARIO("liveness/suspect-and-recover")
{
    // The client's owner stalls (no updates, so no heartbeats) for 1.6 s:
    // longer than suspect, shorter than the timeout.
    Cast cast = MakeCast(1);
    Srv srv(rig, cast, FastConfig());
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast());
    REQUIRE(JoinAll(rig, srv, {&a}));
    const std::int64_t t0 = rig.now_ms();
    Run(rig, [&] {
        srv.update();
        return rig.now_ms() - t0 >= 1600;
    }, 4000);
    CHECK(srv.count(SessionEventType::Suspect) == 1);
    REQUIRE(srv.s->players().size() == 1);
    CHECK(srv.s->players()[0].suspect);
    REQUIRE(Run(rig, [&] {
        srv.update();
        a.update();
        return srv.count(SessionEventType::Recovered) == 1;
    }, 2000));
    CHECK(srv.count(SessionEventType::Left) == 0 && a.c->state() == ClientState::Active);
    CHECK(!srv.s->players()[0].suspect);
}

SCENARIO("liveness/resume-same-session")
{
    Cast cast = MakeCast(1);
    Srv srv(rig, cast, FastConfig());
    auto a = std::make_unique<Cli>(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast());
    REQUIRE(JoinAll(rig, srv, {a.get()}));
    const proto::Welcome first = a->c->welcome();
    const auto rq = a->c->resume_request(7, 9);
    REQUIRE(rq.has_value());
    // The client's process goes away without a goodbye.
    a->c.reset();
    a->t.reset();
    REQUIRE(Run(rig, [&] {
        srv.update();
        return srv.count(SessionEventType::Left) == 1;
    }, 5000));
    Cli a2(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast(), rq);
    REQUIRE(JoinAll(rig, srv, {&a2}));
    REQUIRE(a2.find(ClientEventType::Welcomed));
    CHECK(a2.find(ClientEventType::Welcomed)->resumed);
    const proto::Welcome& w = a2.c->welcome();
    CHECK(w.session_id == first.session_id && w.slot == first.slot && w.slot_generation == first.slot_generation);
    CHECK(w.resume_token != first.resume_token); // rotated
    const SessionEvent* j = Last(srv, SessionEventType::Joined);
    CHECK(j->resumed && j->last_event == 7 && j->last_snapshot == 9 && j->session_id == first.session_id);
    // The old token is dead: a connection presenting it gets a fresh session
    // (and, being the same player, replaces a2).
    Cli a3(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast(), rq);
    REQUIRE(a3.c->connect(srv.address));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a2.update();
        a3.update();
        return a3.c->state() == ClientState::Active && a2.c->state() == ClientState::Ended;
    }, 5000));
    CHECK(!a3.find(ClientEventType::Welcomed)->resumed);
    CHECK(a3.c->welcome().session_id != first.session_id);
    CHECK(a3.c->welcome().slot == first.slot);
    CHECK(srv.s->counters().resumed == 1 && srv.s->counters().resume_refused == 1);
    CHECK(srv.s->players().size() == 1);
}

SCENARIO("liveness/stale-connection-cannot-submit")
{
    // The old connection keeps sending actions while a new connection of the
    // same player takes over; with lag, some are still in flight at the switch.
    Cast cast = MakeCast(1);
    Srv srv(rig, cast, FastConfig());
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast());
    REQUIRE(JoinAll(rig, srv, {&a}));
    GnsImpairment m;
    m.lag_ms = 150; // about three of the old connection's actions in flight at any time
    rig.impair(m);
    const std::uint64_t oldSerial = srv.s->players()[0].join_serial;
    std::uint64_t op = 0;
    int sentAfterSwitch = 0, refusedAfterEnd = 0, oldSent = 0;
    std::int64_t nextSend = rig.now_ms();
    auto pump = [&](std::int64_t ms, Cli* other) {
        const std::int64_t t0 = rig.now_ms();
        Run(rig, [&] {
            if (rig.now_ms() >= nextSend) // 20/s, under the 30/s action limit
            {
                nextSend += 50;
                proto::ActionIntent ai;
                ai.operation_id = ++op;
                ai.kind = 1;
                const bool ok = a.c->send(ai);
                oldSent += ok ? 1 : 0;
                if (a.c->state() == ClientState::Ended)
                    refusedAfterEnd += ok ? 0 : 1;
                else if (other && other->c->state() == ClientState::Active)
                    sentAfterSwitch += ok ? 1 : 0;
            }
            srv.update();
            a.update();
            if (other)
                other->update();
            return rig.now_ms() - t0 >= ms;
        }, ms + 2000);
    };
    pump(300, nullptr);
    Cli a2(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast(), a.c->resume_request(0, 0));
    REQUIRE(a2.c->connect(srv.address));
    pump(1500, &a2);
    REQUIRE(a2.c->state() == ClientState::Active && a.c->state() == ClientState::Ended);
    CHECK(a.find(ClientEventType::Ended)->code == session::close_code::kReplaced);
    // After the new connection's Joined, nothing from the old one is accepted.
    std::size_t joinedAt = 0;
    std::uint64_t newSerial = 0;
    for (std::size_t i = 0; i < srv.ev.size(); ++i)
        if (srv.ev[i].type == SessionEventType::Joined && srv.ev[i].join_serial != oldSerial)
        {
            joinedAt = i;
            newSerial = srv.ev[i].join_serial;
        }
    REQUIRE(newSerial != 0);
    int before = 0, after = 0;
    for (std::size_t i = 0; i < srv.ev.size(); ++i)
        if (srv.ev[i].type == SessionEventType::Message && srv.ev[i].join_serial == oldSerial)
            (i < joinedAt ? before : after)++;
    metric("old_actions_accepted_before_switch", before);
    metric("old_actions_sent_while_replaced", sentAfterSwitch);
    metric("old_sends_refused_after_end", refusedAfterEnd);
    metric("old_actions_sent", oldSent);
    metric("old_actions_in_flight_dropped", oldSent - before);
    CHECK(before > 0);
    CHECK(after == 0);
    // Some were still on their way when the old connection was replaced;
    // they died with it.
    CHECK(oldSent - before >= 1);
    CHECK(refusedAfterEnd > 0);
    // The new connection's actions come through under its own serial.
    proto::ActionIntent ai;
    ai.operation_id = ++op;
    ai.kind = 1;
    CHECK(a2.c->send(ai));
    REQUIRE(Run(rig, [&] {
        srv.update();
        a2.update();
        const SessionEvent* msg = Last(srv, SessionEventType::Message);
        return msg && msg->join_serial == newSerial;
    }, 3000));
}

SCENARIO("liveness/repeated-reconnect")
{
    // B stays connected throughout while A drops and resumes 15 times, either
    // vanishing first or reconnecting over a live connection.
    Cast cast = MakeCast(2);
    Srv srv(rig, cast, FastConfig());
    Cli b(rig, cast.ids[1], Copy(*cast.keys[1]), Build(), Fast());
    auto a = std::make_unique<Cli>(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast());
    REQUIRE(JoinAll(rig, srv, {&b, a.get()}));
    const proto::Welcome first = a->c->welcome();
    std::uint64_t lastSerial = srv.s->players()[0].join_serial;
    int resumedCount = 0, replacedCount = 0;
    for (int i = 0; i < 15; ++i)
    {
        auto rq = a->c->resume_request(std::uint64_t(i), std::uint64_t(i));
        const bool vanish = i % 2 == 0;
        if (vanish)
        {
            a->c.reset();
            a->t.reset();
            const int lefts = srv.count(SessionEventType::Left);
            REQUIRE(Run(rig, [&] {
                srv.update();
                b.update();
                return srv.count(SessionEventType::Left) > lefts;
            }, 5000));
        }
        auto next = std::make_unique<Cli>(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast(), rq);
        REQUIRE(next->c->connect(srv.address));
        REQUIRE(Run(rig, [&] {
            srv.update();
            b.update();
            if (a->c)
                a->update();
            next->update();
            return next->c->state() == ClientState::Active;
        }, 5000));
        resumedCount += next->find(ClientEventType::Welcomed)->resumed ? 1 : 0;
        if (!vanish)
        {
            REQUIRE(Run(rig, [&] {
                srv.update();
                b.update();
                a->update();
                return a->c->state() == ClientState::Ended;
            }, 5000));
            replacedCount += a->find(ClientEventType::Ended)->code == session::close_code::kReplaced ? 1 : 0;
        }
        const proto::Welcome& w = next->c->welcome();
        CHECK(w.session_id == first.session_id && w.slot == first.slot && w.slot_generation == first.slot_generation);
        const SessionEvent* j = Last(srv, SessionEventType::Joined);
        CHECK(j->join_serial > lastSerial);
        lastSerial = j->join_serial;
        a = std::move(next);
    }
    CHECK(resumedCount == 15);
    CHECK(replacedCount == 7);
    CHECK(srv.s->counters().resumed == 15 && srv.s->counters().replaced == 7);
    const auto players = srv.s->players();
    CHECK(players.size() == 2); // no duplicate slot
    for (const PlayerView& p : players)
        CHECK(p.connected);
    CHECK(b.c->state() == ClientState::Active && b.count(ClientEventType::Suspect) == 0);
}

SCENARIO("liveness/server-shutdown")
{
    Cast cast = MakeCast(2);
    auto srv = std::make_unique<Srv>(rig, cast, FastConfig());
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast()), b(rig, cast.ids[1], Copy(*cast.keys[1]), Build(), Fast());
    REQUIRE(JoinAll(rig, *srv, {&a, &b}));
    const auto rq = a.c->resume_request(1, 1);
    const std::int64_t t0 = rig.now_ms();
    srv->s->shutdown(500, srv->ev);
    CHECK(srv->count(SessionEventType::Left) == 2);
    for (const SessionEvent& e : srv->ev)
        if (e.type == SessionEventType::Left)
            CHECK(e.reason == LeftReason::Shutdown);
    REQUIRE(Run(rig, [&] {
        a.update();
        b.update();
        return a.c->state() == ClientState::Ended && b.c->state() == ClientState::Ended;
    }, 3000));
    metric("clients_told_after_ms", double(rig.now_ms() - t0));
    CHECK(a.find(ClientEventType::Ended)->reason == net::CloseReason::PeerShutdown);
    CHECK(b.find(ClientEventType::Ended)->reason == net::CloseReason::PeerShutdown);
    // A new server process knows nothing of the old session: resuming gives a
    // fresh one (no recovery across a restart before Stage 7).
    srv.reset();
    Srv again(rig, cast, FastConfig());
    Cli a2(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), Fast(), rq);
    REQUIRE(JoinAll(rig, again, {&a2}));
    REQUIRE(a2.c->state() == ClientState::Active);
    CHECK(!a2.find(ClientEventType::Welcomed)->resumed);
    CHECK(again.s->counters().resume_refused == 1);
}

SCENARIO("liveness/release-after-grace")
{
    Cast cast = MakeCast(3);
    ServerConfig cfg = FastConfig();
    cfg.liveness.release_after_ms = 1000;
    Srv srv(rig, cast, cfg);
    Cli a(rig, cast.ids[0], Copy(*cast.keys[0]), Build(), cfg.liveness),
        b(rig, cast.ids[1], Copy(*cast.keys[1]), Build(), cfg.liveness);
    REQUIRE(JoinAll(rig, srv, {&a, &b}));
    const std::uint8_t slotA = a.c->welcome().slot;
    const std::uint32_t genA = a.c->welcome().slot_generation;
    a.c->close(net::close_code::kNormal, "bye");
    REQUIRE(Run(rig, [&] {
        srv.update();
        b.update();
        return srv.count(SessionEventType::Left) == 1;
    }, 3000));
    const std::int64_t t0 = rig.now_ms();
    REQUIRE(Run(rig, [&] {
        srv.update();
        b.update();
        return srv.count(SessionEventType::Released) == 1;
    }, 3000));
    metric("released_after_ms", double(rig.now_ms() - t0));
    CHECK(Last(srv, SessionEventType::Released)->player == cast.ids[0]);
    Cli c(rig, cast.ids[2], Copy(*cast.keys[2]), Build(), cfg.liveness);
    REQUIRE(JoinAll(rig, srv, {&c}));
    CHECK(c.c->state() == ClientState::Active && c.c->welcome().slot == slotA && c.c->welcome().slot_generation == genA + 1);
    CHECK(srv.s->counters().auto_released == 1);
}
} // namespace coop::test
