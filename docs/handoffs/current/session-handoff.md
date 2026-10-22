# Session handoff — Stage 6 done (S06-T08 completed); next S07-T01

## State (2026-10-22)

- **G06 accepted** 2026-10-21 ([G06](../../validation/G06.md#user-decision)); the
  three tasks added at the gate are done, so **every Stage 6 task is
  Completed**. New bugs go to [known issues](../../known-issues.md), not into
  the current task.
  - S06-T06 Cordon fixture, S06-T07 jump and lean: Completed.
  - S06-T08 storm and emission visuals: **Completed** 2026-10-22
    ([S06-T08](../../validation/S06-T08.md), patch 0041, no protocol change).
    Three new `CampaignEvent` kinds: environment (cycle, the two blended
    descriptors, the weather effect and its time left), thunderbolt (the
    server draws each bolt), presentation (the scripts' camera/post-process
    effectors, sounds and particles from the participant and broadcast
    outboxes). cweather-2 (nominal), cweather-3 (5 % loss, 40 ms) and
    cweather-4 (final build) pass every check on both clients. Regression:
    smoke-170, capture-60/61, wenc-1 pass. L09 closed; L28/L29 added (the
    emission's actor-relative presentation and gameplay use participant 1's
    actor: S10-T05).
  - S07-T07 respawn with a lootable corpse (after S07-T02/T03; starting kit).
- Engine: patches 0001–0041 (check `builds/stage2/patch-check-44b`); protocol
  **v1.5** `proto-build-32` (98/98, pt-30). Final build `campaign-222` /
  `client-124`. Tool defaults: `proto-build-32`, `--fixture cordon`.

## First thing next session

1. Start **S07-T01** (specify recoverable state and critical transaction
   boundaries): see plan.md §C Stage 7. It is a specification task; read
   `docs/validation/S02-T06.md` part 5 (the native checkpoint round-trip) and the
   checkpoint code (`CampaignCheckpoint.cpp`, `CoopServerHost.cpp`
   `start_resync`) first.
2. Weather demo for the user, if asked: `prepare-coop-session.py <run> --task
   S06-T08 --weather c1,c2` then `Run-CoopSession.ps1 ... -ClientSeconds
   '480,470'` (storm 10 s after both are in the world, emission 70 s after;
   `--psi-storm` for a psi-storm instead; untested on a psi-storm).

## Known issues (not to chase now)

[known issues](../../known-issues.md): K1 the other player's knife bends your
own first-person pistol; K2 NPC shuffle (S10-T02); K3 slight choppiness
(S11); K4 items an NPC held after the server released it (S08).

## Hands-on demo for the user

`bash tools/start-coop-demo.sh` (Cordon, armed, boar after 30 s, 28 min;
`S06-T07 0 15` for no boar, 15 min); end by closing both windows, or
`bash tools/stop-coop-demo.sh`. Windows: alt-tab out, drag by the title bar.
Test drivers: `tools/Send-ClientActions.ps1` (keys, holds, clicks),
`tools/Test-WindowCapture.ps1` (both drive the real mouse; tell the user first).
Never screen-capture the desktop; use `PrintWindow` on the game window.

## Run names

- Used (S06-T08): builds `campaign-218…222`, `client-120…124`; `cweather-1…4`,
  `wenc-1`; `smoke-170`; `capture-60`, `capture-61`; patch check 44a/44b.
- Next: `campaign-223`, `client-125`, `proto-build-33`, `pt-31`,
  `smoke-171`, `capture-62`, patch check 45.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S07-T01](../done/snapshot-before-S07-T01.md).
