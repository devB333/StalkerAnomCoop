# Session handoff — S06-T07 in progress (jump and lean)

## State (2026-10-22)

- **G06 accepted** 2026-10-21 ([G06](../../validation/G06.md#user-decision)).
  Stage 6 stays In Progress for the tasks added at the gate:
  - S06-T06 Cordon fixture: **Completed** ([S06-T06](../../validation/S06-T06.md)).
  - S06-T07 jump and lean: **In Progress**. Part 1 done (user request):
    windowed clients take the mouse only on a click inside, so windows can
    be dragged ([S06-T07](../../validation/S06-T07.md), patch 0037, build
    `campaign-212` / `client-114`). Jump and lean not started.
  - S06-T08 storm and emission visuals.
  - S07-T07 respawn with a lootable corpse (after S07-T02/T03; the respawned
    player gets the faction's starting kit).
- Engine: patches 0001–0037 (check `builds/stage2/patch-check-40b`);
  protocol `proto-build-31` (wire v1.4 unchanged; session join fix). Final
  build `campaign-212` / `client-114`. Tool defaults: `proto-build-31`, and
  `prepare-coop-session.py --fixture cordon`.
- **Cordon fixture:** user-made save `cordon fixture`
  ([record](../../cordon-fixture.json)), Loner, outdoors about 140 m east of
  the Rookie Village, slight radiation (0.3 %/min). The marsh fixture stays
  for the single-player smoke and captures.

## Fixed in S06-T06 (found by running in Cordon)

- Campaign server crash on blood/bullet marks (no wallmark engine on the
  server; marks within 100 m of the origin reached it, Cordon only).
- Join race under loss: other channels now wait until the client has sent a
  message in-session (it then has its Welcome).
- Client replica kept a stale offline object when the server reused its id
  for an object of the same section (participant 2's PDA never appeared).
- Capture harness: `keypress_on_start off` (the "hung" captures were waiting
  at "Press any key").

## S06-T07 first steps

Add jump and lean (left/right) to the input buttons (`protocol/` input,
`kButtonFire` is bit 1), press the native keys on the player's own actor on
the server like fire (`CoopServerHost.cpp` `apply_inputs`), predict jump on
the client, validate (no jump while airborne or dead), and add legs to the
movement program; rerun the Stage 6 programs in Cordon.

## Run names

- Used: builds up to `campaign-210`, `client-113`; protocol `proto-build-31`,
  tests `pt-29`; `cordon-1…12`, `cenc-1…22`, `ccombat-1…5`, `cpred-1…5`,
  `csession-1…5`, `csnap-1…5`; `smoke-164…168`; `capture-44…53`; patch
  check 39.
- Also used: `campaign-211` (failed: a demo held the server exe), `client-114`,
  `campaign-212`, `win-1`, `demo-1` (S06-T06), `demo-2` (S06-T07, prepared on client-114: armed, boar after 30 s), patch check 40.
- Next: `campaign-213`, `client-115`, `proto-build-32`, `pt-30`,
  `smoke-169`, `capture-54`, patch check 41.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S06-T07 continued](../done/snapshot-before-S06-T07-continued.md).
