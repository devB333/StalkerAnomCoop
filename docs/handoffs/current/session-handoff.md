# Session handoff — G03 accepted; next: S04-T01

## State at the end of the session (2026-10-19)

**Stage 3 Completed.** S03-T01..T06 Completed; the user **accepted
[G03](../../validation/G03.md) for development on 2026-10-19** with its limits
carried to their named owners. Stage 4 starts with S04-T01.

**S03-T05** — native and Lua participant context contract
([report](../../validation/S03-T05.md), patch 0025, `23fa820f…3f0a`):
participant script contexts entered only by native entry points (dialogue,
phrases, use, owned tasks, captured deferred work), strict `campaign.*` Lua
lookups, a server-only compatibility layer (`db.actor`/`AC_ID` per context,
out-of-context reads audited), presentation outboxes. Both participants ran
the installed mechanic quest concurrently with rewards on their own actors.

**S03-T06** — server light model for AI vision, added this session by user
decision ([report](../../validation/S03-T06.md), patch 0026, `0e7a46aa…776e`,
20 files, `patch-check-29b`, 204 files byte-equal): every tick, per
participant, the client's own light tracking (`CROS_impl::update`) runs on the
actor and one object of its view, with the environment blended at its eye
(`CEnvironment::UpdateCPU`) and the level's sun and static lights loaded as
CPU objects (`Render->cpu_lighting_*`, R4). No other NPC is refreshed (user
decision). At steady state server = client: 0.0213 / 0.0213 (capture-22 vs
smoke-144). AI observers read the participants 14–42× brighter than the old
darkness floor; 38.9 µs per tick.

Final runs: **smoke-144** (campaign-122, GameState, all 66 stages),
**smoke-145** (Identity, 18 stages), capture-22 (lighting match),
capture-23/24 (single-player quest `match: true`, client-32).

## Found this session (open items)

1. Lit vision changed behaviour (all handled or recorded): Clear Sky guards
   now engage a monster spawned beside actor 1 in their base (the S03-T04
   awareness phase moves actor 1 to a quiet vertex and back); the installed
   campfire hint reached the server UI and faulted the actor binder
   (`game.start_tutorial` is now a presentation); **smoke-143's commanded
   stalker (S02-T04c) stopped mid-walk once** in nine light-model runs, never in
   37 earlier runs — cause not established; watch it, rerun if it recurs and
   log the stalker's weapon-handler and danger state.
2. Installed scripts outside a context still decide for actor 0 (167 lines,
   S03-T05 audit); `has_alife_info`, `alife():actor()`, story object not
   remapped; party vs personal info portions are S08.
3. k00_marsh has no static point lights (its `build.lights` holds 196
   directional records), so the static-light term is 0 there on both products;
   other levels will exercise it.
4. capture-21's client hung at start-up (`Starting engine...`), killed at
   600 s; unexplained, unrelated to the level.
5. Carried: server-driven camera yaw (Stage 4); `EntityId`s, task owners and
   participant script state across a checkpoint (Stage 7); fixture `PlayerId`s
   (S04-T04); detached release (S04-T05); friendly fire and god mode per
   participant (Stage 6); presentation delivery (Stage 4); hearing-stage
   `npc_walk_events=0` (not seen in smoke-134..145).

## Next

1. **S04-T01 — Validate GameNetworkingSockets in isolation**. First
   action: pin a GNS release and its crypto/protobuf dependencies against the
   recorded VS2022 v143 toolchain and build an isolated listener and two peers
   outside engine-work.

## Tools and runs

- Last run names: `campaign-122`, `client-32`, `smoke-145`, `capture-24`,
  `patch-check-29a/b`. Next: `campaign-123`, `client-33`, `smoke-146`,
  `capture-25`, patch check `30`.
- Run PowerShell scripts with the Windows path in single quotes from bash.
  Don't build while a GameState smoke runs (the server is real-time).
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Identity|GameState` (GameState
  ~4 min, 66 stages).
- Lighting check: `prepare-stage2-smoke.py capture-N S03-T06`,
  `Run-ClientCollisionCapture.ps1 -Run capture-N -Capture lighting -CheckpointRun <smoke>`,
  `python3 tools/compare-lighting-capture.py builds/logs/S02-T02/run-<smoke>/server-lighting-snapshot.csv builds/logs/S03-T06/run-capture-N/client-lighting.csv`.
- Quest check: `-Capture quest|quest-persisted` (task S02-T06e); copy
  `builds/logs/S02-T06e/run-capture-N/client-quest*.csv` to
  `builds/logs/S02-T06/` before `compare-quest-capture.py`.
- Installed scripts for grepping: `python3 tools/xray-db-extract.py -q extract -o <dir>
  builds/build_2/test-install/source-client/db/configs/scripts.db0` (cp1251/CRLF).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02 carried limits are unchanged.
Previous snapshot: [before S04-T01](../done/snapshot-before-S04-T01.md).
