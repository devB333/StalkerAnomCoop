// S06-T03: prediction and reconciliation of a player's own actor.
//
// The client moves its own actor at once, through the same native control
// path the server runs (prediction), and records where the actor was when it
// sampled each input tick. The server's snapshot names the last input tick it
// consumed (L) and carries the actor's authoritative position, which is the
// result of the inputs before L (the server consumes L before its tick's
// simulation and sends the snapshot between the two). So the error is
//     e = server position - recorded position at L.
// Correcting means moving the actor by e: its displacement since L, the
// unacknowledged inputs' movement, is kept (the replay of what was predicted
// after L), and every newer record moves by e too, so the next snapshot does
// not correct the same error again.
//
// Corrections under `tolerance` are ignored (the two simulations' noise);
// corrections over `snap_distance` (a teleport, a respawn, a refused stance)
// are applied as one jump with no smoothing. The visual side (Smoother) is
// separate: the collision state takes the correction at once, the view
// follows over a short time.
//
// A reset (a new connection, a new epoch, a level change) forgets every
// record; a snapshot naming an input tick from before the reset finds no
// record and changes nothing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

namespace coop::proto
{
struct Vec3
{
    float x = 0, y = 0, z = 0;
};

struct PredictionConfig
{
    std::size_t capacity = 256; // input ticks remembered (5 s at 50 Hz)
    float tolerance = 0.03f;    // metres: smaller errors are left alone
    float snap_distance = 1.5f; // metres: larger errors jump, unsmoothed
};

enum class CorrectionKind : std::uint8_t
{
    None,    // within tolerance
    Smooth,  // applied; the view follows over time
    Snap,    // applied as one jump
    Unknown, // no record for that input tick (too old, from before a reset, or not sent yet)
};

struct Correction
{
    CorrectionKind kind = CorrectionKind::None;
    Vec3 offset; // add to the actor's position (zero unless Smooth or Snap)
    float error = 0; // |server - recorded|
};

struct PredictionCounters
{
    std::uint64_t recorded = 0;
    std::uint64_t reconciled = 0; // snapshots with a record
    std::uint64_t within = 0;
    std::uint64_t smoothed = 0;
    std::uint64_t snapped = 0;
    std::uint64_t unknown = 0;
    std::uint64_t resets = 0;
    float max_error = 0;
};

class Reconciler
{
public:
    Reconciler() : Reconciler(PredictionConfig{}) {}
    explicit Reconciler(const PredictionConfig& config) : config_(config) {}

    // Where the actor was when input `tick` was sampled. Ticks increase.
    void record(std::uint64_t tick, const Vec3& position);
    // The server's position after the inputs before `last_input`.
    Correction reconcile(std::uint64_t last_input, const Vec3& server);
    void reset();

    const PredictionCounters& counters() const { return counters_; }
    const PredictionConfig& config() const { return config_; }
    std::size_t size() const { return records_.size(); }

private:
    struct Record
    {
        std::uint64_t tick = 0;
        Vec3 position;
    };
    PredictionConfig config_;
    PredictionCounters counters_;
    std::deque<Record> records_;
};

// The view's offset from the actor after a correction: it starts at minus
// the correction (the view stays where it was) and decays to zero with the
// given half-life, so the eye glides to the corrected position.
class Smoother
{
public:
    explicit Smoother(float half_life_ms = 60.f) : half_life_ms_(half_life_ms) {}
    void add(const Vec3& correction); // the view keeps -correction
    Vec3 step(float dt_ms);           // decays and returns the offset to add to the view
    Vec3 offset() const { return offset_; }
    void clear() { offset_ = Vec3{}; }
    float max_offset() const { return max_offset_; }

private:
    float half_life_ms_;
    Vec3 offset_;
    float max_offset_ = 0;
};

} // namespace coop::proto
