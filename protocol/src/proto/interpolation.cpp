// S06-T02: see coop/proto/interpolation.h.
#include "coop/proto/interpolation.h"

#include <cmath>

namespace coop::proto
{
namespace
{
float Lerp(float a, float b, float k) { return a + (b - a) * k; }

float AngleLerp(float a, float b, float k)
{
    const float pi = 3.14159265359f, two_pi = 6.28318530718f;
    float d = std::fmod(b - a, two_pi);
    if (d > pi)
        d -= two_pi;
    else if (d < -pi)
        d += two_pi;
    float r = a + d * k;
    if (r > pi)
        r -= two_pi;
    else if (r < -pi)
        r += two_pi;
    return r;
}
} // namespace

EntityState interpolate(const EntityState& a, const EntityState& b, float k)
{
    if (k <= 0.f)
        return a;
    if (k >= 1.f || !(a.entity == b.entity))
        return b;
    EntityState r = k < 0.5f ? a : b;
    r.components = std::uint16_t(a.components | b.components);
    r.transform.x = Lerp(a.transform.x, b.transform.x, k);
    r.transform.y = Lerp(a.transform.y, b.transform.y, k);
    r.transform.z = Lerp(a.transform.z, b.transform.z, k);
    r.transform.yaw = AngleLerp(a.transform.yaw, b.transform.yaw, k);
    r.vx = Lerp(a.vx, b.vx, k);
    r.vy = Lerp(a.vy, b.vy, k);
    r.vz = Lerp(a.vz, b.vz, k);
    r.health = std::uint16_t(std::lround(Lerp(float(a.health), float(b.health), k)));
    r.aim_yaw = AngleLerp(a.aim_yaw, b.aim_yaw, k);
    r.aim_pitch = AngleLerp(a.aim_pitch, b.aim_pitch, k);
    // Quaternions: the shorter arc (q and -q are one rotation), normalised.
    const float dot = a.qx * b.qx + a.qy * b.qy + a.qz * b.qz + a.qw * b.qw;
    const float s = dot < 0.f ? -1.f : 1.f;
    float qx = Lerp(a.qx, s * b.qx, k), qy = Lerp(a.qy, s * b.qy, k), qz = Lerp(a.qz, s * b.qz, k),
          qw = Lerp(a.qw, s * b.qw, k);
    const float n = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (n > 1e-6f)
    {
        r.qx = qx / n;
        r.qy = qy / n;
        r.qz = qz / n;
        r.qw = qw / n;
    }
    return r;
}

void ServerClock::observe(double local_ms, double server_ms)
{
    const double offset = local_ms - server_ms;
    while (!window_.empty() && window_.back().second >= offset)
        window_.pop_back();
    window_.emplace_back(local_ms, offset);
    while (window_.size() > 1 && window_.front().first < local_ms - window_ms_)
        window_.pop_front();
    const double target = window_.front().second;
    if (!ready_)
    {
        offset_ = target;
        ready_ = true;
        return;
    }
    const double step = target - offset_;
    const double limit = fast_beyond_ms_ > 0 && (step > fast_beyond_ms_ || step < -fast_beyond_ms_) ? 5.0 : max_slew_ms_;
    offset_ += step > limit ? limit : step < -limit ? -limit : step;
}

void ServerClock::reset()
{
    ready_ = false;
    offset_ = 0;
    window_.clear();
}

bool InterpolationBuffer::moving(const EntityState& s)
{
    return (s.components & kVelocity) && s.vx * s.vx + s.vy * s.vy + s.vz * s.vz > 0.01f;
}

bool InterpolationBuffer::push(double time_ms, const EntityState& state, double interval_ms)
{
    if (!samples_.empty())
    {
        const Sample& last = samples_.back();
        if (time_ms <= last.time_ms)
            return false;
        // A new incarnation starts its own history.
        if (!(last.state.entity == state.entity))
            samples_.clear();
        else if (interval_ms > 0 && time_ms - last.time_ms > 1.5 * interval_ms && !moving(last.state))
        {
            Sample hold = last;
            hold.time_ms = time_ms - interval_ms;
            samples_.push_back(hold);
        }
    }
    Sample s;
    s.time_ms = time_ms;
    s.state = state;
    samples_.push_back(s);
    while (samples_.size() > capacity_)
        samples_.pop_front();
    return true;
}

bool InterpolationBuffer::sample(double render_ms, Presented& out) const
{
    out = Presented{};
    if (samples_.empty())
        return false;
    if (render_ms >= samples_.back().time_ms)
    {
        out.state = samples_.back().state;
        out.holding = render_ms > samples_.back().time_ms;
        if (out.holding && max_extrapolate_ms_ > 0 && moving(out.state))
        {
            const double ahead = render_ms - samples_.back().time_ms;
            double carried = ahead < max_extrapolate_ms_ ? ahead : max_extrapolate_ms_;
            if (ahead > max_extrapolate_ms_) // back to the newest state over max_extrapolate_ms
                carried = ahead >= 2 * max_extrapolate_ms_ ? 0.0 : 2 * max_extrapolate_ms_ - ahead;
            const float dt = float(carried / 1000.0);
            out.state.transform.x += out.state.vx * dt;
            out.state.transform.y += out.state.vy * dt;
            out.state.transform.z += out.state.vz * dt;
            out.extrapolated = true;
        }
        return true;
    }
    if (render_ms <= samples_.front().time_ms)
    {
        out.state = samples_.front().state;
        out.before_first = render_ms < samples_.front().time_ms;
        return true;
    }
    for (std::size_t i = 0; i + 1 < samples_.size(); ++i)
    {
        const Sample& a = samples_[i];
        const Sample& b = samples_[i + 1];
        if (render_ms >= a.time_ms && render_ms <= b.time_ms)
        {
            const double span = b.time_ms - a.time_ms;
            const float k = span > 0 ? float((render_ms - a.time_ms) / span) : 1.f;
            out.state = interpolate(a.state, b.state, k);
            return true;
        }
    }
    out.state = samples_.back().state;
    return true;
}

} // namespace coop::proto
