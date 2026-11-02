# Session handoff — S10-T07 part 4 built; waiting on the user's hands-on check (demo-4)

## State (2026-11-02)

- **Part 4 is in code and built**: patch **0090** `bug-pass-4`, builds
  **campaign-424 / client-307** (one build id `241701b9…`), no protocol change
  (proto-build-59). Report: [S10-T07](../../validation/S10-T07.md), section
  "Part 4". campaign-423 / client-306 failed to compile (a debug-only
  engine call in the new log line); fixed.
- **B4 (NPCs walking backwards)**: cause found in the source. The co-op link
  runs in `CGamePersistent::OnFrame` (frame priority HIGH+1), before the level's
  `Objects.Update`. So `replica_present` placed the body, and then the
  stalker's `Exec_Look` (`sight_manager.cpp`) or the monster's
  `CControlDirection::update_frame` rebuilt the rotation from `m_body` yaw,
  which is never set on a replica. Fix: `replica_present` sets `m_body`, and
  a replica's own pickers no longer turn it. Proof: the line
  `replica-anim … legs=<idx>:<name> torso=<idx>:<name> yaw= drawn= travel=
  visual=` (facing forward means `drawn ≈ yaw ≈ travel`).
- **B23 (campfires)**: decision **T07-10** ([Stage 10 decisions](../../stage10-decisions.md)).
  The server never turns a campfire, so all of them burn. On a client a
  campfire casts a shadow only within 20 m of the camera (off beyond 25 m).
  New limit, to list at G10: fires cannot be put out in co-op.
- **B1 (trader portrait)**: not found by reading. The client's checkpoint
  names id 15398 `esc_m_trader`, and the getters read the object that logs
  `Sidorovich`. When the trade window opens, the client now shows a hidden
  partner panel and fills it under `pcall`. `trade-open … panel=` logs what
  it found: `shown=false fixed=1` means it was hidden, and an `update=` value
  other than `ok` is the Lua error.
- Engine-work equals patch 0090. Scripts for reading were extracted to the
  session scratchpad only (`tools/xray-db-extract.py extract -m .script -o
  <dir> <install>/db/configs/scripts.db0`).
- **Not verified**: B1, B4 and B23 are fixed in code only. The user tests
  them by hand because they affect what players see.
- **Still open**: the rest of B21 (Smooth corrections while moving). B5 and
  B9 wait on the user's decisions, B11 on a recurrence, B20 on a dump (needs
  B5).

## Next

1. The user's hands-on check: `bash tools/start-coop-demo.sh S10-T07 0 30 120 0`
   (it becomes demo-4). Look at walking NPCs, campfires near and far (the
   frame rate and shadows), and a trade at Sidorovich.
2. Read demo-4's logs (`builds/stage2/test-install/S10-T07/demo-4-*/appdata/logs`):
   `replica-anim` (drawn vs. yaw vs. travel), `trade-open … panel=`, and the
   status line's `fps= fires_on=`. Record the results in the report and in
   known-issues (B1, B4, B23).
3. B5/B9 decisions; the G10 review, limits L60…, close G10, Stage 11.

## Run names

- Used: `campaign-334…424`, `client-226…307`, `proto-build-53…59`,
  `st-28…34`, `bugfix-1…16`, `demo-1…3` (S10-T07), patch checks 77–102.
- Next: `campaign-425`, `client-308`, `proto-build-60`, `st-35`,
  `bugfix-17`, patch check 103, `demo-4`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T07 demo-4](../done/snapshot-before-S10-T07-demo-4.md).
