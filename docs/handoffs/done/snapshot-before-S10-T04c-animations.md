## Complete

# Session handoff — S10-T05b complete; S10-T04c in progress (door, trade run; repair condition and hands-on left)

## State (2026-10-29, end of session)

- **Completed this session, committed:** S10-T05b every quest is a party
  quest ([S10-T05b](../../validation/S10-T05b.md), patch 0060, party-3
  pass). Decision rows T05b-3…5. L10 narrowed.
- **S10-T04c In Progress** ([S10-T04c](../../validation/S10-T04c.md)):
  patches **0063** (`use-object-money`: a client's USE on a world object →
  kind 17; the door's joint drive to every client; the purse in the needs
  event), **0064** (`trade`: kinds 18–20, reasons 37–39, journal record 14;
  the native trade window with the server's prices; the trade fixture;
  the party-task map-spot crash fix), **0065** (`remote-weapon-switch`:
  the K1 follow-up). Protocol **v1.7** (`proto-build-50`, `-51`; st-26,
  st-27 51/51). Builds **campaign-328 / client-221**. Runs: trade-1
  (server crash in the trade apply: the trader's script hooks, fixed),
  trade-2 pass, **trade-3 pass** (door, open, 33/34 refusals, buy 144,
  sell 593, two journal replays after a kill; the partner's weapon switch
  completes). Decision rows T04c-1…5 (the user chose the native window and
  the upgrades split, 2026-10-29). L01 and L36 narrowed.
- **Small patches on the way (all committed):** 0061 `demo-place-giver`
  (the `place_giver` knob, `start-coop-demo.sh`'s fifth argument; a spot
  in front of Sidorovich is inside his desk, the fixture's side spots are
  used), 0062 `party-task-map-npc-sounds` (the task's map location resent
  and applied; NPC `xr_sound` speech as sounds at the NPC's position; L05
  narrowed; L57 added: NPC walking animation slides on clients).
- **Hands-on by the user (demo-1…5 of S10-T04a/c):** the busy hint works;
  the party accept through the window works; the door works (demo-1 of
  S10-T04c); friendly fire and shooting work; **not yet seen by hand**:
  the task's map spot after 0062, Sidorovich's greeting voice after 0062,
  the trade window (0064), the partner's weapon switch (0065). Open
  visible limits: the partner's **melee swing / attack animations** are
  not played on a replica ("there is just no swinging animation"), NPCs
  slide (L57), a `sound-volume` presentation flood for sounds the clients
  lack (since S06-T08; not fixed).
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…5.

## Next

1. **Finish S10-T04c:** (a) the repaired item's condition to the client
   (a `condition` byte in the layout event 9 or a small event; the
   mechanic's repair dialogue already runs on the server); (b) the user's
   hands-on trade check with `bash tools/start-coop-demo.sh S10-T04c 0 20 0`
   (walk in, Trade, buy and sell, the partner refused busy); (c) L48 (an
   outfit with belt slots, an artefact on the belt) by a fixture step or
   by hand; (d) decide whether the fixture's door should be the bunker
   door (today it picks the nearest `ph_door`-scheme object: a cupboard
   door 1.3 m from Sidorovich; the real door is `physic_object` 15560,
   opened by hand). Then the report's status to Completed, the patch
   series stays (0063–0065 are final), end-session.
2. **The partner's attack animations** (melee swing, reload, draw,
   holster) on a replica. The user's fork read it (2026-10-29): the
   third-person torso animation is picked from the active weapon's
   `GetState()` (`ActorAnimation.cpp:517-549`: attack / reload / draw /
   holster); on a replica the other player's weapon state is forced to
   eIdle/eHidden (K1 + 0065), so the body never animates. The server
   already sends `EntityState::weapon_state` for actors (kWeapon) and
   `apply_actor_outcomes` ignores it. Plan: set the snapshot's weapon
   state on the replica's weapon as presentation (`SetState` /
   `SetNextState`, no motion, no event), return it to idle after the
   animation or the next snapshot, keep 0065's instant slot switch, never
   block `CInventory::Update`; log `remote-weapon-state`; verify with the
   combat fixture and by hand. The partner's aim yaw/pitch is separate.
3. Then S10-T04f (upgrades), S10-T04d, S10-T04e.

## Run names

- Used: `proto-build-49…51`, `st-24…27`, `campaign-306…328`,
  `client-203…221`, `talk-1…9`, `info-1`, `party-1…3`, `trade-1…3`, patch
  checks 65–75; demos `demo-1…5` (S10-T04a), `demo-1` (S10-T04c).
- Next: `campaign-329`, `client-222`, `proto-build-52`, `st-28`, patch
  check 76; session names `trade-4…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T04c repair](./snapshot-before-S10-T04c-repair.md).
