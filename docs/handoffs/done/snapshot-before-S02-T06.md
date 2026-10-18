## Complete

# Session handoff — S02-T04 complete; S02-T05c (CPU sound events and AI hearing) in progress

## Next session: finish S02-T05c, then S02-T06

S02-T04c is **Completed**: the normal client (client-17) passed the same
observe-and-walk fixture as the server (smoke-57) and the comparison agrees on
every rule but one asserted ALife parameter, recorded as a finding (below).
Details: [S02-T04c report](../../validation/S02-T04c.md).

S02-T05c is **In Progress** with the engine changes written and the first
server build running (`campaign-41`; `campaign-40` failed on a missing include,
fixed). The design, all in `builds/build_1/engine-work`:

- **CPU sound core** `xrSound/SoundRender_CoreCPU.{h,cpp}`, selected with
  `-snd_cpu` (`sound.cpp`): the native core with no OpenAL device and no
  listener. Sources load their ogg headers, emitters run the native state
  machine (`SoundRender_Emitter_FSM.cpp`) and propagate AI events; 2D events
  are dropped instead of being placed at a fabricated listener; the
  listener-distance start delay is skipped (`has_listener()`); playback
  occlusion is 1. `CSound_stats` gained `_started`, `_events_total`,
  `_events_2d_skipped`. `NotificationClient.cpp` guards a null `SoundRenderA`.
- **Server:** `StartCampaignSound` requires `-snd_cpu`
  (`Run-Stage2Smoke.ps1` passes it instead of `-nosound`); material pairs now
  create their sounds and still no wallmarks (`GMLib.SetPairSounds/SetPairMarks`,
  `StartCampaignMaterials` expects `created_sounds != 0`); the campaign clock's
  Pause/Resume pause and resume emitters.
- **Camera range gates → activity distance:** `activity_range_distance()`
  (`xr_object.{h,cpp}`) is used by `step_manager.cpp:168` (footstep sounds)
  and `physics_game.cpp:272` (impact sounds); the client keeps its camera.
- **Hearing counters:** `IGame_Level::snd_Stats` and an optional
  `snd_Trace` (one record per registered AI sound event) in
  `IGame_Level.{h,cpp}`; `CSoundMemoryManager::s_feel_calls` and
  `feel_count()`.
- **Fixture** `xrGame/CampaignHearingCapture.{h,cpp}` (rows in the header)
  runs alongside the activity fixture: server stage `campaign-hearing-passed`
  (exit 16 on failure; `x_ray.cpp`), client flag `-s02-hearing-capture`
  (driven from `ClientActivityCaptureOnFrame`), capture `s02-t05c-hearing.csv`.
  Pass rule: emitters started, events reached hearers and `feel_sound_new`, the
  walked stalker produced an event while walking. `Run-ClientCollisionCapture.ps1
  -Capture hearing` and `tools/compare-hearing-capture.py` are written.

Remaining steps, in order:

1. Finish the `campaign-41` build (fix compile errors if any; rebuild under a
   new name).
2. `python3 tools/prepare-stage2-smoke.py smoke-58` then
   `Run-Stage2Smoke.ps1 -Run smoke-58 -Scope GameState` (expects
   `campaign-hearing-passed` after `campaign-activity-passed`). Check the log
   for `S02-T05c hearing …` and that `Couldnt find sounds` aborts are gone
   (they came from `-nosound`: `sound_theme.script:125` aborts when
   `npc:add_sound` returns 0 because sounds had no handles).
3. Build the client (`client-18`), prepare `capture-07` with task `S02-T05c`,
   run `-Capture hearing`, compare with `tools/compare-hearing-capture.py`.
4. Write `docs/validation/S02-T05c.md`, export patch 0014
   (`python3 tools/export-engine-patch.py 0014-cpu-sound-events.patch 15`),
   update `docs/source-manifest.json` and `patches/engine/README.md`, set
   T05c and parent S02-T05 to Completed in `plan.md`, handoff, commit.
5. Then S02-T06 (native campaign simulation and checkpoint round-trip), which
   also owns the findings below.

## Findings that shape the next tasks

- **The actor's Lua binder first update did not run on the server.**
  `bind_stalker.script` `actor_binder:reinit()` sets
  `objects_per_update` to 65534 at spawn and `actor_binder:update()`'s
  first-update block queues the reset to 20 (a one-second time event). The
  client log shows the block ran; the smoke-57 server log does not, with no
  Lua error. Consequences: ALife processes every offline object each tick on
  the server (~91 brain updates per 20 ms tick), and the block's other work
  (info portions, PDA objects, active slot) is missing. Isolate in S02-T06:
  the actor's scheduled update, `CScriptBinder::shedule_Update`
  (`GameObject.cpp:1091`, guarded only by `g_dedicated_server`), or the
  time-event pump (`level.add_call` → `ProcessEventQueue`).
- **Actor-owned items are not crows on the server** (no view entity), so item
  `UpdateCL` logic runs only on scheduled updates (Stage 3 actor context).
- **Objects at 674.3–674.9 m from the actor switched online on the client, not
  the server**: verify in S02-T06 that the server's ALife actor position
  follows the live actor.
- `tools/xray-db-extract.py` lists/extracts/greps the game's `.db` archives
  (LZHUF chunks, LZO1X files; format cited from `LocatorAPI.cpp`), so Lua
  scripts and configs inside archives can be read. Extracted text is cp1251:
  use `LC_ALL=C grep -a`.
- Content ALife parameters (R11): switch distance 750 m, ALife manager every
  tick, time factor 6; the activity policy (60 m around the anchors) bounds
  per-frame object work.
- Earlier decisions stand: no control/view entity on the server; map
  locations keep data without spot widgets; the spawn tick is long.

## Tools and runs

- **Last run names:** `campaign-41` (building), `campaign-40` (failed
  compile), `client-17`, `capture-06`, `smoke-57`; `smoke-20` reserved for
  the user. Next: `campaign-42`, `client-18`, `smoke-58`, `capture-07`.
  Check `builds/logs/S02-T02/`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`.
- **Client captures:** `python3 tools/prepare-stage2-smoke.py <run> <task>`
  then `Run-ClientCollisionCapture.ps1 -Run <run> -Capture collision|physics|activity|hearing`.
- **Build:** `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`. Quote
  the script path when calling `powershell.exe` from WSL. A change to
  `xr_object.h` or `Sound.h` rebuilds most of the solution (~10 min).
- **Patches:** next export is `python3 tools/export-engine-patch.py <name> 15`.
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous snapshot: [before S02-T05c](./snapshot-before-S02-T05c.md).
