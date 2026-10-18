# Session handoff — S02-T06a/b complete; next: S02-T06 combat and save/reload

## Next session: continue S02-T06

S02-T04 and S02-T05 are Completed; S02-T06 is In Progress with two items
done today (details: [S02-T06 report](../../validation/S02-T06.md)):

- **S02-T06a (patches 0015, 0016):** the actor's Lua binder never completed
  its first update on the server (a native fault in a HUD accessor,
  swallowed by the binder, then a fatal Lua error from a HUD script). The user
  chose an inert server-side game UI (blocker B20): the server now creates the
  UI core, fonts, HUD manager and the real `CUIGameSP` as CPU objects that
  are never updated or drawn. The binder completes its first update,
  `objects_per_update` is 20 as on the client, the server runs real-time,
  smoke-74 passes and the client (client-20, capture-08) matches on both
  fixtures.
- **S02-T06b (patch 0017):** the ALife actor follows the live actor on the
  server (3 m teleport, 50 ticks, gap 0.000; stage
  `campaign-actor-position-passed`, smoke-76).

Remaining S02-T06 items, suggested order:

1. **Combat between NPCs headless.** capture-07 saw a Clear Sky stalker fight
   monsters within 31 s on the client; the server's 21 s saw none. Build a
   fixture that forces an encounter (spawn or teleport a hostile within sight
   of a stalker) and observe vision, shooting, hits (`Level_Bullet_Manager`),
   health and death on both products; the hearing fixture already counts the
   shots' AI events.
2. **Native save/reload.** Save on the server (the actor's binder save,
   `alife_storage_manager`, is now reachable), reload in the same process or a
   second run, compare the counters the smoke already reports; then the G02
   evidence record (plan.md §D).

## Findings that shape the next tasks

- **The HUD window's per-frame update costs ~100 ms on the server** (no
  device); the inert UI is therefore never updated (`CHUDManager::OnFrame`
  returns on the server). Profile before any server-side UI update.
- **Timing instruments** in the smoke's tick report: `S02-T06 timing`
  (script binder updates; the actor's is ~5.5 ms per call, every tick) and
  `S02-T06 frame_ms` (`SLevelFrameTiming`: game events, object update, script
  pumps, Lua GC). Use them first when the clock logs `behind`.
- **`CScriptBinder` logs native faults with symbols** (`! CScriptBinder::…
  native exception …` plus `fault at`/`frame` lines) instead of deleting the
  binder silently; Lua runtime errors are fatal on both products.
- `Device.dwWidth/dwHeight` are 1024×768 on the server (UI base resolution)
  when no window set them.
- Offline ALife scheduling is ~1.4 advances per tick with the binder fixed
  (the earlier ~91 per tick was the broken binder's 65534).
- `tools/compare-activity-capture.py` tolerates ±10 on the begin counts
  (simulation squads vary per run) and accepts the old `end` row layout.
- `tools/xray-db-extract.py` reads Lua and configs from the `.db` archives
  (`list`, `extract`, `grep`; extracted text is cp1251: `LC_ALL=C grep -a`).

## Tools and runs

- **Last run names:** `campaign-61`, `smoke-76`, `client-20`, `capture-08`
  (all passed; `campaign-53…60`, `smoke-68…75` were iterations); `smoke-20`
  reserved for the user. Next: `campaign-62`, `client-21`, `smoke-77`,
  `capture-09`. Check `builds/logs/S02-T02/`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`; the server
  runs with `-snd_cpu`. GameState expects `campaign-activity-passed`,
  `campaign-hearing-passed` and `campaign-actor-position-passed`; the loaded
  report expects `hud=1 game_ui=1 ui_in_frame=0`.
- **Client captures:** `python3 tools/prepare-stage2-smoke.py <run> <task>`
  then `Run-ClientCollisionCapture.ps1 -Run <run> -Capture collision|physics|activity|hearing`
  (`hearing` writes both the activity and hearing CSVs; task `S02-T05c`).
- **Compare:** `tools/compare-activity-capture.py`, `tools/compare-hearing-capture.py`.
- **Build:** `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`; quote
  the script path from WSL. Game-module-only changes build in 1–2 min.
- **Patches:** 0001–0017 exported; next export is
  `python3 tools/export-engine-patch.py <name> 19`.
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous snapshot: [before S02-T06c](../done/snapshot-before-S02-T06c.md).
