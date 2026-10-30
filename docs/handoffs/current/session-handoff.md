# Session handoff — S10-T02 complete, S10-T03a complete; next S10-T03b

## State (2026-10-30)

- **Completed and committed: S10-T03a ALife switching and respawn**
  ([S10-T03a](../../validation/S10-T03a.md), patch 0076 `alife-respawn`,
  patch check 88; builds **campaign-379 / client-269**). The installed
  respawn guard (`se_smart_terrain.try_respawn`, no respawn within 150 m of
  "the actor") and a squad's `dist_to_actor` measured actor 0 only, so a
  squad could respawn beside player 2; both now measure the nearest player
  (compat-layer wraps, `campaign.nearest_participant_distance`). Fixture
  `--alife c1,c2` (`alife_party`) + `tools/compare-coop-alife.py`
  (`--variant restart|travel`): **alife-4 and alife-6 pass 9/9**, **alife-5**
  (travel to Darkscape) all ALife checks pass, 8/9 only for client 1's known
  teardown crash (B5). `compare-coop-world.py` fixed (reparents, a killed
  server's cut row, a travel's reload). Decisions T03a-1…3.
- **Completed and committed: S10-T02f companions per recruiter**
  ([S10-T02f](../../validation/S10-T02f.md), patch 0075 `companions`, patch
  check 87; builds **campaign-372 / client-264**; **companion-4 pass 6/6**:
  player 2's companion walked 103 m to player 2 in 27 s past player 1, the
  other player is refused its command dialogue, and it passed every check
  after a server kill and restart). The cause of companion-1…3's failure: a
  stalker's scheme actions (`axr_beh`'s walk) run from its planner
  (`CAI_Stalker::Think`), outside the S10-T02a evaluation scope; the scope
  now covers the stalker's and the monster's planner too (T02a-1's wording
  corrected in the decisions file). With it, **S10-T02 is complete**.
- **New by the user (2026-10-30): a major bug-fixing stage right before
  Stage 11** = plan.md task **S10-T07** (after S10-T06 and the G10 decision
  review, before G10 closes). Keep adding rows to the
  [bug-squashing list](../../known-issues.md) (B1…B12 now; B12 the companion
  hotkeys and wheel, T02f-5).
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
  weapon animations; the relations tab; **each player recruiting a
  companion that follows only its recruiter**.
  `bash tools/start-coop-demo.sh S10-T02c 0 20 0`.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, T04d-1…4, T04e-1…3, T02a-1…2, T02b-1…2, T02c-1…3,
  T02d-1…3, T02e-1…4, T02f-1…5, **T03a-1…3**. Open question: T05b-4 (who pays a
  task's money cost).

## Next

1. **S10-T03b offline combat and squad targets** (matrix F8). Lead: a
   squad's script target `"actor"` returns 0 (`sim_squad_scripted:get_script_target`,
   `sim_squad_scripted.script:128`), actor 0, player 1; the companion wrap
   (S10-T02f) already intercepts `t == 0` for companion squads; extend it so
   any other squad's 0 becomes the nearest participant's actor id, then check
   `specific_update` / `simulation_objects.get_server_entity` accept player
   2's actor id (is the second actor in the sim object registry?). Offline
   combat (`sim_offline_combat.script`) has no actor reference. Fixture: a
   squad sent at "the actor" near player 2 (player 1 far, L58) must reach
   player 2. Then S10-T03c…f, S10-T05a/c/d, S10-T06; stop at G10 with the
   decisions list and the bug list; S10-T07 (bugs) follows the user's review.

## Run names

- Used: `campaign-334…379` (373, 374 failed to compile), `client-226…269`,
  `proto-build-52` (unused), `proto-build-53`, `st-28`, `companion-1…4`,
  `alife-1…6`, patch checks 77–88; the rest in the snapshots.
- Next: `campaign-380`, `client-270`, `proto-build-54`, `st-29`, patch
  check 89.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T03b](../done/snapshot-before-S10-T03b.md).
