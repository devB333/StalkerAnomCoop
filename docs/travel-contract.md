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

## 9. Execution: the barrier, the take, the commit, the reload (S09-T02)

Code: `CoopServerHost.cpp` "S09-T02" (`travel_execute_tick`, `travel_take`,
`travel_on_published`, `travel_before_reload`, `travel_after_reload`),
`x_ray.cpp` (the serve loop reloads the level in process),
`CoopPersistence` (`TransitionRecord`, `load_generation`). Validation:
[S09-T02](validation/S09-T02.md).

An agreed transition is carried out as the native `change_level`
(`alife_update_manager.cpp:166`) carries out a single player's: the live
state is written into the ALife entities (`ClientSend`), the actor's entity
is given the destination (graph vertex, level vertex, position, angles,
torso), the objects are saved (`ClientSave`) and the game is saved; then
the level is restarted from that save. Here it is done for both participant
actors, through the durable store, with the clients following:

| Phase | What happens | A crash here recovers |
|---|---|---|
| **Draining** (the barrier) | inputs are not applied and every action is refused (reason 30 `TravelInProgress`; the client re-sends unanswered ones later); in-flight durable work drains: a publication, deferred pickups, inventory and stack work, quest answers; 10 s at most. A party member's disconnect or death here rolls back (nothing durable changed) | the source generation; nothing durable changed |
| **Taking** | the `transition` record phase 1 `prepared` is committed (the transition id, the source generation = the rollback point, the generation the take will publish, the destination); `ClientSend`; both actors' entities get the destination (the second 0.8 m beside the first), the arrival becomes each player's entry point (the respawn point on the new level, G07 decision 2); `ClientSave`; the destination generation is taken by the ALife save itself (not `M_SAVE_GAME`, whose `prepare_objects_for_save` would write the live positions back over the moved entities); the entities are restored at once (the source level keeps running until the publication is done) | the source generation (`recovery-transition … prepared-only-rolled-back`) |
| **Publishing** | the worker publishes the generation; **its pointer swap is the commit point** | before the swap: the source generation; after it: the destination generation, the actors on the new level |
| **Committed** | the record's phase 2 `committed`; both clients are told (CampaignEvent 13 state 8) and leave their world to rejoin; the serve loop ends its tick loop | the destination generation (`committed-rolled-forward`) |
| **Reloading** | the sessions are detached from the level that goes away (the transport and the session server stay; the authority epoch does not change: the same incarnation continues); the level and its identity registry are stopped; the destination generation is loaded the way a recovery loads one (`load_generation`: its record and its save, no epoch change, no replay); the identity registry restarts and re-binds the roster from that record; the level starts, ticks, passes the identity check; the host's per-level state is rebuilt (adapters, party tasks, the published set); both clients rejoin and get the new level as a fresh checkpoint stream, their replicas reset | the destination generation |

A failed take or publication rolls back: phase 3 `rolled back` is
committed, the entities were already restored, both players are told
(Aborted, reason 8 `execution-failed`). The travel fixture's round 0 under
`travel_variant 2` carries one out; failpoints `travel:before-take` and
`travel:after-commit` are the two crash windows.

### 9.1 Not carried over

- A vehicle the actor sits in (`m_holderID`): logged, not moved.
- Online objects the players did not own: the destination level's ALife
  state is whatever the save holds (the one world; offline ALife continues
  as in single player).
- The item graph is checked unchanged across the travel (the participants'
  layout hash); the party's tasks are rebuilt from the save.

## 10. The coordinated resume (S09-T03)

Code: `CoopServerHost.cpp` "S09-T03" (`take_travel_ready`,
`travel_hold_tick`, `travel_resume`), `CoopClientLink.cpp`
(`send_travel_ready`). Validation: [S09-T03](validation/S09-T03.md).

After the reload the destination world is **held**: the barrier stays up
(each player's input for the tick is consumed and discarded, every action
but the ready is refused 30), so nobody plays against a level the partner
has not loaded. Each client, once its world is live on the destination,
sends a **ready** (ActionIntent kind 13: `u64 transition id, u8 length,
the level name it is live on`). The server accepts it only when it names
the pending transition **and** its level (else 31 `WrongDestination`), and
only once that client's world is live on the server's side too (its
checkpoint streamed and acknowledged; else 32 `NotLoaded`, and the client
sends it again 2 s later). A ready's repeat returns its stored outcome. The
hold ends when every bound player is ready (`travel-resumed why=all-ready`,
CampaignEvent 13 state 9) or at its deadline, **60 s** after the reload
(`why=timeout`, abort field 9): the present players resume and the absent
one rejoins later as a detached player would (its actor stands where it
arrived). The committed event names the destination generation.

What a client cannot do against the old level: it left that world on the
committed event (replica reset, prediction baselines gone), its inputs are
per connection (the queue starts afresh at the join) and the hold discards
them until the resume; its unanswered actions are re-sent on the new level
and judged there.

## 8. Not in S09-T01

- A live proposal is **not durable**: a crash forgets it (the players
  request again). From Agreed on, S09-T02 records it (§9).
- (S09-T02 adds the execution: §9.)
- The combat test is the NPCs' enemy memory; a player shot at by someone
  who has not selected him as an enemy, or hit by an anomaly, is not "in
  combat". Hits in the last seconds could be added if play shows the need.
- One-player travel while the partner is away: refused, pending the
  user's decision (§3).
