// Co-op transport adapter: the only network interface gameplay code sees.
//
// S04-T02. No transport library type appears here; GameNetworkingSockets is
// used only in src/net/gns_transport.cpp, and an in-memory backend
// (fake_network.h) implements the same interface for tests.
//
// Threading contract
// - A Transport belongs to the thread that created it (the owner, normally the
//   simulation thread). Every call from another thread is refused with
//   SendResult::WrongThread / false and counted; nothing is delivered on any
//   other thread.
// - Nothing calls back into game code. Connection changes and received
//   messages become Events, returned by poll() on the owner thread, so network
//   input can only reach the world at the point the owner chooses (the tick).
// - Destroy the transport on the owner thread. The destructor closes every
//   connection without flushing; call shutdown() first to flush.
//
// Delivery classes (one per channel; a channel is one library lane)
// - Critical:  reliable, ordered within its channel, never dropped. If the
//   connection's unacknowledged backlog on the channel exceeds its bound, the
//   adapter closes the connection (CloseReason::Overloaded) instead of
//   silently losing a critical message.
// - Bulk:      reliable, ordered within its channel, normally low priority. An
//   over-bound send is refused (SendResult::Backpressure); the caller keeps
//   the data and retries later. Nothing is lost.
// - Transient: unreliable. A send is dropped (SendResult::Dropped) when the
//   channel's queue is older than max_queue_ms or over its byte bound, so
//   stale data never fills the connection's shared send buffer. The receiver
//   may see loss, duplicates are removed by the library, and ordering between
//   messages is not guaranteed; sequence policy belongs to the protocol.
//
// Ordering is per channel only. Transport order is not gameplay order.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace coop::net
{
using ConnectionId = std::uint32_t; // assigned by the transport; never reused by it
using ChannelId = std::uint8_t;     // index into TransportConfig::channels

constexpr ConnectionId kNoConnection = 0;
constexpr std::size_t kMaxChannels = 16;
// A transient message is at most one library fragment group; anything larger
// would be sent reliably by the library, so the config refuses it.
constexpr std::uint32_t kMaxTransientMessageBytes = 16500;
constexpr std::uint32_t kMaxMessageBytes = 512 * 1024;

enum class Delivery : std::uint8_t
{
    Critical,
    Bulk,
    Transient,
};

struct ChannelSpec
{
    std::string name;
    Delivery delivery = Delivery::Critical;
    // Lower is sent first. Channels with equal priority share by weight.
    std::uint8_t priority = 0;
    std::uint16_t weight = 1;
    // Largest message this side sends or accepts on the channel. A peer that
    // sends more is closed for a protocol violation.
    std::uint32_t max_message_bytes = 1200;
    // Per connection. Critical and Bulk: bytes queued plus sent but not yet
    // acknowledged. Transient: bytes queued.
    std::uint32_t max_backlog_bytes = 64 * 1024;
    // Transient only: drop new messages while the channel's estimated queue
    // time exceeds this. 0 disables the age rule (the byte bound still holds).
    std::uint32_t max_queue_ms = 50;
};

struct TransportConfig
{
    std::vector<ChannelSpec> channels; // 1..kMaxChannels; both peers must agree
    // Listening side: open incoming connections (handshaking and connected)
    // beyond this are refused with close_code::kServerFull. Higher than the
    // player count so a reconnect can be authenticated while a stale session
    // still holds its slot (S04-T01 finding 2).
    std::uint32_t max_connections = 4;
    std::uint32_t send_rate_bytes_per_s = 256 * 1024;
    // Library send buffer per connection, shared by every channel. Must be at
    // least the sum of the channels' max_backlog_bytes, so the library itself
    // never refuses a send the adapter admitted (S04-T01 finding 3).
    std::uint32_t send_buffer_bytes = 512 * 1024;
    // Library receive buffer per connection. When the owner does not poll,
    // received data waits here; beyond it reliable data is not acknowledged
    // (the sender retries) and unreliable data is dropped.
    std::uint32_t recv_buffer_bytes = 1024 * 1024;
    std::uint32_t recv_buffer_messages = 1000;
    std::uint32_t connect_timeout_ms = 10000;
    // Dead-peer detection needs traffic that expects a reply (S04-T01
    // finding 1); liveness heartbeats are the session layer's job (S04-T05).
    std::uint32_t connected_timeout_ms = 10000;
    // Messages taken from one connection per poll(); the rest stay in the
    // library's bounded receive buffer for the next poll.
    std::uint32_t max_messages_per_poll = 256;
};

// Application close codes on the wire. The session layer uses
// [kSessionMin, kMax] for its own reasons (S04-T04/T05).
namespace close_code
{
constexpr int kNormal = 1000;
constexpr int kServerFull = 1001;
constexpr int kOverloaded = 1002;
constexpr int kProtocolViolation = 1003;
constexpr int kShutdown = 1004;
constexpr int kSessionMin = 1100;
constexpr int kMax = 1999;
} // namespace close_code

enum class CloseReason : std::uint8_t
{
    None,
    PeerClosed,        // the peer closed; see Event::code
    Refused,           // the listener refused us (full)
    PeerShutdown,      // the peer's transport shut down
    Timeout,           // no reply within connected_timeout_ms
    ConnectFailed,     // never connected (timeout, unreachable, bad address)
    LocalProblem,      // library-detected problem on this side
    RemoteProblem,     // the peer's library reported a problem
    Overloaded,        // we closed it: a critical backlog exceeded its bound
    ProtocolViolation, // we closed it: unknown channel, wrong delivery or oversize message
};

const char* to_string(CloseReason reason);
const char* to_string(Delivery delivery);

enum class EventType : std::uint8_t
{
    Connected,
    Disconnected,
    Message,
};

// For one connection, events arrive in the order Connected, Message...,
// Disconnected. Messages the peer sent before closing are delivered before its
// Disconnected. A connection the owner closed itself gets no Disconnected.
struct Event
{
    EventType type = EventType::Message;
    ConnectionId connection = kNoConnection;
    bool incoming = false;                 // Connected: accepted by our listener
    ChannelId channel = 0;                 // Message
    std::vector<std::uint8_t> payload;     // Message; owned, independent of the transport
    std::int64_t received_us = 0;          // Message: library receipt time (transport clock)
    CloseReason reason = CloseReason::None; // Disconnected
    int code = 0;                          // Disconnected: wire or library code
    std::string detail;                    // Disconnected: diagnostic text, never shown to players
};

enum class SendResult : std::uint8_t
{
    Sent,
    Dropped,        // Transient: stale or over its bound; counted, nothing queued
    Backpressure,   // Bulk: over its bound; nothing queued, retry later
    Overloaded,     // Critical: over its bound; the connection is now closed
    NotConnected,   // unknown, closed or not yet connected
    InvalidChannel, // no such channel
    TooLarge,       // over the channel's max_message_bytes
    WrongThread,    // called off the owner thread
    Failed,         // library refused for another reason (counted)
};

const char* to_string(SendResult result);

enum class ConnectionState : std::uint8_t
{
    Connecting,
    Connected,
    Closed,
};

struct ChannelBacklog
{
    std::uint32_t queued_bytes = 0; // not yet sent
    std::uint32_t unacked_bytes = 0; // sent, reliable, not yet acknowledged
    std::uint32_t queue_ms = 0;     // estimated wait for a message queued now
};

struct ConnectionStatus
{
    ConnectionState state = ConnectionState::Closed;
    bool incoming = false;
    std::string remote_address;
    int ping_ms = -1;
    float quality_local = -1.f;  // fraction of packets delivered to us
    float quality_remote = -1.f; // fraction of our packets the peer received
    float out_bytes_per_s = 0.f;
    float in_bytes_per_s = 0.f;
    std::vector<ChannelBacklog> channels; // library figures at the last poll, plus bytes queued since
};

struct ChannelCounters
{
    std::uint64_t sent_messages = 0;
    std::uint64_t sent_bytes = 0;
    std::uint64_t received_messages = 0;
    std::uint64_t received_bytes = 0;
    std::uint64_t dropped_stale = 0;   // Transient: queue older than max_queue_ms
    std::uint64_t dropped_backlog = 0; // Transient: over max_backlog_bytes
    std::uint64_t backpressure = 0;    // Bulk refusals
    std::uint64_t overloads = 0;       // Critical: connections closed
};

struct TransportCounters
{
    std::vector<ChannelCounters> channels;
    std::uint64_t connections_accepted = 0;
    std::uint64_t connections_refused = 0; // listener full
    std::uint64_t connections_failed = 0;  // outbound that never connected
    std::uint64_t protocol_violations = 0;
    std::uint64_t wrong_thread_calls = 0;
    std::uint64_t stale_notifications = 0; // library notices for connections already closed
    std::uint64_t send_failures = 0;
};

struct ShutdownReport
{
    std::uint32_t connections_closed = 0;
    // True when every connection's reliable data was acknowledged before
    // closing (or flushing was not requested and nothing was pending).
    bool flushed = true;
    std::uint64_t unflushed_bytes = 0; // reliable bytes not acknowledged at close
    std::uint32_t waited_ms = 0;
};

class Transport
{
public:
    virtual ~Transport() = default;
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    // Start accepting connections ("ip:port" for the network backend, any
    // name for the fake). One listener per transport.
    virtual bool listen(const char* address, std::string* error = nullptr) = 0;

    // Start an outbound connection. Returns its id at once; a Connected or
    // Disconnected (ConnectFailed/Refused) event follows. kNoConnection on a
    // bad address or a closed transport.
    virtual ConnectionId connect(const char* address, std::string* error = nullptr) = 0;

    // Queue one message. Only on a Connected connection. The data is copied.
    virtual SendResult send(ConnectionId connection, ChannelId channel, const void* data, std::uint32_t size) = 0;

    // Ask the library to send what is queued now instead of waiting to batch.
    virtual void flush(ConnectionId connection) = 0;

    // Append everything that happened since the last poll to `out`.
    virtual void poll(std::vector<Event>& out) = 0;

    // Close one connection with a close_code in [kNormal, kMax]. flush=true
    // lets queued reliable data go out first (the library lingers). No
    // Disconnected event is raised for it. False if it was not open.
    virtual bool close(ConnectionId connection, int code, const char* detail, bool flush) = 0;

    // Stop listening and close every connection, first waiting up to
    // flush_timeout_ms for their reliable data to be acknowledged. The
    // transport is closed afterwards: sends fail and poll returns nothing.
    virtual ShutdownReport shutdown(std::uint32_t flush_timeout_ms) = 0;

    virtual bool status(ConnectionId connection, ConnectionStatus& out) const = 0;
    virtual TransportCounters counters() const = 0;
    virtual const TransportConfig& config() const = 0;

    // Microseconds on the transport's clock (receipt times use it).
    virtual std::int64_t now_us() const = 0;

protected:
    Transport() = default;
};

// Checks a config: channel count, sizes, bounds and that the sum of channel
// backlogs fits the send buffer. Empty string when valid.
std::string validate(const TransportConfig& config);

// GameNetworkingSockets backend. Null with *error set on failure.
std::unique_ptr<Transport> create_gns_transport(const TransportConfig& config, std::string* error = nullptr);

// Optional sink for library diagnostics. May be called on the library's own
// thread, so it must be thread-safe and must not touch game state.
enum class LogLevel : std::uint8_t
{
    Error,
    Warning,
    Info,
    Debug,
};
using LogSink = void (*)(LogLevel level, const char* text);
void set_log_sink(LogSink sink, LogLevel max_level);

} // namespace coop::net
