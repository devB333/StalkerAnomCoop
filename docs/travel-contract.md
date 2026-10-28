# Party travel contract (Stage 9)

How two players move the one online world to another level together.
S09-T01 (2026-10-27) defines and implements the **request and the two-player
consent**; the barrier, the native level migration and the clients' loads
are S09-T02…T04 and extend this document. Decision D5 (plan.md §B) binds:
the server validates a request, notifies both players and requires both
confirmations; no automatic or unconfirmed teleport; one detailed online
simulation.

Code: `protocol/include/coop/proto/transition.h` (the consent state
machine, unit-tested in `tests/protocol/test_transition.cpp`),
`CoopServerHost.cpp` "S09-T01" (the engine side, the fixture),
`CoopClientLink.cpp` "S09-T01" (the prompts, the keys, the step driver),
`level_changer.cpp` (the touch on a client). Validation:
[S09-T01](validation/S09-T01.md).

## 1. Transition identity

A transition id is `(authority_epoch << 32) | serial`, a `u64`. The serial
counts proposals in one server incarnation from 1; the epoch is the one the
durable store allocated at recovery (S07-T04). So no two transitions ever
share an id, and a restarted server never continues a transition from
before its crash: its answers meet "no transition" (§4) and the players
request afresh. Zero is never a transition id.

## 2. States

One transition at a time (a party of two travels together).

| State | Meaning | Leaves it |
|---|---|---|
| Idle | no travel in progress | a validated request → Proposed |
| Proposed | the requester consented by asking; the partner's answer is awaited, for **30 s** (`kTravelConsentMs`) | the partner's accept → Agreed; refuse → Refused; the requester's cancel → Cancelled; the deadline → Expired; the world (§5) → Aborted |
| Agreed | both consented | the executor (S09-T02: the barrier and the migration). **In S09-T01 nothing follows:** the server logs `travel-agreed … executor=none`, tells both players, and returns to Idle |
| Refused, Expired, Cancelled, Aborted | ends; both players are told why | → Idle |

Every end is reported to both players (CampaignEvent 13 with the end state
and, for Aborted, the reason). A player entering the world while a proposal
is live receives it.

## 3. The request (ActionIntent kind 11)

Sent by the client when its player, standing in a level changer, confirms
the prompt (USE); the target is the changer's entity. The server, in the
tick it receives it:

| Check | Refusal (`ActionReason`) |
|---|---|
| a known operation id | its stored outcome (an accepted request's repeat returns the same transition id; refusals are not stored, so a later request is re-evaluated) |
| the session is attached and the own actor alive | 3 NoActor |
| params empty | 2 Malformed |
| no proposal is live | 23 TravelBusy |
| the target is an enabled `CLevelChanger` online on the server | 24 NoChanger |
| the actor is inside the changer's shape (the engine's own touch test, `CCF_Shape::Contact`) or within 1 m of its bounding sphere (the server's actor trails the client's) | 25 NotAtChanger |
| the partner (the other roster slot) is in the world (its client sends input) with a living actor | 26 PartnerUnavailable |
| no alive NPC or monster online has either party actor as its selected enemy (`CMemoryManager::enemy().selected()`) | 27 InCombat |

Accepted: the proposal is created (its id in `ActionResult.revision`),
remembered under the operation id, and sent to both players with the
destination (the changer's level and arrival position) and the deadline.

**No partner, no travel.** A request while the partner is disconnected or
dead is refused (26). This is the conservative reading of D5; letting one
player travel alone when the other is away would be a one-player consent
and needs the user's approval (plan.md R10).

## 4. The answer (ActionIntent kind 12)

Params: `u64 transition id, u8 answer` (1 accept, 2 refuse, 3 cancel).

| Check | Refusal |
|---|---|
| a known operation id | its stored outcome |
| the session attached | 3 |
| params of 9 bytes, answer 1–3 | 2 |
| **the id is the live proposal's** | 28 NoTransition — a stale, replayed, unknown or pre-restart id approves nothing, whoever sends it |
| accept or refuse from the partner; cancel from the requester | 29 WrongParty (the requester's own accept is not the partner's consent) |
| accept: the partner's actor alive | 3 |
| accept: the party out of combat now | 27 (the proposal stands; the tick's check may abort it) |

Accepted: the transition ends (Agreed, Refused or Cancelled), both players
are told, the answer is remembered under its operation id (a duplicate
delivery returns the same outcome and does nothing).

The acceptance rule of S09-T01, in these terms: a transition reaches Agreed
only through an accept of its own id from the partner of its own proposal,
while it is live; every other path is a refusal with no effect.

## 5. What the world does to a live proposal

Checked every half second (and at a disconnect at once):

| Event | End |
|---|---|
| the requester's or the partner's session ends (`on_left`) | Aborted: requester-left / partner-left. A rejoin starts over: the consent does not outlive the session |
| the requester's or the partner's actor dies | Aborted: requester-dead / partner-dead |
| an NPC or monster takes a party actor as its enemy | Aborted: combat |
| the changer leaves the online world or is disabled | Aborted: changer-gone |
| the 30 s pass | Expired |

A disconnect never substitutes the remaining player's consent for the
absent one's (R10).

## 6. Presentation (the client)

- Touching a level changer (the engine's `feel_touch_new`, refreshed every
  5 s while inside, silent changers included on a client) shows: *"Travel
  to <level>? USE asks your partner to come along, DROP stays."* for 6 s.
  USE sends the request; DROP declines and silences that changer for 20 s.
  The native change-level path stays refused on clients and on the server
  (L14; S06-T05).
- The proposal: the requester sees *"Waiting for Player N to agree to
  travel to <level> (30 s). DROP cancels."* (refreshed every 5 s with the
  time left); the partner sees *"Player N wants to travel to <level>. USE to
  go along, DROP to refuse."* The keys send kind 12.
- Every end shows one line (agreed, refused, expired, cancelled, aborted
  with its reason). In S09-T01 "agreed" adds that the level change itself is
  not in this build.
- A refusal of the player's own request or answer shows its reason in
  plain words (`travel_result`).
- Requests and answers ride the pending-request queue: an unanswered one
  is re-sent every 3 s with the same operation id (the server's stored
  outcome makes the repeat harmless).
- A new connection (Welcome) voids any proposal the client held: the
  server aborted it when the session ended.

## 7. Wire

- ActionIntent kinds 11 and 12, reasons 23–29
  ([protocol](network-protocol.md#messages-and-limits)).
- CampaignEvent kind 13 `transition`: `u64 id, u8 state, u8 requester
  slot, u8 partner slot, u8 abort reason, u64 changer entity, str level,
  f32 x, y, z (the arrival position), u32 remaining ms`.

## 8. Not in S09-T01

- The transition is **not durable**: nothing is journaled before Agreed;
  a crash forgets a live proposal (the players request again). S09-T02
  records the transition and the source recovery generation durably at
  the barrier, where loss would matter.
- No load, no barrier, no mutation ordering: Agreed is the hand-over
  point for S09-T02.
- The combat test is the NPCs' enemy memory; a player shot at by someone
  who has not selected him as an enemy, or hit by an anomaly, is not "in
  combat". Hits in the last seconds could be added if play shows the need.
- One-player travel while the partner is away: refused, pending the
  user's decision (§3).
