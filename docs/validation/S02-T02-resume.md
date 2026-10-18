# S02-T02 — Settings repair and scoped smoke validation

Recorded 2026-10-17 local time (2026-10-18 UTC). The settings-load failure is
fixed. **Core startup/shutdown passes; GameState remains blocked on a real
native level context.** S02-T02 stays In Progress and G02 is unpassed.

## Cause and repair

`CLocatorAPI::LoadArchive` (`src/xrCore/LocatorAPI.cpp:340`) mounts archives
whose entry point is `gamedata` at `$fs_root$\\gamedata`, independently of
`$game_data$`. The original smoke fixture put those two roots in different
directories. `CInifile` therefore found no `system.ltx` under the requested
virtual path; `InitSettings` failed its section-count check. The process's
exit 1 and last settings-load line were consistent with `Debug.do_exit`.

`tools/prepare-stage2-smoke.py` now aligns the roots inside each isolated run,
copies and hash-checks the small accepted loose-file overlay, and continues
reading archives from the accepted content copy. It asserts that the virtual
mount and game-data paths match. No original installation or accepted-runtime
file was modified. Smoke-04 with the previous executable passed settings and
exposed the next dependency: `CConsole` constructed an editor that dereferenced
the absent `pInput`.

The second engine patch gives `CConsole` an explicit command-only mode. The
normal constructor defaults to interactive behavior. Command-only mode retains
command registration/execution, skips the keyboard editor and presentation
callbacks, and does not install the presentation-dependent crash-variable dump.
Native stage logs survive core teardown, and the launcher checks the ordered
stages as well as the actual process exit code. The build wrapper also now owns
its .NET Process handle, fixing null exit-code capture.

## Scope correction and remaining boundary

The original test attempted `game_cl_Single` construction before a `CLevel`
existed. Smoke-06's trace points to `game_cl_GameState` construction; its base
`game_GameState` (`src/xrGame/game_base.cpp:205`) immediately reads
`Level().timeServer_Async()`. Its level `VERIFY` is absent in this release build.
The test violated the native initialization order. No dummy actor, replacement
clock or empty model was added to make that construction succeed.

The launcher now requires an explicit `-Scope Core` or `-Scope GameState`.
Core checks the supported startup boundary and teardown. GameState retains the
additional requirement, reports `level-context-required`, returns 3, and tears
down cleanly. **The Core pass does not replace the failed GameState requirement.**
`game-factory-ready` means game entry points and console commands were installed;
it does not mean a campaign game instance, Lua world or ALife simulator was created.
The earlier native game-state attempt also logged an unknown renderer console
command from installed scripts; full script startup remains unvalidated.

## Results

| Run | Observed result |
| --- | --- |
| campaign-02 | Build succeeded, 0 errors; command-only console and stage logs. |
| smoke-04 | Corrected paths with old binary: settings passed; input-editor access violation. |
| smoke-05 | Settings/scheduler/console/factory entry points reached; game-state creation failed. |
| campaign-03 / smoke-06 | Build succeeded; reliable crash trace exposed game-state construction without native level. |
| campaign-04 | Final campaign build succeeded, 20 warnings, 0 errors; recorded exit 0. |
| client-02 | Normal client build succeeded, 212 warnings, 0 errors; recorded exit 0. No new interactive gameplay run. |
| smoke-07, Core | Exit 0; all nine required stages passed, including graphics/window/input absence and completed teardown. |
| smoke-08, GameState | Exit 3; `passed=false`, `level-context-required`; completed teardown, no timeout. |

Final campaign executable SHA-256:
`eb47df04637723f2cf6b1aac5ac592049f581053db273342587e38f77a3175c7`.
Results and exact command lines are in `builds/logs/S02-T02/<run>/result.json`.
The smoke stage logs are copied there; full engine logs are under the matching
`builds/stage2/test-install/S02-T02/smoke-*/appdata/logs/` directories.

At the Core boundary, assertions checked `Device.m_hWnd`, `Device.m_pRender`,
`Device.b_is_Ready`, `Render` and `pInput` were all absent/false. This is a bounded
startup-state check plus source-path evidence, not an all-API GPU trace or a test
on a machine without a GPU. No level, actor, animated bones, physics step,
ballistic event or ALife tick ran. Those require the later service extraction.

## Reproduction and source identity

From the workspace, prepare a **new** run name, then invoke the Windows scripts:

```sh
python3 tools/prepare-stage2-smoke.py smoke-09
```

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\djb20\StalkerAnomCoop\tools\Run-Stage2Smoke.ps1 -Run smoke-09 -Scope Core
# Use a different prepared fixture with -Scope GameState to exercise the unmet level prerequisite.
```

Builds used `tools/Build-Stage2.ps1 -Run campaign-04 -Product Campaign` and
`-Run client-02 -Product Client`, through the same PowerShell invocation. The
source base and toolchain pins are unchanged. Patch 0001 is preserved; new
`patches/engine/0002-campaign-core-smoke.patch` has SHA-256
`2e5bd39bfecf4872cac44f12c2724e596941128b9bfe420706c1408fb5cab72a`.
Both patches apply in a fresh clone at `builds/stage2/patch-check-02`, and all
eight changed source files match the build checkout byte for byte; see
`builds/logs/S02-T02/patch-reconstruction-02.json`. Source diff whitespace and
manifest JSON checks passed. Root files remain uncommitted/untracked.

Reverse by reconstructing the pinned source with the preceding patch series in
a new directory. Preserve current logs, source changes, reference checkout and
accepted saved-game fixture. The two G01 waivers remain unchanged.
