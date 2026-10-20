// Stateful checks: epochs, stream sequences, entity generations, resync.
#include "fixtures.h"

namespace coop::test
{
namespace
{
Header H(MessageType t, std::uint64_t epoch, std::uint64_t seq)
{
    Header h;
    h.type = std::uint16_t(t);
    h.epoch = epoch;
    h.sequence = seq;
    return h;
}
} // namespace

TEST("guard/handshake-and-epochs")
{
    StreamGuard g(Role::Server);
    // Nothing but the handshake before the session.
    CHECK(g.check(kActions, H(MessageType::ActionIntent, 5, 1)).verdict == Verdict::Violation);
    StreamGuard s(Role::Server);
    CHECK(s.check(kControl, H(MessageType::Hello, 0, 1)).verdict == Verdict::Accept);
    s.begin_session(5);
    CHECK(s.in_session() && s.epoch() == 5);
    s.begin_session(9); // only once
    CHECK(s.epoch() == 5);
    // A second Hello inside the session.
    CHECK(s.check(kControl, H(MessageType::Hello, 0, 2)).verdict == Verdict::Violation);
    StreamGuard t(Role::Server);
    t.check(kControl, H(MessageType::Hello, 0, 1));
    t.begin_session(5);
    CHECK(t.check(kControl, H(MessageType::Heartbeat, 5, 2)).verdict == Verdict::Accept);
    CHECK(t.check(kActions, H(MessageType::ActionIntent, 5, 1)).verdict == Verdict::Accept);
    // From the future: violation.
    CHECK(t.check(kActions, H(MessageType::ActionIntent, 6, 2)).verdict == Verdict::Violation);
    // After a resync to epoch 7, epoch-5 messages still in flight are dropped,
    // but their reliable sequence still counts.
    StreamGuard u(Role::Server);
    u.check(kControl, H(MessageType::Hello, 0, 1));
    u.begin_session(5);
    CHECK(!u.advance_epoch(5) && !u.advance_epoch(4));
    CHECK(u.advance_epoch(7));
    const GuardResult old = u.check(kActions, H(MessageType::ActionIntent, 5, 1));
    CHECK(old.verdict == Verdict::Drop);
    note(std::string("stale epoch: ") + old.why);
    CHECK(u.check(kActions, H(MessageType::ActionIntent, 7, 2)).verdict == Verdict::Accept);
    CHECK(u.check(kInput, H(MessageType::InputBatch, 5, 10)).verdict == Verdict::Drop);
    CHECK(u.dropped_stale_epoch == 2);
    StreamGuard cold(Role::Client);
    CHECK(!cold.advance_epoch(3)); // no session yet
}

TEST("guard/sequences")
{
    StreamGuard g(Role::Client);
    g.check(kControl, H(MessageType::Welcome, 0, 1));
    g.begin_session(5);
    // Reliable: exactly +1.
    CHECK(g.check(kLifecycle, H(MessageType::Spawn, 5, 1)).verdict == Verdict::Accept);
    CHECK(g.check(kLifecycle, H(MessageType::Spawn, 5, 2)).verdict == Verdict::Accept);
    CHECK(g.check(kLifecycle, H(MessageType::Spawn, 5, 2)).verdict == Verdict::Violation); // repeat
    StreamGuard g2(Role::Client);
    g2.check(kControl, H(MessageType::Welcome, 0, 1));
    g2.begin_session(5);
    CHECK(g2.check(kLifecycle, H(MessageType::Spawn, 5, 2)).verdict == Verdict::Violation); // gap
    // Transient: forward only, gaps fine, older or repeated dropped.
    CHECK(g.check(kSnapshot, H(MessageType::Snapshot, 5, 4)).verdict == Verdict::Accept);
    CHECK(g.check(kSnapshot, H(MessageType::Snapshot, 5, 9)).verdict == Verdict::Accept);
    CHECK(g.check(kSnapshot, H(MessageType::Snapshot, 5, 7)).verdict == Verdict::Drop);
    CHECK(g.check(kSnapshot, H(MessageType::Snapshot, 5, 9)).verdict == Verdict::Drop);
    CHECK(g.dropped_obsolete == 2);
    // Channels are independent.
    CHECK(g.check(kControl, H(MessageType::Heartbeat, 5, 2)).verdict == Verdict::Accept);
    CHECK(g.check(9, H(MessageType::Heartbeat, 5, 3)).verdict == Verdict::Violation);
    SequenceCounter c;
    CHECK(c.next(kControl) == 1 && c.next(kControl) == 2 && c.next(kInput) == 1);
}

TEST("guard/entity-generations")
{
    EntityTable t(4);
    CHECK(t.spawn({10, 1}) == EntityTable::Apply::Ok);
    CHECK(t.classify({10, 1}) == EntityTable::Ref::Current);
    CHECK(t.spawn({10, 1}) == EntityTable::Apply::Duplicate);
    CHECK(t.despawn({10, 2}) == EntityTable::Apply::NotLive); // wrong generation
    CHECK(t.despawn({10, 1}) == EntityTable::Apply::Ok);
    CHECK(t.classify({10, 1}) == EntityTable::Ref::Stale); // tombstoned
    CHECK(t.despawn({10, 1}) == EntityTable::Apply::NotLive);
    // A recycled (not newer) generation is refused; a newer one comes back.
    CHECK(t.spawn({10, 1}) == EntityTable::Apply::Recycled);
    CHECK(t.spawn({10, 2}) == EntityTable::Apply::Ok);
    CHECK(t.classify({10, 1}) == EntityTable::Ref::Stale);
    CHECK(t.classify({10, 2}) == EntityTable::Ref::Current);
    CHECK(t.classify({10, 3}) == EntityTable::Ref::Unknown); // newer, spawn not here yet
    CHECK(t.classify({11, 1}) == EntityTable::Ref::Unknown);
    CHECK(t.classify({0, 1}) == EntityTable::Ref::Invalid);
    CHECK(t.spawn({0, 1}) == EntityTable::Apply::Invalid);
    // A newer incarnation replaces a live one.
    CHECK(t.spawn({10, 5}) == EntityTable::Apply::Ok);
    CHECK(t.live() == 1 && t.classify({10, 2}) == EntityTable::Ref::Stale);
    // Capacity: live entities are never evicted; tombstones are, oldest first.
    CHECK(t.spawn({20, 1}) == EntityTable::Apply::Ok);
    CHECK(t.spawn({21, 1}) == EntityTable::Apply::Ok);
    CHECK(t.spawn({22, 1}) == EntityTable::Apply::Ok);
    CHECK(t.spawn({23, 1}) == EntityTable::Apply::Full);
    CHECK(t.despawn({20, 1}) == EntityTable::Apply::Ok);
    CHECK(t.despawn({21, 1}) == EntityTable::Apply::Ok);
    CHECK(t.tombstones() == 2);
    CHECK(t.spawn({23, 1}) == EntityTable::Apply::Ok); // evicts tombstone 20
    CHECK(t.classify({20, 1}) == EntityTable::Ref::Unknown);
    CHECK(t.classify({21, 1}) == EntityTable::Ref::Stale);
    CHECK(t.live() == 3);
    // Many cycles stay bounded.
    EntityTable big(1000);
    for (std::uint64_t i = 1; i <= 100000; ++i)
    {
        CHECK(big.spawn({i, 1}) == EntityTable::Apply::Ok);
        CHECK(big.despawn({i, 1}) == EntityTable::Apply::Ok);
    }
    CHECK(big.live() == 0 && big.tombstones() <= 1000);
    metric("tombstones_after_100k", double(big.tombstones()));
}

TEST("guard/resync")
{
    ResyncBegin b;
    b.resync_id = 3;
    b.checkpoint_id = 1;
    b.authority_epoch = 6;
    b.total_bytes = 25;
    b.chunk_bytes = 10;
    b.chunk_count = 3;
    std::vector<std::uint8_t> all(25);
    for (std::size_t i = 0; i < all.size(); ++i)
        all[i] = std::uint8_t(i * 7);
    auto chunk = [&](std::uint32_t index, std::size_t from, std::size_t n) {
        ResyncChunk c;
        c.resync_id = 3;
        c.index = index;
        c.data.assign(all.begin() + std::ptrdiff_t(from), all.begin() + std::ptrdiff_t(from + n));
        return c;
    };
    const std::uint32_t crc = crc32(all.data(), all.size());
    ResyncTracker t;
    CHECK(t.chunk(chunk(0, 0, 10)).verdict == Verdict::Violation); // before begin
    CHECK(t.begin(b).verdict == Verdict::Accept);
    CHECK(t.begin(b).verdict == Verdict::Violation);
    CHECK(t.chunk(chunk(1, 10, 10)).verdict == Verdict::Violation); // out of order
    CHECK(t.chunk(chunk(0, 0, 10)).verdict == Verdict::Accept);
    CHECK(t.chunk(chunk(1, 10, 9)).verdict == Verdict::Violation); // short middle chunk
    CHECK(t.chunk(chunk(1, 10, 10)).verdict == Verdict::Accept);
    CHECK(t.chunk(chunk(2, 20, 5)).verdict == Verdict::Accept);
    CHECK(t.end({3, 3, crc}).verdict == Verdict::Accept);
    CHECK(t.received_bytes() == 25 && !t.active());
    // Wrong checksum, incomplete, another resync's chunk.
    ResyncTracker t2;
    t2.begin(b);
    t2.chunk(chunk(0, 0, 10));
    CHECK(t2.end({3, 3, crc}).verdict == Verdict::Violation);
    ResyncTracker t3;
    t3.begin(b);
    ResyncChunk other = chunk(0, 0, 10);
    other.resync_id = 4;
    CHECK(t3.chunk(other).verdict == Verdict::Violation);
    t3.chunk(chunk(0, 0, 10));
    t3.chunk(chunk(1, 10, 10));
    t3.chunk(chunk(2, 20, 5));
    CHECK(t3.end({3, 3, crc ^ 1}).verdict == Verdict::Violation);
    CHECK(t3.chunk(chunk(3, 20, 5)).verdict == Verdict::Violation); // after the end
}

TEST("guard/fresh-inputs")
{
    InputBatch b;
    for (int i = 0; i < 4; ++i)
    {
        Input in;
        in.tick = 100 - std::uint64_t(i);
        b.inputs.push_back(in);
    }
    // Batch 20 holds inputs 20, 19, 18, 17.
    auto f = FreshInputs(b, 20, 18);
    REQUIRE(f.size() == 2);
    CHECK(f[0].first == 19 && f[0].second.tick == 99);
    CHECK(f[1].first == 20 && f[1].second.tick == 100);
    CHECK(FreshInputs(b, 20, 20).empty());
    CHECK(FreshInputs(b, 20, 0).size() == 4);
}
} // namespace coop::test
