# Session handoff — S02-T04c server part passed; next: client comparison, then complete S02-T04

## Next session: finish S02-T04c with the client comparison

The server half of S02-T04c is done and passing: the campaign server now
prioritises simulation from activity anchors (the online actors) instead of
the camera, and the GameState smoke (`smoke-57`) runs objects, the scheduler,
the stalker planners, ALife switching, offline scheduled work and the ALife
clock from the server ticks, then walks stalker 19409 along a 10.5 m reachable
path to arrival (`campaign-activity-passed`). Details, source citations and
numbers: [S02-T04c report](../../validation/S02-T04c.md). Patch 0013 is exported
(see below).

Remaining steps, in order:

1. Build the normal client from the same source:
   `Build-Stage2.ps1 -Run client-17 -Product Client` (check no compile line
   has `XR_CAMPAIGN_SERVER`; the activity policy must stay disabled on the
   client, which the capture's `meta` row reports as `policy=0`).
2. Run the client capture: `capture-06` is already prepared
   (`builds/stage2/test-install/S02-T04c/capture-06`), so run
   `Run-ClientCollisionCapture.ps1 -Run capture-06 -Capture activity`
   (800×600 window, about 10 s observe + walk + load, under two minutes).
3. Compare: `python3 tools/compare-activity-capture.py builds/logs/S02-T04c/server-activity-smoke-57.csv builds/logs/S02-T04c/run-capture-06/client-activity.csv`
   (copy the client CSV beside the server one as the earlier tasks did), then
   record the result in the T04c report, set T04c and parent S02-T04 to
   Completed in `plan.md` (§C task entry, §I table and the §I closing
   paragraph), and commit.
4. Then S02-T05c (CPU sound stimuli, B18) or S02-T06 per `plan.md`.

Known risk for step 2: on the client the fanatic stalker's Lua schemes run in
real time; if a scheme re-targets the walk, the `npc` rows show the
destination vertex changing during phase 1 (the compare tool reports
`dest_vertex_changes_in_walk`). If the client fixture fails only on arrival,
record it as a finding and compare the rest.

G02 is In Progress. Stage 2 status (details in `plan.md`):

| Task | Status | Evidence |
|---|---|---|
| S02-T03a–d | Completed | patches 0004, 0005, 0007, 0011 |
| S02-T04a/b | Completed | patches 0006, 0012 |
| S02-T04c | **In Progress** (server passed; client comparison pending) | patch 0013; [T04c](../../validation/S02-T04c.md) |
| S02-T05a/b/d | Completed | patches 0008, 0009, 0010 |
| S02-T05c | In Progress | particles done; sound is the `-nosound` stand-in |
| S02-T06 | Not Started | |

## Findings that shape the next tasks

- **Content ALife parameters:** switch distance 750 m (online < 675 m,
  offline > 825 m), ALife manager `schedule_min = schedule_max = 1`,
  `objects_per_update` 65534, time factor 6. So on `k00_marsh` every object is
  online, ALife updates every tick, and offline scheduled work is ~91 brain
  updates per 20 ms tick. The activity policy (60 m crow radius around the
  anchors) is what bounds per-frame object work: 42.9 crows of ~305 active.
  Record for R11 before Stage 3 adds the second anchor.
- Before this task, the server's null view entity made *every* top-level
  object a crow and scheduled by distance to the level origin
  (`xr_object.cpp` `UpdateCL`); now replaced by the anchor policy
  (`SActivityPolicy g_activity_policy`, enabled only while the campaign level
  runs). The client is unchanged.
- Earlier findings stand: engine physics is gravity 19.62 m/s² with velocity
  /1.01 per step; sleep timing is phase-dependent; hand/toe hit boxes far from
  the body (G03); the server logs 1,002 `Couldnt find sounds` errors (S02-T05c).
- Earlier decisions stand: no control/view entity on the server; map
  locations keep data without spot widgets; the spawn tick is long.

## Tools and runs

- **Last run names:** `campaign-39`, `smoke-57` (both passed), `client-16`,
  `capture-05`; `capture-06` prepared but not run; `smoke-20` reserved for the
  user. Next: `campaign-40`, `client-17`, `smoke-58`, `capture-06` (run it),
  then `capture-07`. Check `builds/logs/S02-T02/`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`; GameState
  now also expects `campaign-activity-passed` and copies `server-activity.csv`.
- **Client captures:** `python3 tools/prepare-stage2-smoke.py <run> <task>`
  then `Run-ClientCollisionCapture.ps1 -Run <run> -Capture collision|physics|activity`.
- **Build:** `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`. Quote
  the script path when calling `powershell.exe` from WSL. A change to
  `xr_object.h` rebuilds everything (campaign-39 took 10 min).
- **Patches:** next export is `python3 tools/export-engine-patch.py <name> 15`.
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous handoff: [before the T04c client comparison](session-handoff-before-S02-T04c-walk.md).
