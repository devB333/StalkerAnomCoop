# Session handoff — S10-T02 complete, S10-T03a…c complete; next S10-T03d

## State (2026-10-30)

- **Completed and committed: S10-T03c world time and sleep**
  ([S10-T03c](../../validation/S10-T03c.md), patch 0078 `sleep`, patch check
  90; protocol kinds 24/25, reason 42, CampaignEvent 19, **proto-build-54**
  (st-29 115/115); builds **campaign-383 / client-272**). The client's sleep
  window asks the partner (travel consent rules); agreed, the server skips
  the world's time once and rests both; passing out
  (`ui_sleep_dialog.sleep_forced`, wrapped on the server) is that player's
  8 s blackout, no skip. **sleep-2**: all sleep checks pass; 4/5 for client
  2's teardown crash (B5). Decisions T03c-1…5. Fixture `--sleep c1,c2`,
  `tools/compare-coop-sleep.py`. New bug **B13** (client clocks step every
  second). The window button and keys await the user's hands-on.
- **Completed and committed: S10-T03b offline combat and squad targets**
  ([S10-T03b](../../validation/S10-T03b.md), patch 0077 `squad-targets`,
  patch check 89; builds **campaign-380 / client-270**; **squad-1 pass
  6/6**): a squad whose script target is "the actor" (bounty hunters, the
  measuring task) went for actor 0 (player 1); now the nearest player
  (`campaign.nearest_participant_id`, the `get_script_target` wrap). Offline
  combat has no actor reference. Decisions T03b-1…3; **T03b-2 is a gameplay
  call for the user** (bounty hunters triggered by player 1's rank and
  reputation only). Fixture `--squad c1,c2`, `tools/compare-coop-squad.py`.
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
  weapon animations; the relations tab; each player recruiting a
  companion that follows only its recruiter; **the sleep window asking the
  partner (USE sleeps both, DROP refuses)**.
  `bash tools/start-coop-demo.sh S10-T02c 0 20 0`.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, T04d-1…4, T04e-1…3, T02a-1…2, T02b-1…2, T02c-1…3,
  T02d-1…3, T02e-1…4, T02f-1…5, T03a-1…3, T03b-1…3 (T03b-2 open), **T03c-1…5**. Open question: T05b-4 (who pays a
  task's money cost).

## Next

1. **S10-T03d guides, fast travel and backpack travel as party travel**
   (matrix F10, decision T01-9: both consent, both arrive, time skipped once,
   the fee paid once by the requester; refusals as in S09; a same-level
   teleport moves both). Start from the coverage matrix F10 row and the
   installed scripts (guides' dialogues, `pda`/fast-travel markers,
   `item_backpack`/MLR teleports); reuse the S09 travel execution for a
   level change and the sleep/travel consent coordinator for the request.
   Then S10-T03e…f, S10-T05a/c/d, S10-T06; stop at G10 with the decisions
   list and the bug list; S10-T07 (bugs) follows the user's review.

## Run names

- Used: `campaign-334…383` (373, 374 failed to compile, 381 to link), `client-226…272`,
  `proto-build-52` (unused), `proto-build-53`, `st-28`, `companion-1…4`,
  `alife-1…6`, `squad-1`, `sleep-1…2`, `proto-build-54`, `st-29`, patch
  checks 77–90; the rest in the snapshots.
- Next: `campaign-384`, `client-273`, `proto-build-55`, `st-30`, patch
  check 91.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T03d](../done/snapshot-before-S10-T03d.md).
