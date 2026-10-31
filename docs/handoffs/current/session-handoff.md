# Session handoff — stopped at the G10 review (S10-T07 part 1 done)

## State (2026-10-31)

- **Stopped for the user** (direction 2026-10-31: "finish the rest of G10
  until you get to the bug fixes that require my input"). The review record
  is [G10](../../validation/G10.md). It has two open questions (T05b-4 who
  pays a fee inside a task, T03b-2 what attracts bounty squads), the decision
  list ([Stage 10 decisions](../../stage10-decisions.md), up to T07-4), the
  bug rows waiting for the user, the hands-on checks and the limits to add.
- **Completed and committed this session:**
  - S10-T05d: presentation split, patches 0084–0085.
  - S10-T06: co-op menu and session control, patch 0086, proto-build-56,
    session-2 8/8.
  - **S10-T07 part 1**: [S10-T07](../../validation/S10-T07.md), patch 0087,
    proto-build-57 st-32, campaign-415 / client-301. Fixed: B6, B8, B10,
    B12, B13, B16, B17, B18 and the new B19. Runs bugfix-1…11:
    - bugfix-10: emission 6/6, presentation 5/5, clock 5/1 steps.
    - bugfix-5: music for player 2 only, a 525 m jump survived.
    - bugfix-6: the grab hold works.
    - bugfix-7: the tooltip percent matches the server's.
    - bugfix-4: companion orders reach the player's own companions.

    L58 closed, L28 narrowed. Decisions T07-1…4.
- **Waiting on the user:**
  - B1: a hands-on trade (client 2 logs `trade-open-sent … icon= name=`).
  - B2: details.
  - B4, B7: body animation now or in Stage 11.
  - B5 and B20: permission for crash dumps. B20 is a client hung once at
    level load in bugfix-8 and has not repeated.
  - B9: a "pending" mark, or waive.
  - B11: waiting for it to recur.
- The music fixture (`--music`) sometimes does not get the stalker to
  engage (bugfix-11). If it is used again, re-set the enemy relation every
  few seconds until it fights.
- Tools added this session:
  - `tools/compare-coop-session.py`
  - `tools/compare-coop-bugfix.py`
  - `tools/Start-CoopServer.ps1` (the dedicated server's launcher: it
    relaunches on exit 75)
  - `prepare-coop-session.py` options `--session`, `--music`,
    `--companion-test`
- The installed scripts are packed in `scripts.db0` and `configs.db0`.
  Extract them with `tools/xray-db-extract.py`; grep them with `grep -a`.

## Next

1. The user's G10 review. Implement the revisions they ask for.
2. Do the bug rows the user answers (S10-T07 part 2). Carry or waive the
   rest. Add the new limits (L60…).
3. Close G10, then Stage 11.

## Run names

- Used: `campaign-334…415` (373, 374, 384, 385, 395, 407 failed to compile;
  381 failed to link), `client-226…301` (293 failed to compile),
  `proto-build-52` (unused), `proto-build-53…57`, `st-28…32`, `sweep-1…7`,
  `newgame-1…4`, `emission-1…5`, `story-1…2`, `presentation-1…5`,
  `session-1…2`, `bugfix-1…11`, patch checks 77–99.
- Next: `campaign-416`, `client-302`, `proto-build-58`, `st-33`,
  `bugfix-12`, patch check 100.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the G10 review](../done/snapshot-before-G10-review.md).
