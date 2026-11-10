## Complete

Snapshot of the handoff as it stood before S10-T07 part 4 (2026-11-02). History only.

# Session handoff — S10-T07 part 3 in code, the user's hands-on check next

## State (2026-11-02)

- **S10-T07 part 3** ([S10-T07](../../validation/S10-T07.md), part 3
  section): the user's hands-on session demo-1 (2026-11-01) passed the PDA
  tasks, task updates and trade, and failed B4, B22 and B23. Each was traced
  to its real cause in the session's logs and the source; fixed in code,
  patch **0089** (`bug-pass-3`), no protocol change (proto-build-59), builds
  **campaign-422 / client-305** (both succeeded, 0 errors):
  - B4: the animation pickers' `standing()` read a path speed a replica never
    computes, so every replica stood; now the snapshot's speed. Monsters play
    the server's motion as it is.
  - B22: the server took every jump; the client's own actor refused a held
    key after a correction's hop on a slope set the landing lockout, and the
    corrections then hoisted and slammed it. The client now follows the
    server's jump (`replica_follow_jump`). Pitch never reached the movement.
  - B23: the campaign server's campfire turn-on/off were no-ops (gated on
    renderer flags it never sets), so all 46 Cordon campfires stayed lit and
    the join lit them on every client. Part 2's "72 campfires off" was wrong
    (72 anomaly zones). The turn is allowed on the server now.
  - New proof lines: `replica-anim`, `jump-followed`, `jump-refused`, and
    `fps= fires_on= fires_near= jumps_followed= jump_refusals=` in the
    client status line (`hud_still_frames` was never a frame rate).
  - Not exercised by a run (no fixture covers them): all three are the
    user's hands-on check.
- **Still open from demo-1:** B21's remaining part: a Smooth correction on
  62-84 % of snapshots while moving (mean 11-14 cm, flat ground too), a
  steady prediction/server speed mismatch ([known issues](../../known-issues.md)).
- **Still waiting on the user:** B1 (a hands-on trade's portrait), B5 and B9
  (decisions), B11 (to recur), B20 (a dump needs B5).

## Next

1. The user's hands-on check with campaign-422 / client-305:
   `bash tools/start-coop-demo.sh S10-T07 0 30 120 0` (it picks the next
   free demo name). Watch: NPCs walk and run (not slide); jumps on hills
   while looking down; frame rate near the rookie village campfire. Then
   read the clients' `replica-anim`, `jump-followed`/`jump-refused` lines
   and the status line's `fps=`, `fires_on=`.
2. Fix what fails; then B21's speed mismatch if the user still feels it.
3. The user's decisions on B5 and B9; B1's trade.
4. The G10 review (two open questions, the decision list up to T07-9), the
   new limits (L60…), then close G10 and start Stage 11.

## Run names

- Used: `campaign-334…422` (373, 374, 384, 385, 395, 407, 417 failed to
  compile; 381 failed to link), `client-226…305` (293, 302 failed to
  compile), `proto-build-52` (unused), `proto-build-53…59`, `st-28…34`,
  `sweep-1…7`, `newgame-1…4`, `emission-1…5`, `story-1…2`,
  `presentation-1…5`, `session-1…2`, `bugfix-1…16`, `demo-1` (S10-T07),
  patch checks 77–101.
- Next: `campaign-423`, `client-306`, `proto-build-60`, `st-35`,
  `bugfix-17`, patch check 102, the next free demo name.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T07 part 3](snapshot-before-S10-T07-part3-hands-on.md).
