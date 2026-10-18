# Session handoff — S02-T05d complete; next S02-T03d (skeleton collision, hit picking, physics shell)

## Next session: start S02-T03d

The campaign server now loads the level and runs server ticks with no device, so
the level-dependent tasks are unblocked. Start with S02-T03d (plan.md task
entry): skeleton collision, hit picking and physics-shell setup for one
actor/NPC fixture on the loaded level. S02-T04b (physics stepping, falling body)
and S02-T04c (objects, AI, ALife, activity policy) follow; S02-T05c (CPU sound
stimulus layer, B18) is still In Progress and owns the open sound finding below.
Keep the rhythm: one step, export the patch, record the result, commit.

G02 is In Progress. Renderer-free load, spawn and first ticks are done; no
gameplay behaviour (hit, AI decision, ALife offline switch, task, save) has
been checked headless. Stage 2 status (details in `plan.md`):

| Task | Status | Evidence |
|---|---|---|
| S02-T03a/b/c | Completed | patches 0004, 0005, 0007 |
| S02-T04a server clock and frame driver | Completed | patch 0006 |
| S02-T05a/b persistent, level, server, ALife | Completed | patches 0008, 0009 |
| S02-T05c CPU particles and sound stimuli | In Progress | particles done; sound is the `-nosound` stand-in |
| S02-T05d renderer-free level load, spawn, first ticks | Completed | [S02-T05d](../../validation/S02-T05d.md), patch 0010 |
| S02-T03d, S02-T04b, S02-T04c | Not Started | prerequisites now met |

## What the server does today (campaign-32, exe `39dd62ec…`)

`smoke-52 -Scope GameState` **passed** (exit 0, every stage in order):
- nine services, four fixtures, server stages from the fixture save (level
  `k00_marsh`, ALife, 1,074 entities) as in S02-T05b
- client stages in 2.2 s: direct in-process connection, CFORM (3,077,895
  triangles), physics world, `game_cl_Single`, level script process; HUD,
  render-level load, texture upload and precache skipped; `level.som` is
  absent from this level's content
- 50 server ticks (1 s of simulation): all 1,090 server entities spawned
  client-side (saved actor, 20 stalkers, 14 monsters, 374 zones, 120 items),
  370 physics steps, no current/view/control entity; then a clean teardown
  with no leaked object

Patches 0001–0010 rebuild all 59 changed files byte for byte
(`builds/logs/patch-reconstruction-11.json`). Next patch:
`python3 tools/export-engine-patch.py <name> 12`.

## Decisions and findings that shape the next tasks

- **No control or view entity.** `g_sv_Spawn` and the stalker spawn fallback
  (`ai_stalker.cpp:739`) adopt nothing on the campaign server. The saved actor
  is a real, enabled, local object and `g_actor`. Its first-eye camera and the
  single-actor `Actor()` assumptions are Stage 3 (G03) items. Code that reads
  `CurrentControlEntity()` or `CurrentViewEntity()` must null-check; anomaly
  zones stay scheduled (no fast mode) without one, and the activity policy is
  S02-T04c.
- **Map locations keep data, no spot widgets**; the actor stores its PDA task
  filters (`CActor::m_pda_task_filters`) so save/load needs no HUD.
- **Open: NPC voice sounds reported missing.** `sound_theme.init_npc_sound`
  logged 1,002 `Couldnt find sounds … characters_voice\…` errors in smoke-52;
  the accepted client's log has none, and both mount 67 archives. The Lua
  check is in archive-only scripts (`xr_sound`/`sound_theme`); likely the
  `-nosound` manager (no sound handle) or a lookup difference. Owned by S02-T05c.
- **Spawn tick is long.** The first `CLevel::OnFrame` spawns everything; the
  clock drops ~0.7 s of simulation twice (bounded catch-up, counted).
- **Crash loop method.** Each crash leaves a stack trace in
  `appdata/logs/*.log`; when the line looks impossible, disassemble the fault
  address with `dumpbin /DISASM /RANGE:` from `builds/build_1/vs2022` (used to
  find the stalker fallback). A crashed process is held by the crash handler
  until the 420 s watchdog; stopping it by hand is safe.

## Tools and runs

- **Last run names:** `campaign-32`, `client-14`, `smoke-52`, `capture-02`
  (`smoke-20` was reserved for the user). Check `builds/logs/S02-T02/`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`. GameState
  now passes with exit 0; `-campaign-ticks N` (default 50) sets the tick count.
- **Build:** `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`.
- **Smart App Control** is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous handoff: [before S02-T03d](snapshot-before-S02-T03d.md).
