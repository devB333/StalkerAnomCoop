// Player authentication for protocol v1 (S04-T04).
//
//   client: Hello {player id, client nonce, build, content}
//   server: AuthChallenge {server nonce, campaign id}
//   client: AuthProof {ECDSA P-256 signature over transcript()}
//   server: Welcome, or Reject AuthFailed
//
// Both nonces are fresh per connection, so a proof captured on one connection
// is worthless on another (replay). The transcript also binds the player id,
// campaign and build/content digests, so it cannot be moved to another
// player, campaign or build. The server knows players only by their public
// keys (the roster); a leaked roster lets nobody in.
//
// Limit: GNS's encryption is unauthenticated (S04-T01), and this signature is
// not bound to its session keys, so an attacker who sits on the network path
// and relays a live handshake can take over the session it relays. It stops
// strangers, eavesdroppers, impersonation by the other player and replay.
#pragma once

#include "coop/proto/messages.h"
#include "coop/session/crypto.h"

#include <string>
#include <vector>

namespace coop::session
{
proto::Digest transcript(const proto::Hello& hello, const proto::AuthChallenge& challenge);

bool make_proof(const proto::Hello& hello, const proto::AuthChallenge& challenge, const PrivateKey& key,
    proto::AuthProof& proof);
bool check_proof(const proto::Hello& hello, const proto::AuthChallenge& challenge, const PublicKey& pub,
    const proto::AuthProof& proof);

struct RosterEntry
{
    proto::Uuid128 player_id;
    PublicKey key{};
    std::string display_name;
};

// The server's list of players allowed into a campaign. A text file, one
// player per line, public data only:
//   player <player-id: 32 hex> <public key: 128 hex> <display name>
// Blank lines and lines starting with '#' are ignored.
class Roster
{
public:
    static constexpr std::size_t kMaxPlayers = 64;
    bool add(const RosterEntry& e, std::string* error = nullptr);
    bool load(const std::string& path, std::string* error = nullptr);
    bool parse(const std::string& text, std::string* error = nullptr);
    const RosterEntry* find(const proto::Uuid128& player) const;
    std::size_t size() const { return entries_.size(); }
    static std::string line(const RosterEntry& e);

private:
    std::vector<RosterEntry> entries_;
};
} // namespace coop::session
