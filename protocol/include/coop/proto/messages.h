// Co-op protocol v1.0 messages and codec (S04-T03). Wire format:
// docs/network-protocol.md "Wire format v1". No engine or transport types:
// the server maps EntityRef to its registry (docs/participant-identity.md),
// the transport carries each message on the channel its type names.
//
// decode() is the only way in: it bounds every length, count, enum, float
// and identity before the caller sees a field, checks the message is on its
// own channel and travelling the right way, and refuses trailing bytes.
// encode() refuses to produce what decode() would reject.
#pragma once

#include "coop/net/transport.h"
#include "coop/proto/wire.h"

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace coop::proto
{
constexpr std::uint32_t kMagic = 0x504F4F43; // "COOP" little-endian
constexpr std::uint16_t kProtocolMajor = 1;
// 1.1 (S05-T02): Spawn state up to 16 KiB (a native spawn packet), lifecycle
// messages up to 20 KiB with a 1 MiB backlog.
constexpr std::uint16_t kProtocolMinor = 1;
constexpr std::size_t kHeaderBytes = 24;

// ------------------------------------------------------------- channels
enum Channel : net::ChannelId
{
    kControl = 0,   // Critical: handshake (hello, challenge, proof, welcome, reject), heartbeat, resync begin/end
    kActions = 1,   // Critical: action intents and results
    kLifecycle = 2, // Critical: spawn, despawn, campaign events
    kBulk = 3,      // Bulk: resync chunks
    kSnapshot = 4,  // Transient: snapshots (server) and snapshot acks (client)
    kInput = 5,     // Transient: input batches
    kChannelCount = 6,
};

// The transport layout for protocol v1; both peers must use it.
net::TransportConfig transport_config();

// ------------------------------------------------------------- limits
constexpr std::size_t kMaxMessageBytes[kChannelCount] = {1024, 512, 20 * 1024, 64 * 1024, 1200, 512};
constexpr std::size_t kMaxDisplayName = 32;
constexpr std::size_t kSignatureBytes = 64; // ECDSA P-256 r||s
constexpr std::size_t kMinResumeToken = 16;
constexpr std::size_t kMaxResumeToken = 64;
constexpr std::size_t kMaxRejectDetail = 128;
constexpr std::size_t kMaxActionParams = 256;
constexpr std::uint16_t kMaxActionKind = 1023;
constexpr std::size_t kMaxArchetype = 64;
constexpr std::size_t kMaxSpawnState = 16 * 1024;
constexpr std::size_t kMaxEventPayload = 1024;
constexpr std::size_t kMaxInputsPerBatch = 8;
constexpr std::size_t kMaxSnapshotEntities = 64;
constexpr std::uint8_t kMaxSnapshotParts = 16;
constexpr std::uint32_t kMaxResyncChunk = 60 * 1024;
constexpr std::uint64_t kMaxResyncBytes = 256ull * 1024 * 1024;
constexpr std::uint32_t kMaxResyncChunks = 65536;
constexpr std::uint8_t kMaxSlots = 8;
constexpr float kMaxCoord = 1.0e5f;     // metres from the level origin
constexpr float kMaxSpeed = 1000.f;     // metres per second
constexpr float kMaxAngle = 4.f;        // radians (|yaw| <= pi, |pitch| <= pi/2, with margin)

// ------------------------------------------------------------- types
enum class Role : std::uint8_t
{
    Client,
    Server,
};

enum class MessageType : std::uint16_t
{
    Hello = 0x0001,
    Welcome = 0x0002,
    Reject = 0x0003,
    Heartbeat = 0x0004,
    ResyncBegin = 0x0005,
    ResyncEnd = 0x0006,
    ActionIntent = 0x0010,
    ActionResult = 0x0011,
    Spawn = 0x0020,
    Despawn = 0x0021,
    CampaignEvent = 0x0022,
    ResyncChunk = 0x0030,
    Snapshot = 0x0040,
    SnapshotAck = 0x0041,
    InputBatch = 0x0050,
    AuthChallenge = 0x0007,
    AuthProof = 0x0008,
};
// A type with this bit that the receiver does not know is skipped, not an
// error: a later minor version may add optional messages.
constexpr std::uint16_t kOptionalTypeBit = 0x8000;

struct Header
{
    std::uint16_t type = 0;
    std::uint16_t flags = 0; // v1: must be 0
    std::uint32_t payload_bytes = 0;
    std::uint64_t epoch = 0;    // authority epoch; 0 only before the session exists
    std::uint64_t sequence = 0; // per channel and direction, from 1
};

// A replicated entity: campaign-scoped EntityId plus incarnation generation.
struct EntityRef
{
    std::uint64_t id = 0;
    std::uint32_t generation = 0;
    bool none() const { return id == 0 && generation == 0; }
    bool operator==(const EntityRef& o) const { return id == o.id && generation == o.generation; }
};

struct Transform
{
    float x = 0, y = 0, z = 0, yaw = 0;
};

struct ResumeRequest
{
    Uuid128 session_id;
    std::vector<std::uint8_t> token;
    std::uint64_t last_event = 0;
    std::uint64_t last_snapshot = 0;
};

struct Hello // client -> server, control
{
    std::uint16_t major = kProtocolMajor;
    std::uint16_t minor = kProtocolMinor;
    Digest build_id{};
    Digest content_digest{};
    Uuid128 player_id; // a claim until AuthProof proves it (S04-T04)
    std::string display_name;
    Digest client_nonce{}; // fresh per connection; part of the signed transcript
    bool resume = false;
    ResumeRequest resume_request; // only when resume
};

struct AuthChallenge // server -> client, control, after Hello
{
    Digest server_nonce{}; // fresh per connection
    Uuid128 campaign_id;   // the campaign the client is proving itself to
};

struct AuthProof // client -> server, control, answers AuthChallenge
{
    std::array<std::uint8_t, kSignatureBytes> signature{}; // over the transcript (session/auth.h)
};

struct Welcome // server -> client, control
{
    std::uint16_t major = kProtocolMajor;
    std::uint16_t minor = kProtocolMinor; // negotiated
    Uuid128 campaign_id, session_id, player_id, party_id;
    std::uint8_t slot = 0;
    std::uint32_t slot_generation = 0;
    std::uint64_t authority_epoch = 0;
    std::uint64_t server_tick = 0;
    std::uint16_t tick_rate_hz = 30;
    std::uint16_t snapshot_rate_hz = 20;
    std::vector<std::uint8_t> resume_token;
};

enum class RejectReason : std::uint16_t
{
    VersionMismatch = 1,
    BuildMismatch = 2,
    ContentMismatch = 3,
    ServerFull = 4,
    AuthFailed = 5,
    Malformed = 6,
    RateLimited = 7,
    ResumeExpired = 8,
    Other = 100,
};

struct Reject // server -> client, control
{
    std::uint16_t reason = 0; // RejectReason; other non-zero values are kept as sent
    std::uint16_t server_major = kProtocolMajor;
    std::uint16_t server_minor = kProtocolMinor;
    std::string detail;
};

struct Heartbeat // both ways, control
{
    std::uint64_t nonce = 0;
    std::uint64_t echo = 0; // the peer's nonce being answered, or 0
    std::uint64_t sent_tick = 0;
};

enum class ActionOutcome : std::uint8_t
{
    Accepted = 1,
    Rejected = 2,
    Duplicate = 3,
};

struct ActionIntent // client -> server, actions
{
    std::uint64_t operation_id = 0; // unique per player; dedupe key with the player
    std::uint16_t kind = 0;         // 1..kMaxActionKind
    EntityRef target;               // none allowed
    std::uint64_t expected_revision = 0;
    std::vector<std::uint8_t> params; // kind-specific, decoded later with its own bounds
};

struct ActionResult // server -> client, actions
{
    std::uint64_t operation_id = 0;
    ActionOutcome outcome = ActionOutcome::Accepted;
    std::uint16_t reason = 0;
    std::uint64_t commit_id = 0;
    std::uint64_t server_tick = 0;
    std::uint64_t revision = 0;
};

struct Spawn // server -> client, lifecycle
{
    EntityRef entity;
    std::string archetype; // section name: [a-z0-9_.-]
    std::uint16_t level = 0;
    Transform transform;
    EntityRef parent; // none allowed; never the entity itself
    std::uint64_t commit_id = 0;
    std::vector<std::uint8_t> state; // opaque here; decoded later with its own bounds
};

enum class DespawnReason : std::uint8_t
{
    Destroyed = 1,
    OutOfRange = 2,
    Released = 3,
};

struct Despawn // server -> client, lifecycle
{
    EntityRef entity;
    DespawnReason reason = DespawnReason::Destroyed;
    std::uint64_t commit_id = 0;
};

struct CampaignEvent // server -> client, lifecycle
{
    std::uint64_t event_id = 0;
    std::uint16_t kind = 0;
    std::uint64_t revision = 0;
    std::vector<std::uint8_t> payload;
};

enum SnapshotComponent : std::uint16_t
{
    kTransform = 1 << 0, // x, y, z, yaw
    kVelocity = 1 << 1,  // vx, vy, vz
    kHealth = 1 << 2,    // u16, 0..10000 (hundredths of a percent)
    kStateBits = 1 << 3, // u16, opaque state flags
    kKnownComponents = kTransform | kVelocity | kHealth | kStateBits,
};

struct SnapshotEntity
{
    EntityRef entity;
    std::uint16_t components = 0;
    Transform transform;
    float vx = 0, vy = 0, vz = 0;
    std::uint16_t health = 0;
    std::uint16_t state_bits = 0;
};

struct Snapshot // server -> client, snapshot
{
    std::uint64_t server_tick = 0;
    std::uint64_t baseline_id = 0; // 0: full; otherwise an earlier snapshot's sequence
    std::uint64_t last_input = 0;  // last input tick the server consumed for this player (S06-T01)
    std::uint8_t part = 0;
    std::uint8_t part_count = 1;
    std::vector<SnapshotEntity> entities;
};

struct SnapshotAck // client -> server, snapshot
{
    std::uint64_t snapshot_id = 0; // latest fully decoded snapshot's sequence
    std::uint64_t baseline_id = 0;
    std::uint64_t event_cursor = 0;
};

enum class Stance : std::uint8_t
{
    Stand = 0,
    Crouch = 1,
    Prone = 2,
    Sprint = 3,
};
constexpr std::uint32_t kKnownButtons = 0x0000FFFF;

struct Input
{
    std::uint64_t tick = 0;
    float move_x = 0, move_y = 0; // -1..1
    float yaw = 0, pitch = 0;
    std::uint32_t buttons = 0;
    Stance stance = Stance::Stand;
};

struct InputBatch // client -> server, input
{
    // Newest first; input i has sequence header.sequence - i, ticks strictly
    // decreasing. Older inputs are repeated for loss.
    std::vector<Input> inputs;
};

struct ResyncBegin // server -> client, control
{
    std::uint64_t resync_id = 0;
    std::uint64_t checkpoint_id = 0;
    std::uint64_t authority_epoch = 0;
    Digest content_hash{};
    std::uint64_t event_watermark = 0;
    std::uint64_t total_bytes = 0;
    std::uint32_t chunk_bytes = 0;
    std::uint32_t chunk_count = 0; // == ceil(total_bytes / chunk_bytes)
};

struct ResyncChunk // server -> client, bulk
{
    std::uint64_t resync_id = 0;
    std::uint32_t index = 0;
    std::vector<std::uint8_t> data; // CRC-32 travels with it and is checked on decode
};

struct ResyncEnd // server -> client, control
{
    std::uint64_t resync_id = 0;
    std::uint32_t chunk_count = 0;
    std::uint32_t total_crc32 = 0;
};

using Message = std::variant<Hello, Welcome, Reject, Heartbeat, ResyncBegin, ResyncEnd, ActionIntent, ActionResult,
    Spawn, Despawn, CampaignEvent, ResyncChunk, Snapshot, SnapshotAck, InputBatch, AuthChallenge, AuthProof>;

// Handshake types travel with epoch 0, before the session exists.
bool handshake_type(std::uint16_t type);

struct Decoded
{
    Header header;
    Message body;
};

struct DecodeResult
{
    DecodeError error = DecodeError::None;
    std::size_t offset = 0;
    const char* field = "";
    bool ignorable = false; // unknown optional type: skip it
    bool ok() const { return error == DecodeError::None && !ignorable; }
};

// Facts about each type.
bool known_type(std::uint16_t type);
net::ChannelId channel_of(MessageType type);
Role sender_of(MessageType type, bool* both = nullptr); // who may send it
MessageType type_of(const Message& m);
const char* to_string(MessageType type);

// Decodes one transport message received on `channel` by `receiver`.
DecodeResult decode(net::ChannelId channel, Role receiver, const void* data, std::size_t size, Decoded& out);

// Encodes `m` with the given epoch and sequence. Refuses (false, *error set)
// anything decode() would reject, so a bug cannot put it on the wire.
bool encode(const Message& m, std::uint64_t epoch, std::uint64_t sequence, std::vector<std::uint8_t>& out,
    std::string* error = nullptr);

// ------------------------------------------------------------- compatibility
struct LocalBuild
{
    std::uint16_t major = kProtocolMajor;
    std::uint16_t minor = kProtocolMinor;
    Digest build_id{};
    Digest content_digest{};
};

// Server side of the hello check: 0 when compatible (and *negotiated_minor
// set to the lower minor), else the reason to send in a Reject.
std::uint16_t check_hello(const Hello& hello, const LocalBuild& local, std::uint16_t* negotiated_minor);

} // namespace coop::proto
