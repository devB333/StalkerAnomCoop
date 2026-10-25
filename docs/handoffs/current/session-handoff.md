# Session handoff — S08-T03 (inventory recovery and reconnect) In Progress

## State (2026-10-24)

- **S08-T01 Completed** ([report](../../validation/S08-T01.md), patch 0045):
  durable authoritative pickup; race fixture with the real use key; crash
  matrix, impaired run pass.
- **S08-T02 Completed** ([report](../../validation/S08-T02.md), patch 0046):
  drops (kind 6) and box/corpse transfers (kind 7) from the client's own
  inventory events; A-B-A refused `Stale`; ownership-invariant checker;
  **EntityId reservation** (journal kind 9) fixed ids being reissued after
  a restart. Final builds campaign-256 / client-156, proto-build-41.
- **S08-T03 In Progress:** engine-work has the **item-graph check** (not yet
  exported; next patch 0047): the server hashes every published item's
  owner every 10 s and sends it as CampaignEvent 8 in the lifecycle order;
  each client compares its replica at that point and checks its own
  inventory (`tools/compare-coop-item-graph.py`). Builds campaign-257 /
  client-157. inv-1 (nominal transfer fixture): 35/35 points match, own
  inventories match. Running/queued: inv-2 (external kill at 100 s), inv-3
  (kill at 125 s), inv-4 (10 %/100 ms), inv-5 (`transfer:after-flush:3`),
  inv-6 (pickup race, kill at 95 s). No `docs/validation/S08-T03.md` yet.
- **S08-T04 research** (quest candidates in Cordon) was started; reward
  allocation between two players is a user decision (plan R09) to raise
  when S08-T05 needs it.
- K1/K5 user retest still open (any client from client-148 on contains
  0043/0044).

## Next

1. Read inv-2…6 (`compare-coop-item-graph.py`, `compare-coop-transfer.py
   --expect-restart`, `compare-coop-pickup.py --expect-restart`), write
   `docs/validation/S08-T03.md` (crash matrix incl. external kills, the
   accepted durable schema: pickup, transfer, entity-reserve), export 0047.
2. S08-T04: pick the quest, map its mutations, ask the user the reward
   policy before S08-T05.

## Run names

- Used: campaign-247…257, client-149…157, proto-build-40/41, st-7…10,
  pick-1…15, tr-1…18 (tr-3…7 prepared only), inv-1…6, patch checks 50–52.
- Next: `campaign-258`, `client-158`, `proto-build-42`, `st-11`, `inv-7`,
  `tr-19`, `pick-16`, patch check 53.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S08-T03](../done/snapshot-before-S08-T03.md).
