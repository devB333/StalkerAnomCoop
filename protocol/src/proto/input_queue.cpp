// S06-T01: see input_queue.h.
#include "coop/proto/input_queue.h"

#include <algorithm>
#include <cmath>

namespace coop::proto
{
const char* to_string(InputVerdict v)
{
    switch (v)
    {
    case InputVerdict::Queued: return "queued";
    case InputVerdict::Stale: return "stale";
    case InputVerdict::Ahead: return "ahead";
    case InputVerdict::Invalid: return "invalid";
    }
    return "?";
}

const char* validate_input(const Input& in, float max_pitch)
{
    if (!std::isfinite(in.move_x) || !std::isfinite(in.move_y) || !std::isfinite(in.yaw) || !std::isfinite(in.pitch))
        return "not finite";
    if (in.move_x < -1.f || in.move_x > 1.f || in.move_y < -1.f || in.move_y > 1.f)
        return "axis out of range";
    if (in.yaw < -kMaxAngle || in.yaw > kMaxAngle)
        return "yaw out of range";
    if (in.pitch < -max_pitch || in.pitch > max_pitch)
        return "pitch out of range";
    if (in.buttons & ~kKnownButtons)
        return "unknown button";
    switch (in.stance)
    {
    case Stance::Stand:
    case Stance::Crouch:
    case Stance::Sprint: break;
    default: return "unsupported stance"; // Prone: the actor has no such mode
    }
    if (in.tick == 0)
        return "tick 0";
    return "";
}

InputQueue::InputQueue(const InputQueueConfig& config) : config_(config)
{
    if (config_.capacity == 0)
        config_.capacity = 1;
}

void InputQueue::reset()
{
    queue_.clear();
    last_consumed_ = 0;
    newest_ = 0;
    has_last_ = false;
    held_run_ = 0;
}

InputVerdict InputQueue::offer(const Input& in, const char** why)
{
    ++counters_.offered;
    const char* reason = validate_input(in, config_.max_pitch);
    if (*reason)
    {
        ++counters_.invalid;
        if (why)
            *why = reason;
        return InputVerdict::Invalid;
    }
    if (in.tick <= last_consumed_)
    {
        ++counters_.stale;
        if (why)
            *why = "not after the last consumed tick";
        return InputVerdict::Stale;
    }
    if (in.tick > newest_ + config_.max_gap)
    {
        ++counters_.ahead;
        if (why)
            *why = "tick jump beyond max_gap";
        return InputVerdict::Ahead;
    }
    // Tick order; a repeat is stale.
    auto at = std::lower_bound(queue_.begin(), queue_.end(), in.tick,
        [](const Input& q, std::uint64_t tick) { return q.tick < tick; });
    if (at != queue_.end() && at->tick == in.tick)
    {
        ++counters_.stale;
        if (why)
            *why = "already queued";
        return InputVerdict::Stale;
    }
    queue_.insert(at, in);
    if (in.tick > newest_)
        newest_ = in.tick;
    ++counters_.queued;
    while (queue_.size() > config_.capacity)
    {
        queue_.pop_front();
        ++counters_.dropped_oldest;
    }
    counters_.depth_peak = std::max(counters_.depth_peak, queue_.size());
    if (why)
        *why = "";
    return InputVerdict::Queued;
}

std::size_t InputQueue::offer_batch(const InputBatch& batch)
{
    std::size_t queued = 0;
    for (std::size_t i = batch.inputs.size(); i-- > 0;)
        if (offer(batch.inputs[i]) == InputVerdict::Queued)
            ++queued;
    return queued;
}

InputSource InputQueue::consume(Input& out)
{
    if (!queue_.empty())
    {
        out = queue_.front();
        queue_.pop_front();
        last_consumed_ = out.tick;
        last_ = out;
        has_last_ = true;
        held_run_ = 0;
        ++counters_.consumed;
        return InputSource::Queued;
    }
    if (has_last_ && held_run_ < config_.hold_ticks)
    {
        ++held_run_;
        ++counters_.held;
        out = last_;
        return InputSource::Held;
    }
    ++counters_.idle;
    return InputSource::None;
}
} // namespace coop::proto
