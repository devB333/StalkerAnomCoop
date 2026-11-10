## Complete

# Session handoff — G10 waits on the user's verification; S11-T00 (local hosting) built, waiting on a two-PC test

## State (2026-11-02)

- **G10**: all nine revisions in code (patch 0093), plus B26 (a world item's
  pickup crashed a client: patch 0094) and the demo kit. Only the user's
  hands-on verification is left ([S10-T07](../../validation/S10-T07.md) parts
  7-8, decision rows G10-1…G10-9 in
  [Stage 10 decisions](../../stage10-decisions.md)). Demo command for it:
  `bash tools/start-coop-demo.sh S10-T07 0 30 120 0 1` (last argument: the
  demo kit: a repair kit, a tent, a sleeping bag, an AK-74 and ammo, a worn pistol, sleepy
  players). demo-7's three bugs (B27 monster-animation crash, B28 tent and
  sleeping bag, B29 a dead player's body could not be looted) are fixed in
  patch **0097** (campaign-443 / client-325; bugfix-20 pass; S10-T07 part 9);
  their hands-on check is part of the user's verification. demo-8's bugs
  (B30 fire in the inventory, B31 hovering drops and the unseen tent) are
  fixed in patch **0098** (campaign-445 / client-327; S10-T07 part 10);
  bugfix-21 (pickup) was contaminated by keyboard input (B32: rerun it untouched). demo-9: the sleeping bag's window and a dead player's jumping body and grab crash (B28, B33) fixed in patch **0099**; demo-10: the tent shelters within 3 m in co-op (B34, changes G10-5: the user's OK pending) and the demo kit adds an AK-74 and ammo, patch **0100** (campaign-447 / client-330; part 12). demo-11: the damage tint per player (B35) and player 1's body looted on a client (B36), patch **0101** (campaign-448 / client-331; part 13); B37 (AK magazine) waits on the user's answer (which view); player 2's heavy-hit effects (weapon drop) need the user's OK. demo-12: NPCs hold and fire their weapons on a client (B38) and the status icons and blood overlay (B39), patch **0102** (campaign-449 / client-333; part 14). demo-13: a client's F on an NPC body crashed (B40): the use goes to the server, patch **0103** (campaign-450 / client-334; part 15). demo-14: the user OK'd player 2's heavy-hit effects and the AK magazine; tasks lost their map targets after player 1's respawn (B41: the server's actor update stopped) and a stash spot's text crashed the server (B42), patch **0104** (campaign-452 / client-336; part 16). The quest fixture's variant 2 is order-dependent (fix the fixture: give the patches to the survivor). demo-15: the weapon swap nudge on both screens and a server crash at the travel after player 1's respawn (B43), and no client resume after a travel (B44), patch **0105** (campaign-454 / client-338; part 17). demo-16: weapons out of sync after a travel (B45: a slot intent refused in the hold), patch **0106** (campaign-457 / client-341; part 18); demo-17 still out of sync (both actors' weapon change pending for good after the travel), patch **0107** (campaign-459 / client-342; part 19: the server updates a stalled inventory; cause not reproduced in the fixture). Next: the user's hands-on of 0107 (read `actor-weapons` and `tag=kick` in the demo's server log), then the first network test (the kits are stale: remake them then). **The test kits below are stale**
  (built from campaign-443 / client-325): the user asked to remake them only
  once everything works.
- **Stage 11 started by the user's direction** (Discord, 2026-11-02): S11-T00
  local hosting and the host and join menu ([S11-T00](../../validation/S11-T00.md),
  patches **0095** `local-hosting` and **0096** `dedicated-server`, engine-work
  equals the series of 96). Builds **campaign-442 / client-324**, protocol
  **proto-build-63** (st-38: 122/122). A player who hosts (menu-7, kit-9) and
  a dedicated server both players join (menu-10, kit-8) pass on this PC.
- **Test kits** (`tools/make-coop-kit.py`, from campaign-443 / client-325;
  remake them after any rebuild: their server runs from the build folder):
  `builds/kits/ServerHostKit/server-2/Start-Server.cmd` (this PC only hosts;
  prints the Tailscale address 100.94.188.110:27515 and a join code;
  `Stop-Server.cmd`), `builds/kits/client1HostKit/host-5/Start-Coop.cmd` (play
  and host here), `builds/kits/clientKit/client-5.zip` (every player: unzip,
  `Setup-Coop.ps1` with the Anomaly folder, `Start-Coop.cmd`, New game > Join
  co-op). Distribution kits go to `Kits/` only after the final test (plan.md's
  last note, `--dist`).
- The user is away and reachable on Discord (chat `1566639386420973638`,
  reply with the Discord reply tool); they asked to be kept updated.

## Next

1. The user's G10 verification (checklist sent on Discord) and the two-PC
   test over Tailscale with the kits. Record both (S10-T07 part 9, S11-T00).
2. Close G10 only on the user's word (present G10-1…G10-9 and the part 7
   limits: the workbench `ui_workshop` not routed; both players away keeps
   player 1's area online).
3. Stage 11 work that needs no hands-on: S11-T01 (adverse-network
   qualification on one PC with simulated loss and lag), S11-T03, S11-T04.
   S11-T02 needs the B5 permission (ProcDump).

## Picking this up

- Read `CLAUDE.md` / `AGENTS.md`, `plan.md` §I, this file, then the S10-T07
  and S11-T00 reports.
- **The menu** exists only when a client's app data holds `coop-menu.ltx`;
  test runs: `python3 tools/prepare-coop-menu.py <run> --test` then
  `tools/Run-CoopMenuTest.ps1 -Run <run> -Phases 2` (clients start without
  `-coop-config`). Kit runs: `tools/Test-CoopKits.ps1`.
- **CInifile drops spaces outside double quotes** (`_parse`): quote any value
  with spaces in a co-op profile (the config reader unquotes). Do not pass a
  quoted `-fsltx` path (the quotes stay in the name); without `-fsltx` the
  engine finds `fsgame.ltx` beside `bin`.
- **One cycle**: edit engine-work → `Build-Stage2.ps1` Campaign and Client
  with `-ProtocolBuild proto-build-63` (a protocol change needs a new
  protocol build first) → `python3 tools/export-engine-patch.py
  <NNNN-name>.patch <check>` → manifest, README, report, handoff, commit.
  Do not edit engine-work while a build compiles.
- Before a build or a run, check for running `AnomalyDX11` /
  `AnomalyCampaignServer` processes; ask the user before stopping one they
  started.

## Run names

- Used: `campaign-334…459`, `client-226…342`, `proto-build-53…63`,
  `st-28…38`, `bugfix-1…33`, `demo-1…17` (S10-T07), `menu-1…10`, `kit-1…9`
  (S11-T00), patch checks 77–119, kits host-1…5, friend-1…3, client-4…5,
  server-1…2.
- Next: `campaign-460`, `client-343`, `proto-build-64`, `st-39`,
  `bugfix-34`, `demo-18`, `menu-11`, `kit-10`, patch check 120, kits host-6,
  client-6, server-3.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S11-T00 and the two-PC test](snapshot-before-S11-T00-two-pc-test.md).
