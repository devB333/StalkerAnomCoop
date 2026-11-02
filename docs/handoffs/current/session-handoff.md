# Session handoff — S10-T07 part 5 built; waiting on the user's hands-on check (demo-5)

## State (2026-11-02)

- **demo-4** (campaign-424 / client-307, patch 0090), the user's verdict:
  - **B1: fixed.** "the trader screen at Sidorovich now works"
    (`panel=… not_npc=true shown=false fixed=1`).
  - **B4: "so close, but the bodies are twisted at the hip".** The facing is
    right: the drawn heading equals the sent one in 216 of 218 samples.
  - **B23: "still causing the game to lag out when near it".** The 30 s
    average reads 60 fps with 46 fires lit, so the stall is local to a fire
    and not measured per frame yet.
- **Part 5 is in code and built**: patch **0091** `bug-pass-5`, builds
  **campaign-426 / client-309**; campaign-425 / client-308 failed to link (an
  `extern` placed inside the `campaign` namespace). Report:
  [S10-T07](../../validation/S10-T07.md), section "Part 5".
  - **B4**: a stalker replica no longer runs its own sight action
    (`CAI_Stalker::UpdateCL`), which overwrote the server's head target
    every frame. Its `Exec_Look` uses `process_action` only, and the torso
    share follows the server's mental state. Proof:
    `replica-anim … head=<cur>><target> body= danger=`.
  - **B23**: no campfire shadow on a client by default. New game-console
    settings: `coop_campfire_shadow <m>` and `coop_campfire_light 0/1`.
    `frame-trace frames= max_ms= slow= fire_m= fire_light= fire_shadow= lit=
    shadowed= set_light= set_shadow=` is logged once a second when a frame
    takes over 40 ms or a fire is within 30 m. Decision T07-10 updated.
- Engine-work equals patch 0091.
- **Not verified**: B4 and B23 are in code only; the user tests them by hand.
- **Still open**: the rest of B21. B5 and B9 wait on the user's decisions,
  B11 on a recurrence, B20 on a dump (needs B5).

## Next

1. The user's hands-on check: `bash tools/start-coop-demo.sh S10-T07 0 30 120 0`
   (it becomes demo-5). Watch walking NPCs (the hips). Stand at a fire, then
   open the game console (`~`) and try `coop_campfire_shadow 20`, then
   `coop_campfire_light 0`, then `coop_campfire_light 1`.
2. Read demo-5's logs (`builds/stage2/test-install/S10-T07/demo-5-*/appdata/logs`):
   `frame-trace` (is `max_ms` high with the light and shadow off?) and
   `replica-anim` (`head` vs. `body`). Record the results in the report and
   in known-issues (B4, B23).
3. B5/B9 decisions; the G10 review, limits L60…, close G10, Stage 11.

## Run names

- Used: `campaign-334…426`, `client-226…309`, `proto-build-53…59`,
  `st-28…34`, `bugfix-1…16`, `demo-1…4` (S10-T07), patch checks 77–103.
- Next: `campaign-427`, `client-310`, `proto-build-60`, `st-35`,
  `bugfix-17`, patch check 104, `demo-5`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T07 demo-5](../done/snapshot-before-S10-T07-demo-5.md).
