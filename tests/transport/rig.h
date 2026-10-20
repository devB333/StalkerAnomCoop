// Shared scaffolding for transport tests: one scenario body runs against the
// fake backend (virtual time) and the GNS backend (real time, loopback).
#pragma once

#include "check.h"
#include "coop/net/fake_network.h"
#include "coop/net/gns_testing.h"
#include "coop/net/transport.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace coop::test
{
using namespace coop::net;

// Channel layout used by the tests, shaped like docs/network-protocol.md's
// lanes. The real layout is S04-T03's.
enum : ChannelId
{
    kControl = 0,  // Critical, priority 0
    kActions = 1,  // Critical, priority 0 (independent of control: own lane)
    kBulk = 2,     // Bulk, priority 2
    kSnapshot = 3, // Transient, priority 1
    kInput = 4,    // Transient, priority 0
};
TransportConfig TestConfig();

class Rig
{
public:
    virtual ~Rig() = default;
    virtual const char* name() const = 0;
    virtual bool real() const = 0;
    virtual std::unique_ptr<Transport> make(const TransportConfig& config) = 0;
    virtual std::string address() = 0; // a fresh listen address
    virtual void tick() = 0;           // ~1 ms
    virtual std::int64_t now_ms() const = 0;
    virtual void crash(Transport& t) = 0; // fake: crash; gns: unsupported (REQUIRE fails)
    // Call after creating the transports. GNS: the library's simulated loss,
    // reordering, duplication and lag (process-global; the GNS rig clears them
    // when it creates its first transport). Fake: transient loss and one-way
    // latency only.
    virtual void impair(const GnsImpairment& m) = 0;
};

std::unique_ptr<Rig> MakeFakeRig(const FakeLinkConfig& link = {});
std::unique_ptr<Rig> MakeGnsRig();
std::string NextGnsAddress(); // 127.0.0.1 with a port no other test used

struct Node
{
    std::unique_ptr<Transport> t;
    std::vector<Event> events; // everything polled so far, in order
    bool polling = true;       // false: simulate an owner that stopped polling
    std::size_t seen = 0;      // cursor for Take()

    void Poll()
    {
        if (polling && t)
            t->poll(events);
    }
    // Events added since the last Take().
    std::vector<Event> Take()
    {
        std::vector<Event> out(events.begin() + std::ptrdiff_t(seen), events.end());
        seen = events.size();
        return out;
    }
    int Count(EventType type, ConnectionId conn = kNoConnection) const;
    const Event* Find(EventType type, ConnectionId conn = kNoConnection) const;
    std::vector<const Event*> Messages(ConnectionId conn, ChannelId channel) const;
};

// Ticks the rig and polls every node until done() or timeout_ms passes.
bool Pump(Rig& rig, const std::vector<Node*>& nodes, const std::function<bool()>& done, std::int64_t timeout_ms);
void PumpFor(Rig& rig, const std::vector<Node*>& nodes, std::int64_t ms);

// Checks the per-connection event order: Connected before any Message, nothing
// after Disconnected, at most one of each.
void CheckEventOrder(const Node& node, const char* label);

// Payload with a sequence number and a checkable pattern.
std::vector<std::uint8_t> Payload(std::uint32_t seq, std::uint32_t size);
bool PayloadOk(const std::vector<std::uint8_t>& p, std::uint32_t* seq);

// Server + n clients connected through a rig. False if they did not connect.
bool ConnectClients(Rig& rig, Node& server, std::vector<Node*> clients, std::int64_t timeout_ms = 5000);

struct Percentiles
{
    double p50 = 0, p95 = 0, p99 = 0, max = 0;
    std::size_t count = 0;
};
Percentiles Summarise(std::vector<double> v);

// Registers "fake/<name>" and "gns/<name>" for one scenario body.
struct ScenarioRegistrar
{
    ScenarioRegistrar(const char* name, void (*fn)(Rig&));
};
#define SCENARIO(name)                                                                          \
    static void COOP_CAT(scenario_fn_, __LINE__)(::coop::test::Rig&);                          \
    static ::coop::test::ScenarioRegistrar COOP_CAT(scenario_reg_, __LINE__)(name, &COOP_CAT(scenario_fn_, __LINE__)); \
    static void COOP_CAT(scenario_fn_, __LINE__)(::coop::test::Rig & rig)

} // namespace coop::test
