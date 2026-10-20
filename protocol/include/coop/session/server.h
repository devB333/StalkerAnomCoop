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
} // namespace close_code

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
    Joined,  // a player's session started (or replaced an older one)
    Left,    // a player's session ended; the slot stays theirs
    Message, // a message from a joined player
};

struct SessionEvent
{
    SessionEventType type = SessionEventType::Message;
    proto::Uuid128 player;
    std::uint8_t slot = 0;
    std::uint32_t slot_generation = 0;
    proto::Uuid128 session_id;
    net::ChannelId channel = 0;
    proto::Decoded message; // Message
    bool replaced = false;  // Left: a newer connection of the same player took over
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
};

struct PlayerView
{
    proto::Uuid128 player;
    std::uint8_t slot = 0;
    std::uint32_t slot_generation = 0;
    bool connected = false;
    proto::Uuid128 session_id;
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

    std::vector<PlayerView> players() const;
    const ServerCounters& counters() const { return counters_; }
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
    };
    struct Slot
    {
        bool bound = false;
        proto::Uuid128 player;
        std::uint32_t generation = 0;
        net::ConnectionId conn = net::kNoConnection;
        proto::Uuid128 session_id;
        proto::Digest resume_token_hash{}; // S04-T05 uses it; the token itself is never stored
    };

    void onConnected(net::ConnectionId id);
    void onDisconnected(net::ConnectionId id, const std::string& why, std::vector<SessionEvent>& out);
    void onMessage(Conn& c, const net::Event& e, std::vector<SessionEvent>& out);
    void onHello(Conn& c, const proto::Hello& h);
    void onProof(Conn& c, const proto::AuthProof& p, std::vector<SessionEvent>& out);
    bool sendTo(Conn& c, const proto::Message& m);
    void reject(Conn& c, proto::RejectReason reason, const char* detail);
    void violation(Conn& c, const std::string& why, std::vector<SessionEvent>& out);
    void closeConn(net::ConnectionId id, int code, const char* detail, bool flush, std::vector<SessionEvent>* out,
        const std::string& why);
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
};
} // namespace coop::session
