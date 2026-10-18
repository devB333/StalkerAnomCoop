# Session handoff — 2026-10-17, S02-T02 paused

The user had to leave and asked for a note so work can resume later. Stop here.
The active objective is [S02-T02](../../validation/S02-T02.md): campaign-preserving
dedicated configuration and startup smoke. **In Progress:** both builds compiled,
but the campaign startup smoke exits 1 while loading `system.ltx` during
`InitSettings`. G02 is still unpassed. S02-T03–T06 are not started.

## Exact resume point

1. Read [S02-T02 validation](../../validation/S02-T02.md) for commands, artifacts,
   SHA-256 values and the failure boundary. Inspect
   `builds/logs/S02-T02/run-smoke-03/result.json` and the associated runtime
   `appdata/logs/xray-campaign-server_djb20.log`.
2. Diagnose `InitSettings`/`CInifile` and the isolated `fsgame.ltx`. Add native
   `Msg` markers around settings substeps, since GUI `printf` supplied no stdout.
   Rebuild only with a new run name; do not overwrite logs or smoke fixtures.
3. Update and hash the root patch after any engine edit, and re-check it in a
   fresh clone. Keep original installation, reference checkout, accepted client
   test installation and user saves untouched.
4. Once settings/factory/shutdown smoke passes, finish S02-T02 evidence and
   advance to S02-T03 CPU model, animation and collision extraction. A successful
   factory smoke alone is not renderer-free level simulation.

## What changed this session

- Engine working tree `builds/build_1/engine-work`: six-file source patch at
  `patches/engine/0001-campaign-server-startup.patch`, listed by
  `patches/engine/series` and `docs/source-manifest.json`.
- Root scripts: `tools/Build-Stage2.ps1`, `tools/prepare-stage2-smoke.py`,
  `tools/Run-Stage2Smoke.ps1`. The build wrapper still needs reliable exit-code
  capture; `Start-Process -PassThru` returned null `ExitCode` even after exit.
  The smoke wrapper now starts its own .NET Process and recorded exit 1.
- Client MSBuild log: success, 0 errors. Campaign MSBuild log: success, 0 errors.
  The campaign compile command carries `XR_CAMPAIGN_SERVER` with single-player
  and ALife code included. The first honest smoke result is failure at settings.
- `docs/session-handoff-before-S02-T02.md` preserves the previous S02-T01
  handoff. Root repository files remain uncommitted/untracked; preserve them.

Stage 0/G00 and Stage 1/G01 remain accepted for development with the two
user-approved G01 waivers (completed quest/reward persistence and copied
original-client comparison). [plan.md](../../../plan.md) is authoritative.
