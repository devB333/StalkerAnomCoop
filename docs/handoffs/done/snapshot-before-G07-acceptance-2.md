## Complete

# Session handoff — Stage 7 complete; G07 ready for acceptance

## State (2026-10-23)

- **Every Stage 7 task is Completed** under engine patch 0042
  (`patches/engine/0042-checkpoint-generations-and-journal.patch`, patch
  check 45) and protocol `proto-build-35` (v1.5; `coop_store` library).
  Final builds `campaign-233` / `client-134`. Reports: [S07-T02](../../validation/S07-T02.md)
  (generations), [S07-T03](../../validation/S07-T03.md) (journal, ledger),
  [S07-T04](../../validation/S07-T04.md) (recovery), [S07-T05](../../validation/S07-T05.md)
  (reconnect), [S07-T06](../../validation/S07-T06.md) (termination matrix, deaths),
  [S07-T07](../../validation/S07-T07.md) (respawn). Spec: [persistence contract](../../persistence-contract.md).
- **[G07 report](../../validation/G07.md) is ready for the user's acceptance**
  with three decisions (checkpoint cadence, respawn point, respawn kit).
  Stage 7 stays In Progress in plan.md until the user accepts.
- Observed: st-2 107/107 store+protocol tests; rec-2 and rec-3 (restart
  with 50 journal records replayed, clients rejoin under epoch 2); matrix
  mx1 10/10 (campaign-230) and mx2-10 (final build; fail-closed, the client
  sees reason 5); death-2 (death + respawn committed, server terminated
  after the apply, both replayed, corpse kept on both clients; all 7
  checks). Defects found and fixed on the way are in the reports (a
  result-dispatch bug that sent rejections as Accepted; the HUD prompt
  fault; the tick check for dead/two actors).
- Carried limits: L15 closed for Stage 7's parts; L32–L36 added
  ([carried limits](../../carried-limits.md)). Known issue K4 seen again (rec-3).
- Unverified: power loss (not claimed, L35); client reconnect UI (main
  menu shows); the respawn kit is the `actor` section's supplies.

## First thing next session

1. Ask the user to accept G07 and answer its three decisions; record the
   acceptance in `docs/validation/G07.md`, plan.md §I (G07 Completed, Stage 7
   Completed) and the carried-limits header if rows change.
2. Then S08-T01 (durable item pickup) under the contract: a `pickup` record
   and adapter, its crash test with `tools/run-recovery-matrix.py`-style
   failpoints, and `compare-coop-recovery.py` extended for items.
3. Store/session tooling to reuse: `prepare-coop-session.py --ledger
   --checkpoint-seconds --failpoint point[:count][:fail] --auto-respawn`,
   `Run-CoopSession.ps1 -ServerKill N|0 -ServerRestartAfter M`,
   `compare-coop-recovery.py`, `compare-coop-respawn.py`, `run-recovery-matrix.py`.

## Known issues (not to chase now)

[known issues](../../known-issues.md): K1–K4 unchanged (K4 seen again in rec-3).

## Run names

- Used this session: proto-build-33/34/35, st-1/st-2, campaign-223…233,
  client-125…134, rec-1/rec-2 (S07-T02), rec-3 (S07-T04), mx1-1…10 and
  mx2-10 (S07-T06), death-1/death-2 (S07-T07), patch check 45.
- Next: `proto-build-36`, `st-3`, `campaign-234`, `client-135`, `pt-31`,
  `smoke-171`, `capture-62`, patch check 46.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before G07 acceptance](./snapshot-before-G07-acceptance.md).
