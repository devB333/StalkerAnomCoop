## Complete

# Session handoff — S02-T06d checkpoint round-trip passes on the server; finish evidence, patch 0019 and docs

## State at the end of the session (2026-10-18, usage limit reached mid-task)

S02-T06d is implemented and **passed once on the server** (smoke-80, exit 0,
61 s wall, every stage including `campaign-checkpoint-saved`,
`campaign-restart-server-ready`, `campaign-restart-level-loaded`,
`campaign-checkpoint-passed`). Nothing is exported, documented or committed
yet. Uncommitted work:

- **Engine (`builds/build_1/engine-work`, not yet patch 0019):**
  `xrGame/CampaignCheckpoint.{h,cpp}` (new; added to `xrGame.vcxproj` next to
  the combat fixture), `CampaignCombatCapture.{h,cpp}` (`CombatFixtureMonsterId()`),
  `GamePersistent.cpp` (client hook `ClientCheckpointCaptureOnFrame`),
  `xrEngine/x_ray.cpp` (checkpoint stages after combat, `g_campaign_checkpoint_name`).
- **Root repo:** `tools/Run-Stage2Smoke.ps1` (new stage list, keeps
  `server-checkpoint.csv` and the `.scop/.scoc`, 600 s timeout),
  `tools/Run-ClientCollisionCapture.ps1` (`-Capture checkpoint -CheckpointRun <smoke>`),
  `tools/compare-checkpoint-capture.py` (lint clean; dry run on the server rows OK).

### Design (cite in the report)

- Save = the `M_SAVE_GAME` packet through `CLevel::Send`, exactly what
  `CCC_ALifeSave` (`console_commands.cpp:1045-1130`) and the autosave manager
  send, minus the screenshot and UI static. Direct connect makes it synchronous
  (`Level_network.cpp:296-305` → `xrServer::OnMessage` → `game_sv_Single::save_game`
  → `CALifeStorageManager::save(NET_Packet&)`: `ClientSend`, `ClientSave`, Lua
  `CALifeStorageManager_before_save` writes the `.scoc`, then the `.scop`).
- Reload = `xrCampaignLevel_Stop()` + `xrCampaignLevel_Start("<name>/single/alife/load")`,
  the KERNEL:disconnect/KERNEL:start sequence `MakeReconnect` uses; the
  same-level quick load (`Game:QuickLoad` → `restart_simulator`) was not used.
- Rules are asserted in C++ (`S02-T06d rule ok/FAIL` lines) and the snapshots
  go to `s02-t06d-checkpoint.csv` (rows `before-save`, `after-save`, `save`,
  `reloaded-alife`, `reloaded-ticked`; client row `client-loaded`).

### smoke-80 result (campaign-67 exe `1508baf2…5ebd3b`)

Save 51 ms, 26,205 objects, `.scop` 1,478,670 B (`3f22aa76…b41e9`), `.scoc`
317,019 B (`e72552c9…fbf09`); header game time, level `k00_marsh(1)` and actor
health equal the live values. After the restart: ALife 26,205 objects and 1,040
level objects (exactly what was written), game time identical before the first
tick, ALife actor at `[561.041,2.229,-180.640]` graph 223 node 516443, stalker
19409 health 0.6208 (wounded by the boar in that run) and the dead boar 4136
(health -1) all back; after 50 ticks the live actor is 0.000 m from the
checkpoint, health 1.000, 1,101 objects online on both sides, binder
`after_first_update=true`, marshal `GAME_VERSION=1.5.3` decoded from the
`.scoc`. No Lua error; the `axr_main on_game_end` notice now appears once per
level stop (two). `builds/logs/S02-T02/run-smoke-80/` holds everything.
smoke-79 (campaign-66) failed only on my `.scoc` check: Lua writes it with
`io.open`, which `FS` never registers, so the check now uses `_stat64`.

### Next steps, in order

1. `smoke-82` from the final source (campaign-68 exe `bae830a8…5616`, built
   from the committed-to-be source). smoke-81 from it failed at the earlier
   **hearing** stage (`npc_walk_events=0`, a per-run variation of S02-T05c; every
   other run had 5–27); the checkpoint stages never ran. Just rerun.
2. Client cross-check: `python3 tools/prepare-stage2-smoke.py capture-10 S02-T06d`
   is already prepared; run
   `Run-ClientCollisionCapture.ps1 -Run capture-10 -Capture checkpoint -CheckpointRun smoke-82`
   (client-23 exe `9692f3aa…55bde`, final source), then
   `python3 tools/compare-checkpoint-capture.py builds/logs/S02-T02/run-smoke-82/server-checkpoint.csv builds/logs/S02-T06d/run-capture-10/client-checkpoint.csv`
   and store the JSON under `builds/logs/S02-T06/`.
3. `python3 tools/export-engine-patch.py 0019-checkpoint-round-trip.patch 20`,
   update `docs/source-manifest.json` and `patches/engine/README.md`.
4. Part 5 of `docs/validation/S02-T06.md`, plan.md (S02-T06d line, §I table,
   stage summary), then the G02 evidence record (`docs/validation/G02.md`):
   save/reload is now observed; task (quest) execution is observed only as the
   task scripts running without error through the actor binder, so state that
   gap and leave acceptance to the user.
5. Commit: `engine: save and reload a native checkpoint headless on the campaign server`.

## Tools and runs

- Last run names: `campaign-68`, `smoke-81`, `client-23`; `capture-10` prepared,
  not run. Next: `campaign-69`, `smoke-82`, `client-24`, `capture-11`.
- Everything else in the previous handoff still applies (smoke/capture/compare
  commands, `-s02-combat-*` options, patch export, Smart App Control off).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous snapshot: [before finishing S02-T06d](./snapshot-before-S02-T06d-finish.md).
