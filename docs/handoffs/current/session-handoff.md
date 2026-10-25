# Session handoff — G08 ready for acceptance (Stage 8 complete)

## State (2026-10-25)

Every Stage 8 task is Completed; **[G08](../../validation/G08.md) waits for
the user's acceptance** (plan.md §D). Engine patches 0001–0048, protocol
v1.6 `proto-build-42`, final builds **campaign-267 / client-167**.

- [S08-T01](../../validation/S08-T01.md) durable pickup (patch 0045).
- [S08-T02](../../validation/S08-T02.md) drops and box/corpse transfers,
  ownership invariants, **EntityId reservation** (patch 0046).
- [S08-T03](../../validation/S08-T03.md) in-stream item-graph check; fixed a
  rejoining client's stale tombstones (K4) and a negative recovery wait
  (patch 0047).
- [S08-T04](../../validation/S08-T04.md) / [T05](../../validation/S08-T05.md)
  / [T06](../../validation/S08-T06.md): Sidorovich's "Mobbing the Mob" as a
  party task (action kind 8), the user's reward policy of 2026-10-24
  (pooled patches; each paid for his own; goodwill to both; one stash);
  answers only after a published generation holds them (contract §8a);
  restarts at every window; fixed recovered tasks being unable to complete
  with two actors (patch 0048).
- Open from earlier: the user's K1/K5 hands-on retest (any client from
  client-148 on has 0043/0044).

## On acceptance

1. Add the proposed carried-limit rows from G08 to `docs/carried-limits.md`
   (L37…), update L01/L10/L26/L36 as G08 lists, mark G08 Completed in
   plan.md (§I row, roadmap table row 8 → Completed), record the decision
   in `docs/validation/G08.md`.
2. Next: Stage 9, S09-T01 (request and two-player consent for a level
   transition); read plan.md §Stage 9 first.

## Run names

- Used this session: campaign-247…267 (263 built only; 251/252/260/264
  failed to compile, fixed), client-149…167 (163 interrupted),
  proto-build-40…42, st-7…11, pick-1…15, tr-1…18 (tr-3…7 prepared only),
  inv-1…12, q-1…6 (q-3/q-4 abandoned), qn-1/2, qx-1…6, patch checks 50–54.
- Next: `campaign-268`, `client-168`, `proto-build-43`, `st-12`, patch
  check 55.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before G08 acceptance](../done/snapshot-before-G08-acceptance.md).
