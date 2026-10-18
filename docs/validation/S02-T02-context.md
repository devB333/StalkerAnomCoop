# S02-T02 — Internal campaign context and services validation

Recorded 2026-10-17 local time (2026-10-18 UTC). This run completes S02-T02 under
the user's 2026-10-18 scope decision: game-state/ALife construction from fixture
assertion H02 moves to S02-T05/T06. **No headless gameplay, level, ALife tick or
physics step has run.** G02 is unpassed. The context definition is in
[campaign-server-context.md](../campaign-server-context.md). Earlier S02-T02 evidence
is in [S02-T02.md](S02-T02.md) and [S02-T02-resume.md](S02-T02-resume.md).

## Acceptance

| Criterion (plan.md S02-T02) | Evidence |
|---|---|
| Distinct dedicated role that keeps the campaign services | `XR_CAMPAIGN_SERVER` and `g_campaign_server`; `g_dedicated_server` stays false. Patches 0001–0003. |
| Dedicated target compiles with the campaign code included | `campaign-07` builds with 0 errors. `xrgame_dll_detach.cpp` compiles with `XRGAME_EXPORTS`, `dSINGLE`. `x_ray.cpp` compiles with `XR_CAMPAIGN_SERVER`. No `NO_SINGLE` or `DEDICATED_SERVER` on any compile line in the `campaign-07` log. The `campaign-01` full build compiled `game_sv_single.cpp`, `game_cl_single.cpp` and the ALife sources ([S02-T02.md](S02-T02.md)). |
| SP target preserved | `client-04` builds with 0 errors, and none of its compile lines has `XR_CAMPAIGN_SERVER`. The xrGame additions are new functions. `init_game_globals` and `clean_game_globals` are unchanged. |
| Server-owned context, configuration roots, lifecycle teardown | [Context definition](../campaign-server-context.md); fixture generator roots; reverse-order owner teardown checked by `smoke-14`–`smoke-18`. |
| Initialization/shutdown exercised as far as currently supported | Core and Services pass. GameState stops at the first unmet prerequisite, `persistent-context-required`, and then tears down cleanly. |

## Changes

- **`0003-campaign-services-smoke.patch`** (SHA-256 `449a85f513ff5df65b3e6f5770bdb8eb923dec6af178aee221e47938586fa68b`).
  - `xrEngine/x_ray.cpp`:
    - a Services scope (`-campaign-services-smoke`)
    - a service owner list that stops in reverse
    - `<service>-failed` handling with exit 4
    - a test-only `-campaign-fail-at <service>`
    - a graphics-absence recheck after services
    - GameState now reports `persistent-context-required`
  - `xrGame/xrgame_dll_detach.cpp`: `xrCampaignScriptEngine_Initialize/Destroy` and `xrCampaignGameGlobals_Initialize/Destroy`.
- **`tools/Run-Stage2Smoke.ps1`**:
  - a `Services` scope and an optional `-InjectFailure`
  - an expected stage list and exit code for each scope, plus `outcome_as_expected`
  - GameState never reports `passed`
- **Reconstruction.** All three patches apply in the new clone
  `builds/stage2/patch-check-04`. All nine changed files are byte-equal to
  `builds/build_1/engine-work`
  (`builds/logs/S02-T02/patch-reconstruction-04.json`). Submodules were not
  initialized; no changed file is inside one. `docs/source-manifest.json` lists
  all three patch hashes.

## Builds

All builds ran through
`powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\djb20\StalkerAnomCoop\tools\Build-Stage2.ps1 -Run <run> -Product <Campaign|Client>`
on the pinned source and toolchain. Logs are in `builds/logs/S02-T02/<run>/`.

| Run | Source state | Result |
|---|---|---|
| campaign-05 | Services with materials stage | Exit 0, 110 warnings, 0 errors, 00:02:18; exe `2ab69411…` |
| client-03 | Same xrGame code as campaign-05 | Exit 0, 110 warnings, 0 errors, 00:01:59 |
| campaign-06 | Materials stage removed | Exit 0, 20 warnings, 0 errors; exe `ce754d7f…` |
| campaign-07 | Final: script engine before registries | Exit 0, 110 warnings, 0 errors, 00:01:15; exe `0d02287f4e32858983582700f430d76831325ace174fde46b02ad8fbde24f1b9` |
| client-04 | Final source | Exit 0, 110 warnings, 0 errors, 00:01:31; `_build/_game/bin_dbg/AnomalyDX11.exe` `f2fb745db56b40acb75db392ba62404daa406ce242419ed160a886dea7463f3f` |

These were incremental builds. No compile warning points at the new code.

## Smoke runs

Each run had its own fixture from `python3 tools/prepare-stage2-smoke.py <run>`,
then
`powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\djb20\StalkerAnomCoop\tools\Run-Stage2Smoke.ps1 -Run <run> -Scope <scope> [-InjectFailure <service>]`.
Results are in `builds/logs/S02-T02/run-<run>/result.json`. Engine logs are in
`builds/stage2/test-install/S02-T02/<run>/appdata/logs/`.

| Run | Exe | Scope | Exit | Observed |
|---|---|---|---|---|
| smoke-09, smoke-10 | `2ab69411…` | Core | — | **Not run.** Windows Smart App Control (enforce) blocked the new unsigned exe before the process started (CodeIntegrity 3033/3077). The user turned Smart App Control off; see `blocked.txt` in each run's log directory. |
| smoke-11 | `2ab69411…` | Core | 0 | Passed. Core regression. |
| smoke-12 | `2ab69411…` | Services | timeout | **Failed.** Access violation in `GMLib.Load` → `SGameMtlPair::Load` → `CreateSounds` (`GameMtlLib_Engine.cpp:57`). There is no sound manager, and the crash handler held the process until the 120 s watchdog. Minidump in the run's logs. Led to the materials blocker. |
| smoke-13 | `ce754d7f…` | Services | 0 | Passed under the old order. The log shows the script engine was created during the registries stage (installed `_G` and `modxml_*` scripts loaded before `campaign-registries-ready`). Led to the reorder. |
| smoke-14 | `0d02287f…` | Services | 0 | **Passed.** spatial-db → script-engine (`_G.start_game_callback resolved=1`) → campaign-registries → graphics/window/input still absent → stop in reverse. |
| smoke-15 | `0d02287f…` | GameState | 3 | As expected, not a pass: `persistent-context-required` after all services, then a full reverse unwind. |
| smoke-16 | `0d02287f…` | Services, inject `script-engine` | 4 | As expected: `script-engine-failed`, then only `spatial-db-stopped`. |
| smoke-17 | `0d02287f…` | Services, inject `campaign-registries` | 4 | As expected: `campaign-registries-failed`, then `script-engine-stopped` and `spatial-db-stopped`. |
| smoke-18 | `0d02287f…` | Core | 0 | Passed. Core regression on the final exe. |

One installed-script warning appears in every Services log:
`! Unknown command: r__clear_models_on_unload`. That is a renderer console command
the command-only server doesn't register. It isn't fatal, and it's recorded rather
than suppressed.

## Limits and moved work

- **Moved to S02-T05/T06 (user decision):** real `game_sv_Single`, ALife,
  `CLevel` and `game_cl_Single` construction (H02). The blockers, with source lines,
  are in the context definition: the material-pair media, the persistent render
  factory, ALife's main-menu reset, and the `CLevel` player HUD models.
- **Material library:** CPU physics data; the split with its media is S02-T03/T04,
  stimulus-only sound is S02-T05. It isn't loaded by the campaign server yet.
- **Script engine:** startup only. No level script process, ALife callback, task
  evaluation or Lua world access.
- **Graphics absence:** checked through engine state only. No native graphics API
  trace.
- **Startup assertions:** a failure that raises an engine assertion still
  terminates the process (smoke-12 hit the crash handler). Owner unwind covers the
  service-level failures checked above.
- **Client:** no new interactive SP gameplay run. The accepted source-client
  runtime and its G01 evidence are unchanged.
- **Waivers:** the two G01 waivers remain unchanged.

Reverse by reconstructing the pinned source with patches 0001–0002 in a new
directory. Logs, fixtures and the earlier executables' evidence are preserved.
