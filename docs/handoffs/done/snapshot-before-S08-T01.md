## Complete

# Session handoff — Stage 7 complete (incl. S07-T08); G07 awaits acceptance

## State (2026-10-23)

- **Every Stage 7 task is Completed**, including **S07-T08** (added by the
  user at G07: no stall at a checkpoint) and the S07-T07 revision under the
  user's G07 decisions (respawn at the level entry point; the faction kit).
  Engine patch 0042 (re-exported, patch check 46), protocol `proto-build-37`
  (supplemental schema 2), final builds `campaign-238` / `client-138`.
- **[G07 report](../../validation/G07.md) awaits the user's acceptance.** The
  three decisions are answered and re-validated (its "User decision" section).
  Stage 7 stays In Progress in plan.md until the user accepts.
- Reports: [S07-T02](../../validation/S07-T02.md) … [S07-T07](../../validation/S07-T07.md),
  [S07-T08](../../validation/S07-T08.md). Observed this session:
  - rec-4: recovery with the asynchronous publication passes; the client's
    update gap at a checkpoint fell from 183–367 ms (rec-3) to about 100 ms
    (one 168 ms); the simulation thread keeps the world serialization
    (45–65 ms, up to 129 ms under load).
  - mx3: the five `checkpoint:*` termination points pass on the final build.
  - death-3 / death-4: respawn at the recorded entry point, the faction kit
    for both slots (`stalker_loadout`, 13 items + ammo, identity and money
    copied), the restart replays death and respawn; 7/7 checks.
  - st-3 / st-4: 108/108 store tests (new `async-publication-keeps-previous-journal`).
- Unverified: whether a ~100 ms pause at a checkpoint is noticed in real
  play (L34 revised; a serialization task only if it is); power loss (L35).
- `git diff --check` flags tabs inside `patches/engine/*.patch` (the
  engine's indentation in context lines); check docs and tools with
  `git diff --check -- . ':!patches/engine/*.patch'`.

## First thing next session

1. Ask the user to accept G07; on acceptance record it in
   `docs/validation/G07.md`, plan.md §I (G07 Completed, Stage 7 Completed)
   and the carried-limits header.
2. Then S08-T01 (durable item pickup): a `pickup` record and adapter under
   the contract, a crash test through `tools/run-recovery-matrix.py`-style
   failpoints, `compare-coop-recovery.py` extended for items.
3. Tooling: `prepare-coop-session.py` (default protocol build proto-build-37;
   `--ledger --checkpoint-seconds --failpoint point[:count][:fail]
   --auto-respawn`), `Run-CoopSession.ps1 -ServerKill N|0 -ServerRestartAfter M`,
   `compare-coop-recovery.py` (incl. the `stalls` section),
   `compare-coop-respawn.py`, `run-recovery-matrix.py`.

## Known issues (not to chase now)

[known issues](../../known-issues.md): K1–K4 unchanged (K4 seen again in rec-3).

## Run names

- Used this session: proto-build-33…37, st-1…st-4, campaign-223…238,
  client-125…138, rec-1/rec-2 (S07-T02), rec-3 (S07-T04), mx1-1…10 and
  mx2-10 (S07-T06), death-1…4 (S07-T07), timing-1, rec-4, mx3-1…5 (S07-T08),
  patch checks 45 and 46.
- Next: `proto-build-38`, `st-5`, `campaign-239`, `client-139`, `pt-31`,
  `smoke-171`, `capture-62`, patch check 47.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before G07 acceptance (2)](./snapshot-before-G07-acceptance-2.md).
