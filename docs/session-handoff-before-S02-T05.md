# Session handoff — S02-T02 complete; next S02-T03/T04

S02-T02 is **Completed** under the user's 2026-10-18 scope decision. Moving the
real game-state/ALife construction (H02) to S02-T05/T06 was the user's decision.
G02 is In Progress. No headless gameplay, level, ALife tick or physics step has run.

Read first:
1. [internal context definition](campaign-server-context.md): what the server owns,
   the native start order, teardown, and the five blockers with source lines
2. [context and services validation](validation/S02-T02-context.md): builds, smokes,
   hashes and limits

## Confirmed results

- **Builds:**
  - `campaign-07` (exe `0d02287f…`) and `client-04`: exit 0, 0 errors.
  - No `NO_SINGLE` or `DEDICATED_SERVER` on any campaign compile line.
- **Smokes:**
  - `smoke-14 -Scope Services` passes. The services start in this order: spatial
    DBs → script engine (installed `_G` and common scripts; ALife start callback
    resolves) → character registries. Graphics, window and input stay absent. Stop
    runs in reverse.
  - `smoke-15 -Scope GameState`: exit 3, `persistent-context-required`, then a clean
    unwind. This is not a pass.
  - `smoke-16` and `smoke-17` (`-InjectFailure script-engine` /
    `campaign-registries`): exit 4. Only the services that had started were stopped.
  - `smoke-18 -Scope Core` still passes.
- **Patches:** 0001–0003 rebuild all nine changed files byte for byte in
  `builds/stage2/patch-check-04`. Their hashes are in `docs/source-manifest.json`.

## Findings that shape the next tasks

- **Materials (`GMLib.Load`).** Material pairs create sounds and render wallmark
  arrays when they load. `smoke-12` crashed in `CreateSounds`. The CPU/media split
  is S02-T03; stimulus-only sound is S02-T05.
- **Script engine first.** Every XML load calls Lua `_G.COnXmlRead`, so the script
  engine must exist before the registries.
- **Blockers for S02-T05/T06:**
  - Persistent state needs the render factory (`CEnvironment`).
  - ALife's `restart_all` uses the main menu.
  - The `CLevel` constructor loads player HUD models through `Render`.
- **Smart App Control.** It was enforcing and blocked newly built unsigned
  executables (`smoke-09`/`smoke-10`). The user turned it off on 2026-10-17. If
  launches fail with "Application Control policy has blocked this file", check it
  again.

## Next work

S02-T03 (CPU models, animation, collision and the material library split) and
S02-T04 (explicit clock/frame driver) can proceed; their prerequisite S02-T02 is met.
Keep GameState unpassed until S02-T05 constructs the real persistent, `CLevel` and
ALife context. Don't use placeholders.

**Run names.** Use a new name for every run. The last ones used are `campaign-07`,
`client-04` and `smoke-18`. Fixture `smoke-19` is unused. `smoke-20` was suggested
to the user for a manual run, so skip it.

**Launcher.** `Run-Stage2Smoke.ps1 -Scope Core|Services|GameState [-InjectFailure <service>]`.
Preserve the original installation, reference checkout, accepted source-client
runtime and saved-game fixture. The two G01 waivers are unchanged. Root files are
still uncommitted.
Previous handoff: [before S02-T03](session-handoff-before-S02-T03.md).
