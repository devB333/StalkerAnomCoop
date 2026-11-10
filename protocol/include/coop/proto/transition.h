// S09-T01: party travel consent (docs/travel-contract.md). The pure state
// machine behind a level transition's request and two-player consent; no
// engine or transport types. The server host wraps it: it resolves the
// level changer, the players' positions, presence and combat state, and
// turns verdicts into ActionResults and CampaignEvents (kind 13).
//
// One transition at a time (a party of two travels together). Its id is
// (authority epoch << 32) | serial: a server incarnation never repeats an id
// and a restart (a new epoch) never continues one. An answer names the id it
// answers; anything but the live proposal's id is refused, so a duplicate,
// replayed or stale confirmation cannot approve a different transition.
//
//   Idle --propose--> Proposed --accept--> Agreed (an end: the executor's, S09-T02)
//                       |--refuse--> Refused    |--cancel--> Cancelled
//                       |--expire--> Expired    |--abort---> Aborted (a party left, died, fought; the changer went)
//   every end returns to Idle; the ended transition is remembered (last_ended) for the clients' presentation
#pragma once

#include <cstdint>

namespace coop::proto
{
constexpr std::uint64_t kTravelConsentMs = 30000; // the partner's time to answer

enum class TravelState : std::uint8_t
{
    Idle = 0,
    Proposed = 1, // the requester consented by asking; the partner's answer is awaited
    Agreed = 2,   // both consented (an end state for this coordinator)
    Refused = 3,
    Expired = 4,
    Cancelled = 5, // by the requester
    Aborted = 6,   // by the world: see TravelAbort
    // 8 is used on the wire only (CampaignEvent 13): S09-T02 `committed`: the destination generation is
    // published and both clients rejoin on the new level; 9 (S09-T03) `resumed`: the hold on the destination
    // level ended (abort field 9: at its deadline); 10 (S09-T04) `held`: sent to a client whose world went live
    // during the hold without having been told the transition (it rejoined or restarted), so it reports ready
};

enum class TravelAnswer : std::uint8_t
{
    Accept = 1, // the partner
    Refuse = 2, // the partner
    Cancel = 3, // the requester
};

enum class TravelAbort : std::uint8_t
{
    None = 0,
    RequesterLeft = 1,
    PartnerLeft = 2,
    RequesterDead = 3,
    PartnerDead = 4,
    Combat = 5,
    ChangerGone = 6,
    Shutdown = 7,
    ExecutionFailed = 8, // S09-T02: the take or the publication of the destination failed; rolled back
};

// Why a request or an answer was refused (0: accepted). The host maps them
// to ActionReason values (messages.h).
enum class TravelVerdict : std::uint8_t
{
    Ok = 0,
    Busy = 1,         // a transition is live already (cancel or finish it first)
    NoTransition = 2, // the id names no live proposal (stale, replayed, unknown, or after a restart)
    WrongParty = 3,   // accept/refuse from the requester, or cancel from the partner, or a third slot
};

struct Transition
{
    std::uint64_t id = 0;
    std::uint8_t requester = 0;
    std::uint8_t partner = 0;
    std::uint64_t destination = 0; // host-defined key (the level changer's entity id)
    std::uint64_t proposed_ms = 0;
    std::uint64_t deadline_ms = 0;
    TravelState state = TravelState::Idle;
    TravelAbort abort = TravelAbort::None;
    std::uint64_t ended_ms = 0;
};

struct TravelCounters
{
    std::uint64_t proposed = 0, agreed = 0, refused = 0, expired = 0, cancelled = 0, aborted = 0;
    std::uint64_t busy = 0, no_transition = 0, wrong_party = 0;
};

class TransitionCoordinator
{
public:
    explicit TransitionCoordinator(std::uint64_t epoch, std::uint64_t consent_ms = kTravelConsentMs);

    // A request from `requester` to travel with `partner`. Ok: `live()` is
    // the new proposal (its id in out.id). Busy: one is live already.
    TravelVerdict propose(std::uint8_t requester, std::uint8_t partner, std::uint64_t destination,
        std::uint64_t now_ms, Transition& out);
    // An answer to transition `id` from `slot`. Ok: the transition ended
    // (Agreed, Refused or Cancelled; `out` is its final record).
    TravelVerdict answer(std::uint8_t slot, std::uint64_t id, TravelAnswer answer, std::uint64_t now_ms,
        Transition& out);
    // The live proposal past its deadline ends Expired (true, `out`).
    bool expire(std::uint64_t now_ms, Transition& out);
    // The world ends the live proposal (true, `out`); nothing when none is live.
    bool abort(TravelAbort why, std::uint64_t now_ms, Transition& out);

    const Transition* live() const { return m_live.state == TravelState::Proposed ? &m_live : nullptr; }
    const Transition& last_ended() const { return m_last; }
    std::uint64_t epoch() const { return m_epoch; }
    std::uint64_t serial() const { return m_serial; }
    const TravelCounters& counters() const { return m_counters; }

private:
    void end(TravelState state, TravelAbort why, std::uint64_t now_ms, Transition& out);

    std::uint64_t m_epoch;
    std::uint64_t m_consent_ms;
    std::uint32_t m_serial = 0;
    Transition m_live;
    Transition m_last;
    TravelCounters m_counters;
};

const char* to_string(TravelState s);
const char* to_string(TravelAbort a);
const char* to_string(TravelVerdict v);
} // namespace coop::proto
