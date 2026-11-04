// S06-T03: see coop/proto/prediction.h.
#include "coop/proto/prediction.h"

#include <cmath>

namespace coop::proto
{
namespace
{
float Length(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
} // namespace

void Reconciler::record(std::uint64_t tick, const Vec3& position)
{
    if (!records_.empty() && tick <= records_.back().tick)
        return; // ticks only increase
    records_.push_back({tick, position});
    while (records_.size() > config_.capacity)
        records_.pop_front();
    ++counters_.recorded;
}

Correction Reconciler::reconcile(std::uint64_t last_input, const Vec3& server)
{
    Correction c;
    // Records older than last_input are no longer needed.
    while (!records_.empty() && records_.front().tick < last_input)
        records_.pop_front();
    if (records_.empty() || records_.front().tick != last_input)
    {
        ++counters_.unknown;
        c.kind = CorrectionKind::Unknown;
        return c;
    }
    ++counters_.reconciled;
    const Vec3& at = records_.front().position;
    Vec3 e{server.x - at.x, server.y - at.y, server.z - at.z};
    c.error = Length(e);
    if (c.error > counters_.max_error)
        counters_.max_error = c.error;
    if (c.error <= config_.tolerance)
    {
        ++counters_.within;
        c.kind = CorrectionKind::None;
        return c;
    }
    c.kind = c.error > config_.snap_distance ? CorrectionKind::Snap : CorrectionKind::Smooth;
    if (c.kind == CorrectionKind::Snap)
        ++counters_.snapped;
    else
        ++counters_.smoothed;
    if (c.kind == CorrectionKind::Smooth && config_.blend < 1.f && c.error <= config_.blend_below)
    {
        e.x *= config_.blend;
        e.y *= config_.blend;
        e.z *= config_.blend;
        ++counters_.blended;
    }
    c.offset = e;
    // Everything predicted from here on was predicted from the wrong place.
    for (Record& r : records_)
    {
        r.position.x += e.x;
        r.position.y += e.y;
        r.position.z += e.z;
    }
    return c;
}

void Reconciler::reset()
{
    records_.clear();
    ++counters_.resets;
}

void Smoother::add(const Vec3& correction)
{
    offset_.x -= correction.x;
    offset_.y -= correction.y;
    offset_.z -= correction.z;
    const float m = Length(offset_);
    if (m > max_offset_)
        max_offset_ = m;
}

Vec3 Smoother::step(float dt_ms)
{
    if (half_life_ms_ <= 0.f || dt_ms <= 0.f)
        return offset_;
    const float k = std::pow(0.5f, dt_ms / half_life_ms_);
    offset_.x *= k;
    offset_.y *= k;
    offset_.z *= k;
    if (Length(offset_) < 0.001f)
        offset_ = Vec3{};
    return offset_;
}

} // namespace coop::proto
