## Complete

# Session handoff — Stage 8 condition met; waiting for the user's G08 confirmation

## State (2026-10-26)

- **S08-T07, S08-T08, S08-T09 Completed** (patches 0049–0051):
  [S08-T07](../../validation/S08-T07.md) equip/use/consumption,
  [S08-T08](../../validation/S08-T08.md) containers and stacks,
  [S08-T09](../../validation/S08-T09.md) party quest presentation and the
  anchor's death. **L37–L42 closed**; L43 stays with S10-T04.
- The [G08 condition review](../../validation/G08.md#condition-review--ready-for-the-users-confirmation-2026-10-26)
  and the [gameplay decisions](../../stage8-closing-decisions.md)
  (T07-1…9, T08-1…8, T09-1…7) **wait for the user's confirmation**. Stage 8
  stays In Progress until then; the proposed new carried limits are listed
  in the review (rows to add on confirmation).
- Fixed on the way: a restart after participant 1's respawn could not start
  (S07-T07 era); a client's Lua "Use" would have dropped the eaten item.
- Protocol v1.6 `proto-build-44` (st-14, st-15); final builds campaign-292 /
  client-191 (patches 0052, 0053). Runs: eq-1…18, eqr-1 (S08-T07), ct-1…12, ctr-1
  (S08-T08), pq-1…6, pqr-1…6 (S08-T09); eq-3 unused.
- 2026-10-27, the user's review: T07-1…4, 6, 7, 9 approved; T07-5 (no
  firing during the use animation) and T07-8 (thirst, sleep, psy health per
  player) revised and implemented in patch 0052; each HUD shows the server's
  hunger, stamina and radiation (patch 0053); proposed limits L44–L49 in
  carried-limits.md. T08 and T09 decisions and the gate still wait.
- Open from earlier: the user's K1/K5 hands-on retest.

## Next

On the user's answer: record the confirmation (or changes to the decisions)
in G08.md and plan.md, add the new carried-limit rows, set Stage 8
Completed; then S09-T01 (party travel: request and two-player consent).

## Run names

- Next: `campaign-293`, `client-192`, `proto-build-45`, `st-16`, patch
  check 60.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before G08 confirmation](./snapshot-before-G08-confirmation.md).
