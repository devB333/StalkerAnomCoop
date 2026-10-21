// S06-T01: input sequencing and validation (coop/proto/input_queue.h). The
// property under test: an input can advance the actor for at most one server
// tick however it is repeated, reordered, replayed or time-shifted.
#include "check.h"
#include "coop/proto/input_queue.h"

#include <cmath>
#include <limits>

namespace coop::test
{
using namespace coop::proto;

namespace
{
Input In(std::uint64_t tick, float move_y = 1.f)
{
    Input in;
    in.tick = tick;
    in.move_y = move_y;
    return in;
}

InputBatch Batch(std::uint64_t newest, std::size_t count)
{
    InputBatch b;
    for (std::size_t i = 0; i < count && newest > i; ++i)
        b.inputs.push_back(In(newest - i));
    return b;
}
} // namespace

TEST("input-queue/order-once-hold")
{
    InputQueueConfig c;
    c.hold_ticks = 3;
    InputQueue q(c);
    for (std::uint64_t t = 1; t <= 4; ++t)
        CHECK(q.offer(In(t)) == InputVerdict::Queued);
    Input out;
    for (std::uint64_t t = 1; t <= 4; ++t)
    {
        CHECK(q.consume(out) == InputSource::Queued);
        CHECK(out.tick == t);
        CHECK(q.last_consumed_tick() == t);
    }
    // Empty: the last input is held for hold_ticks ticks, then nothing.
    for (int i = 0; i < 3; ++i)
    {
        CHECK(q.consume(out) == InputSource::Held);
        CHECK(out.tick == 4);
    }
    CHECK(q.consume(out) == InputSource::None);
    CHECK(q.consume(out) == InputSource::None);
    CHECK(q.counters().consumed == 4 && q.counters().held == 3 && q.counters().idle == 2);
    // A new input ends the idle and restarts the hold budget.
    CHECK(q.offer(In(5)) == InputVerdict::Queued);
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 5);
    CHECK(q.consume(out) == InputSource::Held);
}

TEST("input-queue/duplicates-and-stale")
{
    InputQueue q;
    CHECK(q.offer(In(3)) == InputVerdict::Queued);
    CHECK(q.offer(In(3)) == InputVerdict::Stale); // a repeat
    CHECK(q.offer(In(1)) == InputVerdict::Queued); // late but not yet consumed
    Input out;
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 1);
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 3);
    CHECK(q.offer(In(2)) == InputVerdict::Stale);  // older than the last consumed
    CHECK(q.offer(In(3)) == InputVerdict::Stale);  // the one just consumed
    CHECK(q.consume(out) == InputSource::Held);    // nothing new was taken
    CHECK(q.counters().consumed == 2 && q.counters().stale == 3);
}

TEST("input-queue/redundant-batches-consume-each-tick-once")
{
    // Batches of 8 sent for ticks 1..40; every batch repeats the previous 7.
    InputQueue q;
    std::uint64_t ticks_seen = 0, consumed = 0;
    Input out;
    for (std::uint64_t newest = 1; newest <= 40; ++newest)
    {
        q.offer_batch(Batch(newest, 8));
        if (q.consume(out) == InputSource::Queued)
        {
            ++consumed;
            CHECK(out.tick > ticks_seen);
            ticks_seen = out.tick;
        }
    }
    CHECK(consumed == 40);
    CHECK(q.counters().queued == 40);
    CHECK(q.counters().stale == 40 * 8 - 40 - 28); // every repeat; the first 7 batches are shorter
    CHECK(q.counters().dropped_oldest == 0);
}

TEST("input-queue/flood-cannot-double-advance")
{
    // Four inputs per server tick (a 200 Hz client, or one replaying its own
    // stream): one is consumed per tick, the rest are dropped as the backlog
    // overflows. The actor advances by ticks, not by inputs.
    InputQueueConfig c;
    c.capacity = 5;
    InputQueue q(c);
    Input out;
    std::uint64_t tick = 0, consumed = 0;
    for (int server_tick = 0; server_tick < 100; ++server_tick)
    {
        for (int k = 0; k < 4; ++k)
            q.offer(In(++tick));
        if (q.consume(out) == InputSource::Queued)
            ++consumed;
    }
    CHECK(consumed == 100);
    CHECK(q.counters().queued == 400);
    // Each tick ends with four queued: the fifth slot went to the consumed one.
    CHECK(q.counters().dropped_oldest == 400 - 100 - 4);
    CHECK(q.depth() == 4);
    // Replaying the whole stream again does nothing.
    const auto before = q.counters().queued;
    for (std::uint64_t t = 1; t <= tick; ++t)
        CHECK(q.offer(In(t)) == InputVerdict::Stale);
    CHECK(q.counters().queued == before);
}

TEST("input-queue/capacity-drops-oldest")
{
    InputQueueConfig c;
    c.capacity = 3;
    InputQueue q(c);
    for (std::uint64_t t = 1; t <= 7; ++t)
        q.offer(In(t));
    CHECK(q.depth() == 3 && q.counters().dropped_oldest == 4);
    Input out;
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 5);
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 6);
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 7);
    // The dropped ticks are behind the last consumed one for good.
    CHECK(q.offer(In(2)) == InputVerdict::Stale);
}

TEST("input-queue/ahead-and-out-of-order")
{
    InputQueueConfig c;
    c.max_gap = 250;
    c.capacity = 8;
    InputQueue q(c);
    CHECK(q.offer(In(251)) == InputVerdict::Ahead); // nothing accepted yet: 0 + 250 < 251
    CHECK(q.offer(In(1)) == InputVerdict::Queued);
    CHECK(q.offer(In(1 + 250)) == InputVerdict::Queued); // newest is now 251
    CHECK(q.offer(In(251 + 251)) == InputVerdict::Ahead); // beyond the newest + gap
    CHECK(q.offer(In(100000)) == InputVerdict::Ahead);
    CHECK(q.offer(In(2)) == InputVerdict::Queued);      // the normal stream goes on
    Input out;
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 1);
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 2);
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 251);
    CHECK(q.counters().ahead == 3);
    // Out of order arrival (a transient channel): consumed in tick order.
    InputQueue r;
    r.offer(In(12));
    r.offer(In(10));
    r.offer(In(11));
    CHECK(r.consume(out) == InputSource::Queued && out.tick == 10);
    CHECK(r.consume(out) == InputSource::Queued && out.tick == 11);
    CHECK(r.consume(out) == InputSource::Queued && out.tick == 12);
}

TEST("input-queue/invalid-fields")
{
    InputQueue q;
    const char* why = "";
    Input in = In(1);
    in.move_x = std::numeric_limits<float>::quiet_NaN();
    CHECK(q.offer(in, &why) == InputVerdict::Invalid);
    in = In(1);
    in.move_y = 1.5f;
    CHECK(q.offer(in, &why) == InputVerdict::Invalid);
    in = In(1);
    in.pitch = 1.7f;
    CHECK(q.offer(in, &why) == InputVerdict::Invalid);
    in = In(1);
    in.yaw = 4.5f;
    CHECK(q.offer(in, &why) == InputVerdict::Invalid);
    in = In(1);
    in.stance = Stance::Prone;
    CHECK(q.offer(in, &why) == InputVerdict::Invalid);
    in = In(1);
    in.buttons = 0x10000;
    CHECK(q.offer(in, &why) == InputVerdict::Invalid);
    in = In(0);
    CHECK(q.offer(in, &why) == InputVerdict::Invalid);
    CHECK(q.counters().invalid == 7 && q.counters().queued == 0 && q.depth() == 0);
    in = In(1);
    in.stance = Stance::Sprint;
    in.pitch = -1.5f;
    in.buttons = 1;
    CHECK(q.offer(in, &why) == InputVerdict::Queued);
    // An invalid input is not applied: nothing was consumed before it.
    Input out;
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 1);
}

TEST("input-queue/reset-restarts-the-sequence")
{
    InputQueue q;
    for (std::uint64_t t = 1; t <= 3; ++t)
        q.offer(In(t));
    Input out;
    q.consume(out);
    q.consume(out);
    CHECK(q.last_consumed_tick() == 2 && q.depth() == 1);
    q.reset(); // a new connection: ticks start at 1 again
    CHECK(q.last_consumed_tick() == 0 && q.depth() == 0 && !q.has_last());
    CHECK(q.consume(out) == InputSource::None); // nothing held across connections
    CHECK(q.offer(In(1)) == InputVerdict::Queued);
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 1);
    CHECK(q.counters().consumed == 3); // counters span the link
}
TEST("input-queue/processed-tick-covers-refused-inputs")
{
    // S06-T03: inputs the server refuses as invalid are dealt with too, once
    // nothing older is still queued; ahead and stale ones are not.
    InputQueue q;
    Input out;
    CHECK(q.offer(In(1)) == InputVerdict::Queued);
    CHECK(q.processed_tick() == 0);
    CHECK(q.consume(out) == InputSource::Queued);
    CHECK(q.processed_tick() == 1);
    Input bad = In(2);
    bad.stance = Stance::Prone;
    CHECK(q.offer(bad) == InputVerdict::Invalid);
    CHECK(q.processed_tick() == 2); // nothing queued: tick 2 is dealt with
    CHECK(q.offer(In(4)) == InputVerdict::Queued);
    bad.tick = 5;
    CHECK(q.offer(bad) == InputVerdict::Invalid);
    CHECK(q.processed_tick() == 1); // tick 4 is still waiting
    CHECK(q.consume(out) == InputSource::Queued && out.tick == 4);
    CHECK(q.processed_tick() == 5);
    bad.tick = 5 + 100000; // a jump: not dealt with
    CHECK(q.offer(bad) == InputVerdict::Invalid);
    CHECK(q.processed_tick() == 5);
    q.reset();
    CHECK(q.processed_tick() == 0);
}

} // namespace coop::test
