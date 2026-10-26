# Session handoff — S08-T07 completed; next S08-T08 (containers, stacks)

## State (2026-10-26)

- **S08-T07 Completed** ([report](../../validation/S08-T07.md), patch 0049):
  equip (kind 9) and use (kind 10) are validated durable server actions from
  every client path; inventory layouts replicate (CampaignEvent 9); a used
  item stays used through every restart window. **L37 and L39 closed.**
- G08 was accepted on the condition that L37–L43 are solved; Stage 8 stays
  In Progress until S08-T08 and S08-T09 are done. Every gameplay decision
  taken on the way is listed for the user's approval in
  [Stage 8 closing decisions](../../stage8-closing-decisions.md).
- Engine patches 0001–0049, protocol v1.6 `proto-build-43` (st-12, st-13),
  final builds campaign-276 / client-175. Runs: eq-1…eq-13, eqr-1 (eq-3 unused).
- Open from earlier: the user's K1/K5 hands-on retest.

## Next

S08-T08 (L38, L40). Drafts are in the session scratchpad only (lost with
the session): the design is in the S08-T07 report's limits and in plan.md
S08-T08 — container `entity` records before the first committed transfer
touching a box or body after each publication (recovery re-creates it);
server-made changes to participant inventories observed per tick and
journaled (`pickup`/`transfer`/`tombstone`, combat consumables excluded);
ammunition merges in boxes on the server (`stack` record, CampaignEvent 10),
the client's local merge disabled; layout amounts u16.

## Run names

- Next: `campaign-277`, `client-176`, `proto-build-44`, `st-14`, patch
  check 56; session names `ct-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S08-T08](../done/snapshot-before-S08-T08.md).
