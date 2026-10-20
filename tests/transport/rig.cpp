#include "rig.h"

#include <windows.h>

#include <algorithm>
#include <chrono>

namespace coop::test
{
TransportConfig TestConfig()
{
    TransportConfig c;
    c.channels = {
        {"control", Delivery::Critical, 0, 1, 4096, 64 * 1024, 0},
        {"actions", Delivery::Critical, 0, 1, 4096, 64 * 1024, 0},
        {"bulk", Delivery::Bulk, 2, 1, 64 * 1024, 192 * 1024, 0},
        {"snapshot", Delivery::Transient, 1, 1, 1200, 16 * 1024, 50},
        {"input", Delivery::Transient, 0, 1, 512, 8 * 1024, 50},
    };
    c.max_connections = 4;
    c.connected_timeout_ms = 5000;
    c.connect_timeout_ms = 5000;
    return c;
}

namespace
{
class FakeRig final : public Rig
{
public:
    explicit FakeRig(const FakeLinkConfig& link) : net_(link) {}
    const char* name() const override { return "fake"; }
    bool real() const override { return false; }
    std::unique_ptr<Transport> make(const TransportConfig& config) override
    {
        std::string error;
        auto t = net_.create(config, &error);
        REQUIRE_MSG(t != nullptr, "create: %s", error.c_str());
        return t;
    }
    std::string address() override { return "server-" + std::to_string(next_++); }
    void tick() override { net_.advance(1); }
    std::int64_t now_ms() const override { return net_.now_us() / 1000; }
    void crash(Transport& t) override { net_.crash(t); }
    void impair(const GnsImpairment& m) override
    {
        FakeLinkConfig l;
        l.latency_ms = m.lag_ms > 0 ? std::uint32_t(m.lag_ms) : 10u;
        l.transient_loss = m.loss_pct / 100.0;
        net_.set_link(l);
    }
    FakeNetwork& net() { return net_; }

private:
    FakeNetwork net_;
    int next_ = 1;
};

class GnsRig final : public Rig
{
public:
    const char* name() const override { return "gns"; }
    bool real() const override { return true; }
    std::unique_ptr<Transport> make(const TransportConfig& config) override
    {
        std::string error;
        auto t = create_gns_transport(config, &error);
        REQUIRE_MSG(t != nullptr, "create: %s", error.c_str());
        if (!cleared_)
        {
            // Impairments are static in the library and outlive Kill/Init.
            REQUIRE(set_gns_impairment({}));
            cleared_ = true;
        }
        return t;
    }
    std::string address() override { return "127.0.0.1:" + std::to_string(NextPort()); }
    void tick() override { Sleep(1); }
    std::int64_t now_ms() const override
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
    void crash(Transport&) override { REQUIRE_MSG(false, "the GNS rig cannot crash an in-process transport"); }
    void impair(const GnsImpairment& m) override { REQUIRE(set_gns_impairment(m)); }

    static int NextPort()
    {
        static int port = 27300 + int(GetCurrentProcessId() % 50) * 20;
        return port++;
    }

private:
    bool cleared_ = false;
};
} // namespace

std::unique_ptr<Rig> MakeFakeRig(const FakeLinkConfig& link) { return std::make_unique<FakeRig>(link); }
std::unique_ptr<Rig> MakeGnsRig() { return std::make_unique<GnsRig>(); }
std::string NextGnsAddress() { return "127.0.0.1:" + std::to_string(GnsRig::NextPort()); }

int Node::Count(EventType type, ConnectionId conn) const
{
    int n = 0;
    for (const Event& e : events)
        if (e.type == type && (conn == kNoConnection || e.connection == conn))
            ++n;
    return n;
}

const Event* Node::Find(EventType type, ConnectionId conn) const
{
    for (const Event& e : events)
        if (e.type == type && (conn == kNoConnection || e.connection == conn))
            return &e;
    return nullptr;
}

std::vector<const Event*> Node::Messages(ConnectionId conn, ChannelId channel) const
{
    std::vector<const Event*> out;
    for (const Event& e : events)
        if (e.type == EventType::Message && e.connection == conn && e.channel == channel)
            out.push_back(&e);
    return out;
}

bool Pump(Rig& rig, const std::vector<Node*>& nodes, const std::function<bool()>& done, std::int64_t timeout_ms)
{
    const std::int64_t end = rig.now_ms() + timeout_ms;
    for (;;)
    {
        for (Node* n : nodes)
            n->Poll();
        if (done())
            return true;
        if (rig.now_ms() >= end)
            return false;
        rig.tick();
    }
}

void PumpFor(Rig& rig, const std::vector<Node*>& nodes, std::int64_t ms)
{
    Pump(rig, nodes, [] { return false; }, ms);
}

void CheckEventOrder(const Node& node, const char* label)
{
    std::map<ConnectionId, int> state; // 0 new, 1 connected, 2 ended
    for (const Event& e : node.events)
    {
        CHECK_MSG(e.connection != kNoConnection, "%s: event without a connection", label);
        int& s = state[e.connection];
        switch (e.type)
        {
        case EventType::Connected:
            CHECK_MSG(s == 0, "%s: connection %u connected twice or after its end", label, e.connection);
            s = 1;
            break;
        case EventType::Message:
            CHECK_MSG(s == 1, "%s: message on connection %u in state %d", label, e.connection, s);
            break;
        case EventType::Disconnected:
            CHECK_MSG(s != 2, "%s: connection %u disconnected twice", label, e.connection);
            s = 2;
            break;
        }
    }
}

std::vector<std::uint8_t> Payload(std::uint32_t seq, std::uint32_t size)
{
    size = std::max<std::uint32_t>(size, 8);
    std::vector<std::uint8_t> p(size);
    for (int i = 0; i < 4; ++i)
        p[std::size_t(i)] = std::uint8_t(seq >> (8 * i));
    for (int i = 0; i < 4; ++i)
        p[std::size_t(4 + i)] = std::uint8_t(size >> (8 * i));
    for (std::uint32_t i = 8; i < size; ++i)
        p[i] = std::uint8_t(seq * 31u + i * 7u);
    return p;
}

bool PayloadOk(const std::vector<std::uint8_t>& p, std::uint32_t* seq)
{
    if (p.size() < 8)
        return false;
    std::uint32_t s = 0, size = 0;
    for (int i = 0; i < 4; ++i)
        s |= std::uint32_t(p[std::size_t(i)]) << (8 * i);
    for (int i = 0; i < 4; ++i)
        size |= std::uint32_t(p[std::size_t(4 + i)]) << (8 * i);
    if (size != p.size())
        return false;
    for (std::uint32_t i = 8; i < size; ++i)
        if (p[i] != std::uint8_t(s * 31u + i * 7u))
            return false;
    if (seq)
        *seq = s;
    return true;
}

bool ConnectClients(Rig& rig, Node& server, std::vector<Node*> clients, std::int64_t timeout_ms)
{
    std::string error;
    const std::string address = rig.address();
    if (!server.t->listen(address.c_str(), &error))
    {
        fail(__FILE__, __LINE__, "listen: " + error);
        return false;
    }
    for (Node* c : clients)
        if (c->t->connect(address.c_str(), &error) == kNoConnection)
        {
            fail(__FILE__, __LINE__, "connect: " + error);
            return false;
        }
    std::vector<Node*> all = clients;
    all.push_back(&server);
    return Pump(
        rig, all,
        [&] {
            for (Node* c : clients)
                if (!c->Find(EventType::Connected))
                    return false;
            return server.Count(EventType::Connected) >= int(clients.size());
        },
        timeout_ms);
}

Percentiles Summarise(std::vector<double> v)
{
    Percentiles p;
    p.count = v.size();
    if (v.empty())
        return p;
    std::sort(v.begin(), v.end());
    auto at = [&](double q) { return v[std::min(v.size() - 1, std::size_t(q * double(v.size() - 1) + 0.5))]; };
    p.p50 = at(0.5);
    p.p95 = at(0.95);
    p.p99 = at(0.99);
    p.max = v.back();
    return p;
}

ScenarioRegistrar::ScenarioRegistrar(const char* name, void (*fn)(Rig&))
{
    const std::string n = name;
    Registrar fake("fake/" + n, [fn] {
        auto rig = MakeFakeRig();
        fn(*rig);
    });
    Registrar gns("gns/" + n, [fn] {
        {
            auto rig = MakeGnsRig();
            fn(*rig);
        }
        CHECK_MSG(gns_library_users() == 0, "%d GNS transports left alive", gns_library_users());
    });
}

} // namespace coop::test
