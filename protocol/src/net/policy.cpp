#include "policy.h"

#include <algorithm>

namespace coop::net
{
const char* to_string(CloseReason reason)
{
    switch (reason)
    {
    case CloseReason::None: return "none";
    case CloseReason::PeerClosed: return "peer_closed";
    case CloseReason::Refused: return "refused";
    case CloseReason::PeerShutdown: return "peer_shutdown";
    case CloseReason::Timeout: return "timeout";
    case CloseReason::ConnectFailed: return "connect_failed";
    case CloseReason::LocalProblem: return "local_problem";
    case CloseReason::RemoteProblem: return "remote_problem";
    case CloseReason::Overloaded: return "overloaded";
    case CloseReason::ProtocolViolation: return "protocol_violation";
    }
    return "?";
}

const char* to_string(Delivery delivery)
{
    switch (delivery)
    {
    case Delivery::Critical: return "critical";
    case Delivery::Bulk: return "bulk";
    case Delivery::Transient: return "transient";
    }
    return "?";
}

const char* to_string(SendResult result)
{
    switch (result)
    {
    case SendResult::Sent: return "sent";
    case SendResult::Dropped: return "dropped";
    case SendResult::Backpressure: return "backpressure";
    case SendResult::Overloaded: return "overloaded";
    case SendResult::NotConnected: return "not_connected";
    case SendResult::InvalidChannel: return "invalid_channel";
    case SendResult::TooLarge: return "too_large";
    case SendResult::WrongThread: return "wrong_thread";
    case SendResult::Failed: return "failed";
    }
    return "?";
}

std::string validate(const TransportConfig& c)
{
    if (c.channels.empty() || c.channels.size() > kMaxChannels)
        return "channel count must be 1.." + std::to_string(kMaxChannels);
    std::uint64_t backlogSum = 0;
    std::uint32_t largest = 0;
    for (std::size_t i = 0; i < c.channels.size(); ++i)
    {
        const ChannelSpec& ch = c.channels[i];
        const std::string at = "channel " + std::to_string(i) + " (" + ch.name + "): ";
        if (ch.max_message_bytes < 1 || ch.max_message_bytes > kMaxMessageBytes)
            return at + "max_message_bytes must be 1.." + std::to_string(kMaxMessageBytes);
        if (ch.delivery == Delivery::Transient && ch.max_message_bytes > kMaxTransientMessageBytes)
            return at + "transient max_message_bytes must be <= " + std::to_string(kMaxTransientMessageBytes);
        if (ch.max_backlog_bytes < ch.max_message_bytes)
            return at + "max_backlog_bytes must be >= max_message_bytes";
        if (ch.weight < 1)
            return at + "weight must be >= 1";
        backlogSum += ch.max_backlog_bytes;
        largest = std::max(largest, ch.max_message_bytes);
    }
    if (c.send_buffer_bytes > 64u * 1024 * 1024)
        return "send_buffer_bytes must be <= 64 MB";
    if (backlogSum > c.send_buffer_bytes)
        return "sum of channel max_backlog_bytes (" + std::to_string(backlogSum) + ") exceeds send_buffer_bytes (" +
               std::to_string(c.send_buffer_bytes) + ")";
    if (c.recv_buffer_bytes <= largest)
        return "recv_buffer_bytes must exceed the largest max_message_bytes";
    if (c.recv_buffer_messages < 2)
        return "recv_buffer_messages must be >= 2";
    if (c.max_connections < 1)
        return "max_connections must be >= 1";
    if (c.send_rate_bytes_per_s < 1024)
        return "send_rate_bytes_per_s must be >= 1024";
    if (c.connect_timeout_ms < 100 || c.connected_timeout_ms < 100)
        return "timeouts must be >= 100 ms";
    if (c.max_messages_per_poll < 1)
        return "max_messages_per_poll must be >= 1";
    return {};
}
} // namespace coop::net

namespace coop::net::detail
{
Admission admit(const ChannelSpec& spec, const ChannelEstimate& estimate, std::uint32_t size)
{
    const bool overBound = estimate.backlog(spec.delivery) + size > spec.max_backlog_bytes;
    switch (spec.delivery)
    {
    case Delivery::Critical:
        return overBound ? Admission::Overload : Admission::Send;
    case Delivery::Bulk:
        return overBound ? Admission::Backpressure : Admission::Send;
    case Delivery::Transient:
        if (spec.max_queue_ms > 0 && estimate.last.queue_ms > spec.max_queue_ms)
            return Admission::DropStale;
        return overBound ? Admission::DropBacklog : Admission::Send;
    }
    return Admission::Send;
}

InboundCheck check_inbound(const TransportConfig& config, unsigned channel, bool reliable, std::uint32_t size)
{
    if (channel >= config.channels.size())
        return InboundCheck::UnknownChannel;
    const ChannelSpec& spec = config.channels[channel];
    if (reliable != (spec.delivery != Delivery::Transient))
        return InboundCheck::WrongDelivery;
    if (size > spec.max_message_bytes)
        return InboundCheck::TooLarge;
    return InboundCheck::Ok;
}

const char* to_string(InboundCheck check)
{
    switch (check)
    {
    case InboundCheck::Ok: return "ok";
    case InboundCheck::UnknownChannel: return "unknown channel";
    case InboundCheck::WrongDelivery: return "wrong delivery class for channel";
    case InboundCheck::TooLarge: return "message over channel limit";
    }
    return "?";
}

CloseReason reason_from_peer_code(int code)
{
    switch (code)
    {
    case close_code::kServerFull: return CloseReason::Refused;
    case close_code::kShutdown: return CloseReason::PeerShutdown;
    default: return CloseReason::PeerClosed;
    }
}

void count_send(ChannelCounters& c, Admission admission, std::uint32_t size)
{
    switch (admission)
    {
    case Admission::Send:
        ++c.sent_messages;
        c.sent_bytes += size;
        break;
    case Admission::DropStale: ++c.dropped_stale; break;
    case Admission::DropBacklog: ++c.dropped_backlog; break;
    case Admission::Backpressure: ++c.backpressure; break;
    case Admission::Overload: ++c.overloads; break;
    }
}

bool valid_close_code(int code) { return code >= close_code::kNormal && code <= close_code::kMax; }

} // namespace coop::net::detail
