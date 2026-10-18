# Session handoff — S02-T03 complete (T03d collision); next S02-T04b (physics stepping, falling body)

## Next session: start S02-T04b

S02-T03d is done and the parent S02-T03 is Completed, so the CPU model,
animation and collision services are all in place. Start S02-T04b (plan.md task
entry): native physics stepping and a falling-body collision fixture on the
loaded level (H06: pin one movable shell and a level floor query, observe gravity,
collision and settling; compare with a client capture). The physics world already
steps during the first ticks (372–418 steps in 50 ticks) and 247 item and
physic-object shells exist, so the fixture is a matter of choosing and driving one
shell, not of creating the world. S02-T04c (objects, AI, ALife, activity policy)
follows; S02-T05c (CPU sound stimuli, B18) still owns the open sound finding.
Keep the rhythm: one step, export the patch, record the result, commit.

G02 is In Progress. Stage 2 status (details in `plan.md`):

| Task | Status | Evidence |
|---|---|---|
| S02-T03a–d | **Completed** (parent Completed) | patches 0004, 0005, 0007, 0011; [T03d](../../validation/S02-T03d.md) |
| S02-T04a server clock and frame driver | Completed | patch 0006 |
| S02-T05a/b/d persistent, level, server, ALife, spawn, first ticks | Completed | patches 0008, 0009, 0010 |
| S02-T05c CPU particles and sound stimuli | In Progress | particles done; sound is the `-nosound` stand-in |
| S02-T04b, S02-T04c | Not Started | prerequisites met |

## What S02-T03d showed (campaign-35 / client-15, smoke-54, capture-03)

`smoke-54 -Scope GameState` **passed** (exit 0) with the new stage
`campaign-collision-passed`. On the loaded level, with no device: 491 skeleton
collision forms with 1,780 elements, 36 alive entities all with a physics
character, 247 shells with 264 elements; the actor's picks hit `bip01_head`
from above, `bip01_spine1` from the front and the ground 0.5 m below its feet.
The normal client on the same save (`capture-03`) builds **identical structure
for every common object**; the actor's trunk hit boxes agree within 2.3 cm.
Compare: `compare-03-smoke-54.json` and `compare-03-smoke-53.json` in
`builds/logs/S02-T03d/`, both `match`. No engine behaviour changed: patch 0011
is the capture itself. Patches 0001–0011 rebuild all 61 changed files byte for
byte (`builds/logs/patch-reconstruction-12.json`). Next patch:
`python3 tools/export-engine-patch.py <name> 13`.

## Findings that shape the next tasks

- **Pose differs only where the client has what the server lacks.** The
  actor's arms and `front` pick differ (client holds the HUD weapon, torso
  follows the camera): G03 items, reported not asserted. NPC poses differ
  because the client's AI runs in real time. Lamp 10736's light bone differs
  by random flicker state.
- **Hand/toe hit boxes far from the body, on both sides.** Some stalkers'
  hand, finger and toe elements lie 3–56 m from their origin at the sampled
  frame on the server (12 objects) and on the client (13 objects, different
  ones). Native callback-driven bones (IK legs, stalker bone callbacks); not a
  CPU-only regression. Recorded for S02-T04c/G03.
- **Script spawns vary per run.** 1,090 / 1,095 / 1,094 / 1,097 objects across
  smoke-52/53/54 and the client (crows, bolts). The compare tool reports IDs
  present on one side only instead of failing.
- **Sound finding confirmed server-side.** The server logs 1,002
  `Couldnt find sounds … characters_voice\…` errors while NPCs spawn; the client
  run of the same save logs none. The cause is the server's `-nosound` stand-in
  or lookup (S02-T05c), not the content.
- Earlier decisions stand: no control/view entity on the server; map
  locations keep data without spot widgets; the spawn tick is long (bounded
  catch-up, counted). Crash loop method: stack trace in `appdata/logs/*.log`,
  `dumpbin /DISASM /RANGE:` from `builds/build_1/vs2022` when needed.

## Tools and runs

- **Last run names:** `campaign-35`, `client-15`, `smoke-54`, `capture-03`
  (`campaign-33` failed to compile; `smoke-20` reserved for the user). Next:
  `campaign-36`, `client-16`, `smoke-55`, `capture-04`. Check `builds/logs/S02-T02/`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`; GameState now
  expects `campaign-collision-passed` and copies `server-collision.csv` into the run log.
- **Client collision capture:** `python3 tools/prepare-stage2-smoke.py <run> S02-T03d`,
  then `Run-ClientCollisionCapture.ps1 -Run <run>` (opens an 800×600 window,
  about a minute); compare with
  `python3 tools/compare-collision-capture.py <server.csv> <client.csv>`.
- **Build:** `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`.
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous handoff: [before S02-T04b](snapshot-before-S02-T04b.md).
