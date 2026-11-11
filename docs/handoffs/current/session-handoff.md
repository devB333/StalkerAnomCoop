# Session handoff — B64 fixed; B61 and B56 in code; rerun the sweep on campaign-504 / client-387

## State (2026-11-11, paused by the user)

- **S10-T07 part 28** ([report](../../validation/S10-T07.md), bugs in
  [known issues](../../known-issues.md) B56, B61, B64). engine-work equals the
  series of **118** patches (`0118-outbox-storyline-party`, patch check 132).
  Latest builds **campaign-504 / client-387** on **proto-build-69**: built from
  one unchanged tree, **not yet run** (bugfix-93 was stopped at the pause
  before its logs were kept).
- **B64 fixed and confirmed**: the party task list (CampaignEvent 11) went
  over the 1024-byte event cap past ~9 tasks and blocked the lifecycle
  outbox. Now split into several events; an unencodable message is dropped
  and logged (`outbox-unencodable`); a reliable channel's sequence is taken
  only by a sent message (transient channels keep one per attempt: input
  batches need sequence >= count). bugfix-86: random stalkers 35/35, no stall.
- **B61 fixed in code**: DoctorX storyline dialogue actions run in the
  anchor's context and the tasks they give become party tasks
  (`quest-story-adopted`). bugfix-88/89 showed `story-talk` accepts and talk
  turn-ins passing with both paid. The sweep's last fixes (story pick only
  when the task's precondition passes; per-instance completion check) are in
  campaign-504 and not yet run.
- **B56 fixed in code**: a code join into a full roster is refused
  ("the party is full", news on the client); the roster is written only for a
  player who took a slot. Protocol test st-44 (128/128) only.
- bugfix-90/91 are **invalid** (a protocol edit during the builds split the
  build identity; memory: no edits during builds).
- The user is updated on Discord (chat `1566639386420973638`).
- Old logs: `builds/stage2/test-install/S11-T00/kit-host-17-server/appdata/logs/coop-server.log`
  is 132 GB: ask the user before deleting it.

## Next

1. Three sweep runs on campaign-504 / client-387 (about 15 min each; game
   windows open, the user should not type), as (run, fixture, list, first,
   count): `bugfix-96 cordon cordon 0 35`, `bugfix-97 marsh marsh 0 37`,
   `bugfix-98 cordon cordon 70 34`. For each:
   `python3 tools/prepare-coop-session.py <run> --task S10-T07 --protocol-build proto-build-69 --fixture <fixture> --quest-sweep c1,c2 --quest-sweep-list builds/quest-sweep/<list>.txt --quest-sweep-first <first> --quest-sweep-count <count> --checkpoint-seconds 120 --client-exit-after 1800`,
   then `Run-CoopSession.ps1 -Run <run> -Task S10-T07 -ServerAloneSeconds 10 -ClientStarts '0,15' -ClientSeconds '840,825' -AfterSeconds 15`,
   then `python3 tools/compare-coop-quest-sweep.py builds/logs/S10-T07/run-<run> --list builds/quest-sweep/<list>.json --out .../compare-quest-sweep.json --md .../quest-sweep.md`.
   A script for the chain is easy to rebuild from these lines (the last one
   was in the session's scratchpad).
2. Record the results in part 28 and known issues B61 (and B64's row).
3. Phase 2: the other levels through the passage sweep's travel.
4. Hands-on for B56 (a third player joining a full party) when the user
   tests; new kits only once the sweep is clean (memory).

## Picking this up

- Read `CLAUDE.md`, `plan.md` §I, this file, S10-T07 part 28.
- **One cycle:** edit engine-work (and `protocol/` → a new proto-build, tests
  with `run-protocol-tests.py`) → `Build-Stage2.ps1` Campaign then Client with
  the same `-ProtocolBuild`, **no edits while they compile** → a sweep run →
  `python3 tools/export-engine-patch.py <NNNN-name>.patch <check>` → manifest,
  README, report, known issues, handoff, commit.
- Installed scripts: `python3 tools/xray-db-extract.py -q grep 'pattern'
  /mnt/c/Users/djb20/StalkerAnom/db/configs/scripts.db0` (configs.db0 for
  task ini and dialogs), or `extract -m <name> -o <scratch dir>`.

## Run names

- Used: `campaign-334…504`, `client-226…387`, `proto-build-53…69`,
  `st-28…44` (and `st-42t`), `bugfix-1…93` (S10-T07), `demo-1…22`,
  `menu-1…10`, `kit-1…16`, `kit-19…23`, `two-pc-1`, patch checks 77–132,
  kits host-1…17, friend-1…3, client-4…11, server-1…12.
- Next: `campaign-505`, `client-388`, `proto-build-70`, `st-45`,
  `bugfix-96` (94 and 95 unused; skip them), `demo-23`,
  `menu-11`, `kit-24`, patch check 133, kits host-18, client-12, server-13.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the B61/B56 sweep runs](../done/snapshot-before-B61-B56-sweep-runs.md).
