// S06-T03: own-actor prediction and reconciliation (coop/proto/prediction.h).
// Properties: a wrong prediction converges to the server; a correction is not
// applied twice; records from before a reset never correct; the view's
// offset decays.
#include "check.h"
#include "coop/proto/prediction.h"

#include <cmath>

namespace coop::test
{
using namespace coop::proto;

namespace
{
float Len(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
}

TEST("prediction/exact-and-tolerance")
{
    Reconciler r;
    for (std::uint64_t t = 1; t <= 10; ++t)
        r.record(t, Vec3{float(t) * 0.05f, 0, 0});
    // The server agrees with what was recorded at tick 5 (within 1 cm).
    Correction c = r.reconcile(5, Vec3{0.25f + 0.01f, 0, 0});
    CHECK(c.kind == CorrectionKind::None && c.offset.x == 0.f);
    CHECK(r.size() == 6); // ticks 5..10 kept
    CHECK(r.counters().within == 1);
    // A tick never recorded, and one already dropped.
    CHECK(r.reconcile(40, Vec3{}).kind == CorrectionKind::Unknown);
    Reconciler r2;
    r2.record(5, Vec3{});
    r2.record(6, Vec3{});
    CHECK(r2.reconcile(6, Vec3{}).kind == CorrectionKind::None);
    CHECK(r2.reconcile(5, Vec3{}).kind == CorrectionKind::Unknown);
    r2.record(6, Vec3{9, 9, 9}); // not newer: ignored
    CHECK(r2.size() == 1);
}

TEST("prediction/correction-moves-newer-records-once")
{
    Reconciler r;
    for (std::uint64_t t = 1; t <= 10; ++t)
        r.record(t, Vec3{float(t), 0, 0});
    // The server had the actor 0.5 m short at tick 4.
    Correction c = r.reconcile(4, Vec3{3.5f, 0, 0});
    REQUIRE(c.kind == CorrectionKind::Smooth);
    CHECK(std::fabs(c.offset.x + 0.5f) < 1e-5f && std::fabs(c.error - 0.5f) < 1e-5f);
    // The next snapshot agrees with the corrected history: nothing more.
    Correction d = r.reconcile(6, Vec3{5.5f, 0, 0});
    CHECK(d.kind == CorrectionKind::None);
    // A teleport-sized error jumps.
    Correction e = r.reconcile(8, Vec3{50.f, 0, 0});
    CHECK(e.kind == CorrectionKind::Snap && std::fabs(e.offset.x - (50.f - 7.5f)) < 1e-4f);
    CHECK(r.counters().smoothed == 1 && r.counters().snapped == 1);
}

TEST("prediction/wrong-prediction-converges")
{
    // The client predicts 3 m/s forward for 1.5 s while the server keeps the
    // actor still; snapshots arrive every 2.5 ticks, 5 ticks late (100 ms).
    Reconciler r;
    float client = 0.f;
    const float server = 0.f;
    float worst_after = 0.f;
    std::uint64_t corrections = 0;
    for (std::uint64_t t = 1; t <= 200; ++t)
    {
        r.record(t, Vec3{client, 0, 0});
        const bool wrong = t <= 75;
        if (wrong)
            client += 3.f * 0.02f; // the local actor walks
        if (t % 5 == 0 || t % 5 == 3) // ~20 Hz
        {
            if (t > 5)
            {
                const Correction c = r.reconcile(t - 5, Vec3{server, 0, 0});
                if (c.kind == CorrectionKind::Smooth || c.kind == CorrectionKind::Snap)
                {
                    client += c.offset.x;
                    ++corrections;
                }
            }
        }
        if (t > 85)
            worst_after = std::fmax(worst_after, std::fabs(client - server));
    }
    metric("corrections", double(corrections));
    metric("max_error_m", double(r.counters().max_error));
    CHECK_MSG(worst_after <= r.config().tolerance, "still %.3f m off after the wrong leg", worst_after);
    // While wrong, the error stays bounded by the latency (5 ticks + a
    // snapshot interval at 3 m/s): about 0.5 m, never growing.
    CHECK(r.counters().max_error < 0.6f);
}

TEST("prediction/reset-forgets-old-input")
{
    Reconciler r;
    for (std::uint64_t t = 100; t <= 140; ++t)
        r.record(t, Vec3{});
    r.reset();
    CHECK(r.size() == 0);
    // A snapshot from the old connection names tick 120: nothing to correct.
    Correction c = r.reconcile(120, Vec3{10, 0, 0});
    CHECK(c.kind == CorrectionKind::Unknown && c.offset.x == 0.f);
    // The new connection counts from 1 again.
    r.record(1, Vec3{});
    CHECK(r.reconcile(1, Vec3{0.5f, 0, 0}).kind == CorrectionKind::Smooth);
    CHECK(r.counters().resets == 1);
}

TEST("prediction/capacity")
{
    PredictionConfig c;
    c.capacity = 8;
    Reconciler r(c);
    for (std::uint64_t t = 1; t <= 20; ++t)
        r.record(t, Vec3{});
    CHECK(r.size() == 8);
    CHECK(r.reconcile(5, Vec3{}).kind == CorrectionKind::Unknown); // evicted
    CHECK(r.reconcile(13, Vec3{}).kind == CorrectionKind::None);
}

TEST("prediction/smoother-decays")
{
    Smoother s(60.f);
    s.add(Vec3{0.4f, 0, 0}); // the actor moved +0.4 m; the view stays behind
    CHECK(std::fabs(s.offset().x + 0.4f) < 1e-6f);
    Vec3 o = s.step(60.f);
    CHECK(std::fabs(o.x + 0.2f) < 1e-4f);
    for (int i = 0; i < 50; ++i)
        o = s.step(16.f);
    CHECK(Len(o) == 0.f);
    CHECK(std::fabs(s.max_offset() - 0.4f) < 1e-6f);
    s.add(Vec3{0.1f, 0, 0});
    s.clear();
    CHECK(Len(s.offset()) == 0.f);
}

} // namespace coop::test
