# Session handoff — S10-T02 and S10-T03 complete; next S10-T05a

## State (2026-10-31)

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
  [previous snapshot](../done/snapshot-before-S10-T05a.md) and plan.md §I.
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
  (S10-T03f).
  `bash tools/start-coop-demo.sh S10-T02c 0 20 0`.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, T04d-1…4, T04e-1…3, T02a-1…2, T02b-1…2, T02c-1…3,
  T02d-1…3, T02e-1…4, T02f-1…5, T03a-1…3, T03b-1…3 (T03b-2 open), T03c-1…5, T03d-1…5, T03e-1…3, **T03f-1…3**. Open question: T05b-4 (who pays a
  task's money cost).

## Next

1. **S10-T05a emission and psi-storm gameplay per participant** (matrix
   F18, carried limits L28/L29 of S06-T08, T01-11: one schedule for the
   world; cover, damage, immunity and the death camera per player). Read
   the installed `surge_manager.script` and `psi_storm_manager.script`
   (extracted scripts: re-extract `scripts.db0` into the scratchpad if
   needed) and the S06-T08 report for what already runs on the server. Plan:
   run the cover check, the hit or kill and `surge_immuned` for each
   participant (in its script context), send each client its own
   presentation (wave, death camera, sounds), and make a late join see the
   right phase. Fixture: an emission with the players apart, one in cover;
   a psi-storm.
2. Then S10-T05c (storylines), S10-T05d (presentation split; B4/B7),
   S10-T06 (co-op menu); stop at G10 with the decisions list and the bug
   list (B1…B16) for item-by-item approval; S10-T07 (bugs) follows.

## Run names

- Used: `campaign-334…397` (373, 374, 384, 385, 395 failed to compile, 381
  to link), `client-226…283`, `proto-build-52` (unused), `proto-build-53…55`,
  `st-28…30`, `sweep-1…7`, `newgame-1…4`, patch checks 77–93; the rest in
  the snapshots.
- Next: `campaign-398`, `client-284`, `proto-build-56`, `st-31`, patch
  check 94.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T05a](../done/snapshot-before-S10-T05a.md).
