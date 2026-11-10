## Complete

Snapshot of the handoff as it stood before S10-T07 part 3 (2026-11-02). History only.

# Session handoff — S10-T07 part 2 in code, the user's hands-on check next

## State (2026-11-01)

- **S10-T07 part 2** ([S10-T07](../../validation/S10-T07.md), part 2
  section): the user's answers of 2026-11-01 turned B2 into two faults and
  added B21 (movement), B22 (jump), B23 (campfire lag). Fixed in code, patch
  **0088**, proto-build-59 / st-34, builds **campaign-419 / client-304**:
  - B2: task and script map markers on objects a client does not hold stand
    at the server's place; a dead player's icon goes at death.
  - B4: stalkers' and monsters' animation state in the snapshot.
  - B7: the partner's zoom pose (new input bit 5); the controller's hold
    during its psy glide (bugfix-13 psy 6/6).
  - B21: one input per tick period, records at the tick's time, a held
    snapshot skipped. The first form (held ticks numbered on) was wrong and
    is reverted.
  - B22: a jump is committed only when the physics can take it; the server
    latches a press for 10 ticks.
  - B23: zone states are presentation on a client (every campfire burned on
    every client before); 72 campfires turned off per client at the join.
  - Runs bugfix-12…16. Not exercised by a run: the fixed map spot, the
    death icon removal, the jump, smoothness, the campfire frame rate.
- **The user's hands-on check is the acceptance of part 2.** Start it with
  `bash tools/start-coop-demo.sh S10-T07 0 30 120 0` (the current builds; it
  picks demo-3 or the next free name). Checklist in the report's part 2.
- **Answered in the session, still the user's to decide:** B5 (ProcDump per
  process, no registry change, or Windows LocalDumps, a machine-wide key)
  and B9 (a local "pending" mark in the Lua windows, or waive).
- **Still waiting on the user:** B1 (a hands-on trade), B11 (to recur), B20
  (a dump needs B5).
- Decisions T07-5…T07-9 added ([Stage 10 decisions](../../stage10-decisions.md));
  L47 narrowed, L57 fixed in code (closes after the hands-on check).
- Noted on the rig: key presses typed into the terminal during a fixture
  session reach the game windows; a zombie killed player 2 before the music
  fixture's 525 m move in bugfix-12 and bugfix-15 (the jump check could not
  run; nothing in part 2 changes server NPC behaviour).

## Next

1. The user's hands-on check of B2, B4, B7, B21, B22, B23; fix what fails.
2. The user's decisions on B5 and B9; B1's trade.
3. The G10 review (two open questions, the decision list up to T07-9), the
   new limits (L60…), then close G10 and start Stage 11.

## Run names

- Used: `campaign-334…419` (373, 374, 384, 385, 395, 407, 417 failed to
  compile; 381 failed to link), `client-226…304` (293, 302 failed to
  compile), `proto-build-52` (unused), `proto-build-53…59`, `st-28…34`,
  `sweep-1…7`, `newgame-1…4`, `emission-1…5`, `story-1…2`,
  `presentation-1…5`, `session-1…2`, `bugfix-1…16`, patch checks 77–100.
- Next: `campaign-420`, `client-305`, `proto-build-60`, `st-35`,
  `bugfix-17`, patch check 101, `demo-3` (or the next free demo name).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T07 part 2](snapshot-before-S10-T07-part2.md).
