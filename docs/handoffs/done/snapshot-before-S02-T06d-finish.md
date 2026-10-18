## Complete

# Session handoff — S02-T06c complete; next: S02-T06d native save/reload

## Next session: finish S02-T06 with the save/reload round-trip

S02-T04 and S02-T05 are Completed; S02-T06 is In Progress with T06a, T06b
and T06c done (details: [S02-T06 report](../../validation/S02-T06.md)):

- **S02-T06c (patch 0018):** combat between NPCs runs headless. The shared
  combat fixture (`xrGame/CampaignCombatCapture.{h,cpp}`) spawns a
  `boar_normal` through the native ALife spawn 12 m from the fixture stalker
  (actor god mode for the fixture's duration) and observes the fight through
  observation hooks (every bullet the bullet manager adds, every hit event it
  commits, every `CEntityAlive::Hit` and `Die`) plus the stalker's visual
  memory and enemy selection and the monster's enemy manager. Server
  (smoke-78, `campaign-combat-passed`): boar online in 0.2 s, seen and
  selected, 19 rounds, 6 fire wounds, death at 9.5 s, real-time. Client
  (client-21, capture-09): same rules pass (2 rounds, 3 fire wounds, death at
  10.3 s); `compare-combat-capture.py` matches.

Remaining S02-T06 items:

1. **Native save/reload (S02-T06d).** Save on the server (the actor's binder
   save, `alife_storage_manager`, is reachable since T06a), reload in the same
   process or a second run, compare the counters the smoke already reports
   (object counts, ALife registries, the actor's position, the stalker's
   state). The server's save path is the host's: `CLevel::ClientSave` /
   `xrServer` `M_SAVE_GAME` → `CALifeSimulator::save`; check whether the
   console `save` command path or a direct call is the honest entry.
2. **G02 evidence record** (plan.md §D) once T06d is in.

## Findings that shape the next tasks

- **Hits are events through the host's own server loop** (`GE_HIT` via
  `xrServer`), so the first hit lands one tick after the first impact on both
  products; anything that routes events differently in Stage 3 must keep
  that ordering.
- **Physics contact damage is live headless**: a dog that ran into the
  stalker's character controller died of a 1.58 strike (smoke-77). The same
  installed behaviour as on the client; a fast small monster is a poor target
  for the "bullet hits the monster" rule, which is why the default is a boar.
- **Fixture spawns go online within ~10 ticks** through the level registry's
  time-limited switch update; no manual switch was needed.
- The `-s02-combat-*` options (`section`, `distance`, `ms`, `online-ms`) are
  read from the command line by both products.
- **Timing instruments** in the smoke's tick report: `S02-T06 timing`
  (script binder updates; the actor's is ~5.8 ms per call, every tick) and
  `S02-T06 frame_ms`. Use them first when the clock logs `behind`.
- `CScriptBinder` logs native faults with symbols; Lua runtime errors are
  fatal on both products.
- `tools/xray-db-extract.py grep` escapes its pattern unless `--regex`; the
  section lookups in this session used literal `[name]` patterns with
  `-m creatures`.

## Tools and runs

- **Last run names:** `campaign-65`, `smoke-78`, `client-21`, `capture-09`
  (all passed; `campaign-62…64`, `smoke-77` were iterations); `smoke-20`
  reserved for the user. Next: `campaign-66`, `client-22`, `smoke-79`,
  `capture-10`. Check `builds/logs/S02-T02/`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`; the server
  runs with `-snd_cpu`. GameState expects `campaign-activity-passed`,
  `campaign-hearing-passed`, `campaign-actor-position-passed` and
  `campaign-combat-passed`; the loaded report expects `hud=1 game_ui=1
  ui_in_frame=0`. The GameState smoke takes ~50 s wall.
- **Client captures:** `python3 tools/prepare-stage2-smoke.py <run> <task>`
  then `Run-ClientCollisionCapture.ps1 -Run <run> -Capture collision|physics|activity|hearing|combat`
  (`combat` is task `S02-T06c`; `hearing` writes both the activity and
  hearing CSVs, task `S02-T05c`).
- **Compare:** `tools/compare-activity-capture.py`,
  `tools/compare-hearing-capture.py`, `tools/compare-combat-capture.py`.
- **Build:** `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`; quote
  the script path from WSL. Game-module-only changes build in 1–2 min.
- **Patches:** 0001–0018 exported; next export is
  `python3 tools/export-engine-patch.py <name> 20`.
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous snapshot: [before S02-T06d](./snapshot-before-S02-T06d.md).
