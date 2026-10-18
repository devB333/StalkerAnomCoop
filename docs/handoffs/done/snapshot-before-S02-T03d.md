# Session handoff — S02-T05a/b complete; next S02-T05d (level load, spawn, first tick)

## Next session: start S02-T05d

Begin with S02-T05d (renderer-free level load, spawn and first tick) using the
suffix-task entry in `plan.md`, this handoff and the findings below. The work
mixes engine edits in `builds/build_1/engine-work`, Windows builds through
`tools/Build-Stage2.ps1`, and smoke runs through `tools/Run-Stage2Smoke.ps1`.
Keep the rhythm used so far: finish one step, export the patch, record the
result in the task report, then commit. Smart App Control is off on this
machine, so newly built unsigned executables run; if a launch fails with
"Application Control policy has blocked this file", it has been turned back on.

G02 is In Progress. No headless gameplay, level geometry, physics step or
simulation tick has run yet. Stage 2 status (details in `plan.md`):

| Task | Status | Evidence |
|---|---|---|
| S02-T03a materials without media | Completed | [S02-T03a](../../validation/S02-T03a.md), patch 0004 |
| S02-T03b CPU-only model pool | Completed | [S02-T03b](../../validation/S02-T03b.md), patch 0005 |
| S02-T03c bones match the client | Completed | [S02-T03c](../../validation/S02-T03c.md), patch 0007 |
| S02-T04a server clock and frame driver | Completed | [S02-T04a](../../validation/S02-T04a.md), patch 0006 |
| S02-T05a persistent object with no device | Completed | [S02-T05a](../../validation/S02-T05a.md), patch 0008 |
| S02-T05b level object, server and ALife from the save | Completed | [S02-T05b](../../validation/S02-T05b.md), patch 0009 |
| S02-T05c CPU particles and sound stimuli | In Progress | particles done (patch 0009); sound is the engine's `-nosound` stand-in, AI hearing not modelled |
| S02-T05d renderer-free level load, spawn, first tick | Not Started | next |
| S02-T03d, S02-T04b, S02-T04c | Not Started | follow T05d's level load |

Start with the [internal context definition](../../campaign-server-context.md), which
lists the service order, the resolved blockers and what remains. Services start
in this order and stop in reverse: `sound` (interim), `cpu-models`, `spatial-db`,
`materials`, `script-engine`, `campaign-registries`, `application`, `persistent`,
`frame-driver`.

## What the server does today (campaign-27, exe `f695b6af…`)

`smoke-44 -Scope GameState` (exit 3, as expected, not a pass):
- all nine services start with no graphics device, window, input or sound device
- fixtures: CPU models, persistent/environment, 300-tick frame driver, bone capture
- the native start sequence for the fixture save `player - autosave`, then the
  server loading stages in 2.9 s: `xrServer`, `game_sv_Single`, ALife loads the
  save, picks level **`k00_marsh`** from the saved actor (ID 0, graph vertex 223),
  loads the level AI graph (532,432 vertices), and switches 1,074 entities online
- planned stop `level-context-required`, then native `net_Stop`, level deletion,
  persistent disconnect, and every service stopped in reverse

Patches 0001–0009 rebuild all 44 changed files byte for byte. Use
`python3 tools/export-engine-patch.py <name> <n>` for the next one.

## Findings that shape S02-T05d

- **Scripts create particles and sounds while Lua initialises.** The ALife
  constructor re-initialises the script engine; installed scripts construct
  `particles_object`s and `sound_object`s then. Particle definitions now load on
  the CPU with the model pool. Sound uses `-nosound` for now (the launcher passes
  it); no sound has feedback, so AI hearing needs the CPU stimulus layer (B18).
- **The client stages are the next blockers** (see the boundary doc's corrections
  section): `IGame_Level::Load` creates the HUD (`CHitMarker` shaders), calls
  `Render->level_Load` (all presentation), sets sound geometry; `net_start_client5`
  uploads textures; `client6` creates the game UI and precaches; spawned objects
  create blood wallmarks, lights with textures and map spots. Keep CFORM, AI data,
  the physics world, the level script process and `level.som` real.
- **View entity.** Native spawn makes the saved actor the view entity client-side;
  decide this in T05d, when spawns are processed.
- **Loading pump.** The server pumps `g_loading_events` itself with a deadline;
  the native idle loop never pumps while the device is not ready.

## Tools and runs

- **Last run names:** `campaign-27`, `client-13`, `smoke-47`, `capture-02`.
  `smoke-20` was suggested to the user for a manual run, so skip it.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>` (now also copies the
  fixture save pair), then `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`.
  Options: `-InjectFailure <service>`, `-Models a,b`, `-ExpectModelFailure`.
  GameState has a 420 s timeout.
- **Client capture:** `prepare-stage2-smoke.py <run> S02-T03c`, then
  `Run-ClientBoneCapture.ps1 -Run <run>` (opens a client window briefly).
- **Smart App Control** is off (user, 2026-10-17); newly built unsigned
  executables are blocked when it is on.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous handoff: [before S02-T05d](snapshot-before-S02-T05d.md).
