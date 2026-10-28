// S09-T01: party travel consent state machine (coop/proto/transition.h).
#include "coop/proto/transition.h"

namespace coop::proto
{
TransitionCoordinator::TransitionCoordinator(std::uint64_t epoch, std::uint64_t consent_ms)
    : m_epoch(epoch), m_consent_ms(consent_ms)
{
}

TravelVerdict TransitionCoordinator::propose(std::uint8_t requester, std::uint8_t partner, std::uint64_t destination,
    std::uint64_t now_ms, Transition& out)
{
    if (m_live.state == TravelState::Proposed)
    {
        ++m_counters.busy;
        out = m_live;
        return TravelVerdict::Busy;
    }
    if (requester == partner)
    {
        ++m_counters.wrong_party;
        return TravelVerdict::WrongParty;
    }
    Transition t;
    t.id = (m_epoch << 32) | std::uint64_t(++m_serial);
    t.requester = requester;
    t.partner = partner;
    t.destination = destination;
    t.proposed_ms = now_ms;
    t.deadline_ms = now_ms + m_consent_ms;
    t.state = TravelState::Proposed;
    m_live = t;
    ++m_counters.proposed;
    out = t;
    return TravelVerdict::Ok;
}

TravelVerdict TransitionCoordinator::answer(std::uint8_t slot, std::uint64_t id, TravelAnswer answer,
    std::uint64_t now_ms, Transition& out)
{
    // The id first: an answer to anything but the live proposal is refused
    // whoever sends it, so a stale or replayed confirmation cannot land on a
    // later transition.
    if (m_live.state != TravelState::Proposed || id == 0 || id != m_live.id)
    {
        ++m_counters.no_transition;
        return TravelVerdict::NoTransition;
    }
    const bool requester = slot == m_live.requester;
    const bool partner = slot == m_live.partner;
    if (answer == TravelAnswer::Cancel ? !requester : !partner)
    {
        ++m_counters.wrong_party;
        return TravelVerdict::WrongParty;
    }
    switch (answer)
    {
    case TravelAnswer::Accept: end(TravelState::Agreed, TravelAbort::None, now_ms, out); break;
    case TravelAnswer::Refuse: end(TravelState::Refused, TravelAbort::None, now_ms, out); break;
    case TravelAnswer::Cancel: end(TravelState::Cancelled, TravelAbort::None, now_ms, out); break;
    default:
        ++m_counters.wrong_party;
        return TravelVerdict::WrongParty;
    }
    return TravelVerdict::Ok;
}

bool TransitionCoordinator::expire(std::uint64_t now_ms, Transition& out)
{
    if (m_live.state != TravelState::Proposed || now_ms < m_live.deadline_ms)
        return false;
    end(TravelState::Expired, TravelAbort::None, now_ms, out);
    return true;
}

bool TransitionCoordinator::abort(TravelAbort why, std::uint64_t now_ms, Transition& out)
{
    if (m_live.state != TravelState::Proposed)
        return false;
    end(TravelState::Aborted, why, now_ms, out);
    return true;
}

void TransitionCoordinator::end(TravelState state, TravelAbort why, std::uint64_t now_ms, Transition& out)
{
    m_live.state = state;
    m_live.abort = why;
    m_live.ended_ms = now_ms;
    switch (state)
    {
    case TravelState::Agreed: ++m_counters.agreed; break;
    case TravelState::Refused: ++m_counters.refused; break;
    case TravelState::Expired: ++m_counters.expired; break;
    case TravelState::Cancelled: ++m_counters.cancelled; break;
    case TravelState::Aborted: ++m_counters.aborted; break;
    default: break;
    }
    m_last = m_live;
    out = m_live;
    m_live = Transition(); // Idle; the id is never reused (the serial only grows)
}

const char* to_string(TravelState s)
{
    switch (s)
    {
    case TravelState::Idle: return "idle";
    case TravelState::Proposed: return "proposed";
    case TravelState::Agreed: return "agreed";
    case TravelState::Refused: return "refused";
    case TravelState::Expired: return "expired";
    case TravelState::Cancelled: return "cancelled";
    case TravelState::Aborted: return "aborted";
    }
    return "?";
}

const char* to_string(TravelAbort a)
{
    switch (a)
    {
    case TravelAbort::None: return "none";
    case TravelAbort::RequesterLeft: return "requester-left";
    case TravelAbort::PartnerLeft: return "partner-left";
    case TravelAbort::RequesterDead: return "requester-dead";
    case TravelAbort::PartnerDead: return "partner-dead";
    case TravelAbort::Combat: return "combat";
    case TravelAbort::ChangerGone: return "changer-gone";
    case TravelAbort::Shutdown: return "shutdown";
    }
    return "?";
}

const char* to_string(TravelVerdict v)
{
    switch (v)
    {
    case TravelVerdict::Ok: return "ok";
    case TravelVerdict::Busy: return "busy";
    case TravelVerdict::NoTransition: return "no-transition";
    case TravelVerdict::WrongParty: return "wrong-party";
    }
    return "?";
}
} // namespace coop::proto
