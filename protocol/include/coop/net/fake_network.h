// In-memory backend of coop::net::Transport for deterministic tests.
//
// One FakeNetwork holds a virtual clock and any number of transports created
// from it. Time moves only in advance(); everything is single-threaded (the
// network and its transports belong to the thread that created them).
//
// Model, per connection and direction: the sender's channels queue messages;
// each millisecond the sender transmits up to its send_rate_bytes_per_s
// (lowest priority value first, round-robin between equal priorities; weights
// are ignored); a message arrives latency_ms later; transient messages are
// lost with probability transient_loss; a reliable message is acknowledged
// latency_ms after the receiver buffers it. The receiver's buffer is bounded by
// its recv_buffer_bytes/messages: a reliable message that does not fit waits
// (holding back later ones on its channel), an unreliable one is dropped.
//
// Differences from GameNetworkingSockets, on purpose: no packets, headers,
// retransmission or congestion control; a crashed peer is detected exactly
// connected_timeout_ms after it stops (GNS needs traffic that expects a reply,
// S04-T01 finding 1); reliable data is never lost, only delayed.
#pragma once

#include "coop/net/transport.h"

#include <cstdint>
#include <memory>
#include <string>

namespace coop::net
{
struct FakeLinkConfig
{
    std::uint32_t latency_ms = 10; // one way
    double transient_loss = 0.0;   // 0..1
    std::uint64_t seed = 1;
};

class FakeNetwork
{
public:
    explicit FakeNetwork(const FakeLinkConfig& link = {});
    ~FakeNetwork();
    FakeNetwork(const FakeNetwork&) = delete;
    FakeNetwork& operator=(const FakeNetwork&) = delete;

    // Any listen address is a name; connect() looks it up.
    std::unique_ptr<Transport> create(const TransportConfig& config, std::string* error = nullptr);

    // Move virtual time forward in 1 ms steps.
    void advance(std::uint32_t ms);
    std::int64_t now_us() const;

    // Applies to transmissions from now on.
    void set_link(const FakeLinkConfig& link);

    // The transport's process dies: it stops sending, receiving and accepting,
    // closes nothing, and its peers time out. The object stays valid (for the
    // test to destroy) but does nothing more.
    void crash(Transport& transport);

    struct State;

private:
    std::shared_ptr<State> state_;
};
} // namespace coop::net
