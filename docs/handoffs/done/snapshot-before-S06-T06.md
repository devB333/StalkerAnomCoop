## Complete

# Session handoff — S06-T05 done; G06 awaits user acceptance

## State (2026-10-21)

Stage 6: S06-T01–T05 **Completed**. **G06 evidence recorded, awaiting user
acceptance** ([G06](../../validation/G06.md)). Do not start Stage 7 (S07-T01)
before the user accepts G06.

- Engine: patches 0001–0035 (0035 `coop-npc-encounter-and-deaths`, 9 files;
  check `builds/stage2/patch-check-38b`). engine-work equals the series.
- Final build `campaign-202` / `client-105`; protocol v1.4 (`proto-build-30`).
- Report: [S06-T05](../../validation/S06-T05.md). Design:
  [co-op session](../../coop-session.md#npc-encounter-and-deaths-s06-t05).

## What S06-T05 found and fixed this session

- **enc-9 crash:** actor 0 sprinted into a `k00_marsh` level changer; the
  headless server ran the native level change and crashed (`main_menu off`
  with no main menu). Level changers and `M_CHANGE_LEVEL` are now refused on
  the campaign server and co-op clients (L14). enc-12 hit one; the server
  logged it and kept running.
- **Pistol dropped mid-fight** (enc-10, combat-15, S06-T04's combat-10):
  player 1's rounds hit Fanat, whose melee (`xrs_facer.script`) always drops
  the punched actor's weapon. Native behaviour; no pick-up action yet (L01).
- **Spawn** now picks the reachable node 8–12 m from player 2 farthest from
  Fanat and player 1; `lethal` also fires 60 s after the spawn.
- **Single-player capture timeouts** (capture-32/33, 38–41) were the "Press
  any key to continue" screen (`keypress_on_start on`).
  `Run-ClientCollisionCapture.ps1` now turns it off in its runtime copy;
  capture-42/43 took 20 s and 30 s.

## Final evidence (campaign-202 / client-105)

- enc-15, enc-19 (nominal), enc-17 (5 % / 40 ms), enc-18 (10 % / 100 ms):
  all 13 checks pass. enc-16 (nominal): converged, but the boar never
  attacked (`npc_damage`).
- combat-16 21/21; session-12 replicas match; snap-18 12/12; smoke-163 66
  stages; capture-42 and capture-43 match.

## Decisions for the user at G06

Listed in [G06](../../validation/G06.md#decisions-for-the-user): friendly fire
default, the dead-player flow, the Azazel/Ironman death modes, the fixture
spot (Fanat interference), an earlier storm-visuals task. On acceptance, add
the G06 new limits to `docs/carried-limits.md` and record the decision in
G06.md, then start S07-T01.

## Offered to the user

A hands-on local session (server + two clients on this PC, no scripted
program) on the final build, to be prepared after the G06 stop.

## Run names

- Used: builds up to `campaign-202`, `client-105`; `enc-1…19`,
  `combat-14…16`, `session-11/12`, `snap-17/18`, `smoke-162/163`,
  `capture-38…43`; patch check 38.
- Next: `campaign-203`, `client-106`, `smoke-164`, `capture-44`, patch
  check 39.
- Right after a build, wait about a minute before a session.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before G06](../done/snapshot-before-G06.md).
