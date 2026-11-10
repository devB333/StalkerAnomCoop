// Client side of S04-T04: connects, proves the player's identity with its
// key, and turns server messages into ClientEvents once welcomed.
#pragma once

#include "coop/net/transport.h"
#include "coop/proto/guard.h"
#include "coop/proto/messages.h"
#include "coop/session/auth.h"
#include "coop/session/server.h"

#include <deque>
#include <optional>

#include <string>
#include <vector>

namespace coop::session
{
struct ClientConfig
{
    proto::LocalBuild build;
    proto::Uuid128 player_id;
    std::string display_name;
    // If set, refuse to authenticate to any other campaign.
    proto::Uuid128 expected_campaign;
    // S11-T00: the host's join code: the proof also carries this player's
    // public key, so a server that does not list it yet can let it in.
    std::string join_code;
    LivenessConfig liveness;
    // Continue an earlier session (same process on the server): its session
    // id, its latest token (tokens are single-use) and the client's cursors.
    std::optional<proto::ResumeRequest> resume;
};

enum class ClientState : std::uint8_t
{
    Idle,
    Connecting,
    AwaitChallenge,
    AwaitWelcome,
    Active,
    Ended,
};

enum class ClientEventType : std::uint8_t
{
    Welcomed,
    Rejected,  // the server said why; the connection ends next
    Ended,     // the connection is gone
    Message,
    Suspect,   // nothing heard from the server for suspect_after_ms
    Recovered, // heard again
};

struct ClientEvent
{
    ClientEventType type = ClientEventType::Message;
    proto::Welcome welcome;   // Welcomed
    bool resumed = false;     // Welcomed: the requested session continues
    proto::Reject reject;     // Rejected
    net::CloseReason reason = net::CloseReason::None; // Ended
    int code = 0;             // Ended
    std::string detail;       // Ended
    net::ChannelId channel = 0;
    proto::Decoded message; // Message
};

class Client
{
public:
    Client(net::Transport& transport, const ClientConfig& config, PrivateKey key);
    bool connect(const std::string& address);
    void update(std::vector<ClientEvent>& out);
    bool send(const proto::Message& m); // only when Active
    void close(int code, const char* detail);

    ClientState state() const { return state_; }
    const proto::Welcome& welcome() const { return welcome_; }
    std::uint64_t violations() const { return violations_; }
    // What a later connection needs to resume this session (empty before a
    // Welcome). The token in it is the latest one.
    std::optional<proto::ResumeRequest> resume_request(std::uint64_t last_event, std::uint64_t last_snapshot) const;
    // Heartbeat round trips measured so far, in ms.
    const std::vector<double>& rtts() const { return rtts_; }

private:
    bool sendRaw(const proto::Message& m);
    void end(net::CloseReason reason, int code, const std::string& detail, std::vector<ClientEvent>& out);
    void violation(const std::string& why, std::vector<ClientEvent>& out);
    void tick(std::vector<ClientEvent>& out);

    net::Transport& t_;
    ClientConfig config_;
    PrivateKey key_;
    ClientState state_ = ClientState::Idle;
    net::ConnectionId conn_ = net::kNoConnection;
    proto::StreamGuard guard_{proto::Role::Client};
    proto::SequenceCounter seq_;
    proto::Hello hello_;
    proto::Welcome welcome_;
    std::vector<net::Event> events_;
    std::uint64_t violations_ = 0;
    std::int64_t lastHeard_us_ = 0;
    std::int64_t nextBeat_us_ = 0;
    bool suspect_ = false;
    std::uint64_t beatNonce_ = 0;
    std::deque<std::pair<std::uint64_t, std::int64_t>> beats_; // nonce, sent time
    std::vector<double> rtts_;
};
} // namespace coop::session
