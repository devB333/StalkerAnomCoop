# Session handoff — S08-T08 completed; next S08-T09 (party quest presentation, anchor death)

## State (2026-10-26)

- **S08-T07 Completed** ([report](../../validation/S08-T07.md), patch 0049):
  equip and use are durable server actions; L37, L39 closed.
- **S08-T08 Completed** ([report](../../validation/S08-T08.md), patch 0050):
  post-checkpoint containers are recorded and re-created; the server's own
  inventory changes are journaled; ammunition merges durably on the server;
  L38, L40 closed. Two S08-T07 use-record defects fixed in 0050.
- G08 was accepted on the condition that L37–L43 are solved; Stage 8 stays
  In Progress until S08-T09 is done (L43 belongs to S10-T04). Every gameplay
  decision is listed for the user's approval in
  [Stage 8 closing decisions](../../stage8-closing-decisions.md).
- Engine patches 0001–0050, protocol v1.6 `proto-build-44` (st-14, st-15),
  final builds campaign-282 / client-181. Runs: ct-1…ct-12, ctr-1.
- Open from earlier: the user's K1/K5 hands-on retest.

## Next

S08-T09 (L41, L42): party task presentation on both clients (CampaignEvent
11, a client keeps a logic-free copy and evaluates no task), the scripts'
map spots to both players, news missed while away replayed on rejoin, the
party task surviving participant 1's death (participant 1's live actor runs
the turn-in; the task's script data stays in the original body's store);
fixture `--quest-variant 2` (anchor killed, auto respawn); tool
`tools/compare-coop-party.py`.

## Run names

- Next: `campaign-283`, `client-182`, `proto-build-45`, `st-16`, patch
  check 57; session names `pq-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S08-T09](../done/snapshot-before-S08-T09.md).
