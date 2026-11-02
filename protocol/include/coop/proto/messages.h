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
// 1.2 (S06-T02): Snapshot carries its own snapshot id and is a delta against
// an acknowledged baseline (coop/proto/snapshot.h); rotation and removal
// components; SnapshotAck reports a missing baseline. Both peers are built
// from one tree (build id), so the layout change needs no negotiation.
// 1.3 (S06-T03): an aim component (an actor's torso yaw and pitch: where its
// camera looks, which its arms and weapon follow; the body yaw lags it).
// 1.4 (S06-T04): a weapon component (an actor's active slot, weapon state,
// rounds in the magazine and a shot counter), the fire button, and the first
// action kinds (activate a slot, reload).
// 1.5 (S06-T07): the jump and lean buttons (bits 2-4 of Input.buttons).
// 1.6 (S08-T01): the pickup action (kind 5), the named refusal reasons, and
// Reparent: an entity's owner changed (an item taken into an inventory, or
// put on the ground), in the lifecycle order.
// 1.6 + S10-T04a: the talk kinds (14 start, 15 say, 16 end) and reasons 33-36,
// CampaignEvent kind 14 (the dialogue state); no wire layout change.
// 1.8 + S10-T03c: the sleep kinds (24 request, 25 answer), reason 42 and
// CampaignEvent kind 19 (sleep); no wire layout change.
// 1.8 + S10-T03d: the relocate kind (26); no wire layout change.
// 1.8 + S10-T06: the session kind (27), reasons 43-46 and CampaignEvent kind 20
// (the campaign list, a campaign switch); no wire layout change.
// 1.8 + S10-T07: the companion order kind (28); no wire layout change.
// 1.8 + S10-T07 (B4/B7/B21): the zoom button (bit 5), the state_bits layouts
// per entity class; no wire layout change.
// 1.8 + S11-T00: AuthProof's optional join fields (a player not in the
// roster joins with the host's join code); older proofs decode unchanged.
// 1.8 + S11-T00: RejectReason 9 ServerBusy (the server is replacing its
// world; the client retries).
// 1.8 + S10-T07 (G10): the field repair kind (29), reason 47 (no shelter to
// sleep), kind 19's optional trailing flags byte (bit 0: a donation); no wire
// layout change.
constexpr std::uint16_t kProtocolMinor = 8;
constexpr std::size_t kHeaderBytes = 24;

// ------------------------------------------------------------- channels
enum Channel : net::ChannelId
{
    kControl = 0,   // Critical: handshake (hello, challenge, proof, welcome, reject), heartbeat, resync begin/end
    kActions = 1,   // Critical: action intents and results
    kLifecycle = 2, // Critical: spawn, despawn, reparent, campaign events
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
    Reparent = 0x0023, // v1.6
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
    // S11-T00: a player not yet in the roster joins with the host's join code:
    // its public key (the signature above verifies against it) and a hash of
    // the code bound to this transcript and key (session/auth.h join_mac).
    // Optional trailing fields: absent unless join.
    bool join = false;
    std::array<std::uint8_t, 64> public_key{};
    Digest join_mac{};
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
    ServerBusy = 9, // S11-T00: the server is loading a world (a new game's first level, a travel's reload): retry
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

// v1.6 (S08-T01): an entity the client holds changed owner: `parent` is the
// new owner (an inventory), or none when it was put on the ground at
// `transform`. Applied in commit order with Spawn and Despawn.
struct Reparent // server -> client, lifecycle
{
    EntityRef entity;
    EntityRef parent; // none: on the ground; never the entity itself
    Transform transform;
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
    kStateBits = 1 << 3, // u16, opaque state flags. S10-T07 (B4/B7) layouts, by entity class:
                         //   actor: bits 0-14 the engine's move state, bit 15 zoomed;
                         //   stalker: bit 0 alive, 1-2 movement type, 3 body state, 4-5 mental state,
                         //            6-9 weapon animation, 10 set when bits 1-9 are meant;
                         //   monster: bit 0 alive, 1-7 motion + 1 (0 none), 8-11 motion variant
    kRotation = 1 << 4,  // qx, qy, qz, qw: a unit quaternion (v1.2; physics objects)
    kRemoved = 1 << 5,   // v1.2: the entity leaves the client's snapshot set; no fields, never with another bit
    kAim = 1 << 6,       // v1.3: aim yaw, aim pitch (an actor's torso; radians)
    kWeapon = 1 << 7,    // v1.4: u8 active slot, u8 weapon state, u16 rounds in the magazine, u16 shot counter
    kStateComponents = kTransform | kVelocity | kHealth | kStateBits | kRotation | kAim | kWeapon,
    kKnownComponents = kStateComponents | kRemoved,
};

// One entity in a snapshot. In a delta, `components` names only the fields
// that changed since the baseline; the others are the baseline's.
struct SnapshotEntity
{
    EntityRef entity;
    std::uint16_t components = 0;
    Transform transform;
    float vx = 0, vy = 0, vz = 0;
    std::uint16_t health = 0;
    std::uint16_t state_bits = 0;
    float qx = 0, qy = 0, qz = 0, qw = 1;
    float aim_yaw = 0, aim_pitch = 0;
    std::uint8_t weapon_slot = 0, weapon_state = 0;
    std::uint16_t ammo = 0, shots = 0;
};

// One part of a snapshot. A snapshot is the server's state of the client's
// snapshot set at server_tick, split into part_count messages that each
// carry different entities; all parts share snapshot_id, baseline_id, tick
// and last_input (coop/proto/snapshot.h).
struct Snapshot // server -> client, snapshot
{
    std::uint64_t server_tick = 0;
    std::uint64_t snapshot_id = 0; // v1.2: per connection, from 1
    std::uint64_t baseline_id = 0; // 0: full; otherwise an earlier snapshot id the client acknowledged
    std::uint64_t last_input = 0;  // last input tick the server consumed for this player (S06-T01)
    std::uint8_t part = 0;
    std::uint8_t part_count = 1;
    std::vector<SnapshotEntity> entities;
};

struct SnapshotAck // client -> server, snapshot
{
    std::uint64_t snapshot_id = 0;      // newest snapshot with every part decoded; 0: none yet
    std::uint64_t missing_baseline = 0; // v1.2: a baseline the client does not hold (0: none); asks for a full snapshot
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
constexpr std::uint32_t kButtonWalk = 1u << 0; // S06-T01
constexpr std::uint32_t kButtonFire = 1u << 1; // S06-T04: the fire key is held
constexpr std::uint32_t kButtonJump = 1u << 2;      // S06-T07: the jump key is held
constexpr std::uint32_t kButtonLeanLeft = 1u << 3;  // S06-T07: leaning left
constexpr std::uint32_t kButtonLeanRight = 1u << 4; // S06-T07: leaning right
constexpr std::uint32_t kButtonZoom = 1u << 5;      // S10-T07 (B7): aiming down the sights

// S06-T04: the action kinds the server defines (ActionIntent.kind); every
// other kind is refused. Their params are decoded by the server with their
// own bounds.
enum ActionKind : std::uint16_t
{
    kActionActivateSlot = 1, // params: u8 inventory slot; the item in it becomes the active one
    kActionReload = 2,       // params: none; reload the active weapon from the actor's own ammo
    kActionLedger = 3,       // S07-T03: params: s32 delta (|delta| <= 1000); the synthetic critical effect on the
                             // player's own participant actor's money (docs/persistence-contract.md §10)
    kActionRespawn = 4,      // S07-T07: params: none; the dead player's confirmed respawn (contract §9)
    kActionPickup = 5,       // S08-T01: target: the item; params: none; take an item from the ground into the
                             // player's own actor's inventory (docs/persistence-contract.md §6 `pickup`)
    kActionDrop = 6,         // S08-T02: target: an item in the player's own actor's inventory; params: none; put
                             // it on the ground at the actor (contract §6 `transfer`)
    kActionTransfer = 7,     // S08-T02: target: the item; params: from ref, to ref (u64 id + u32 generation
                             // each, 24 bytes); one side the player's own actor, the other a container (an
                             // inventory box or a dead body); expected revision: the client's lifecycle cursor
                             // (the item must not have changed owner after it; 0: no precondition)
    kActionQuest = 8,        // S08-T04: target: the task giver; params: u8 op (1 accept, 2 turn in), u8 length,
                             // the task id; a party task: one instance for both players (plan.md S08-T04)
    kActionEquip = 9,        // S08-T07: target: an item in the player's own actor's inventory; params: u8 place
                             // (1 slot, 2 belt, 3 ruck), u8 slot (place 1: 1..LAST_SLOT, else 0), u8 flags (bit 0:
                             // do not activate); the engine's slot rules decide (contract §6 `equip`)
    kActionUse = 10,         // S08-T07: target: an eatable item in the player's own actor's inventory; params:
                             // none; used by the engine on the server's actor (contract §6 `use`)
    kActionTravelRequest = 11, // S09-T01: target: the level changer the player's actor stands in; params: none;
                               // asks the partner to travel there (docs/travel-contract.md); the result's
                               // revision is the transition id
    kActionTravelAnswer = 12,  // S09-T01: params: u64 transition id, u8 answer (1 accept, 2 refuse: the partner;
                               // 3 cancel: the requester); refused unless the id is the live proposal's
    kActionTravelReady = 13,   // S09-T03: params: u64 transition id, u8 length, the level name this client's
                               // world is live on; the server holds the destination world until every party
                               // member's ready names the pending transition and its level (contract §10)
    kActionTalkStart = 14,     // S10-T04a: target: the NPC; params: none; the player's actor starts a talk with it
                               // on the server (one speaker per NPC: reason 33 while another player talks to it);
                               // the dialogue state follows as CampaignEvent 14
    kActionTalkSay = 15,       // S10-T04a: params: u8 length, the id of the clicked topic (a dialog id) or phrase
                               // (<= 64 bytes); said in the player's open talk (34 without one, 35 when the id is
                               // not in the current list)
    kActionTalkEnd = 16,       // S10-T04a: params: none; the player closed the talk window (34 without a talk)
    kActionUseObject = 17,     // S10-T04c: target: a script-usable world object (a door, a switch; not an item, a
                               // living thing or a level changer: those have their own kinds); params: none; the
                               // object's use callback runs on the server in the player's context (21 when the
                               // target is not such an object, 10 out of reach, 8 gone)
    kActionTradeOpen = 18,     // S10-T04c: target: the trader the player talks to; params: none; the server starts
                               // the engine's trade session for this buyer and sends the price table as
                               // CampaignEvent 15 (34 without the talk, 37 when the NPC does not trade)
    kActionTrade = 19,         // S10-T04c: target: the trader; params: u8 bought count, u8 sold count, then the
                               // bought item refs (u64 id + u32 generation each) and the sold item refs, u32 the
                               // client's expected net payment (diagnostics); re-priced and checked on the server,
                               // journaled as one `trade` record, applied through CTrade::TransferItem (37 no
                               // open trade, 38 not enough money, 39 an item not where expected or not tradeable)
    kActionTradeClose = 20,    // S10-T04c: target: the trader; params: none; the player closed the trade window
    kActionUpgradeOpen = 21,   // S10-T04f: target: the mechanic the player talks to; params: none; the server opens
                               // an upgrade session for this player and sends the upgrade table (CampaignEvent 16:
                               // per item of the player's inventory whether the mechanic upgrades it and each
                               // upgrade's install state and price) (34 without the talk, 40 not a mechanic)
    kActionUpgrade = 22,       // S10-T04f: target: an item in the player's own actor's inventory; params: u8 op (1
                               // install an upgrade, 2 repair), u8 length, the upgrade id (op 1); checked and run
                               // on the server in the player's context (the installed script charges the purse),
                               // journaled as one `upgrade` record; CampaignEvent 17 tells the clients (40 no open
                               // upgrade session, 41 the upgrade or repair is refused, 38 not enough money)
    kActionUpgradeClose = 23,  // S10-T04f: target: the mechanic; params: none; the player closed the upgrade window
    kActionSleepRequest = 24,  // S10-T03c: params: u8 hours (1..24); asks the partner to sleep: the world's time
                               // skips once when the partner agrees (decision T01-8; the travel consent's rules,
                               // docs/travel-contract.md §3-5); the result's revision is the proposal id
    kActionSleepAnswer = 25,   // S10-T03c: params: u64 proposal id, u8 answer (1 accept, 2 refuse: the partner;
                               // 3 cancel: the requester); refused unless the id is the live sleep proposal's
    kActionRelocate = 26,      // S10-T03d: params: u16 game vertex, u32 level vertex, f32 x, y, z, u16 minutes of
                               // time to skip, u8 source (1 fast travel, 2 backpack, 3 other); a party travel
                               // request to a point instead of a level changer (fast travel, the backpack): the
                               // proposal and the answer are the travel's (CampaignEvent 13, kind 12), the time
                               // skips once when it is carried out (decision T01-9)
    kActionSession = 27,       // S10-T06: session control from the co-op menu; params: u8 op, then by op:
                               // 1 list (none): the server's campaigns follow as CampaignEvent 20;
                               // 2 create part, 4 loadout part: u8 part (0-based), u8 parts (1..8), u8 length,
                               // that many bytes of "key=value" lines (the creation screen's choices); the last
                               // part carries it out: the owner's create starts a new campaign (revision 1), the
                               // partner's is kept as its loadout for the owner's next new game (revision 2);
                               // 3 load: 16 bytes, the campaign id (revision 1). A switch is announced to every
                               // client as CampaignEvent 20 before the server stops and is relaunched on it
    kActionCompanion = 28,     // S10-T07 (B12): a companion order from the hotkeys or the companion wheel; target:
                               // one companion (scope 1) or none; params: u8 command (1 combat, 2 move, 3 stealth,
                               // 4 loot, 5 distance: set to the mode; 6 move to the point), u8 mode (1..4; 0: the
                               // server's next), u8 scope (0 all the player's companions, 1 the target's squad), u32
                               // level vertex (command 6); carried out on the server in the player's context, on
                               // that player's own companions only (decision T02f-1; 4 when it has none there)
    kActionFieldRepair = 29,   // S10-T07 (G10): a repair kit's window (item_repair) on the player's client; target:
                               // the item to repair; params: the kit's ref (u64 id + u32 generation), u8 1 and the
                               // support part's ref or u8 0, u8 the condition % the window showed (diagnostics);
                               // checked and applied on the server in the player's context by the installed
                               // window's rules (41 refused: not a kit, the item or part not accepted by it, the
                               // condition out of its range; 14 an item not in the player's own inventory)
};

// ActionResult.reason of a Rejected outcome (0 with Accepted). Defined by the
// server; the client only presents them.
enum ActionReason : std::uint16_t
{
    kReasonUnsupported = 1, // unknown action kind
    kReasonMalformed = 2,   // params outside the kind's bounds
    kReasonNoActor = 3,     // no attached session or no participant actor (or a dead one where alive is needed)
    kReasonNothing = 4,     // nothing to act on (an empty slot, no weapon)
    kReasonStoreFailed = 5, // S07-T03: the durable store failed closed
    kReasonAlive = 6,       // S07-T07: a respawn while alive
    kReasonNoPoint = 7,     // S07-T07: no respawn point
    kReasonItemGone = 8,    // S08-T01: the target is not a current object the server holds online
    kReasonItemTaken = 9,   // S08-T01: the item already has an owner (another player won it)
    kReasonOutOfReach = 10, // S08-T01: farther than the engine's pickup reach
    kReasonNotTakeable = 11, // S08-T01: not an item, or one the engine would not let an actor take
    kReasonStale = 12,       // S08-T02: the item changed owner after the expected revision (the client's view)
    kReasonNotContainer = 13, // S08-T02: the other side is not a container this slice supports (a living actor
                              // or NPC, a trader, another object)
    kReasonNotDroppable = 14, // S08-T02: a quest item, or one not in the player's own inventory
    kReasonQuestUnsupported = 15, // S08-T04: not a task this slice adapts, or the target is not a giver
    kReasonQuestActive = 16,      // S08-T04: the party already has this task
    kReasonQuestNotOffered = 17,  // S08-T04: the giver does not offer it now
    kReasonQuestNotActive = 18,   // S08-T04: the party does not have this task
    kReasonQuestNotFinished = 19, // S08-T04: its objective is not met (both players' contributions counted)
    kReasonPlaceRefused = 20,     // S08-T07: the engine's slot/belt/ruck rules refuse that place (occupied slot,
                                  // wrong slot for the item, belt full, already there)
    kReasonNotUsable = 21,        // S08-T07: not an eatable item, or none of it left
    kReasonUseRefused = 22,       // S08-T07: the installed scripts refused the use (actor_on_item_before_use: a
                                  // stronger booster active, a required tool missing, anabiotic in combat)
    kReasonTravelBusy = 23,       // S09-T01: a transition is live already (cancel or finish it first)
    kReasonNoChanger = 24,        // S09-T01: the target is not an enabled level changer the server holds online
    kReasonNotAtChanger = 25,     // S09-T01: the player's actor is not inside (or beside) the changer's shape
    kReasonPartnerUnavailable = 26, // S09-T01: no partner in the world with a living actor: no one-player consent
    kReasonInCombat = 27,         // S09-T01: an alive NPC or monster has one of the party's actors as its enemy
    kReasonNoTransition = 28,     // S09-T01: the answer names no live proposal (stale, replayed, unknown, or
                                  // after a restart)
    kReasonWrongParty = 29,       // S09-T01: accept/refuse from the requester, or cancel from the partner
    kReasonTravelInProgress = 30, // S09-T02: an agreed travel is being executed (the barrier): no action until the
                                  // party is on the destination level
    kReasonWrongDestination = 31, // S09-T03: the ready names another transition or another level than the one the
                                  // server holds the party for (or no hold is pending)
    kReasonNotLoaded = 32,        // S09-T03: the ready came before this client's world was live on the server's side
    kReasonTalkBusy = 33,         // S10-T04a: the NPC is talking to the other player (decision T01-15)
    kReasonNotTalking = 34,       // S10-T04a: a say or an end with no open talk
    kReasonWrongPhrase = 35,      // S10-T04a: the said id is not in the talk's current list
    kReasonNoTalk = 36,           // S10-T04a: the target is not a living NPC that talks now (not an inventory owner,
                                  // an actor, talk disabled, dead), or the player's actor is dead or out of reach
    kReasonNotTrading = 37,       // S10-T04c: no open trade with that NPC (or it does not trade)
    kReasonNoMoney = 38,          // S10-T04c: the buyer's purse does not cover the trade
    kReasonTradeRefused = 39,     // S10-T04c: an item is not where the client saw it, not tradeable, or priced 0
    kReasonNoMechanic = 40,       // S10-T04f: no open upgrade session with that NPC (or it is not a mechanic)
    kReasonUpgradeRefused = 41,   // S10-T04f: the upgrade cannot be installed (unknown, installed, locked by its
                                  // group or a precondition) or the item cannot be repaired
    kReasonCannotSleep = 42,      // S10-T03c: the requester bleeds or is irradiated (the installed window's rule),
                                  // or is passed out
    kReasonNotOwner = 43,         // S10-T06: only the session's owner (the first key the server accepted) may load
    kReasonUnknownCampaign = 44,  // S10-T06: no campaign with that id in the server's library, or it is the one
                                  // being served
    kReasonSessionBusy = 45,      // S10-T06: a travel, a sleep or another switch is under way
    kReasonBadChoices = 46,       // S10-T06: the creation choices break the creation screen's rules (an unknown
                                  // faction or start, a loadout item not offered to the faction, over its points)
    kReasonNoShelter = 47,        // S10-T07 (G10, T03c-2): the player is not in shelter to sleep (a sleep zone, an
                                  // emission cover, underground, or within 1.5 m of a set-up tent)
};

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
    Spawn, Despawn, CampaignEvent, ResyncChunk, Snapshot, SnapshotAck, InputBatch, AuthChallenge, AuthProof,
    Reparent>;

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
