// Shutdown and threading races against the GNS backend (real sockets on
// 127.0.0.1, real threads). No sanitizer is available in the pinned toolchain,
// so each race is repeated and checked through invariants: no crash or hang,
// per-connection event order, contiguous reliable prefixes, every connection
// ends with a reason, and no library user is left behind.
#include "rig.h"

#include <windows.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <set>
#include <thread>

namespace coop::test
{
namespace
{
bool Contiguous(const Node& n, ConnectionId conn, ChannelId ch, std::uint32_t* count)
{
    std::uint32_t expect = 0;
    for (const Event* e : n.Messages(conn, ch))
    {
        std::uint32_t seq = 0;
        if (!PayloadOk(e->payload, &seq) || seq != expect)
            return false;
        ++expect;
    }
    if (count)
        *count = expect;
    return true;
}

bool PumpNodes(Rig& rig, std::vector<Node*> nodes, const std::function<bool()>& done, std::int64_t ms)
{
    return Pump(rig, nodes, done, ms);
}

ConnectionId FirstConnected(const Node& n)
{
    const Event* e = n.Find(EventType::Connected);
    return e ? e->connection : kNoConnection;
}

bool Send(Transport& t, ConnectionId c, ChannelId ch, std::uint32_t seq, std::uint32_t size)
{
    const auto p = Payload(seq, size);
    return t.send(c, ch, p.data(), std::uint32_t(p.size())) == SendResult::Sent;
}
} // namespace

TEST("race/create-destroy-cycles")
{
    // Init/Kill of the library with the first/last transport, in every order,
    // including destroying a transport while its connection is still connecting.
    auto rig = MakeGnsRig();
    int cycles = 0;
    for (int i = 0; i < 40; ++i)
    {
        Node server{rig->make(TestConfig())}, client{rig->make(TestConfig())};
        const std::string address = NextGnsAddress();
        REQUIRE(server.t->listen(address.c_str()));
        const ConnectionId c = client.t->connect(address.c_str());
        REQUIRE(c != kNoConnection);
        const int variant = i % 4;
        if (variant == 3)
        {
            client.t.reset(); // still connecting
            server.t.reset();
        }
        else
        {
            REQUIRE(PumpNodes(*rig, {&server, &client},
                [&] { return client.Find(EventType::Connected) && server.Find(EventType::Connected); }, 5000));
            CHECK(Send(*client.t, c, kControl, 0, 100));
            REQUIRE(PumpNodes(*rig, {&server, &client}, [&] { return server.Count(EventType::Message) == 1; }, 5000));
            if (variant == 0)
            {
                server.t.reset();
                CHECK(PumpNodes(*rig, {&client}, [&] { return client.Find(EventType::Disconnected) != nullptr; }, 5000));
                CHECK(client.Find(EventType::Disconnected) &&
                      client.Find(EventType::Disconnected)->reason == CloseReason::PeerShutdown);
                client.t.reset();
            }
            else if (variant == 1)
            {
                client.t.reset();
                CHECK(PumpNodes(*rig, {&server}, [&] { return server.Find(EventType::Disconnected) != nullptr; }, 5000));
                server.t.reset();
            }
            else
            {
                CHECK(server.t->shutdown(200).connections_closed == 1);
                client.t->shutdown(200);
                server.t.reset();
                client.t.reset();
            }
        }
        CHECK_MSG(gns_library_users() == 0, "cycle %d left %d users", i, gns_library_users());
        CheckEventOrder(server, "server");
        CheckEventOrder(client, "client");
        ++cycles;
    }
    metric("cycles", cycles);
    // The library comes back after the last Kill.
    Node server{rig->make(TestConfig())}, client{rig->make(TestConfig())};
    CHECK(ConnectClients(*rig, server, {&client}));
}

TEST("race/cross-thread-dispatch")
{
    // The server is owned by its own thread; clients are created and destroyed
    // without shutdown on the main thread. Each thread's poll runs the library's
    // callbacks for the other's connections, and notices for a destroyed
    // client arrive after it is gone.
    std::atomic<bool> ready{false}, stop{false};
    std::atomic<int> disconnects{0};
    std::string address;
    std::mutex resultMutex;
    std::vector<std::string> problems;
    std::uint64_t stale = 0;
    std::map<ConnectionId, std::uint32_t> delivered;
    std::thread server([&] {
        auto rig = MakeGnsRig();
        Node s{rig->make(TestConfig())};
        address = NextGnsAddress();
        if (!s.t->listen(address.c_str()))
        {
            std::lock_guard<std::mutex> lock(resultMutex);
            problems.push_back("listen failed");
            ready = true;
            return;
        }
        ready = true;
        std::size_t seen = 0;
        while (!stop)
        {
            s.Poll();
            for (; seen < s.events.size(); ++seen)
                if (s.events[seen].type == EventType::Disconnected)
                    ++disconnects;
            Sleep(1);
        }
        std::lock_guard<std::mutex> lock(resultMutex);
        for (const Event& e : s.events)
            if (e.type == EventType::Disconnected)
            {
                std::uint32_t n = 0;
                if (!Contiguous(s, e.connection, kControl, &n))
                    problems.push_back(fmt("conn %u: gap or corrupt message", e.connection));
                delivered[e.connection] = n;
                if (e.reason != CloseReason::PeerShutdown)
                    problems.push_back(fmt("conn %u ended %s", e.connection, to_string(e.reason)));
            }
        CheckEventOrder(s, "server thread");
        stale = s.t->counters().stale_notifications;
        s.t->shutdown(0);
    });
    while (!ready)
        Sleep(1);
    auto rig = MakeGnsRig();
    const int clients = 20;
    std::uint32_t sentTotal = 0;
    for (int i = 0; i < clients; ++i)
    {
        Node c{rig->make(TestConfig())};
        const ConnectionId id = c.t->connect(address.c_str());
        REQUIRE(id != kNoConnection);
        REQUIRE(PumpNodes(*rig, {&c}, [&] { return c.Find(EventType::Connected) != nullptr; }, 5000));
        const std::uint32_t k = std::uint32_t(i * 37) % 200;
        for (std::uint32_t j = 0; j < k; ++j)
            CHECK(Send(*c.t, id, kControl, j, 100));
        sentTotal += k;
        Sleep(DWORD(i % 5));
        c.t.reset(); // no shutdown
    }
    const std::int64_t t0 = rig->now_ms();
    while (disconnects < clients && rig->now_ms() - t0 < 15000)
        Sleep(5);
    stop = true;
    server.join();
    CHECK_MSG(disconnects == clients, "%d of %d disconnects seen", disconnects.load(), clients);
    for (const auto& p : problems)
        fail(__FILE__, __LINE__, p);
    std::uint32_t got = 0;
    for (auto& kv : delivered)
        got += kv.second;
    metric("messages_sent", sentTotal);
    metric("messages_delivered", got);
    metric("server_stale_notifications", double(stale));
    CHECK(got <= sentTotal);
    CHECK(gns_library_users() == 0);
}

TEST("race/simultaneous-close")
{
    // Both ends close at once, or one shortly after the other. For each side,
    // exactly one of: its own close() succeeded, or it saw a Disconnected.
    auto rig = MakeGnsRig();
    Node server{rig->make(TestConfig())};
    const std::string address = NextGnsAddress();
    REQUIRE(server.t->listen(address.c_str()));
    int both = 0, oneSaw = 0;
    for (int i = 0; i < 30; ++i)
    {
        Node client{rig->make(TestConfig())};
        const ConnectionId c = client.t->connect(address.c_str());
        const int before = server.Count(EventType::Connected);
        REQUIRE(PumpNodes(*rig, {&server, &client},
            [&] { return client.Find(EventType::Connected) && server.Count(EventType::Connected) > before; }, 5000));
        ConnectionId s = kNoConnection;
        for (const Event& e : server.events)
            if (e.type == EventType::Connected)
                s = e.connection;
        CHECK(Send(*client.t, c, kControl, 0, 64));
        const int delay = i % 3 == 0 ? 0 : i % 10;
        const bool clientFirst = i % 2 == 0;
        bool cClosed = false, sClosed = false;
        if (clientFirst)
            cClosed = client.t->close(c, close_code::kSessionMin, "client", false);
        else
            sClosed = server.t->close(s, close_code::kSessionMin + 1, "server", false);
        if (delay)
            PumpFor(*rig, {&server, &client}, delay);
        if (clientFirst)
            sClosed = server.t->close(s, close_code::kSessionMin + 1, "server", false);
        else
            cClosed = client.t->close(c, close_code::kSessionMin, "client", false);
        PumpFor(*rig, {&server, &client}, 150);
        const bool cSaw = client.Find(EventType::Disconnected, c) != nullptr;
        const bool sSaw = server.Find(EventType::Disconnected, s) != nullptr;
        CHECK_MSG(cClosed != cSaw, "iteration %d: client closed %d saw %d", i, cClosed, cSaw);
        CHECK_MSG(sClosed != sSaw, "iteration %d: server closed %d saw %d", i, sClosed, sSaw);
        both += cClosed && sClosed ? 1 : 0;
        oneSaw += cSaw || sSaw ? 1 : 0;
        CheckEventOrder(client, "client");
    }
    CheckEventOrder(server, "server");
    metric("both_closed_first", both);
    metric("one_side_saw_peer_close", oneSaw);
    metric("server_stale_notifications", double(server.t->counters().stale_notifications));
}

TEST("race/shutdown-during-connect")
{
    // The server shuts down 0-19 ms after a client starts connecting: the
    // client always ends, never hangs, and is told when the server can.
    auto rig = MakeGnsRig();
    std::map<std::string, int> outcomes;
    for (int i = 0; i < 20; ++i)
    {
        TransportConfig quick = TestConfig();
        quick.connect_timeout_ms = 2000;
        Node server{rig->make(TestConfig())}, client{rig->make(quick)};
        const std::string address = NextGnsAddress();
        REQUIRE(server.t->listen(address.c_str()));
        REQUIRE(client.t->connect(address.c_str()) != kNoConnection);
        PumpFor(*rig, {&server, &client}, i);
        server.t->shutdown(0);
        REQUIRE_MSG(PumpNodes(*rig, {&client}, [&] { return client.Find(EventType::Disconnected) != nullptr; }, 4000),
            "client never ended (delay %d)", i);
        const Event* d = client.Find(EventType::Disconnected);
        const bool connected = client.Find(EventType::Connected) != nullptr;
        if (connected)
            CHECK_MSG(d->reason == CloseReason::PeerShutdown, "connected then %s", to_string(d->reason));
        else
            CHECK_MSG(d->reason == CloseReason::ConnectFailed, "unconnected end %s", to_string(d->reason));
        ++outcomes[std::string(connected ? "connected+" : "") + to_string(d->reason) + (d->code ? "" : "(no code)")];
        CheckEventOrder(client, "client");
    }
    for (auto& kv : outcomes)
        metric("outcome_" + kv.first, kv.second);
}

TEST("race/shutdown-under-load")
{
    // Two client threads flood paced critical and transient traffic; the
    // server's owner shuts down mid-stream. Both clients learn it was a
    // shutdown; nothing hangs.
    auto rig = MakeGnsRig();
    for (int round = 0; round < 5; ++round)
    {
        Node server{rig->make(TestConfig())};
        const std::string address = NextGnsAddress();
        REQUIRE(server.t->listen(address.c_str()));
        std::atomic<int> connected{0};
        std::mutex m;
        std::vector<std::string> ends;
        auto client = [&](int id) {
            auto r = MakeGnsRig();
            Node c{r->make(TestConfig())};
            const ConnectionId cid = c.t->connect(address.c_str());
            bool counted = false;
            std::uint32_t seq = 0;
            const std::int64_t t0 = r->now_ms();
            while (!c.Find(EventType::Disconnected) && r->now_ms() - t0 < 10000)
            {
                c.Poll();
                if (c.Find(EventType::Connected) && !c.Find(EventType::Disconnected))
                {
                    if (!counted)
                    {
                        ++connected;
                        counted = true;
                    }
                    ConnectionStatus st;
                    if (c.t->status(cid, st) && st.channels[kControl].queued_bytes + st.channels[kControl].unacked_bytes < 32 * 1024)
                        Send(*c.t, cid, kControl, seq++, 800);
                    std::vector<std::uint8_t> snap(1000, std::uint8_t(id));
                    c.t->send(cid, kSnapshot, snap.data(), 1000);
                }
                Sleep(1);
            }
            const Event* d = c.Find(EventType::Disconnected);
            std::lock_guard<std::mutex> lock(m);
            ends.push_back(d ? to_string(d->reason) : "none");
            if (d)
                CHECK(c.t->send(cid, kControl, "x", 1) == SendResult::NotConnected);
            CheckEventOrder(c, "client");
        };
        std::thread a(client, 1), b(client, 2);
        const bool both = PumpNodes(*rig, {&server}, [&] { return connected == 2; }, 5000);
        PumpFor(*rig, {&server}, 1000);
        const ShutdownReport r = server.t->shutdown(500);
        a.join();
        b.join();
        CHECK(both);
        CHECK(r.connections_closed == 2);
        for (const auto& e : ends)
            CHECK_MSG(e == "peer_shutdown", "round %d: client ended %s", round, e.c_str());
        if (round == 0)
        {
            metric("shutdown_waited_ms", r.waited_ms);
            metric("shutdown_unflushed_bytes", double(r.unflushed_bytes));
        }
        std::uint32_t n = 0;
        for (const Event& e : server.events)
            if (e.type == EventType::Connected)
                CHECK(Contiguous(server, e.connection, kControl, &n));
    }
    CHECK(gns_library_users() == 0);
}

TEST("race/unpolled-destroy")
{
    // The server's owner stops polling (stalled) while the client sends below
    // the link rate, so data piles up unread in the server's library buffer;
    // then the server is destroyed. The client is told it was a shutdown.
    auto rig = MakeGnsRig();
    Node server{rig->make(TestConfig())}, client{rig->make(TestConfig())};
    REQUIRE(ConnectClients(*rig, server, {&client}));
    server.polling = false;
    const ConnectionId c = FirstConnected(client);
    std::uint32_t seq = 0;
    std::int64_t next = rig->now_ms();
    const std::int64_t t0 = rig->now_ms();
    PumpNodes(*rig, {&client}, [&] {
        if (rig->now_ms() >= next)
        {
            next += 8;
            CHECK(Send(*client.t, c, kControl, seq++, 1000));
        }
        return rig->now_ms() - t0 >= 600;
    }, 2000);
    metric("sent_unread_before_destroy", seq);
    server.t.reset();
    REQUIRE(PumpNodes(*rig, {&client}, [&] { return client.Find(EventType::Disconnected) != nullptr; }, 5000));
    const CloseReason reason = client.Find(EventType::Disconnected)->reason;
    CHECK_MSG(reason == CloseReason::PeerShutdown, "%s", to_string(reason));
}

TEST("race/wrong-thread-storm")
{
    // Four threads call into a transport they do not own while its owner
    // exchanges traffic. Every foreign call is refused and counted; the
    // owner's traffic is unaffected.
    auto rig = MakeGnsRig();
    Node server{rig->make(TestConfig())}, client{rig->make(TestConfig())};
    REQUIRE(ConnectClients(*rig, server, {&client}));
    const ConnectionId c = FirstConnected(client);
    std::atomic<bool> go{true};
    std::atomic<std::uint64_t> calls{0}, accepted{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&] {
            std::vector<Event> sink;
            ConnectionStatus st;
            while (go)
            {
                accepted += client.t->send(c, kControl, "x", 1) == SendResult::Sent ? 1u : 0u;
                client.t->poll(sink);
                accepted += client.t->status(c, st) ? 1u : 0u;
                accepted += sink.size();
                calls += 3;
            }
        });
    std::uint32_t seq = 0;
    const bool ok = PumpNodes(*rig, {&server, &client}, [&] {
        if (seq < 200)
            seq += Send(*client.t, c, kActions, seq, 200) ? 1u : 0u;
        return server.Messages(server.Find(EventType::Connected)->connection, kActions).size() >= 200;
    }, 10000);
    go = false;
    for (auto& t : threads)
        t.join();
    CHECK(ok);
    CHECK(accepted == 0);
    CHECK_MSG(client.t->counters().wrong_thread_calls == calls.load(), "%llu counted vs %llu made",
        (unsigned long long)client.t->counters().wrong_thread_calls, (unsigned long long)calls.load());
    std::uint32_t n = 0;
    CHECK(Contiguous(server, server.Find(EventType::Connected)->connection, kActions, &n) && n == 200);
    metric("foreign_calls", double(calls.load()));
}

} // namespace coop::test
