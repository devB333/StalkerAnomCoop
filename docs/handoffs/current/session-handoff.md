# Session handoff — G10 waits on the user's verification; S11-T00 (local hosting) built, waiting on a two-PC test

## State (2026-11-02)

- **G10**: all nine revisions in code (patch 0093), plus B26 (a world item's
  pickup crashed a client: patch 0094) and the demo kit. Only the user's
  hands-on verification is left ([S10-T07](../../validation/S10-T07.md) parts
  7-8, decision rows G10-1…G10-9 in
  [Stage 10 decisions](../../stage10-decisions.md)). Demo command for it:
  `bash tools/start-coop-demo.sh S10-T07 0 30 120 0 1` (last argument: the
  demo kit: a repair kit, a tent, a worn pistol, sleepy players).
- **Stage 11 started by the user's direction** (Discord, 2026-11-02): S11-T00
  local hosting and the host and join menu ([S11-T00](../../validation/S11-T00.md),
  patch **0095** `local-hosting`, engine-work equals the series of 95). Builds
  **campaign-440 / client-322**, protocol **proto-build-63** (st-38: 122/122).
  menu-7 (new campaign, then continue and rejoin) and kit-7 (the delivered
  kits) pass on this PC.
- **Kits for the user** (`tools/make-coop-kit.py`):
  `builds/kits/host-3/Start-Coop.cmd` (this PC hosts; its server runtime is
  `builds/stage2/test-install/S11-T00/kit-host-3-server`) and
  `builds/kits/friend-3.zip` (for the friend's PC: unzip, run
  `Setup-Coop.ps1`, then `Start-Coop.cmd`; in the game New game > Join co-op:
  Host = the host's Tailscale address, this PC's is 100.94.188.110, Password =
  the join code the host's menu shows).
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

- Used: `campaign-334…440`, `client-226…322`, `proto-build-53…63`,
  `st-28…38`, `bugfix-1…19`, `demo-1…6` (S10-T07), `menu-1…7`, `kit-1…7`
  (S11-T00), patch checks 77–107, kits host-1…3, friend-1…3.
- Next: `campaign-441`, `client-323`, `proto-build-64`, `st-39`,
  `bugfix-20`, `demo-7`, `menu-8`, `kit-8`, patch check 108, kits host-4,
  friend-4.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S11-T00 and the two-PC test](../done/snapshot-before-S11-T00-two-pc-test.md).
