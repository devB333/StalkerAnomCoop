// S06-T02: presentation of a remote entity from its snapshot states.
//
// States are kept by server time. The client shows each entity where the
// server had it a fixed delay ago: between the two states around that
// moment, interpolated. Past the newest state it holds the newest, or, for a
// moving entity with a velocity, carries it on along that velocity for at
// most max_extrapolate_ms (the server updates distant NPCs only at their
// scheduling rate, about every 0.6 s in snap-2, so without it they move in
// steps), then glides back to the newest state over the same time: if no
// newer state came, the entity stopped there. A delta only carries an entity when it changed, so after a pause a
// still entity's previous state is repeated just before the new one: it
// stays where it was until it moved, instead of drifting across the gap. A
// state older than the newest held is ignored.
#pragma once

#include "coop/proto/snapshot.h"

#include <cstddef>
#include <deque>
#include <utility>

namespace coop::proto
{
struct Presented
{
    EntityState state;
    bool holding = false;      // render time is past the newest state
    bool extrapolated = false; // holding, carried on along the newest state's velocity
    bool before_first = false; // render time is before the oldest state
};

// Interpolates two states: position, velocity and health linearly, yaw by
// the shorter way round, rotation by normalised quaternion lerp; state bits
// from the nearer state.
EntityState interpolate(const EntityState& a, const EntityState& b, float k);

// Server time as this client sees it. The offset (local minus server time)
// of the least-delayed recent snapshot is the estimate: a sliding-window
// minimum, so it follows a server whose ticks run slower or faster than
// their nominal length (the S06-T02 snap-1 session drifted about 1 %), and
// the offset used moves towards it by at most max_slew_ms per observation,
// so a change does not jolt the timeline. B51: with fast_beyond_ms > 0 the
// limit is max_slew_ms only while the offset is within fast_beyond_ms of the
// estimate and 5 ms beyond it (a small limit follows a drift in steps too
// small to see; a large change still catches up quickly).
class ServerClock
{
public:
    explicit ServerClock(double window_ms = 2000, double max_slew_ms = 5, double fast_beyond_ms = 0)
        : window_ms_(window_ms), max_slew_ms_(max_slew_ms), fast_beyond_ms_(fast_beyond_ms) {}

    void observe(double local_ms, double server_ms);
    bool ready() const { return ready_; }
    double offset() const { return offset_; }
    double target() const { return window_.empty() ? offset_ : window_.front().second; }
    double server_now(double local_ms) const { return local_ms - offset_; }
    void reset();

private:
    double window_ms_, max_slew_ms_, fast_beyond_ms_;
    bool ready_ = false;
    double offset_ = 0;
    std::deque<std::pair<double, double>> window_; // (local time, offset), offsets increasing: front is the minimum
};

class InterpolationBuffer
{
public:
    InterpolationBuffer() : InterpolationBuffer(16) {}
    explicit InterpolationBuffer(std::size_t capacity) : capacity_(capacity < 2 ? 2 : capacity) {}

    // Moving: a velocity component faster than 0.1 m/s.
    static bool moving(const EntityState& s);
    void set_extrapolation(double max_ms) { max_extrapolate_ms_ = max_ms; }

    // `interval_ms`: the snapshot interval; a gap longer than 1.5 intervals
    // repeats the previous state at time_ms - interval_ms first.
    bool push(double time_ms, const EntityState& state, double interval_ms);
    bool sample(double render_ms, Presented& out) const;

    bool empty() const { return samples_.empty(); }
    std::size_t size() const { return samples_.size(); }
    double newest_ms() const { return samples_.empty() ? 0.0 : samples_.back().time_ms; }
    const EntityState* newest() const { return samples_.empty() ? nullptr : &samples_.back().state; }
    void clear() { samples_.clear(); }

private:
    struct Sample
    {
        double time_ms = 0;
        EntityState state;
    };
    std::size_t capacity_;
    double max_extrapolate_ms_ = 0;
    std::deque<Sample> samples_;
};

} // namespace coop::proto
