# Session handoff — S10-T07 bug pass: B46 and B47 fixed in code, the user's hands-on next

## State (2026-11-03)

- **S10-T07** (bug pass before Stage 11) continues through the user's
  hands-on demos. engine-work equals the series of **109** patches (patch
  check 122). Builds **campaign-467 / client-350**, protocol
  **proto-build-63**. Report: [S10-T07](../../validation/S10-T07.md) parts
  9–21; bug table: [known issues](../../known-issues.md).
- **B46 (patch 0108):** travel matches vanilla (decision T07-11 in
  [Stage 10 decisions](../../stage10-decisions.md)): only fast travel and the
  backpack check combat. The user's demo-19: travel back and forth worked.
- **B47 (patch 0109), found in demo-19:** after player 1 walked into the
  Cordon ↔ Marsh passage the server updated no object for the rest of the
  session (shots from 0,0,0, NPCs standing). The passage's space restrictor
  ran the installed travel box (`ui_sr_teleport`) on the server, which
  paused the device (`set_device_paused_script`); a paused device runs no
  `UpdateCL`. Same in demo-16/17: **the root cause of B45**. Now the campaign
  server refuses every pause (`device-pause-refused`) and shows no window;
  `update-stall` logs any object near a player that stops updating; the
  travel checker's new `updates` check fails on it. Reproduced by
  **bugfix-38/39** (the fixture through the passage's approach:
  `--travel-changer to_marsh_1 --travel-approach=-278.6,-21.0,-268.7`),
  passing in **bugfix-40/41** (8/8) and **bugfix-42** (B46's round, 9/9).
- **Not verified:** shooting an NPC after a travel and the refusal wording
  (hands-on; the fixture does not shoot); a fast travel in combat.
- **Still open from before:** the quest fixture's variant 2 is
  order-dependent; patch 0107's kick stays as a guard though B45's cause is
  now B47.
- **Decisions waiting on the user's word at G10:** G10-5 revised (B34), B35,
  T07-11 (the user's own choice).
- **Test kits in `builds/kits/` are stale** (campaign-443 / client-325);
  remake them only once the user says everything works.
- demo-19's launcher stopped before copying its logs into
  `builds/logs/S10-T07/run-demo-19/` (empty); they are under
  `builds/stage2/test-install/S10-T07/demo-19-*/appdata/logs/`.

## Next

1. The user's hands-on (demo-20): travel through the Cordon ↔ Marsh passage
   both ways, fight the Marsh renegades after arriving, check the refusal
   wording: `bash tools/start-coop-demo.sh S10-T07 0 30 120 0 1`. Afterwards
   check the server log for `update-stall` and the engine log for
   `device-pause-refused`.
2. When the user says everything works: remake the kits
   (`python3 tools/make-coop-kit.py server|host|client <name>`) and run the
   first two-PC test over Tailscale ([S11-T00](../../validation/S11-T00.md)).
3. Close G10 only on the user's word.

## Picking this up

- Read `CLAUDE.md` / `AGENTS.md`, `plan.md` §I, this file, then the S10-T07
  report parts 9–21 and the known issues B27–B47.
- **One cycle:** edit engine-work → `Build-Stage2.ps1` Campaign and Client
  with `-ProtocolBuild proto-build-63` → a fixture run → `python3
  tools/export-engine-patch.py <NNNN-name>.patch <check>` → manifest, README,
  report, known issues, handoff, commit.
- **Travel fixtures:** `--travel c1,c2 --travel-variant 2|5`, optionally
  `--travel-changer <name part> --travel-approach=x,y,z`; check with
  `compare-coop-travel.py --variant 2|5` (`updates` is the B47 check). Others:
  `--sleep`, `--pickup`, `--loot`, `--quest c1,c2 --quest-variant 2
  --auto-respawn 5`; arm with `arm = 1` in the server's `coop.ltx`. Don't
  type in the terminal while a fixture runs.
- Installed scripts: `python3 tools/xray-db-extract.py -q grep 'pattern'
  /mnt/c/Users/djb20/StalkerAnom/db/configs/scripts.db0`, or `extract -m
  <name> -o <scratch dir>`.

## Run names

- Used: `campaign-334…467`, `client-226…350`, `proto-build-53…63`,
  `st-28…38`, `bugfix-1…42`, `demo-1…19` (S10-T07), `menu-1…10`, `kit-1…9`
  (S11-T00), patch checks 77–122, kits host-1…5, friend-1…3, client-4…5,
  server-1…2.
- Next: `campaign-468`, `client-351`, `proto-build-64`, `st-39`,
  `bugfix-43`, `demo-20`, `menu-11`, `kit-10`, patch check 123, kits host-6,
  client-6, server-3.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the B47 hands-on](../done/snapshot-before-B47-hands-on.md).
