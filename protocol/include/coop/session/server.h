// Server side of S04-T04: turns transport connections into authenticated
// sessions bound to canonical players.
//
// Per connection: AwaitHello -> AwaitProof -> Active. A Hello with another
// major, build or content gets a Reject and is closed. A valid Hello gets a
// challenge. A proof that does not verify against the roster key for the
// claimed player gets Reject AuthFailed. Only then is the connection bound to
// the player, who gets their slot (kept across reconnects in this process)
// and a Welcome. Anything out of order is a protocol violation (close 1003).
//
// Game code sees only SessionEvents. A Message event names the player its
// connection is bound to, never one a message claims: no message type
// carries the acting player, so a client cannot act as anyone else, and
// remote players get no privileged local-client role (the engine's bridge
// client stays in-process, docs/participant-identity.md).
//
// Owned by the thread that owns the transport.
#pragma once

#include "coop/net/transport.h"
#include "coop/proto/guard.h"
#include "coop/proto/messages.h"
#include "coop/session/auth.h"

#include <array>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace coop::session
{
// Transport close codes the session layer uses (net::close_code range).
namespace close_code
{
constexpr int kRejected = net::close_code::kSessionMin;            // after a Reject
constexpr int kHandshakeTimeout = net::close_code::kSessionMin + 1;
constexpr int kRateLimited = net::close_code::kSessionMin + 2;
constexpr int kReplaced = net::close_code::kSessionMin + 3;          // the player connected again
constexpr int kReleased = net::close_code::kSessionMin + 4;          // the server released the player
constexpr int kTimeout = net::close_code::kSessionMin + 5;           // nothing heard within the timeout
} // namespace close_code

// S04-T05 liveness (docs/session-authentication.md "Liveness and reconnect").
// The client sends a reliable heartbeat every interval and the server echoes
// it; any message counts as hearing from the peer. Provisional values,
// measured in docs/validation/S04-T05.md.
struct LivenessConfig
{
    std::uint32_t heartbeat_interval_ms = 1000;
    std::uint32_t suspect_after_ms = 5000;
    std::uint32_t timeout_ms = 15000;
    // How long after a session ends its resume token still continues it.
    std::uint32_t resume_window_ms = 10 * 60 * 1000;
    // Free a disconnected player's slot after this long; 0 keeps it for the
    // life of the process (the default: a roster of two needs no reuse).
    std::uint32_t release_after_ms = 0;
};

// The transport layout with the transport's own timeout just above the
// session's, so the session layer decides when a peer is gone.
net::TransportConfig transport_config(const LivenessConfig& liveness);

struct RateLimit
{
    float per_second = 0; // 0: the channel takes nothing from clients
    float burst = 0;
};

struct ServerConfig
{
    proto::LocalBuild build;
    proto::Uuid128 campaign_id;
    proto::Uuid128 party_id;
    std::uint32_t max_players = 2; // slots; a player keeps theirs until released
    std::uint64_t authority_epoch = 1;
    std::uint16_t tick_rate_hz = 30;
    std::uint16_t snapshot_rate_hz = 20;
    std::uint32_t handshake_timeout_ms = 5000;
    // S11-T00: the host's join code (auth.h); empty: only roster players. A
    // player not in the roster with a valid join proof is added to the roster
    // (taken_by_code) while a slot is free.
    std::string join_code;
    LivenessConfig liveness;
    // Handshakes started per remote address (port ignored) per window.
    std::uint32_t max_handshakes_per_address = 6;
    std::uint32_t address_window_ms = 60000;
    // After the Welcome, per connection and channel. Over the limit, a
    // reliable channel closes the connection (kRateLimited) and a transient
    // one drops the message.
    std::array<RateLimit, proto::kChannelCount> rates{{
        {10, 20},   // control: heartbeats
        {30, 60},   // actions
        {0, 0},     // lifecycle (server only)
        {0, 0},     // bulk (server only)
        {60, 120},  // snapshot acks
        {120, 240}, // inputs
    }};
};

enum class SessionEventType : std::uint8_t
{
    Joined,    // a player's session started, resumed, or replaced an older connection
    Left,      // a player's connection ended; the slot stays theirs
    Message,   // a message from a joined player
    Suspect,   // nothing heard for suspect_after_ms
    Recovered, // heard again after Suspect
    Released,  // the slot was freed (release() or release_after_ms)
};

enum class LeftReason : std::uint8_t
{
    Closed,      // the peer or the transport closed it
    Timeout,     // nothing heard for timeout_ms
    Replaced,    // a newer connection of the same player took over
    Released,    // the server released the player
    RateLimited,
    Violation,
    Shutdown,    // the server shut down
};

struct SessionEvent
{
    SessionEventType type = SessionEventType::Message;
    proto::Uuid128 player;
    std::uint8_t slot = 0;
    std::uint32_t slot_generation = 0;
    proto::Uuid128 session_id;
    net::ChannelId channel = 0;
    // Each Joined gets a new serial; every Message carries the serial of the
    // connection it arrived on, so nothing from a replaced connection can be
    // mistaken for the current one.
    std::uint64_t join_serial = 0;
    proto::Decoded message; // Message
    bool resumed = false;   // Joined: continues the previous session (same session id)
    std::uint64_t last_event = 0, last_snapshot = 0; // Joined, resumed: the client's cursors
    LeftReason reason = LeftReason::Closed; // Left
    bool replaced = false;  // Left: reason == Replaced
    std::string detail;     // Left: why (never contains credentials)
};

struct ServerCounters
{
    std::uint64_t handshakes_started = 0;
    std::uint64_t joins = 0;
    std::uint64_t replaced = 0;
    std::uint64_t rejected_version = 0, rejected_build = 0, rejected_content = 0;
    std::uint64_t rejected_auth = 0, rejected_full = 0, rejected_address_rate = 0;
    std::uint64_t handshake_timeouts = 0;
    std::uint64_t violations = 0;
    std::uint64_t rate_limited_closes = 0;
    std::uint64_t rate_limited_drops = 0;
    std::uint64_t dropped_by_guard = 0;
    std::uint64_t resumed = 0;
    std::uint64_t resume_refused = 0; // asked to resume, got a fresh session
    std::uint64_t timeouts = 0;
    std::uint64_t heartbeats = 0;
    std::uint64_t auto_released = 0;
    std::uint64_t joined_by_code = 0, rejected_code = 0; // S11-T00
    std::uint64_t rejected_busy = 0; // S11-T00: a proof while not accepting (set_accepting(false))
};

struct PlayerView
{
    proto::Uuid128 player;
    std::uint8_t slot = 0;
    std::uint32_t slot_generation = 0;
    bool connected = false;
    bool suspect = false;
    proto::Uuid128 session_id;
    std::uint64_t join_serial = 0;
    int ping_ms = -1; // transport's estimate
    // S06-T06: the client has sent a message inside the session, so it has
    // its Welcome. Reliable channels are ordered only within themselves: a
    // message on another channel sent right after the Welcome can arrive
    // before a lost and resent Welcome, and the client closes on it ("message
    // before the session started", cenc-14 at 10 % loss). Send on the other
    // channels only once this is true.
    bool confirmed = false;
};

class Server
{
public:
    Server(net::Transport& transport, const ServerConfig& config, Roster roster);

    // Polls the transport and runs every connection's state machine.
    void update(std::vector<SessionEvent>& out);
    // Sends to a joined, connected player. False otherwise.
    bool send(const proto::Uuid128& player, const proto::Message& message);
    // Frees a player's slot (closing their connection if any); the slot's
    // generation moves on, so references to the old binding go stale.
    bool release(const proto::Uuid128& player);
    // Ends every session (peers see the transport's shutdown) after flushing
    // up to flush_timeout_ms. Slots and tokens die with the process: there is
    // no recovery across a restart yet (Stage 7).
    void shutdown(std::uint32_t flush_timeout_ms, std::vector<SessionEvent>& out);

    std::vector<PlayerView> players() const;
    // S05-T02: the transport's backlog on one channel of a connected player's
    // connection, so a producer can pace critical traffic below its bound.
    bool backlog(const proto::Uuid128& player, net::ChannelId channel, net::ChannelBacklog& out) const;
    const ServerCounters& counters() const { return counters_; }
    // S11-T00: while false, an authenticated player gets Reject ServerBusy (the client retries): the game
    // replaces its world and a session started now would not survive into the new one.
    void set_accepting(bool accepting) { accepting_ = accepting; }
    bool accepting() const { return accepting_; }
    // S11-T00: the players the join code added since the last call (for the
    // host to keep in its roster file); public data only.
    std::vector<RosterEntry> take_added_by_code();
    // Diagnostic lines (never contain credentials, nonces or tokens).
    const std::vector<std::string>& log() const { return log_; }

private:
    enum class State : std::uint8_t
    {
        AwaitHello,
        AwaitProof,
        Active,
    };
    struct Bucket
    {
        bool started = false;
        float tokens = 0;
        std::int64_t at_us = 0;
    };
    struct Conn
    {
        net::ConnectionId id = net::kNoConnection;
        State state = State::AwaitHello;
        proto::StreamGuard guard{proto::Role::Server};
        proto::SequenceCounter seq;
        std::string address;
        std::int64_t started_us = 0;
        proto::Hello hello;
        proto::AuthChallenge challenge;
        int slot = -1;
        std::array<Bucket, proto::kChannelCount> buckets{};
        std::int64_t last_heard_us = 0;
        bool heard_in_session = false; // PlayerView::confirmed
        bool suspect = false;
        std::uint64_t join_serial = 0;
    };
    struct Slot
    {
        bool bound = false;
        proto::Uuid128 player;
        std::uint32_t generation = 0;
        net::ConnectionId conn = net::kNoConnection;
        proto::Uuid128 session_id;
        proto::Digest resume_token_hash{}; // the token itself is never stored
        std::int64_t detached_since_us = -1; // -1 while connected
        std::uint64_t join_serial = 0;
    };

    void onConnected(net::ConnectionId id);
    void onDisconnected(net::ConnectionId id, LeftReason reason, const std::string& why, std::vector<SessionEvent>& out);
    void checkLiveness(std::vector<SessionEvent>& out);
    void onMessage(Conn& c, const net::Event& e, std::vector<SessionEvent>& out);
    void onHello(Conn& c, const proto::Hello& h);
    void onProof(Conn& c, const proto::AuthProof& p, std::vector<SessionEvent>& out);
    bool sendTo(Conn& c, const proto::Message& m);
    void reject(Conn& c, proto::RejectReason reason, const char* detail);
    void violation(Conn& c, const std::string& why, std::vector<SessionEvent>& out);
    void closeConn(net::ConnectionId id, int code, const char* detail, bool flush, std::vector<SessionEvent>* out,
        LeftReason reason, const std::string& why);
    void emitLeft(int slot, LeftReason reason, const std::string& why, std::vector<SessionEvent>& out);
    bool allow(Conn& c, net::ChannelId ch);
    void note(const std::string& line);
    static std::string AddressOf(const std::string& remote);

    net::Transport& t_;
    ServerConfig config_;
    Roster roster_;
    std::map<net::ConnectionId, Conn> conns_;
    std::vector<Slot> slots_;
    std::map<std::string, std::deque<std::int64_t>> handshakesByAddress_;
    std::vector<net::Event> events_;
    ServerCounters counters_;
    std::vector<std::string> log_;
    std::uint64_t nextJoinSerial_ = 1;
    std::vector<RosterEntry> addedByCode_;
    bool accepting_ = true;
};
} // namespace coop::session
