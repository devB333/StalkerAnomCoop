// Library-independent rules shared by the GNS and fake backends, so both
// apply the same overload and validation policy.
#pragma once

#include "coop/net/transport.h"

#include <thread>

namespace coop::net::detail
{
class OwnerThread
{
public:
    OwnerThread() : id_(std::this_thread::get_id()) {}
    bool here() const { return std::this_thread::get_id() == id_; }

private:
    std::thread::id id_;
};

enum class Admission : std::uint8_t
{
    Send,
    DropStale,    // Transient
    DropBacklog,  // Transient
    Backpressure, // Bulk
    Overload,     // Critical
};

// What the adapter knows about one channel of one connection: the library's
// figures at the last refresh, plus what this side queued since. The estimate
// only ever over-counts (the library only drains), so admission is
// conservative between refreshes.
struct ChannelEstimate
{
    ChannelBacklog last;
    std::uint64_t queued_since_refresh = 0;

    std::uint64_t backlog(Delivery delivery) const
    {
        std::uint64_t b = std::uint64_t(last.queued_bytes) + queued_since_refresh;
        if (delivery != Delivery::Transient)
            b += last.unacked_bytes;
        return b;
    }
};

Admission admit(const ChannelSpec& spec, const ChannelEstimate& estimate, std::uint32_t size);

enum class InboundCheck : std::uint8_t
{
    Ok,
    UnknownChannel,
    WrongDelivery,
    TooLarge,
};

InboundCheck check_inbound(const TransportConfig& config, unsigned channel, bool reliable, std::uint32_t size);
const char* to_string(InboundCheck check);

// Close code a peer sent us -> reason reported to the owner.
CloseReason reason_from_peer_code(int code);

// Records the outcome of a send in the channel counters.
void count_send(ChannelCounters& counters, Admission admission, std::uint32_t size);

bool valid_close_code(int code);

} // namespace coop::net::detail
