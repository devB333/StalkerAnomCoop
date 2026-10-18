## Complete

# Session handoff — S02-T04 and S02-T05 complete; next: S02-T06

## Next session: S02-T06, native campaign simulation and checkpoint round-trip

S02-T05c is **Completed** (patch 0014): the campaign server runs the engine's
own sound core on the CPU with no audio device and no listener (`-snd_cpu`),
so sounds have feedback, the native emitters propagate AI sound events, the
level finds the hearers and the NPCs' `feel_sound_new` runs. smoke-60 passed
`campaign-hearing-passed`; the client (client-18, capture-07) passed the same
fixture and the comparison matches. Details, source citations and numbers:
[S02-T05c report](../../validation/S02-T05c.md). With it, S02-T04 and S02-T05
are Completed in `plan.md`; S02-T03 was already. G02 remains In Progress.

S02-T06 (`plan.md` §C) is next: run a bounded campaign fixture with no
external client and exercise AI, physics, time, Lua/task execution and native
save/reload; record the actor-context work for Stage 3; no permanent fake
global player (D1). Suggested order, from the open findings:

1. **Actor Lua binder.** `actor_binder:update()`'s first-update block never
   ran on the server (no `collectgarbage before=` line in any server log; the
   client has it). Instrument `CActor::shedule_Update` →
   `CGameObject::shedule_Update` → `CScriptBinder::shedule_Update`
   (`GameObject.cpp:1091`) and the time-event pump (`level.add_call` →
   `ProcessEventQueue`, `_g.script:451-473`) to find which link is missing.
   `objects_per_update` staying at 65534 (should drop to 20 one second after
   the first update) is the cheap indicator; the activity capture's `meta`
   row reports it.
2. **Combat and hits headless.** In capture-07 a Clear Sky stalker fought
   monsters near the actor within 31 s (336 shooting events, bullet impacts,
   deaths); the server's 21 s saw none. Decide whether that is chance or a
   gap (vision, hits, `Level_Bullet_Manager`), with a fixture that forces an
   encounter.
3. **ALife actor position.** Objects at 674.3–674.9 m from the actor (online
   distance 675 m) switched online on the client, not the server; verify the
   server's `CSE_ALifeCreatureActor::o_Position` follows the live actor.
4. **Save/reload.** Native save on the server, reload, compare counters.
5. Read `tools/xray-db-extract.py --help` style usage in its docstring to
   read Lua and configs from the game's `.db` archives (extracted text is
   cp1251: `LC_ALL=C grep -a`).

G02 status (details in `plan.md`):

| Task | Status | Evidence |
|---|---|---|
| S02-T03a–d | Completed | patches 0004, 0005, 0007, 0011 |
| S02-T04a–c | Completed | patches 0006, 0012, 0013; [T04c](../../validation/S02-T04c.md) |
| S02-T05a–d | Completed | patches 0008, 0009, 0010, 0014; [T05c](../../validation/S02-T05c.md) |
| S02-T06 | Not Started | |

## Findings that shape the next tasks

- **Hearing changes AI behaviour on the server**: once sounds existed the
  walked stalker moved under its own planner during the observation; the
  activity fixture's walked-fraction rule now measures from the walk command
  (`walk_distance`, last `end` field; the compare tool accepts both layouts).
- **`g_bLoaded`** (set by the device after its first frame) gates
  `SoundEvent_Register`; the campaign clock now sets it after its first
  `seqFrame` pass. Look for other `g_bLoaded`-style device-set flags when a
  native path is silent on the server.
- **The actor is a hearer** (`STYPE_REACTTOSOUND`); it received the
  stalker's footsteps on both sides. Its `feel_sound_new` → Lua callbacks are
  Stage 3 actor context.
- **Actor-owned items are not crows on the server** (no view entity); item
  `UpdateCL` logic runs only on scheduled updates (Stage 3).
- The `-nosound` stand-in silently broke scripts (1,002 `sound_theme` aborts
  from sounds without handles). Disabling a presentation service must keep
  the data path (D2).
- Two 2D AI events were dropped by the CPU core in smoke-60 (untraced; the
  client hears them at the camera). Content ALife parameters (R11): switch
  distance 750 m, ALife manager every tick, time factor 6, `objects_per_update`
  65534 on the server until the actor binder issue is fixed.
- Earlier decisions stand: no control/view entity on the server; map
  locations keep data without spot widgets; the spawn tick is long.

## Tools and runs

- **Last run names:** `campaign-44`, `client-18`, `smoke-60`, `capture-07`
  (all passed; `campaign-40/41` failed compile, `smoke-58/59` failed fixtures
  on the way); `smoke-20` reserved for the user. Next: `campaign-45`,
  `client-19`, `smoke-61`, `capture-08`. Check `builds/logs/S02-T02/`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`; GameState
  expects `campaign-activity-passed` and `campaign-hearing-passed` and copies
  `server-activity.csv` and `server-hearing.csv`. The server runs with
  `-snd_cpu` (refuses `-nosound`).
- **Client captures:** `python3 tools/prepare-stage2-smoke.py <run> <task>`
  then `Run-ClientCollisionCapture.ps1 -Run <run> -Capture collision|physics|activity|hearing`
  (`hearing` runs the activity fixture too; task `S02-T05c`).
- **Compare:** `tools/compare-activity-capture.py`, `tools/compare-hearing-capture.py`.
- **Build:** `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`. Quote
  the script path when calling `powershell.exe` from WSL. A change to
  `xr_object.h` or `Sound.h` rebuilds most of the solution (~7–10 min); a
  game-module-only change takes 1–2 min.
- **Patches:** 0001–0014 exported; next export is
  `python3 tools/export-engine-patch.py <name> 16`.
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous snapshot: [before S02-T06](./snapshot-before-S02-T06.md).
