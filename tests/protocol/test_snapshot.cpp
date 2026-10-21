// S06-T02: snapshots against acknowledged baselines (coop/proto/snapshot.h)
// and presentation (coop/proto/interpolation.h). The properties under test:
// a decoded snapshot reproduces the sender's frame exactly whatever was lost,
// reordered or repeated; a missing baseline leads to a bounded full snapshot;
// an old state never overwrites a newer one; a budget only delays changes.
#include "check.h"
#include "coop/proto/interpolation.h"
#include "coop/proto/snapshot.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace coop::test
{
using namespace coop::proto;

namespace
{
EntityState Creature(std::uint64_t id, float x, std::uint32_t generation = 1)
{
    EntityState s;
    s.entity = {id, generation};
    s.components = kTransform | kVelocity | kHealth | kStateBits;
    s.transform = {x, 1.f, 2.f, 0.25f};
    s.health = 10000;
    return s;
}

EntityState Item(std::uint64_t id, float x)
{
    EntityState s;
    s.entity = {id, 1};
    s.components = kTransform | kRotation;
    s.transform = {x, 0.5f, -3.f, 0.f};
    return s;
}

bool SameState(const EntityState& a, const EntityState& b)
{
    return a.entity == b.entity && a.components == b.components && a.transform.x == b.transform.x &&
           a.transform.y == b.transform.y && a.transform.z == b.transform.z && a.transform.yaw == b.transform.yaw &&
           a.vx == b.vx && a.vy == b.vy && a.vz == b.vz && a.health == b.health && a.state_bits == b.state_bits &&
           a.qx == b.qx && a.qy == b.qy && a.qz == b.qz && a.qw == b.qw;
}

bool SameFrame(const SnapshotFrame& a, const SnapshotFrame& b)
{
    if (a.size() != b.size())
        return false;
    for (const auto& e : a)
    {
        const auto it = b.find(e.first);
        if (it == b.end() || !SameState(e.second, it->second))
            return false;
    }
    return true;
}

// Every part through the real encoder and decoder.
std::vector<Snapshot> OverWire(const std::vector<Snapshot>& parts, std::size_t* bytes = nullptr)
{
    std::vector<Snapshot> out;
    std::uint64_t seq = 100;
    for (const Snapshot& p : parts)
    {
        std::vector<std::uint8_t> b;
        std::string error;
        if (!encode(Message(p), 5, ++seq, b, &error))
        {
            fail(__FILE__, __LINE__, "encode: " + error);
            continue;
        }
        if (bytes)
            *bytes += b.size();
        Decoded d;
        if (!decode(kSnapshot, Role::Client, b.data(), b.size(), d).ok())
        {
            fail(__FILE__, __LINE__, "decode refused a built part");
            continue;
        }
        out.push_back(std::get<Snapshot>(d.body));
    }
    return out;
}

SnapshotVerdict OfferAll(SnapshotReceiver& r, const std::vector<Snapshot>& parts, SnapshotUpdate* last = nullptr)
{
    SnapshotVerdict v = SnapshotVerdict::Applied;
    for (const Snapshot& p : parts)
    {
        SnapshotUpdate u;
        v = r.offer(p, u);
        if (last)
            *last = u;
    }
    return v;
}

void Ack(SnapshotReceiver& r, SnapshotSender& s)
{
    SnapshotAck a;
    if (r.take_ack(a))
        s.on_ack(a);
}
} // namespace

TEST("snapshot/sizes-match-encoder")
{
    // The sender's byte accounting is what the encoder produces.
    SnapshotSender s;
    std::vector<EntityState> wanted;
    for (std::uint64_t i = 1; i <= 40; ++i)
        wanted.push_back(i % 3 ? Creature(i, float(i)) : Item(i, float(i)));
    std::vector<Snapshot> parts;
    s.build(10, 3, wanted, parts);
    std::size_t bytes = 0;
    OverWire(parts, &bytes);
    CHECK(parts.size() == 2);
    CHECK_MSG(bytes == s.counters().bytes, "encoded %zu, counted %llu", bytes, (unsigned long long)s.counters().bytes);
    for (const Snapshot& p : parts)
        CHECK(p.snapshot_id == 1 && p.baseline_id == 0 && p.part_count == 2 && p.server_tick == 10 && p.last_input == 3);
}

TEST("snapshot/full-then-delta")
{
    SnapshotSender s;
    SnapshotReceiver r;
    std::vector<EntityState> world = {Creature(1, 0.f), Creature(2, 5.f), Item(3, 9.f)};
    std::vector<Snapshot> parts;
    CHECK(s.build(1, 0, world, parts) == 1);
    REQUIRE(parts.size() == 1 && parts[0].entities.size() == 3 && parts[0].baseline_id == 0);
    SnapshotUpdate u;
    CHECK(OfferAll(r, OverWire(parts), &u) == SnapshotVerdict::Completed);
    CHECK(u.states.size() == 3);
    Ack(r, s);
    CHECK(s.acked() == 1 && s.next_baseline() == 1);

    // Only entity 2 moves: the delta carries its transform alone.
    world[1].transform.x = 6.f;
    s.build(2, 0, world, parts);
    REQUIRE(parts.size() == 1 && parts[0].baseline_id == 1 && parts[0].entities.size() == 1);
    CHECK(parts[0].entities[0].entity.id == 2 && parts[0].entities[0].components == kTransform);
    CHECK(OfferAll(r, OverWire(parts), &u) == SnapshotVerdict::Completed);
    REQUIRE(u.states.size() == 1);
    CHECK(u.states[0].transform.x == 6.f && u.states[0].health == 10000); // baseline fields filled in
    REQUIRE(r.frame(2) && s.frame(2));
    CHECK(SameFrame(*r.frame(2), *s.frame(2)));

    // Nothing changes: still one (empty) part, which keeps the tick moving.
    Ack(r, s);
    s.build(3, 7, world, parts);
    REQUIRE(parts.size() == 1 && parts[0].entities.empty() && parts[0].baseline_id == 2 && parts[0].last_input == 7);
    CHECK(OfferAll(r, OverWire(parts)) == SnapshotVerdict::Completed);
    CHECK(s.counters().unchanged == 3 + 2);
    metric("delta_bytes_one_mover", double(entry_bytes(kTransform) + kSnapshotPartOverhead));
}

TEST("snapshot/removal-generation-and-holds")
{
    SnapshotSender s;
    SnapshotReceiver r;
    std::vector<EntityState> world = {Creature(1, 0.f), Item(2, 1.f), Creature(3, 2.f)};
    std::vector<Snapshot> parts;
    s.build(1, 0, world, parts);
    OfferAll(r, OverWire(parts));
    Ack(r, s);
    CHECK(s.acked_holds(world[1]));
    EntityState moved = world[1];
    moved.transform.x += 0.5f;
    CHECK(!s.acked_holds(moved));
    CHECK(s.acked_holds(world[0]));

    // Entity 2 leaves the set; entity 3 comes back as another incarnation.
    std::vector<EntityState> next = {world[0], Creature(3, 2.f, 2)};
    s.build(2, 0, next, parts);
    REQUIRE(parts.size() == 1 && parts[0].entities.size() == 2);
    CHECK(parts[0].entities[0].entity == (EntityRef{2, 1}) && parts[0].entities[0].components == kRemoved);
    CHECK(parts[0].entities[1].entity == (EntityRef{3, 2}) &&
          parts[0].entities[1].components == (kTransform | kVelocity | kHealth | kStateBits));
    SnapshotUpdate u;
    CHECK(OfferAll(r, OverWire(parts), &u) == SnapshotVerdict::Completed);
    REQUIRE(u.removed.size() == 1 && u.states.size() == 1);
    CHECK(u.removed[0].id == 2 && u.states[0].entity.generation == 2);
    REQUIRE(r.frame(2));
    CHECK(r.frame(2)->size() == 2 && !r.frame(2)->count(2) && r.frame(2)->at(3).entity.generation == 2);
    CHECK(SameFrame(*r.frame(2), *s.frame(2)));
    CHECK(s.counters().removals == 1);
    std::uint64_t tick = 0;
    CHECK(r.newest_frame(&tick) == r.frame(2) && tick == 2);
}

TEST("snapshot/lost-part-keeps-the-old-baseline")
{
    SnapshotSenderConfig c;
    c.max_part_bytes = 400; // a few entries per part
    SnapshotSender s(c);
    SnapshotReceiver r;
    std::vector<EntityState> world;
    for (std::uint64_t i = 1; i <= 30; ++i)
        world.push_back(Creature(i, float(i)));
    std::vector<Snapshot> parts;
    s.build(1, 0, world, parts);
    REQUIRE(parts.size() > 2);
    OfferAll(r, OverWire(parts));
    Ack(r, s);
    REQUIRE(s.acked() == 1);

    // Snapshot 2: every entity moves; its second part is lost.
    for (EntityState& e : world)
        e.transform.x += 1.f;
    s.build(2, 0, world, parts);
    REQUIRE(parts.size() > 2 && parts[0].baseline_id == 1);
    auto wire = OverWire(parts);
    wire.erase(wire.begin() + 1);
    CHECK(OfferAll(r, wire) == SnapshotVerdict::Applied); // its parts apply, but it is not complete
    CHECK(r.newest_complete() == 1);
    SnapshotAck a;
    CHECK(!r.take_ack(a)); // nothing new to acknowledge

    // Snapshot 3 is still a delta against 1, so it repeats what 2 carried.
    s.build(3, 0, world, parts);
    REQUIRE(parts[0].baseline_id == 1);
    std::size_t entries = 0;
    for (const Snapshot& p : parts)
        entries += p.entities.size();
    CHECK(entries == world.size());
    CHECK(OfferAll(r, OverWire(parts)) == SnapshotVerdict::Completed);
    REQUIRE(r.frame(3) && s.frame(3));
    CHECK(SameFrame(*r.frame(3), *s.frame(3)));
    CHECK(r.frame(3)->at(5).transform.x == 6.f);
}

TEST("snapshot/reordered-parts-and-old-snapshots")
{
    SnapshotSenderConfig c;
    c.max_part_bytes = 300;
    SnapshotSender s(c);
    SnapshotReceiver r;
    std::vector<EntityState> world;
    for (std::uint64_t i = 1; i <= 12; ++i)
        world.push_back(Creature(i, 0.f));
    std::vector<Snapshot> p1, p2;
    s.build(10, 0, world, p1);
    for (EntityState& e : world)
        e.transform.x = 1.f;
    s.build(20, 0, world, p2); // also full: nothing acknowledged yet
    REQUIRE(p1.size() > 1 && p2.size() > 1);
    auto w1 = OverWire(p1), w2 = OverWire(p2);
    // Snapshot 2's parts in reverse order first, then snapshot 1's.
    std::reverse(w2.begin(), w2.end());
    CHECK(OfferAll(r, w2) == SnapshotVerdict::Completed);
    SnapshotUpdate u;
    std::size_t handed = 0;
    for (const Snapshot& p : w1)
    {
        r.offer(p, u);
        handed += u.states.size();
    }
    CHECK(handed == 0); // every state of snapshot 1 is older than what was shown
    CHECK(r.counters().older == world.size());
    CHECK(r.newest_complete() == 2);
    REQUIRE(r.frame(1) && r.frame(2));
    CHECK(SameFrame(*r.frame(2), *s.frame(2)) && SameFrame(*r.frame(1), *s.frame(1)));
    // A part offered twice, and a whole snapshot offered again.
    CHECK(r.offer(w2[0], u) == SnapshotVerdict::Duplicate);
    // One entity in two parts of a snapshot is refused.
    Snapshot bad = w2[0];
    bad.snapshot_id = 9;
    bad.baseline_id = 0;
    bad.part = 0;
    bad.part_count = 2;
    Snapshot bad2 = bad;
    bad2.part = 1;
    CHECK(r.offer(bad, u) == SnapshotVerdict::Applied);
    CHECK(r.offer(bad2, u) == SnapshotVerdict::Inconsistent);
    bad2.part_count = 3;
    CHECK(r.offer(bad2, u) == SnapshotVerdict::Inconsistent);
}

TEST("snapshot/baseline-eviction-gives-a-full-snapshot")
{
    // The client stops acknowledging (its acks are lost) for longer than the
    // sender's history: the acknowledged baseline is evicted, and the next
    // snapshot is full and bounded.
    SnapshotSenderConfig c;
    c.history = 8;
    c.max_parts = 3;
    SnapshotSender s(c);
    SnapshotReceiver r;
    std::vector<EntityState> world;
    for (std::uint64_t i = 1; i <= 50; ++i)
        world.push_back(Creature(i, float(i)));
    std::vector<Snapshot> parts;
    s.build(1, 0, world, parts);
    OfferAll(r, OverWire(parts));
    Ack(r, s);
    for (std::uint64_t t = 2; t <= 9; ++t)
    {
        world[0].transform.x += 0.1f;
        s.build(t, 0, world, parts);
        CHECK(parts[0].baseline_id == 1);
        OfferAll(r, OverWire(parts));
        SnapshotAck lost;
        r.take_ack(lost); // never reaches the sender
    }
    // Snapshot 10: baseline 1 is gone from the history of 8.
    s.build(10, 0, world, parts);
    CHECK(parts[0].baseline_id == 0);
    CHECK(s.counters().baseline_lost == 1);
    CHECK(parts.size() <= c.max_parts);
    CHECK(OfferAll(r, OverWire(parts)) == SnapshotVerdict::Completed);
    REQUIRE(r.frame(10) && s.frame(10));
    CHECK(SameFrame(*r.frame(10), *s.frame(10)));
    Ack(r, s);
    s.build(11, 0, world, parts);
    CHECK(parts[0].baseline_id == 10);
}

TEST("snapshot/missing-baseline-asks-for-full")
{
    // A client that lost its copy of the baseline (here its receiver starts
    // over, standing for any loss of client state) says so; the next
    // snapshot is full and bounded.
    SnapshotSender s;
    SnapshotReceiver r;
    std::vector<EntityState> world = {Creature(1, 0.f), Creature(2, 1.f)};
    std::vector<Snapshot> parts;
    s.build(1, 0, world, parts);
    OfferAll(r, OverWire(parts));
    Ack(r, s); // acknowledges 1
    r.reset();
    world[0].transform.x = 3.f;
    s.build(2, 0, world, parts); // against 1, which the client no longer has
    REQUIRE(parts[0].baseline_id == 1);
    CHECK(OfferAll(r, OverWire(parts)) == SnapshotVerdict::MissingBaseline);
    SnapshotAck a;
    REQUIRE(r.take_ack(a));
    CHECK(a.snapshot_id == 0 && a.missing_baseline == 1);
    std::vector<std::uint8_t> bytes;
    CHECK(encode(Message(a), 5, 1, bytes)); // an ack that only asks is valid on the wire
    s.on_ack(a);
    CHECK(s.counters().full_requested == 1 && s.next_baseline() == 0);
    s.build(3, 0, world, parts);
    CHECK(parts[0].baseline_id == 0 && parts[0].entities.size() == 2);
    CHECK(OfferAll(r, OverWire(parts)) == SnapshotVerdict::Completed);
    REQUIRE(r.frame(3) && s.frame(3));
    CHECK(SameFrame(*r.frame(3), *s.frame(3)));
    Ack(r, s);
    CHECK(s.next_baseline() == 3); // back to deltas
}

TEST("snapshot/budget-defers-and-converges")
{
    // 300 entities, two parts per snapshot: the first full snapshot cannot
    // hold them; every snapshot stays within budget and the client's frame
    // reaches the whole world after a few snapshots.
    SnapshotSenderConfig c;
    c.max_parts = 2;
    SnapshotSender s(c);
    SnapshotReceiver r;
    std::vector<EntityState> world;
    for (std::uint64_t i = 1; i <= 300; ++i)
        world.push_back(i % 2 ? Creature(i, float(i)) : Item(i, float(i)));
    std::vector<Snapshot> parts;
    int snapshots = 0;
    for (std::uint64_t t = 1; t <= 40; ++t)
    {
        s.build(t, 0, world, parts);
        ++snapshots;
        CHECK(parts.size() <= 2);
        std::size_t bytes = 0;
        auto wire = OverWire(parts, &bytes);
        CHECK(bytes <= 2 * kMaxMessageBytes[kSnapshot]);
        OfferAll(r, wire);
        Ack(r, s);
        if (r.frame(t) && r.frame(t)->size() == world.size())
            break;
    }
    REQUIRE(r.frame(std::uint64_t(snapshots)));
    CHECK(r.frame(std::uint64_t(snapshots))->size() == world.size());
    CHECK(s.counters().deferred > 0);
    metric("snapshots_to_converge_300_entities_2_parts", snapshots);
    // The most important entity (first wanted) went out in the first snapshot.
    REQUIRE(r.frame(std::uint64_t(snapshots)));
    CHECK(s.frame(1) && s.frame(1)->count(1));
}

TEST("snapshot/random-loss-reorder-duplicates")
{
    // 2,000 snapshots of a world where entities move, appear, leave and come
    // back as new incarnations; parts lost (20 %), duplicated (5 %) and
    // swapped with the next (10 %); acks lost (20 %). Invariants: each
    // snapshot the client completes equals the sender's frame for that id;
    // states reach presentation in tick order per entity; after the loss
    // stops the client converges on the world.
    std::mt19937 rng(20261020);
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    SnapshotSenderConfig c;
    c.max_parts = 3;
    SnapshotSender s(c);
    SnapshotReceiver r;
    std::map<std::uint64_t, EntityState> world;
    for (std::uint64_t i = 1; i <= 80; ++i)
        world[i] = i % 4 ? Creature(i, float(i)) : Item(i, float(i));
    std::map<std::uint64_t, std::uint64_t> last_tick; // per entity, what presentation saw
    int mismatches = 0, order_errors = 0, completed = 0;
    std::size_t bytes = 0;
    std::vector<Snapshot> carry; // a part held back to be delivered after the next snapshot's
    for (std::uint64_t t = 1; t <= 2020; ++t)
    {
        const bool lossy = t <= 2000;
        for (auto& e : world)
        {
            if (u01(rng) < 0.3f)
            {
                e.second.transform.x += u01(rng) - 0.5f;
                e.second.transform.yaw = u01(rng) * 6.f - 3.f;
            }
            if ((e.second.components & kHealth) && u01(rng) < 0.02f)
                e.second.health = std::uint16_t(u01(rng) * 10000);
        }
        if (u01(rng) < 0.05f)
        {
            auto it = world.begin();
            std::advance(it, std::size_t(u01(rng) * float(world.size() - 1)));
            it->second.entity.generation += 1; // a new incarnation
        }
        std::vector<EntityState> wanted;
        for (const auto& e : world)
            if (e.first % 7 != t / 300 % 7) // a slice leaves and comes back
                wanted.push_back(e.second);
        std::vector<Snapshot> parts;
        s.build(t, t, wanted, parts);
        auto wire = OverWire(parts, &bytes);
        std::vector<Snapshot> deliver;
        for (Snapshot& p : wire)
        {
            if (lossy && u01(rng) < 0.2f)
                continue;
            deliver.push_back(p);
            if (lossy && u01(rng) < 0.05f)
                deliver.push_back(p);
        }
        for (Snapshot& p : carry)
            deliver.push_back(p);
        carry.clear();
        if (lossy && !deliver.empty() && u01(rng) < 0.1f)
        {
            carry.push_back(deliver.front());
            deliver.erase(deliver.begin());
        }
        for (const Snapshot& p : deliver)
        {
            SnapshotUpdate up;
            const SnapshotVerdict v = r.offer(p, up);
            for (const EntityState& st : up.states)
            {
                auto& lt = last_tick[st.entity.id];
                if (up.server_tick <= lt)
                    ++order_errors;
                lt = up.server_tick;
            }
            for (const EntityRef& ref : up.removed)
            {
                auto& lt = last_tick[ref.id];
                if (up.server_tick <= lt)
                    ++order_errors;
                lt = up.server_tick;
            }
            if (v == SnapshotVerdict::Completed)
            {
                ++completed;
                const SnapshotFrame* mine = r.frame(p.snapshot_id);
                const SnapshotFrame* theirs = s.frame(p.snapshot_id);
                if (!mine || !theirs || !SameFrame(*mine, *theirs))
                    ++mismatches;
            }
        }
        SnapshotAck a;
        if (r.take_ack(a) && (!lossy || u01(rng) >= 0.2f))
            s.on_ack(a);
    }
    CHECK_MSG(mismatches == 0, "%d completed snapshots differ from the sender's frame", mismatches);
    CHECK_MSG(order_errors == 0, "%d states out of tick order", order_errors);
    // Converged: the newest complete frame holds every wanted entity at its
    // current state.
    const SnapshotFrame* last = r.frame(r.newest_complete());
    REQUIRE(last);
    CHECK(r.newest_complete() == 2020);
    int wrong = 0;
    for (const auto& e : world)
        if (e.first % 7 != 2020 / 300 % 7)
        {
            const auto it = last->find(e.first);
            if (it == last->end() || changed_components(&it->second, e.second, SnapshotTolerance{}) != 0)
                ++wrong;
        }
    CHECK_MSG(wrong == 0, "%d entities not converged", wrong);
    metric("completed", completed);
    metric("full", double(s.counters().full));
    metric("delta", double(s.counters().delta));
    metric("baseline_lost", double(s.counters().baseline_lost));
    metric("deferred", double(s.counters().deferred));
    metric("receiver_missing_baseline", double(r.counters().missing_baseline));
    metric("receiver_older", double(r.counters().older));
    metric("bytes_per_snapshot", double(bytes) / 2020.0);
    CHECK(s.counters().full < 2020 / 10); // mostly deltas
}

TEST("snapshot/ack-validation")
{
    SnapshotSender s;
    std::vector<Snapshot> parts;
    s.build(1, 0, {Creature(1, 0.f)}, parts);
    s.build(2, 0, {Creature(1, 1.f)}, parts);
    s.on_ack(SnapshotAck{9, 0, 0}); // never sent
    CHECK(s.counters().acks_invalid == 1 && s.acked() == 0);
    s.on_ack(SnapshotAck{2, 0, 0});
    s.on_ack(SnapshotAck{1, 0, 0}); // older than the acknowledged one
    CHECK(s.acked() == 2 && s.counters().acks_old == 1);
    s.reset();
    CHECK(s.acked() == 0 && s.next_baseline() == 0);
    s.build(3, 0, {Creature(1, 1.f)}, parts);
    CHECK(parts[0].snapshot_id == 1 && parts[0].baseline_id == 0); // a new connection starts over
}

TEST("interpolation/between-hold-and-gap")
{
    InterpolationBuffer b;
    EntityState a = Creature(1, 0.f), c = Creature(1, 1.f);
    CHECK(b.push(0, a, 50));
    CHECK(b.push(50, c, 50));
    CHECK(!b.push(50, a, 50) && !b.push(20, a, 50)); // not newer
    Presented p;
    REQUIRE(b.sample(25, p));
    CHECK(std::fabs(p.state.transform.x - 0.5f) < 1e-5f && !p.holding);
    REQUIRE(b.sample(80, p));
    CHECK(p.state.transform.x == 1.f && p.holding); // no extrapolation
    REQUIRE(b.sample(-10, p));
    CHECK(p.state.transform.x == 0.f && p.before_first);
    // A pause: the entity was still until shortly before its next state.
    EntityState d = Creature(1, 2.f);
    CHECK(b.push(500, d, 50));
    REQUIRE(b.sample(300, p));
    CHECK(p.state.transform.x == 1.f);
    REQUIRE(b.sample(475, p));
    CHECK(std::fabs(p.state.transform.x - 1.5f) < 1e-5f);
    // A new incarnation starts over.
    CHECK(b.push(520, Creature(1, 9.f, 2), 50));
    CHECK(b.size() == 1);
}

TEST("interpolation/angles-and-rotation")
{
    EntityState a = Item(1, 0.f), b = Item(1, 0.f);
    a.transform.yaw = 3.0f;
    b.transform.yaw = -3.0f; // 0.28 rad apart across pi
    EntityState m = interpolate(a, b, 0.5f);
    CHECK(std::fabs(std::fabs(m.transform.yaw) - 3.14159f) < 0.01f);
    // 90 degrees about y, and the same rotation's negative quaternion.
    a.qx = a.qz = 0.f;
    a.qy = 0.f;
    a.qw = 1.f;
    b.qx = b.qz = 0.f;
    b.qy = -0.70710678f;
    b.qw = -0.70710678f;
    m = interpolate(a, b, 0.5f);
    const float n = std::sqrt(m.qx * m.qx + m.qy * m.qy + m.qz * m.qz + m.qw * m.qw);
    CHECK(std::fabs(n - 1.f) < 1e-4f);
    CHECK(std::fabs(std::fabs(m.qy) - 0.38268343f) < 1e-3f); // 45 degrees, the short way
    CHECK(m.qw > 0.9f);
}

TEST("snapshot/deferred-entities-do-not-starve")
{
    // 100 creatures all moving, one part per snapshot (about 24 entries):
    // the two pinned entities go in every snapshot and every other entity at
    // least every few snapshots.
    SnapshotSenderConfig c;
    c.max_parts = 1;
    SnapshotSender s(c);
    SnapshotReceiver r;
    std::vector<EntityState> world;
    for (std::uint64_t i = 1; i <= 100; ++i)
        world.push_back(Creature(i, float(i)));
    std::map<std::uint64_t, std::uint64_t> last_sent;
    std::uint64_t worst_gap = 0;
    for (std::uint64_t t = 1; t <= 60; ++t)
    {
        for (EntityState& e : world)
            e.transform.x += 0.1f;
        std::vector<Snapshot> parts;
        s.build(t, 0, world, parts, 2);
        REQUIRE(parts.size() == 1);
        bool pinned0 = false, pinned1 = false;
        for (const SnapshotEntity& e : parts[0].entities)
        {
            pinned0 |= e.entity.id == 1;
            pinned1 |= e.entity.id == 2;
            if (t > 10)
                worst_gap = std::max(worst_gap, t - last_sent[e.entity.id]);
            last_sent[e.entity.id] = t;
        }
        CHECK(pinned0 && pinned1);
        OfferAll(r, OverWire(parts));
        Ack(r, s);
    }
    for (const EntityState& e : world)
        CHECK_MSG(60 - last_sent[e.entity.id] <= 6, "entity %llu last sent at %llu",
            (unsigned long long)e.entity.id, (unsigned long long)last_sent[e.entity.id]);
    metric("worst_gap_snapshots", double(worst_gap));
    metric("max_deferrals", double(s.counters().max_deferrals));
    CHECK(worst_gap <= 6);
}

TEST("snapshot/budget-adapts-to-congestion")
{
    SnapshotSenderConfig c;
    c.max_parts = 8;
    c.grow_after = 3;
    SnapshotSender s(c);
    CHECK(s.budget() == 8);
    s.sent(true);
    CHECK(s.budget() == 4);
    s.sent(true);
    s.sent(true);
    s.sent(true);
    CHECK(s.budget() == 1 && s.counters().min_budget == 1 && s.counters().congestions == 4);
    for (int i = 0; i < 3; ++i)
        s.sent(false);
    CHECK(s.budget() == 2);
    s.sent(false);
    s.sent(true); // a congestion restarts the count
    s.sent(false);
    s.sent(false);
    CHECK(s.budget() == 1);
    s.sent(false);
    CHECK(s.budget() == 2);
    // The budget limits the parts of a snapshot.
    std::vector<EntityState> world;
    for (std::uint64_t i = 1; i <= 200; ++i)
        world.push_back(Creature(i, float(i)));
    std::vector<Snapshot> parts;
    s.build(1, 0, world, parts);
    CHECK(parts.size() == 2);
    s.reset();
    CHECK(s.budget() == 8);
}

TEST("interpolation/server-clock-follows-drift")
{
    // The server's 20 ms ticks run 1 % slow; arrival delay jitters 5-40 ms.
    // An all-time minimum would fall 1 % behind (2 s over 200 s), so render
    // time would always be past the newest state. The windowed estimate
    // stays within the jitter of the least delay.
    ServerClock clock;
    std::mt19937 rng(3);
    std::uniform_real_distribution<double> jitter(5.0, 40.0);
    double worst = 0;
    for (int i = 0; i < 4000; ++i) // 200 s at 20 Hz
    {
        const double local_send = 1000.0 + i * 50.0 * 1.01; // the server sends every 50.5 ms of local time
        const double server_ms = i * 50.0;                  // and stamps 50 ms of server time
        clock.observe(local_send + jitter(rng), server_ms);
        if (i > 100)
        {
            // The server's true offset at this moment (local minus server time) plus the least delay (5 ms).
            const double truth = local_send - server_ms + 5.0;
            worst = std::max(worst, std::fabs(clock.offset() - truth));
        }
    }
    metric("worst_offset_error_ms", worst);
    CHECK_MSG(worst < 40.0, "offset error %.1f ms", worst);
    // A step (the server stalls 300 ms once) is followed smoothly.
    ServerClock c2;
    for (int i = 0; i < 100; ++i)
        c2.observe(i * 50.0 + 10.0, i * 50.0);
    const double before = c2.offset();
    c2.observe(100 * 50.0 + 310.0, 100 * 50.0);
    CHECK(c2.offset() - before <= 2.0 + 1e-9); // slewed, not jumped
    c2.reset();
    CHECK(!c2.ready());
}

TEST("interpolation/moving-entities-extrapolate-capped")
{
    // A creature the server updates every 600 ms while it walks at 1 m/s on x.
    InterpolationBuffer b;
    b.set_extrapolation(500);
    EntityState a = Creature(1, 0.f), c = Creature(1, 0.6f);
    a.vx = c.vx = 1.f;
    CHECK(b.push(0, a, 50));
    CHECK(b.push(600, c, 50));
    CHECK(b.size() == 2); // no hold inserted: it was moving
    Presented p;
    REQUIRE(b.sample(300, p));
    CHECK(std::fabs(p.state.transform.x - 0.3f) < 1e-4f && !p.holding);
    REQUIRE(b.sample(900, p)); // 300 ms past the newest: carried on
    CHECK(p.holding && p.extrapolated && std::fabs(p.state.transform.x - 0.9f) < 1e-4f);
    REQUIRE(b.sample(1100, p)); // capped at 500 ms
    CHECK(std::fabs(p.state.transform.x - 1.1f) < 1e-4f);
    REQUIRE(b.sample(1350, p)); // no newer state: gliding back
    CHECK(std::fabs(p.state.transform.x - 0.85f) < 1e-4f);
    REQUIRE(b.sample(5000, p)); // where the last state put it
    CHECK(p.state.transform.x == 0.6f);
    // A still entity is not carried on, and its pause is kept.
    InterpolationBuffer still;
    still.set_extrapolation(500);
    EntityState s0 = Creature(2, 0.f), s1 = Creature(2, 1.f);
    CHECK(still.push(0, s0, 50));
    CHECK(still.push(600, s1, 50));
    CHECK(still.size() == 3); // the hold at 550 ms
    REQUIRE(still.sample(900, p));
    CHECK(p.holding && !p.extrapolated && p.state.transform.x == 1.f);
    REQUIRE(still.sample(400, p));
    CHECK(p.state.transform.x == 0.f);
}

TEST("snapshot/aim-delta-and-interpolation")
{
    // v1.3: an actor's aim (torso yaw and pitch) travels apart from its body
    // yaw: turning the camera sends only the aim.
    SnapshotSender s;
    SnapshotReceiver r;
    EntityState a = Creature(1, 0.f);
    a.components = std::uint16_t(a.components | kAim);
    a.aim_yaw = 0.2f;
    std::vector<Snapshot> parts;
    s.build(1, 0, {a}, parts);
    OfferAll(r, OverWire(parts));
    Ack(r, s);
    a.aim_yaw = 0.6f; // the camera turned; the body did not
    a.aim_pitch = -0.3f;
    s.build(2, 0, {a}, parts);
    REQUIRE(parts.size() == 1 && parts[0].entities.size() == 1);
    CHECK(parts[0].entities[0].components == kAim);
    SnapshotUpdate u;
    OfferAll(r, OverWire(parts), &u);
    REQUIRE(u.states.size() == 1);
    CHECK(u.states[0].aim_yaw == 0.6f && u.states[0].transform.yaw == 0.25f);
    EntityState b = a;
    a.aim_yaw = 3.0f;
    b.aim_yaw = -3.0f;
    const EntityState m = interpolate(a, b, 0.5f);
    CHECK(std::fabs(std::fabs(m.aim_yaw) - 3.14159f) < 0.01f); // the short way round
}

TEST("snapshot/weapon-component")
{
    // v1.4: an actor's weapon state travels as one component; a shot (the
    // counter and the ammo change) sends only it.
    SnapshotSender s;
    SnapshotReceiver r;
    EntityState a = Creature(1, 0.f);
    a.components = std::uint16_t(a.components | kWeapon);
    a.weapon_slot = 2;
    a.ammo = 8;
    std::vector<Snapshot> parts;
    s.build(1, 0, {a}, parts);
    OfferAll(r, OverWire(parts));
    Ack(r, s);
    a.ammo = 7;
    a.shots = 1;
    s.build(2, 0, {a}, parts);
    REQUIRE(parts.size() == 1 && parts[0].entities.size() == 1);
    CHECK(parts[0].entities[0].components == kWeapon);
    SnapshotUpdate u;
    OfferAll(r, OverWire(parts), &u);
    REQUIRE(u.states.size() == 1);
    CHECK(u.states[0].ammo == 7 && u.states[0].shots == 1 && u.states[0].weapon_slot == 2);
    // A slot beyond the inventory's is refused on the wire.
    Snapshot bad = parts[0];
    bad.entities[0].weapon_slot = 16;
    std::vector<std::uint8_t> out;
    CHECK(!encode(Message(bad), 5, 99, out));
}

} // namespace coop::test
