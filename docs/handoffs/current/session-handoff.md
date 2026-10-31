# Session handoff — S10-T05d in progress (child d1, presentation traffic)

## State (2026-10-31, end of session)

- **In progress, not committed: S10-T05d1 presentation traffic.** The
  engine edits are in `builds/build_1/engine-work` only (**not exported**,
  no patch 0084 yet; `git -C builds/build_1/engine-work diff` shows them in
  `script_sound.{h,cpp}`, `script_sound_inline.h`, `level_script.cpp`,
  `CoopServerHost.cpp`). What they do: a script sound's unchanged volume is
  not sent, nor a volume or position for a sound that stopped; the routed
  **looped sounds already playing are played to each client entering the
  world** (`coop_script_sound_replay`, `loops-joined`; the level's ambient
  loops start before anyone joins, so nobody heard them); a post-process
  remove or factor and a camera-effector remove for an effector that is
  not on are not sent (fixes **B14**); counters in `presentation-stop`.
  Tool `tools/compare-coop-presentation.py <run> --baseline <run>`
  (committed).
  - Found by the audit: a 9-minute run sent **~243,000 presentation
    records** (≈250/s per client, almost all `sound-volume` for sounds the
    client never had) and evicted 14,481 from the outboxes
    (`run-emission-4`). **presentation-1** (campaign-403 / client-289) and
    **presentation-2** (campaign-404 / client-290): **1,663–1,677 records,
    10 evicted**, loops replayed at every join (4–6 each, applied), the
    emission checks 6/6 (presentation-2 exits 0). **Still failing the
    `orphans` check:** ~670 `sound-volume … absent` per client, nearly all
    one handle (251: a broadcast, non-looped sound played before anyone
    joined, its volume set for six minutes). The `_feedback()` test
    (presentation-2) did not catch it, so the server still counts it as
    playing. Next: find what sound 251 is (log the name at a routed play
    whose handle later gets volumes with no client copy), then either
    replay non-looped sounds still playing at a join or drop volumes for a
    handle no client has.
- **Plan for the rest of S10-T05d (split, record it in plan.md §I):**
  **d2** HUD custom statics: route `CUIGameCustom::AddCustomStatic` /
  `RemoveCustomStatic` on the server **only inside a player's context**
  (e.g. each player's emission psi indicators), the text set right after an
  add by a tick poll (new presentation kinds after `ControllerPsyHit`;
  client applies with `CurrentGameUI()->AddCustomStatic`); statics added
  outside a context come from actor 0's binder (`actor_effects` helmet
  overlay, vignette) and are dropped: actor 2 has no binder, so record it as
  a limit. Statistics and achievements: the party's (one
  `alife_storage_manager` table; the clients' PDA shows the copy from
  their join): a decision plus a limit. **d3** body presentation (B4 NPC
  movement, B7 zoom, L47 use animation): the plan allows recording them as
  not shown; B4/B7 are already on the S10-T07 bug list.
- **Completed and committed this session:** S10-T03e (sweep, patch 0080,
  L50 closed), S10-T03f (new game, 0081, L51 closed; S10-T03 complete),
  S10-T05a (emission per player, 0082, L29 closed, L28 narrowed),
  S10-T05c (DoctorX chain party-wide, 0083). Reports in `docs/validation/`.
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

1. Finish **S10-T05d1** (above): the handle-251 volumes, a passing run,
   export patch 0084 (`python3 tools/export-engine-patch.py
   0084-presentation-traffic.patch 96`), write `docs/validation/S10-T05d.md`
   (d1 part), update B14 in `docs/known-issues.md`, commit.
2. **S10-T05d2** HUD statics, **d3** record B4/B7/L47; then S10-T06 (co-op
   menu); stop at G10 with the decisions list and the bug list (B1…B16)
   for item-by-item approval; S10-T07 (bugs) follows.

## Run names

- Used: `campaign-334…404` (373, 374, 384, 385, 395 failed to compile, 381
  to link), `client-226…290`, `proto-build-52` (unused), `proto-build-53…55`,
  `st-28…30`, `sweep-1…7`, `newgame-1…4`, `emission-1…5`, `story-1…2`, `presentation-1…2`, patch checks 77–95; the rest in
  the snapshots.
- Next: `campaign-405`, `client-291`, `presentation-3`, `proto-build-56`,
  `st-31`, patch check 96.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T05d1 resume](../done/snapshot-before-S10-T05d1-resume.md).
