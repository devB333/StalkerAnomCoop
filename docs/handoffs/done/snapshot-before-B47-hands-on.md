## Complete

# Session handoff — S10-T07 bug pass: B46 fixed in code, the user's hands-on next

## State (2026-11-03)

- **S10-T07** (bug pass before Stage 11) continues through the user's
  hands-on demos. Bugs B27–B45 were confirmed by the user in demo-18.
  engine-work equals the series of **108** patches (patch check 121).
  Builds **campaign-461 / client-344**, protocol **proto-build-63**.
  Report: [S10-T07](../../validation/S10-T07.md) parts 9–20; bug table:
  [known issues](../../known-issues.md).
- **B46 fixed in code (part 20, patch 0108).** Travel back to Cordon was
  refused "in combat" for 84 s because a far renegade kept player 2 as its
  enemy. The user chose **"match vanilla"** (decision T07-11 in
  [Stage 10 decisions](../../stage10-decisions.md), revising G09's T01-5):
  only a PDA fast travel or a backpack jump is refused in combat; a level
  changer, a guide and other scripts' `ChangeLevel` are taken
  (`travel_combat_rule`, `travel-combat-ignored` lines). A refused travel,
  fast travel, session or repair no longer shows "You can't pick that up."
  **bugfix-35** (travel variant 5, new: the boar hunting player 2): pass
  8/8, the party travelled to Darkscape. bugfix-34 (variant 0) failed as a
  test gap: variant 0's nine rounds cannot run since S09-T02.
- **Not verified:** the client wording (hands-on); a fast travel in combat
  (unchanged code, no fixture). The server ignores the players' "allow fast
  travel in combat" option (limit in the report).
- **Still open from before:** the quest fixture's variant 2 is
  order-dependent (give the patches to the participant it does not kill);
  B45's root cause (read `actor-weapons tag=kick` lines if it recurs).
- **Decisions waiting on the user's word at G10:** G10-5 revised (a tent
  shelters within 3 m, B34); B35 (player 2's heavy-hit effects, approved in
  demo-14); T07-11 is the user's own choice.
- **Test kits in `builds/kits/` are stale** (campaign-443 / client-325);
  remake them only once the user says everything works.

## Next

1. The user's hands-on of B46 (travel Cordon → Marsh → Cordon with an enemy
   around; the refusal wording): `bash tools/start-coop-demo.sh S10-T07 0 30
   120 0 1` (demo-19; opens both game windows).
2. When the user says everything works: remake the kits
   (`python3 tools/make-coop-kit.py server|host|client <name>`, names below)
   and run the first two-PC test over Tailscale
   ([S11-T00](../../validation/S11-T00.md)).
3. Close G10 only on the user's word (G10-1…G10-9, the B34/B35 decisions,
   T07-11, the limits in the report).

## Picking this up

- Read `CLAUDE.md` / `AGENTS.md`, `plan.md` §I, this file, then the S10-T07
  report parts 9–20 and the known issues B27–B46.
- **One cycle:** edit engine-work → `Build-Stage2.ps1` Campaign and Client
  with `-ProtocolBuild proto-build-63` → a fixture run → `python3
  tools/export-engine-patch.py <NNNN-name>.patch <check>` → manifest, README,
  report, known issues, handoff, commit. Don't edit engine-work while a build
  compiles; a running demo server locks the campaign executable.
- **Travel fixtures:** `--travel c1,c2 --travel-variant 2` (round 0 carried
  out) or `--travel-variant 5` (the hunted round, carried out); check with
  `compare-coop-travel.py --variant 2|5`. Others used: `--sleep`,
  `--pickup`, `--loot`, `--quest c1,c2 --quest-variant 2 --auto-respawn 5`;
  a plain demo-kit session with `arm = 1` and `demo_kit = 1` in the server's
  `coop.ltx`. Don't type in the terminal while a fixture runs.
- Installed scripts: `python3 tools/xray-db-extract.py -q grep 'pattern'
  /mnt/c/Users/djb20/StalkerAnom/db/configs/scripts.db0`, or `extract -m
  <name> -o <scratch dir>`.

## Run names

- Used: `campaign-334…461`, `client-226…344`, `proto-build-53…63`,
  `st-28…38`, `bugfix-1…35`, `demo-1…18` (S10-T07), `menu-1…10`, `kit-1…9`
  (S11-T00), patch checks 77–121, kits host-1…5, friend-1…3, client-4…5,
  server-1…2.
- Next: `campaign-462`, `client-345`, `proto-build-64`, `st-39`,
  `bugfix-36`, `demo-19`, `menu-11`, `kit-10`, patch check 122, kits host-6,
  client-6, server-3.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the B46 hands-on](snapshot-before-B46-hands-on.md).
