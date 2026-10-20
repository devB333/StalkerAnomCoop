// Client side of S04-T04: connects, proves the player's identity with its
// key, and turns server messages into ClientEvents once welcomed.
#pragma once

#include "coop/net/transport.h"
#include "coop/proto/guard.h"
#include "coop/proto/messages.h"
#include "coop/session/auth.h"

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
    Rejected, // the server said why; the connection ends next
    Ended,    // the connection is gone
    Message,
};

struct ClientEvent
{
    ClientEventType type = ClientEventType::Message;
    proto::Welcome welcome;   // Welcomed
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

private:
    bool sendRaw(const proto::Message& m);
    void end(net::CloseReason reason, int code, const std::string& detail, std::vector<ClientEvent>& out);
    void violation(const std::string& why, std::vector<ClientEvent>& out);

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
};
} // namespace coop::session
