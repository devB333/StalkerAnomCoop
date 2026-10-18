# Engine source ownership

The root repository owns `docs/source-manifest.json`, this file, `series`, and
the ordered patches. The ignored reference checkout is immutable investigation
evidence. `0001-campaign-server-startup.patch` adds the S02-T02 campaign role,
separate executable name, core/game-factory smoke path and conditional teardown.
`0002-campaign-core-smoke.patch` adds command-only console initialization,
persistent stage checks and explicit Core/GameState smoke scopes. GameState
reports the missing native level prerequisite; it is not a runtime pass.
`0003-campaign-services-smoke.patch` adds the server-owned CPU campaign services
(spatial DBs, script engine, character registries) with reverse-order owner
teardown, a Services smoke scope and a test-only `-campaign-fail-at` unwind check.
Its xrGame functions are additive; the client's game-globals functions are unchanged.
`0004-materials-without-media.patch` (S02-T03a) adds a material-library switch
that keeps pair media names but creates no pair sounds or wallmarks; it defaults
on, so the client is unchanged. The campaign server loads materials with it off.
`0005-cpu-only-models.patch` (S02-T03b) attaches the renderer module with no
device, creates only its model pool, skips and counts shader, geometry and GPU
buffer creation, and registers bone materials without the persistent object.
`0006-campaign-frame-driver.patch` (S02-T04a) adds `CCampaignServerClock`, the
server's tick-based clock and serial frame driver, with pause and bounded catch-up.
`0007-bone-capture.patch` (S02-T03c) adds the shared bone capture used by the
campaign smoke and by the client's opt-in `-s02-bone-capture` flag, and stops
view-based bone throttling in a CPU-only process.
`0008-persistent-without-device.patch` (S02-T05a) constructs the real game
persistent object with no device: shader/texture creation and resource
destructors are skipped in CPU-only mode, environment ambience sounds are
skipped without a sound manager, and the campaign branch of CGamePersistent
skips UI, menu, fonts and prefetch.
`0009-campaign-level-start.patch` (S02-T05b/c) constructs CApplication, CLevel,
xrServer, game_sv_Single and ALife from the fixture save with no device: loading
stages are pumped explicitly, the particle library loads on the CPU, the engine's
-nosound manager stands in for sound, and the presentation calls on that path are
guarded (render-resource dump, camera reset, input release, logo, wallmarks).
`0010-campaign-level-load.patch` (S02-T05d) runs the client loading stages and
the first server ticks with no device: CFORM, physics world, client game state
and level scripts load for real while the HUD, render-level load, texture upload
and precache are skipped; spawns stay inline; the campaign server adopts no
control or view entity (including the stalker fallback); map locations keep
their data without spot widgets; the actor stores its PDA task filters; HUD-pick
users and light shader creation are guarded.
`0011-collision-capture.patch` (S02-T03d) adds the shared level collision
capture (`xrGame/CampaignCollisionCapture.{h,cpp}`): skeleton collision forms,
their elements, character controllers, physics shells and object-space ray picks
for every object on the loaded level, written as CSV by the campaign server after
its first ticks (stage `campaign-collision-passed`) and by the normal client with
the opt-in `-s02-collision-capture` flag one second after the actor spawned. The
native spawn already builds all of that without the renderer; no engine
behaviour changes.
`0012-physics-fixture.patch` (S02-T04b) adds the shared falling-body physics
fixture (`xrGame/CampaignPhysicsCapture.{h,cpp}`): one existing physics shell
is lifted a metre and released, and every native physics step is sampled
through the world's step-time callback (the level's callback keeps running)
and an added contact callback, by the campaign server after its collision
capture (stage `campaign-physics-passed`) and by the normal client with the
opt-in `-s02-physics-capture` flag. The engine's stepping, gravity, per-step
velocity scale, contacts and sleeping are observed, not changed.
`0013-activity-policy.patch` (S02-T04c) adds the server activity policy
(`SActivityPolicy g_activity_policy`, `xrEngine/xr_object.{h,cpp}`): while the
campaign level runs, crow selection and the scheduling scale use the distance
to the nearest activity anchor (one per online actor, refreshed every frame
from `CLevel::OnFrame`) instead of the camera; disabled, the client is
unchanged. `CSheduler::dwUpdated` counts dispatches per frame. The shared
activity fixture (`xrGame/CampaignActivityCapture.{h,cpp}`) observes the object
list, the scheduler, a stalker's planner, ALife switching, offline scheduling
and the ALife clock, then walks the stalker through the native script-control
path (stage `campaign-activity-passed`, client flag `-s02-activity-capture`).
`0014-cpu-sound-events.patch` (S02-T05c) adds the CPU sound core
(`xrSound/SoundRender_CoreCPU.{h,cpp}`, `-snd_cpu`): the native sound core with
no OpenAL device and no listener, so sources, emitters and AI sound events run
on the server and 2D events are dropped rather than placed at a fabricated
listener; `has_listener()` skips the listener-distance start delay. Material
pairs create their sounds on the server (`SetPairSounds`/`SetPairMarks`), the
footstep and impact range gates use `activity_range_distance()` instead of the
camera, the campaign clock pauses emitters, and hearing counters
(`IGame_Level::snd_Stats`/`snd_Trace`, `CSoundMemoryManager::s_feel_calls`)
feed the shared hearing fixture (`xrGame/CampaignHearingCapture.{h,cpp}`, stage
`campaign-hearing-passed`, client flag `-s02-hearing-capture`).
`0015-actor-binder-diagnostics.patch` (S02-T06) makes the script binder's
exception handling visible: xrGame has no C++ exceptions, so its `catch (...)`
blocks caught native faults and deleted the Lua binder silently; the scheduled
update now uses an SEH filter that logs the fault with symbols (dbghelp) and
the other catches log the object and phase. `ActorMenu.get_*` and
`grab_menu_mode` return nil / `mmUndefined` with no game UI instead of
dereferencing null; `CActor::s_schedule_updates` counts actor scheduled
updates; the campaign smoke's tick report prints the actor's scheduling and
Lua binder state (`S02-T06 actor …`). The client's behaviour is unchanged.
`0016-inert-server-game-ui.patch` (S02-T06a, decision B20) gives the campaign
server the UI core, fonts and the real game UI (`CUIGameSP` with its actor
menu, PDA, in-game window and messages) as inert CPU objects: created by the
native path with GPU resources skipped and counted, laid out at the UI base
resolution (`Device.dwWidth/dwHeight` 1024×768 when no window set them), never
updated (`CHUDManager::OnFrame` returns on the server; the game UI joins no
frame sequence) and never drawn. The HUD manager is created on the server too.
The smoke's loaded report expects `hud=1 game_ui=1 ui_in_frame=0`, and its
tick report prints the script binder update time and the level frame's pump
times (`SLevelFrameTiming`, measured on both products). The client is unchanged.
Export later patches with `python3 tools/export-engine-patch.py <name> <check-number>`.

Reconstruct in a new, absent workspace directory (never overwrite an existing tree):

```sh
git clone --no-hardlinks engine/xray-monolith builds/engine-work
git -C builds/engine-work checkout --detach c57fe5b9fab4fe0506659db03137c52d2500eb2f
git -C builds/engine-work submodule update --init --recursive
```

Verify HEAD and submodule SHAs against the manifest before building. The submodule
download requires network access. This procedure was executed on 2026-10-15:
the independent working checkout began clean at the pinned revision and Optick
was initialized at its pinned gitlink. Stage 2 source edits are now exported by
the patch series. The reference checkout remains unchanged.
The source commit also pins bundled SDK inputs; record hashes of externally
installed compiler/SDK/dependency inputs when S01-T01 resolves them.

Apply patches in `series` order with `git apply --check`, then `git apply`, from
the working tree. Use relative patch filenames, one per line. Before ending each
editing session, export reviewed source changes as ordered text patches here,
including newly created files; verify reconstruction in a fresh scratch clone.
Never rely on ignored working-tree edits as the only copy. Update the manifest
and task report with patch names and SHA-256 hashes in the same root change.
Do not export proprietary assets or generated outputs. Revert by reconstructing
the pinned source with only the preceding patches in a new directory, preserving
the old tree until its changes are accounted for.

Root commit identity plus source manifest plus ordered patches defines project
source identity. Root currently has no commits; these artifacts are pending
version control, not a published or committed baseline.
