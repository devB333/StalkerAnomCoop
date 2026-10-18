# Session handoff — S02-T04b complete (native physics stepping); next S02-T04c (objects, AI, ALife, activity policy)

## Next session: start S02-T04c

S02-T04b is done: with no device, the server ticks step the native physics
world exactly as the client does. Start S02-T04c (plan.md task entry): drive
objects, the scheduler, AI and ALife on the loaded level and replace the
camera-derived simulation prioritisation with a server activity policy (H04:
record frame/clock and callback sequence, an identified online NPC's scheduled
and planner work, an ALife switch and offline scheduled work; move a fixture
NPC along a short reachable path and record arrival). The level, the spawned
entities, the physics world and the collision data all exist after 50 ticks
(S02-T05d, T03d), so the first action is to pick one stalker from the save
(the S02-T03d capture lists them with IDs) and trace what its scheduled update
does in the campaign server versus the client. The two open findings for T04c
are the hand/toe hit boxes far from the body (callback-driven bones, see T03d)
and the view-based scheduler priority. S02-T05c (CPU sound stimuli, B18) still
owns the sound finding. Keep the rhythm: one step, export the patch, record the
result, commit.

G02 is In Progress. Stage 2 status (details in `plan.md`):

| Task | Status | Evidence |
|---|---|---|
| S02-T03a–d | Completed (parent Completed) | patches 0004, 0005, 0007, 0011; [T03d](../../validation/S02-T03d.md) |
| S02-T04a server clock and frame driver | Completed | patch 0006 |
| S02-T04b native physics stepping, falling body | **Completed** | patch 0012; [T04b](../../validation/S02-T04b.md) |
| S02-T05a/b/d persistent, level, server, ALife, spawn, first ticks | Completed | patches 0008, 0009, 0010 |
| S02-T05c CPU particles and sound stimuli | In Progress | particles done; sound is the `-nosound` stand-in |
| S02-T04c | Not Started | prerequisites met |

## What S02-T04b showed (campaign-38 / client-16, smoke-56, capture-05)

`smoke-56 -Scope GameState` **passed** (exit 0) with the new stage
`campaign-physics-passed`. A 100 kg wooden crate (object 10865, 20 m from any
other collidable object) lifted 1 m above its saved resting place fell under
the engine's gravity, touched the level floor at step 34, was put to sleep by
the physics world at step 54 and rests 0.5 mm from its saved height, in 104
native 10 ms steps over 54 server ticks. The normal client running the same
fixture on the same save (`capture-05`) **matches**: identical free fall over
all 34 steps, the same contact step and materials, final positions 4.5 mm
apart (`compare-05-smoke-56.json` in `builds/logs/S02-T04b/`). No engine
behaviour changed: patch 0012 is the fixture itself. Patches 0001–0012
rebuild all 63 changed files byte for byte
(`builds/logs/patch-reconstruction-13.json`). Next patch:
`python3 tools/export-engine-patch.py <name> 14`.

## Findings that shape the next tasks

- **The engine's physics is not textbook.** Gravity is 19.62 m/s² and every
  body's velocity is divided by 1.01 after each step (`PHElement.cpp:458-470`,
  `Physics.cpp:31, 54`). The first fixture run failed its own pure-gravity
  rule by 15 %; the rule now uses the native recurrence. Any server-side
  prediction (Stage 6) must do the same.
- **Sleep timing is phase-dependent, not a server defect.** The world's
  disabling counter decides when a settled body deactivates (steps 54–73 across
  runs); compare settled state, not the sleep step.
- Earlier findings stand: limb pose differs only where the client has what the
  server lacks (G03); hand/toe hit boxes far from the body on both sides
  (S02-T04c/G03); script spawns vary per run; the server logs 1,002
  `Couldnt find sounds` errors that the client does not (S02-T05c).
- Earlier decisions stand: no control/view entity on the server; map locations
  keep data without spot widgets; the spawn tick is long (bounded catch-up,
  counted). Crash loop method: stack trace in `appdata/logs/*.log`,
  `dumpbin /DISASM /RANGE:` from `builds/build_1/vs2022` when needed.

## Tools and runs

- **Last run names:** `campaign-38`, `client-16`, `smoke-56`, `capture-05`
  (`campaign-36` failed to compile; `capture-04` passed but its script step
  failed; `smoke-20` reserved for the user). Next: `campaign-39`, `client-17`,
  `smoke-57`, `capture-06`. Check `builds/logs/S02-T02/`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`; GameState
  expects `campaign-collision-passed` and `campaign-physics-passed` and copies
  `server-collision.csv` and `server-physics.csv` into the run log.
- **Client captures:** `python3 tools/prepare-stage2-smoke.py <run> S02-T03d`
  then `Run-ClientCollisionCapture.ps1 -Run <run>` (collision), or task
  `S02-T04b` and `-Capture physics` (falling body); each opens an 800×600
  window for about a minute. Compare with
  `python3 tools/compare-collision-capture.py` or
  `python3 tools/compare-physics-capture.py <server.csv> <client.csv>`.
- **Build:** `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`. Quote
  the script path when calling `powershell.exe` from WSL, or the backslashes
  are lost.
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous handoff: [before S02-T04c](../done/session-handoff-before-S02-T04c.md).
