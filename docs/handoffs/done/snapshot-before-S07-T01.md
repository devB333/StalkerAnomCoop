## Complete

# Session handoff — S06-T07 done; S06-T08 in progress

## State (2026-10-22)

- **G06 accepted** 2026-10-21 ([G06](../../validation/G06.md#user-decision)).
  Stage 6 stays In Progress for the tasks added at the gate. **The user wants
  to move through the gates; new bugs go to [known issues](../../known-issues.md),
  not into the current task.**
  - S06-T06 Cordon fixture: **Completed** ([S06-T06](../../validation/S06-T06.md)).
  - S06-T07: **Completed** 2026-10-22 ([S06-T07](../../validation/S06-T07.md)):
    patches 0037-0040, protocol v1.5; capture-56 (checkpoint) and capture-59
    (combat) match smoke-169.
  - S06-T08 storm and emission visuals: **In Progress** (started 2026-10-22).
  - S07-T07 respawn with a lootable corpse (after S07-T02/T03; starting kit).
- Engine: patches 0001–0040 (check `builds/stage2/patch-check-43b`); protocol
  **v1.5** `proto-build-32` (98/98, pt-30). Final build `campaign-217` /
  `client-119`. Tool defaults: `proto-build-32`, `--fixture cordon`.

## First thing next session

1. S06-T08 (storm and emission visuals): see plan.md and the task report
   `docs/validation/S06-T08.md` once it exists.

## Known issues (not to chase now)

[known issues](../../known-issues.md): K1 the other player's knife bends your
own first-person pistol (cause not found; what was ruled out is listed); K2
NPC shuffle (S10-T02); K3 slight choppiness (S11); K4 items an NPC held
after the server released it (S08).

## Hands-on demo for the user

`bash tools/start-coop-demo.sh` (Cordon, armed, boar after 30 s, 28 min;
`S06-T07 0 15` for no boar, 15 min); end by closing both windows, or
`bash tools/stop-coop-demo.sh`. Windows: alt-tab out, drag by the title bar.
Test drivers: `tools/Send-ClientActions.ps1` (keys, holds, clicks),
`tools/Test-WindowCapture.ps1` (both drive the real mouse; tell the user first).

## Run names

- Used (S06-T07): builds up to `campaign-217`, `client-119`; `proto-build-32`,
  `pt-30`; `win-1`, `act-1…5`, `demo-1…7`, `renc-1/2`, `rcombat-1`,
  `rsession-1`, `rsnap-1`, `tenc-1/2`, `tcombat-1`, `tpred-1`,
  `tsession-1`, `tsnap-1`; `smoke-169`; `capture-54…59`; patch check 43.
- Next: `campaign-218`, `client-120`, `proto-build-33`, `pt-31`,
  `smoke-170`, `capture-60`, patch check 44.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S06-T07 close](snapshot-before-S06-T07-close.md).
