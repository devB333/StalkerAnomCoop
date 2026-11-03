## Complete

# Session handoff — S10-T07 bug pass: B46 open, then the two-PC network test

## State (2026-11-03)

- **S10-T07** (bug pass before Stage 11) continues through the user's
  hands-on demos. This session: demo-7…demo-18, bugs **B27–B46**, patches
  **0097–0107** (engine-work equals the series of 107; patch check 119).
  Builds **campaign-459 / client-342**, protocol **proto-build-63**.
  Report: [S10-T07](../../validation/S10-T07.md) parts 9–19; bug table:
  [known issues](../../known-issues.md).
- **The user confirmed every fix through B45 in demo-18** ("the fix
  worked"), including player 2's heavy-hit effects and the AK magazine (B37
  closed: not a co-op defect).
- **Open: B46.** After travelling to the Marsh, every travel request back to
  Cordon was refused 27 (in combat) for over a minute:
  `travel-in-combat slot=1 enemy=26119:sim_default_renegade_2->10115`
  (`builds/stage2/test-install/S10-T07/demo-18-server/appdata/logs/coop-server.log`,
  1793694290…353). One client worded it "Not while something is hunting
  you.", the user's client "You can't pick that up." Two things to settle:
  (a) whether that renegade really hunted player 2 nearby or stayed its
  enemy from afar (its distance, online state, memory: the hunted check is
  `travel_in_combat` in `CoopServerHost.cpp`); (b) the client that showed the
  generic pickup line for a kind 11 refusal (the result handler's generic
  branch, as B34 was for sleep: add kind 11/12 to its exclusions).
- **Known but not a defect:** the quest fixture's variant 2 is
  order-dependent (bugfix-28): when slot 1 wins the accept race, the fixture
  gives the patches to slot 0 and then kills slot 0. Fix the fixture: give
  the patches to the participant it does not kill.
- **B45's root cause is still unknown** (after a travel both actors' weapon
  change stayed pending: their `CActor::UpdateCL` / `CInventory::Update` did
  not complete it in the demos, but did in the fixture). Patch 0107 completes
  a stalled change from the server's tick (`actor-weapons tag=kick`); read
  those lines if it recurs.
- **Decisions waiting on the user's word at G10:** G10-5 revised: a tent
  shelters within 3 m in co-op (B34); player 2 gets the installed heavy-hit
  shake, sound and weapon-drop chance (B35; the user approved it in demo-14).
- **Test kits in `builds/kits/` are stale** (campaign-443 / client-325).
  The user: remake them only once everything works, then the first two-PC
  network test ([S11-T00](../../validation/S11-T00.md)).

## Next

1. Fix B46 (both parts above), rebuild both products, run the travel
   fixture (`--travel c1,c2 --travel-variant 2`, armed with `arm = 1` in the
   server profile) and hand the build to the user:
   `bash tools/start-coop-demo.sh S10-T07 0 30 120 0 1`.
2. When the user says everything works: remake the kits
   (`python3 tools/make-coop-kit.py server|host|client <name>`, names below)
   and run the first two-PC test over Tailscale (S11-T00).
3. Close G10 only on the user's word (G10-1…G10-9, the B34/B35 decisions,
   the limits in the report).

## Picking this up

- Read `CLAUDE.md` / `AGENTS.md`, `plan.md` §I, this file, then the S10-T07
  report parts 9–19 and the known issues B27–B46.
- **One cycle:** edit engine-work → `Build-Stage2.ps1` Campaign and Client
  with `-ProtocolBuild proto-build-63` → a fixture run → `python3
  tools/export-engine-patch.py <NNNN-name>.patch <check>` → manifest, README,
  report, known issues, handoff, commit. Don't edit engine-work while a build
  compiles; a running demo server locks the campaign executable (ask the user
  to stop it).
- **Fixtures used this session:** `--sleep c1,c2` (`compare-coop-sleep.py`),
  `--pickup c1,c2`, `--loot c1,c2`, `--quest c1,c2 --quest-variant 2
  --auto-respawn 5`, `--travel c1,c2 --travel-variant 2`; a plain demo-kit
  session by setting `arm = 1` and `demo_kit = 1` in the server's `coop.ltx`.
  Don't type in the terminal while a fixture runs (keys reach the game
  windows: B32 was that).
- Installed scripts are read with `python3 tools/xray-db-extract.py -q grep
  'pattern' /mnt/c/Users/djb20/StalkerAnom/db/configs/scripts.db0` (it does
  not recurse into folders).

## Run names

- Used: `campaign-334…459`, `client-226…342`, `proto-build-53…63`,
  `st-28…38`, `bugfix-1…33`, `demo-1…18` (S10-T07), `menu-1…10`, `kit-1…9`
  (S11-T00), patch checks 77–119, kits host-1…5, friend-1…3, client-4…5,
  server-1…2.
- Next: `campaign-460`, `client-343`, `proto-build-64`, `st-39`,
  `bugfix-34`, `demo-19`, `menu-11`, `kit-10`, patch check 120, kits host-6,
  client-6, server-3.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before B46 and the network test](snapshot-before-B46-and-network-test.md).
