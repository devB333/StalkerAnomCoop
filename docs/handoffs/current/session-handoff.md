# Session handoff — S10-T02a–e complete; S10-T02f in progress (companion follow)

## State (2026-10-30)

- **In progress: S10-T02f companions per recruiter**
  ([S10-T02f](../../validation/S10-T02f.md), draft; decisions T02f-1…5 in
  `docs/stage10-decisions.md`). **The engine changes are in
  `builds/build_1/engine-work` only, not yet exported** (patch 0075
  `companions`, patch check 87, is still to do). Latest builds
  **campaign-371 / client-263** (built clean, 0 errors; not run yet).
  What's in it:
  - Engine (`CampaignScriptContext.cpp`): a companion registry (npc → slot),
    Lua `campaign.set_companion / clear_companion / companion_slot /
    slot_actor_id`; `npc_evaluation_actor` answers a companion's `db.actor`
    with its recruiter (`companion_reads`). Compat-layer wrappers:
    `axr_companions.add_to_actor_squad` / `remove_from_actor_squad` (save
    `se_save_var … "coop_recruiter"` + registry),
    `sim_squad_scripted.get_script_target` (target 0 → the recruiter's actor,
    the squad holds if the recruiter is away),
    `dialogs_axr_companion.is_actor_companion` (false for the other player),
    and `get_story_object` / `get_story_object_id` for `"actor"` (the
    evaluated NPC's player, else the context's).
  - Server (`CoopServerHost.cpp`): `coop_server_companion_changed` (logs
    `companion-changed`, asks for a checkpoint); `companion_fixtures`
    (profile `companion_party`, `--companion c1,c2`), helper
    `companion_lua`; `tools/compare-coop-companion.py` (6 checks).
  - **companion-1/2 (5 of 6 each):** the recruit (player 2, a 4-member
    `sim_default_stalker_0` squad 103 m away), every check (slot, evaluation
    with player 1 beside it, dialogue refused to player 1, squad target), the
    server kill at 300 s and the **restored** checks all pass; **the
    companion does not walk to player 2** (stays ~97 m away, beside player
    1). The `get_story_object` wrapper (added after companion-1) did not
    change it. campaign-371 adds a `follow-state` diagnostic every 10 s
    (wait flag, active scheme `beh`, the target `axr_beh` picked, the story
    actor, squad commander, target distance).
- **Completed and committed:** **S10-T02e generic mutants, phantoms, AI
  lighting** ([S10-T02e](../../validation/S10-T02e.md), patch 0074 `fauna`,
  patch check 86; builds **campaign-367 / client-259**; **fauna-4 pass
  7/7**: thirteen mutant classes spawned beside player 2 all took player 2
  as their enemy, never player 1; ten hit, flesh, tushkano and rat fled (their
  morale, as in single player); the pseudogigant's stomp kick reached client
  2 only (`MonsterEffect` phase 4); the psy dog's phantoms reached the
  clients; a client crash on a failed phantom spawn was fixed (uninitialized
  particle names); lighting counts both players' views every frame. L12
  closed, L13 narrowed to the rule (decision T02e-3)). Before it:
  **S10-T02d bloodsucker, burer, poltergeist**
  ([S10-T02d](../../validation/S10-T02d.md), patch 0073 `mutant-abilities`,
  patch check 85; builds **campaign-362 / client-254**; **mutant-2 pass
  6/6**: the vampire grab, the anti-aim with the burer's stamina hit and the
  poltergeist's telekinesis all land on the monster's enemy, player 2, never
  on player 1; the grab's HUD and camera effects and the anti-aim's camera
  kick reach client 2 only; presentation `MonsterEffect`). Before it:
  S10-T04c/f/d/e (patches 0066–0069; S10-T04
  complete), S10-T02a (0070), S10-T02b (0071) and **S10-T02c controller and
  psy auras** ([S10-T02c](../../validation/S10-T02c.md), patch 0072
  `controller-psy`, patch check 84; builds **campaign-357 / client-249**;
  **psy-3 pass 6/6**: the controller's tube attack targets its participant
  enemy on the server, its camera, HUD and sound effects reach only that
  player's client through the new presentation `ControllerPsyHit`; monster
  auras are each viewer's own, driven by the client link's frame because a
  remote creature's scheduled and `UpdateCL` updates do not run on a client).
- **The user's hands-on of demo-2 (S10-T04e, 2026-10-30):** trade at
  Sidorovich works; the weapons work. Two bugs went to the new
  **bug-squashing list** at the top of `docs/known-issues.md` (B1 the
  trader's portrait and name missing in the Lua trade window; B2 a PDA
  glitch, details to confirm with the user — the partner's PDA gets no
  third-person animation because patch 0066 routes the server state to
  weapons and grenades only). The user asked for that list to grow until a
  dedicated bug session; add rows as bugs are found and keep building.
- **Findings that bind later work:** Anomaly's windows are the installed Lua
  UI (T04f-5/6, T04e-1); the engine does not save the faction table
  (T04d-4); NPC script logic decides against the nearest participant inside
  its own binder update (T02a-1); a remote creature on a client runs neither
  its scheduled update nor `UpdateCL` — presentation that lives there must be
  driven from the client link (`present_actors`), as the auras now are.
  luabind pitfalls: never assign a call proxy into or call `type()` on a
  default-constructed `luabind::object`; no `luabind::iterator`.
  `campaign::server_role()` is file-local: use
  `campaign::coop_server_prepared()` (`CoopServerHost.h`).
- **Hands-on by the user, pending (list for G10):** a controller attacking
  one player (effects on that screen only), each player feeling its aura by
  distance; an NPC looting a body; an NPC greeting player 2; the Lua loot,
  repair and trade windows; the partner's weapon animations; the relations
  tab. `bash tools/start-coop-demo.sh S10-T02c 0 20 0`.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, T04d-1…4, T04e-1…3, T02a-1…2, T02b-1…2, T02c-1…3,
  T02d-1…3, **T02e-1…4** (T02e-3: flashlights and campfires do not make a
  player more visible to AI — a gameplay call for the user).
  Open question for the user: T05b-4 (who pays a task's money cost).
- The user's standing directions (2026-10-29): trade is NPC-to-one-player
  (T01-7), the game's own drag-and-drop window (the Lua window), quest
  rewards to both players in full (T05b-2). Keep the decisions list and the
  bug list.

## Next

1. **Finish S10-T02f.** Run companion-3 on campaign-371 / client-263:
   ```
   python3 tools/prepare-coop-session.py companion-3 --task S10-T02f --protocol-build proto-build-53 --companion c1,c2
   powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File 'C:\Users\djb20\StalkerAnomCoop\tools\Run-CoopSession.ps1' -Run companion-3 -Task S10-T02f -ServerAloneSeconds 10 -ClientStarts '0,15' -ClientSeconds '600,585' -AfterSeconds 10 -ServerKill 300 -ServerRestartAfter 10
   python3 tools/compare-coop-companion.py builds/logs/S10-T02f/run-companion-3 --out builds/logs/S10-T02f/run-companion-3/compare-companion.json
   ```
   (companion-3 is already prepared; rerun the prepare line anyway or use companion-4).
   Read the `fixture-companion follow-state` lines in
   `server-first-run/coop-server.log`: whether the companion is waiting
   (`wait=1`), not on the `beh` scheme (`beh=0`), targeting someone else
   (`target` ≠ player 2's id), or not its squad's commander (followers walk
   to the commander, `axr_beh` `goto_target = "{!is_squad_commander}
   commander, actor"`). Leads: the recruited NPC may not be the commander,
   or `beh_move` picks a point near its own position. Fix, rerun, then
   export patch 0075 (`python3 tools/export-engine-patch.py
   0075-companions.patch 87`), add its hash to `docs/source-manifest.json`,
   add the README entry in `patches/engine/README.md`, finish the report,
   plan.md row (S10-T02f) and commit.
2. Then S10-T03a–f, S10-T05a/c/d, S10-T06; stop at G10 with the decisions
   list and the bug list.

## Run names

- Used: `campaign-334…371`, `client-226…263`, `proto-build-52` (unused),
  `proto-build-53`, `st-28`, `upgrade-1…7`, `relations-1…3`, `stash-1…2`,
  `npc-1`, `loot-1`, `psy-1…3`, `mutant-1…2`, `fauna-1…4`,
  `companion-1…2` (`companion-3` prepared, not run), demos
  `demo-1…2` of S10-T04e, patch checks 77–86; the rest in the snapshots.
- Next: `campaign-372`, `client-264`, `proto-build-54`, `st-29`, patch
  check 87; sessions `companion-3…` (S10-T02f).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T02f resume](../done/snapshot-before-S10-T02f-resume.md).
