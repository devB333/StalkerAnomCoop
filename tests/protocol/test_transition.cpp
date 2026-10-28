// S09-T01: party travel consent (coop/proto/transition.h). The property under
// test: no transition is agreed from one player's intent alone, and a
// duplicate, replayed or stale confirmation can never approve a different
// transition than the one it answered.
#include "check.h"
#include "coop/proto/transition.h"

namespace coop::test
{
using namespace coop::proto;

TEST("transition/both-accept")
{
    TransitionCoordinator c(7);
    Transition t;
    REQUIRE(c.propose(0, 1, 42, 1000, t) == TravelVerdict::Ok);
    CHECK(t.id == ((7ull << 32) | 1));
    CHECK(t.state == TravelState::Proposed && t.requester == 0 && t.partner == 1 && t.destination == 42);
    CHECK(t.deadline_ms == 1000 + kTravelConsentMs);
    REQUIRE(c.live() && c.live()->id == t.id);
    // the requester's own "accept" is not the partner's consent
    Transition out;
    CHECK(c.answer(0, t.id, TravelAnswer::Accept, 1500, out) == TravelVerdict::WrongParty);
    CHECK(c.live());
    // a third slot cannot answer
    CHECK(c.answer(2, t.id, TravelAnswer::Accept, 1500, out) == TravelVerdict::WrongParty);
    CHECK(c.answer(1, t.id, TravelAnswer::Accept, 2000, out) == TravelVerdict::Ok);
    CHECK(out.state == TravelState::Agreed && out.id == t.id && out.ended_ms == 2000);
    CHECK(!c.live());
    CHECK(c.last_ended().state == TravelState::Agreed);
    CHECK(c.counters().proposed == 1 && c.counters().agreed == 1 && c.counters().wrong_party == 2);
}

TEST("transition/one-refuses")
{
    TransitionCoordinator c(1);
    Transition t, out;
    REQUIRE(c.propose(1, 0, 5, 0, t) == TravelVerdict::Ok);
    CHECK(c.answer(0, t.id, TravelAnswer::Refuse, 10, out) == TravelVerdict::Ok);
    CHECK(out.state == TravelState::Refused && !c.live());
    // the refused transition takes no later accept
    CHECK(c.answer(0, t.id, TravelAnswer::Accept, 20, out) == TravelVerdict::NoTransition);
    CHECK(c.counters().refused == 1 && c.counters().no_transition == 1);
}

TEST("transition/timeout")
{
    TransitionCoordinator c(1, 5000);
    Transition t, out;
    REQUIRE(c.propose(0, 1, 5, 100, t) == TravelVerdict::Ok);
    CHECK(!c.expire(5099, out));
    CHECK(c.live());
    CHECK(c.expire(5100, out));
    CHECK(out.state == TravelState::Expired && out.id == t.id && !c.live());
    // a late accept approves nothing
    CHECK(c.answer(1, t.id, TravelAnswer::Accept, 5200, out) == TravelVerdict::NoTransition);
    CHECK(!c.expire(9000, out));
}

TEST("transition/cancel-and-busy")
{
    TransitionCoordinator c(1);
    Transition t, t2, out;
    REQUIRE(c.propose(0, 1, 5, 0, t) == TravelVerdict::Ok);
    // a second request while one is live: busy, the live one unchanged
    CHECK(c.propose(1, 0, 6, 1, t2) == TravelVerdict::Busy);
    CHECK(t2.id == t.id && c.live()->id == t.id);
    CHECK(c.propose(0, 1, 5, 1, t2) == TravelVerdict::Busy);
    // the partner cannot cancel; the requester can
    CHECK(c.answer(1, t.id, TravelAnswer::Cancel, 2, out) == TravelVerdict::WrongParty);
    CHECK(c.answer(0, t.id, TravelAnswer::Cancel, 3, out) == TravelVerdict::Ok);
    CHECK(out.state == TravelState::Cancelled && !c.live());
    CHECK(c.answer(1, t.id, TravelAnswer::Accept, 4, out) == TravelVerdict::NoTransition);
    // a self-proposal is refused
    CHECK(c.propose(0, 0, 5, 5, t2) == TravelVerdict::WrongParty);
    CHECK(c.counters().busy == 2 && c.counters().cancelled == 1);
}

TEST("transition/abort-on-disconnect")
{
    TransitionCoordinator c(1);
    Transition t, out;
    CHECK(!c.abort(TravelAbort::PartnerLeft, 0, out)); // nothing live
    REQUIRE(c.propose(0, 1, 5, 0, t) == TravelVerdict::Ok);
    CHECK(c.abort(TravelAbort::PartnerLeft, 10, out));
    CHECK(out.state == TravelState::Aborted && out.abort == TravelAbort::PartnerLeft && !c.live());
    // the partner's accept after its reconnect names the aborted id: refused;
    // nothing is agreed from the requester's intent alone
    CHECK(c.answer(1, t.id, TravelAnswer::Accept, 20, out) == TravelVerdict::NoTransition);
    CHECK(c.counters().aborted == 1 && c.counters().agreed == 0);
}

TEST("transition/stale-and-replayed-confirmation")
{
    TransitionCoordinator c(3);
    Transition first, second, out;
    REQUIRE(c.propose(0, 1, 5, 0, first) == TravelVerdict::Ok);
    CHECK(c.answer(1, first.id, TravelAnswer::Refuse, 1, out) == TravelVerdict::Ok);
    REQUIRE(c.propose(0, 1, 5, 2, second) == TravelVerdict::Ok);
    CHECK(second.id != first.id && second.id == ((3ull << 32) | 2));
    // the partner's accept of the first transition replayed now: it cannot
    // approve the second
    CHECK(c.answer(1, first.id, TravelAnswer::Accept, 3, out) == TravelVerdict::NoTransition);
    CHECK(c.live() && c.live()->id == second.id);
    // an unknown id, a zero id
    CHECK(c.answer(1, second.id + 1, TravelAnswer::Accept, 3, out) == TravelVerdict::NoTransition);
    CHECK(c.answer(1, 0, TravelAnswer::Accept, 3, out) == TravelVerdict::NoTransition);
    CHECK(c.live());
    // the right id agrees it; the same accept again (a duplicate delivery)
    // finds nothing live
    CHECK(c.answer(1, second.id, TravelAnswer::Accept, 4, out) == TravelVerdict::Ok);
    CHECK(out.state == TravelState::Agreed);
    CHECK(c.answer(1, second.id, TravelAnswer::Accept, 5, out) == TravelVerdict::NoTransition);
    CHECK(c.counters().agreed == 1);
}

TEST("transition/ids-never-repeat-across-epochs")
{
    // a restart: a new epoch's coordinator starts a new serial; an id from
    // the old epoch is unknown to it (its epoch bits differ)
    TransitionCoordinator old_epoch(9), new_epoch(10);
    Transition t, out;
    REQUIRE(old_epoch.propose(0, 1, 5, 0, t) == TravelVerdict::Ok);
    CHECK(new_epoch.answer(1, t.id, TravelAnswer::Accept, 1, out) == TravelVerdict::NoTransition);
    Transition n;
    REQUIRE(new_epoch.propose(0, 1, 5, 2, n) == TravelVerdict::Ok);
    CHECK(n.id != t.id && (n.id >> 32) == 10 && (n.id & 0xffffffffull) == 1);
    CHECK(new_epoch.answer(1, t.id, TravelAnswer::Accept, 3, out) == TravelVerdict::NoTransition);
    CHECK(new_epoch.live() && new_epoch.live()->id == n.id);
}
} // namespace coop::test
