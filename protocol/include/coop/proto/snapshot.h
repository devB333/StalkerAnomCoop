// S06-T02: snapshots against acknowledged baselines.
//
// Model (one sender and one receiver per connection):
//
// - A snapshot is what the client holds of its snapshot set after decoding
//   it: a frame, entity id -> state. Every snapshot has an id (1, 2, 3...)
//   and is split into at most max_parts messages, each carrying different
//   entities.
// - A full snapshot (baseline 0) is built from nothing. A delta names a
//   baseline: an earlier snapshot the client acknowledged with every part
//   decoded. Each entry carries only the components that changed since that
//   baseline; a new entity (or a new generation of one) carries all of its
//   components; an entity the baseline holds that is no longer wanted gets a
//   removal entry.
// - The sender keeps the frame each snapshot gives the client, not the
//   world's true state: an entity that changed but did not fit the part
//   budget keeps its baseline state in that frame and is sent later. So any
//   snapshot the client decodes reproduces the sender's frame for that id
//   exactly, however many were lost, and a budget only delays changes.
// - Missing baseline: when the client's acknowledged snapshot has left the
//   sender's history (the client stopped acknowledging), or the client says
//   it lacks the baseline it was sent, the next snapshot is full. A full
//   snapshot is bounded by the same part budget; what does not fit follows
//   in the next deltas.
// - Budget: the part budget adapts to the link (congestion() halves it,
//   clean snapshots grow it back), because a snapshot whose parts the
//   transport drops never completes and a saturated link delays reliable
//   retransmissions (S06-T02 wire test). Changed entities that do not fit
//   are deferred; a deferred entity moves ahead of the others each time, so
//   none starves; the first `pinned` wanted entities always go first.
// - Old state never overwrites newer: the receiver hands an entity's state
//   on for presentation only if its tick is newer than the last one it
//   handed on for that entity.
//
// Transport: snapshots and acks are transient (unreliable, dropped when
// stale), on their own lane; nothing reliable waits for them.
#pragma once

#include "coop/proto/messages.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

namespace coop::proto
{
// One entity's state. `components` names the fields it has (kTransform,
// kVelocity, kHealth, kStateBits, kRotation); other fields are unused.
struct EntityState
{
    EntityRef entity;
    std::uint16_t components = 0;
    Transform transform;
    float vx = 0, vy = 0, vz = 0;
    std::uint16_t health = 0;
    std::uint16_t state_bits = 0;
    float qx = 0, qy = 0, qz = 0, qw = 1;
    float aim_yaw = 0, aim_pitch = 0; // kAim
};

using SnapshotFrame = std::map<std::uint64_t, EntityState>;

// Changes smaller than these are not sent.
struct SnapshotTolerance
{
    float position = 0.001f; // metres
    float yaw = 0.001f;      // radians
    float velocity = 0.01f;  // metres per second
    float rotation = 0.0002f; // quaternion component
};

// The components of `now` that differ from `base` beyond the tolerance; all
// of now's components when there is no base or it is another generation.
std::uint16_t changed_components(const EntityState* base, const EntityState& now, const SnapshotTolerance& tolerance);

// Encoded bytes of one entry with these wire components.
std::size_t entry_bytes(std::uint16_t components);
// Encoded bytes of a part before its entries (transport header included).
constexpr std::size_t kSnapshotPartOverhead = kHeaderBytes + 8 * 4 + 3;

struct SnapshotSenderConfig
{
    std::size_t history = 32;  // snapshots kept as baselines (1.6 s at 20 Hz)
    std::size_t max_parts = 8; // messages per snapshot (the budget's ceiling)
    std::size_t max_part_bytes = kMaxMessageBytes[kSnapshot];
    std::uint32_t grow_after = 10; // clean snapshots before the budget grows by one part
    std::size_t deferral_boost = 64; // places a deferred entity moves ahead per deferral
    SnapshotTolerance tolerance;
};

struct SnapshotSenderCounters
{
    std::uint64_t snapshots = 0;
    std::uint64_t full = 0;  // built from nothing
    std::uint64_t delta = 0; // against an acknowledged baseline
    std::uint64_t parts = 0;
    std::uint64_t bytes = 0; // encoded, transport header included
    std::uint64_t entries = 0;
    std::uint64_t removals = 0;
    std::uint64_t deferred = 0;  // changed entities left for a later snapshot (part budget)
    std::uint64_t unchanged = 0; // wanted entities with nothing to send
    std::uint64_t acks = 0;
    std::uint64_t acks_old = 0;     // not newer than the acknowledged one
    std::uint64_t acks_invalid = 0; // an id never sent
    std::uint64_t baseline_lost = 0; // the acknowledged snapshot left the history: full instead
    std::uint64_t full_requested = 0; // the client reported a missing baseline
    std::size_t max_snapshot_bytes = 0;
    std::size_t max_parts_used = 0;
    std::uint64_t congestions = 0; // budget halvings
    std::size_t min_budget = 0;    // smallest part budget reached
    std::uint32_t max_deferrals = 0; // longest an entity waited (snapshots)
};

class SnapshotSender
{
public:
    SnapshotSender() : SnapshotSender(SnapshotSenderConfig{}) {}
    explicit SnapshotSender(const SnapshotSenderConfig& config);

    // Builds the next snapshot of `wanted`, the client's snapshot set now,
    // most important first (an entity listed twice counts once). Fills
    // `parts` (at least one message, possibly with no entries: the tick and
    // last_input still matter). Returns the snapshot id.
    std::uint64_t build(std::uint64_t server_tick, std::uint64_t last_input, const std::vector<EntityState>& wanted,
        std::vector<Snapshot>& parts, std::size_t pinned = 0);

    void on_ack(const SnapshotAck& ack);

    // After sending a snapshot: whether the link refused parts or queued
    // them too long. Adjusts the part budget.
    void sent(bool congested);
    std::size_t budget() const { return budget_; }

    // The client acknowledged a snapshot holding this entity in this state
    // (within the tolerance).
    bool acked_holds(const EntityState& state) const;

    // The baseline the next build would use (0: full).
    std::uint64_t next_baseline() const;
    std::uint64_t acked() const { return acked_; }
    const SnapshotSenderCounters& counters() const { return counters_; }
    const SnapshotSenderConfig& config() const { return config_; }
    // The frame of a snapshot still in the history (tests), else null.
    const SnapshotFrame* frame(std::uint64_t id) const;

    // A new connection: no baselines; ids restart.
    void reset();

private:
    struct Sent
    {
        std::uint64_t id = 0;
        std::uint64_t tick = 0;
        SnapshotFrame frame;
    };
    const Sent* find(std::uint64_t id) const;

    SnapshotSenderConfig config_;
    SnapshotSenderCounters counters_;
    std::deque<Sent> history_;
    std::uint64_t last_id_ = 0;
    std::uint64_t acked_ = 0;
    bool full_requested_ = false;
    std::size_t budget_ = 0;
    std::uint32_t clean_ = 0;
    std::unordered_map<std::uint64_t, std::uint32_t> deferrals_; // entity id -> snapshots deferred in a row
};

struct SnapshotReceiverConfig
{
    std::size_t history = 64;   // decoded snapshots kept as baselines (more than the sender keeps)
    std::size_t assembling = 4; // snapshots with parts still missing
    std::uint64_t max_age_ticks = 500; // states older than the newest by more are ignored
};

struct SnapshotReceiverCounters
{
    std::uint64_t parts = 0;
    std::uint64_t completed = 0;
    std::uint64_t full = 0;
    std::uint64_t delta = 0;
    std::uint64_t duplicate_parts = 0;
    std::uint64_t missing_baseline = 0; // parts whose baseline is not held
    std::uint64_t inconsistent = 0;     // a part that disagrees with its snapshot's other parts
    std::uint64_t incomplete_dropped = 0; // snapshots that never got every part
    std::uint64_t states = 0;  // entity states handed on
    std::uint64_t older = 0;   // states not newer than what was handed on (old snapshots)
    std::uint64_t removals = 0;
};

// What one part changes for presentation: newer states and removals.
struct SnapshotUpdate
{
    std::uint64_t server_tick = 0;
    std::uint64_t last_input = 0;
    std::vector<EntityState> states;
    std::vector<EntityRef> removed;
};

enum class SnapshotVerdict : std::uint8_t
{
    Applied,         // the part's entries are in; `update` has what is new
    Completed,       // as Applied, and it was the snapshot's last part
    Duplicate,       // this part (or its whole snapshot) was already in
    MissingBaseline, // the baseline is not held; the next ack asks for a full snapshot
    Inconsistent,    // disagrees with the snapshot's other parts; ignored
    TooOld,          // older than every snapshot still held or assembling
};

class SnapshotReceiver
{
public:
    SnapshotReceiver() : SnapshotReceiver(SnapshotReceiverConfig{}) {}
    explicit SnapshotReceiver(const SnapshotReceiverConfig& config);

    SnapshotVerdict offer(const Snapshot& part, SnapshotUpdate& update);

    // The acknowledgement to send now: the newest complete snapshot and any
    // missing baseline since the last call. False when there is nothing new
    // to say.
    bool take_ack(SnapshotAck& out);

    std::uint64_t newest_complete() const { return newest_complete_; }
    const SnapshotFrame* frame(std::uint64_t id) const; // a complete snapshot still held
    // The newest complete snapshot's frame and server tick (null: none). A
    // client seeds an entity it learns about late (its Spawn arrived after
    // its states) from it.
    const SnapshotFrame* newest_frame(std::uint64_t* server_tick = nullptr) const;
    const SnapshotReceiverCounters& counters() const { return counters_; }
    void reset();

private:
    struct Assembly
    {
        std::uint64_t id = 0, tick = 0, baseline = 0, last_input = 0;
        std::uint8_t count = 0;
        std::vector<bool> have;
        std::set<std::uint64_t> touched;
        SnapshotFrame frame;
    };
    struct Complete
    {
        std::uint64_t id = 0;
        std::uint64_t tick = 0;
        SnapshotFrame frame;
    };
    void hand_on(std::uint64_t tick, const EntityState& state, SnapshotUpdate& update);
    void hand_on_removal(std::uint64_t tick, const EntityRef& ref, SnapshotUpdate& update);

    SnapshotReceiverConfig config_;
    SnapshotReceiverCounters counters_;
    std::deque<Assembly> assembling_;
    std::deque<Complete> complete_; // ascending id
    std::uint64_t newest_complete_ = 0;
    std::uint64_t acked_sent_ = 0;
    std::uint64_t missing_ = 0;
    std::uint64_t newest_tick_ = 0;
    std::unordered_map<std::uint64_t, std::uint64_t> handed_; // entity id -> tick of the last state or removal handed on
};

} // namespace coop::proto
