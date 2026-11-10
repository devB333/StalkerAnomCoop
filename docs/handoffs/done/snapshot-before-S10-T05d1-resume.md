## Complete

# Session handoff — S10-T02, S10-T03, S10-T05a…c complete; next S10-T05d

## State (2026-10-31)

- **Completed and committed: S10-T05c storyline chains**
  ([S10-T05c](../../validation/S10-T05c.md), patch 0083 `story-chains`, patch
  check 95; builds **campaign-402 / client-288**; **story-2 pass 5/5**). The
  DoctorX chain's `drx_sl_*` script variables were per actor (player 2's
  dialogue wrote a second chain on its own actor); the compat layer's
  `save_var`/`load_var` now keep them on the host actor, and each change is
  published as a checkpoint (`story_tick`). Living Legend needed nothing new
  (party infos and tasks). Fixture `--story c1,c2` with `-ServerKill`:
  steps by both players, a travel to Garbage, a restart.
  `tools/compare-coop-story.py`. A completed task leaves the task manager
  (`get_task_state` 65535). Decisions T05c-1…3.
- **Completed and committed: S10-T05a emission and psi-storm per player**
  ([S10-T05a](../../validation/S10-T05a.md), patch 0082
  `emission-per-player`, patch check 94; proto-build-55; builds
  **campaign-400 / client-286**; **emission-1 and emission-4 (late join)
  pass 6/6, emission-5 (psi-storm) 4/4**). The compat layer runs the
  installed surge update once for the world (muted, its cover answered "in
  cover") and once per player in that player's context
  (`campaign.enter_slot`), with its own stage flags, waves and the surge
  file's timers (swapped with `debug.getupvalue`/`setupvalue`); psi
  vortices come down around each player in turn and hit by each player's
  own distance and cover (`S.actor_override`). Fixture `--emission c1,c2
  [--emission-variant 1]`, `tools/compare-coop-emission.py`. S06-T08's
  **L29 closed, L28 narrowed** (ambient loops, HUD indicators → S10-T05d).
  Decisions T05a-1…5 (T05a-2: an absent player's actor stays in the
  emission). luabind: a class raises on a missing static
  (`class_rep::static_class_gettable`), so look methods up with `pcall`.
- **Completed and committed: S10-T03f new game on the campaign server**
  ([S10-T03f](../../validation/S10-T03f.md), patch 0081 `new-game`, patch
  check 93; proto-build-55; builds **campaign-397 / client-283**;
  **newgame-2 (loner, Zaton) and newgame-4 (Duty, Bar) pass 4/4**,
  newgame-3 recovers the new campaign after a server kill; **L51 closed**).
  `prepare-coop-session.py --new-game <faction>:<start location>
  [--new-game-loadout …] [--new-game-loadout2 …] [--new-game-money N]`
  writes the creator's choices into the server runtime's
  `gamedata/configs/axr_options.ltx` (the file `axr_main.config` reads, not
  app data); `tools/compare-coop-newgame.py`. The new-game script's
  `ChangeLevel` is the start move, an agreed party travel (T03f-1); player 2
  joins as the creator's faction with its own name, money and profile
  loadout (T03f-2/3). With it, **S10-T03 is complete** (all six children).
- **Completed and committed: S10-T03e passage sweep**
  ([S10-T03e](../../validation/S10-T03e.md), patch 0080; **sweep-7: all 99
  passages ok**, L50 closed; fixes for B11's cleanup crash and B15; new bugs
  B14, B16). Fixture `--sweep c1,c2 --client-exit-after 10800`, about
  71 min.
- S10-T03a…d, S10-T02a…f, S10-T04a…f, S10-T05b: completed earlier; see the
  [previous snapshot](snapshot-before-S10-T05a.md) and plan.md §I.
- **New by the user (2026-10-30): a major bug-fixing stage right before
  Stage 11** = plan.md task **S10-T07** (after S10-T06 and the G10 decision
  review, before G10 closes). Keep adding rows to the
  [bug-squashing list](../../known-issues.md) (B1…B16 now).
- **Findings that bind later work:** Anomaly's windows are the installed Lua
  UI (T04f-5/6, T04e-1); the engine does not save the faction table
  (T04d-4); NPC script logic, its condlists *and* its planner actions,
  decides against the nearest participant (T02a-1), a companion against its
  recruiter (T02f-1); a remote creature on a client runs neither its
  scheduled update nor `UpdateCL` (drive presentation from the client link,
  `present_actors`). luabind: never assign a call proxy into or call
  `type()` on a default-constructed `luabind::object`; no
  `luabind::iterator`. `campaign::server_role()` is file-local: use
  `campaign::coop_server_prepared()`. Fixtures move player 1 only (L58).
- **Hands-on by the user, pending (list for G10):** a controller attacking
  one player; each player's aura by distance; an NPC looting a body; an NPC
  greeting player 2; the Lua loot, repair and trade windows; the partner's
  weapon animations; the relations tab; each player recruiting a
  companion that follows only its recruiter; **the sleep window asking the
  partner (USE sleeps both, DROP refuses); **a guide's trip, a PDA fast
  travel and a backpack travel answered by the partner**; walking through a few
  changers by hand (S10-T03e); a new game's first quest giver and task
  (S10-T03f); an emission with one player in a shelter, a psi-storm,
  joining during an emission (S10-T05a); the DoctorX chain with each player meeting
  a honcho in turn, Living Legend's first steps (S10-T05c).
  `bash tools/start-coop-demo.sh S10-T02c 0 20 0`.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, T04d-1…4, T04e-1…3, T02a-1…2, T02b-1…2, T02c-1…3,
  T02d-1…3, T02e-1…4, T02f-1…5, T03a-1…3, T03b-1…3 (T03b-2 open), T03c-1…5, T03d-1…5, T03e-1…3, T03f-1…3, T05a-1…5, **T05c-1…3**. Open question: T05b-4 (who pays a
  task's money cost).

## Next

1. **S10-T05d presentation split** (matrix F21; L05, L06, L33, L47; bugs B4,
   B7 and L28's rest from S10-T05a): per-player news, PDA updates, HUD
   scripts (custom statics such as the emission's psi indicators),
   statistics and achievements as presentation records to the right
   client; the partner's use animation (L47); NPC movement type, body
   state and aim (B4); the partner's zoom (B7); the ambient loops
   (`sound_ambient.script`). Start from the dropped-presentation audit of
   the S06/S08 fixtures (`present_no_context`, `dropped`,
   `presentations_unknown`) to list what is still lost, then pick the
   families. It is broad: split it into children if needed and say so.
2. Then S10-T06 (co-op menu); stop at G10 with the decisions list and the
   bug list (B1…B16) for item-by-item approval; S10-T07 (bugs) follows.

## Run names

- Used: `campaign-334…402` (373, 374, 384, 385, 395 failed to compile, 381
  to link), `client-226…288`, `proto-build-52` (unused), `proto-build-53…55`,
  `st-28…30`, `sweep-1…7`, `newgame-1…4`, `emission-1…5`, `story-1…2`, patch checks 77–95; the rest in
  the snapshots.
- Next: `campaign-403`, `client-289`, `proto-build-56`, `st-31`, patch
  check 96.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T05d](snapshot-before-S10-T05d.md).
