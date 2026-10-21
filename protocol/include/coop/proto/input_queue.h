// S06-T01: one player's movement inputs, sequenced and validated before the
// server's native control path sees them.
//
// The client numbers its inputs by tick (1, 2, 3... per connection, one per
// server tick length of its own time) and repeats recent ones in every batch
// for loss. The server offers every received input to the player's queue and
// consumes exactly one per server tick, in tick order. So an input moves the
// actor for at most one tick however often it arrives, a tick that was
// consumed or dropped is never applied later, and a backlog can only add a
// bounded latency, never extra distance: the actor advances by the server's
// ticks, not by how many inputs the client sent.
//
// offer() verdicts:
//   Queued   kept, in tick order; over capacity the oldest queued input is
//            dropped (dropped_oldest), so the backlog is at most capacity ticks
//   Stale    tick <= the last consumed tick, or already queued (a repeat)
//   Ahead    tick > newest accepted + max_gap: a jump no loss explains
//   Invalid  a field outside the movement model: non-finite, |axis| > 1,
//            |pitch| > max_pitch, a stance the server has no mode for, an
//            unknown button
// consume() sources, once per server tick:
//   Queued   the oldest queued input; it becomes the last consumed
//   Held     the queue is empty: the last consumed input again, for at most
//            hold_ticks ticks in a row (a lost or late batch does not stop the
//            actor for one tick)
//   None     nothing to apply: the actor gets no input this tick
// reset() forgets the sequence (a new connection restarts the tick count);
// the counters are kept.
#pragma once

#include "coop/proto/messages.h"

#include <cstddef>
#include <cstdint>
#include <deque>

namespace coop::proto
{
struct InputQueueConfig
{
    std::size_t capacity = 5;      // queued inputs kept (ticks of backlog)
    std::uint64_t max_gap = 250;   // ticks an input may lead the newest accepted one
    std::uint32_t hold_ticks = 10; // ticks the last input is reused when the queue is empty
    float max_pitch = 1.6f;        // radians
};

enum class InputVerdict : std::uint8_t
{
    Queued,
    Stale,
    Ahead,
    Invalid,
};

enum class InputSource : std::uint8_t
{
    None,
    Queued,
    Held,
};

struct InputQueueCounters
{
    std::uint64_t offered = 0, queued = 0, stale = 0, ahead = 0, invalid = 0, dropped_oldest = 0;
    std::uint64_t consumed = 0, held = 0, idle = 0;
    std::size_t depth_peak = 0;
};

const char* to_string(InputVerdict v);

// Why an input is Invalid, else "".
const char* validate_input(const Input& in, float max_pitch);

class InputQueue
{
public:
    InputQueue() : InputQueue(InputQueueConfig()) {}
    explicit InputQueue(const InputQueueConfig& config);

    void reset();
    InputVerdict offer(const Input& in, const char** why = nullptr);
    // A whole batch, oldest first (the batch is newest first). Returns how
    // many were queued.
    std::size_t offer_batch(const InputBatch& batch);
    InputSource consume(Input& out);

    std::uint64_t last_consumed_tick() const { return last_consumed_; }
    // S06-T03: the newest input tick the server has dealt with: the last
    // consumed one, or, when nothing is queued, a newer one it refused as
    // Invalid (an input it cannot apply is applied as nothing). Snapshots
    // report it so a client also reconciles the inputs the server refused
    // (a predicted walk with a stance the server has no mode for). Ahead and
    // stale inputs never move it.
    std::uint64_t processed_tick() const;
    std::uint64_t newest_tick() const { return newest_; }
    std::size_t depth() const { return queue_.size(); }
    bool has_last() const { return has_last_; }
    const Input& last() const { return last_; }
    const InputQueueCounters& counters() const { return counters_; }
    const InputQueueConfig& config() const { return config_; }

private:
    InputQueueConfig config_;
    std::deque<Input> queue_; // tick order
    std::uint64_t last_consumed_ = 0;
    std::uint64_t newest_ = 0;
    std::uint64_t refused_newest_ = 0; // newest Invalid tick within (last consumed, newest + max_gap]
    Input last_;
    bool has_last_ = false;
    std::uint32_t held_run_ = 0;
    InputQueueCounters counters_;
};
} // namespace coop::proto
