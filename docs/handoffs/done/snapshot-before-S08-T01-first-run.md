## Complete

# Session handoff — G07 accepted; play-test fixes pending retest; next S08-T01

## State (2026-10-23, end of day)

- **G07 accepted** ([G07](../../validation/G07.md)); Stage 7 Completed. The
  optional host-triggered save is **S07-T09** (Not Started; only if the play
  test shows the checkpoint pause or the user asks).
- **Play-test fixes** found with the user on the final Stage 7 build:
  - Patch **0043** (campaign-240 / client-140): the other player's **lean**
    is shown (K5); the other player's weapons no longer attach to this
    player's first-person hands (K1 root cause: the client's in-process
    server owns them as local objects). **Retested by the user:** lean ok,
    the bent grip and the re-draw are gone.
  - Patch **0044** (campaign-244 / client-144, **not retested**): a residual
    K1 sway when the other player equips ("the sway from the equip
    animation travels over"). cam-1 showed the standing client's camera
    does not move (yaw/pitch/position/FOV constant), so it is the
    hands/weapon layer: `CHudItem::PlayHUDMotion` / `PlayHUDMotion_noCB`
    now play nothing for the other player's item and log
    `S07 K1 remote-hud-motion-blocked`. Details and the check in
    [known issues](../../known-issues.md) K1.
- Builds campaign-243/244 and client-143/144 were started at the end of the
  session; their logs are not in the repo (`builds/logs/S02-T02/`). **Check
  they compiled** before the retest (`msbuild.log` 0 Error(s)); if 244/144
  failed, the one-line log statement in `PlayHUDMotion` is the only new code
  since 243/143.
- Everything is committed; the engine tree matches the exported series
  (patch check 48).

## First thing next session

1. Confirm campaign-244 / client-144 compiled; if not, fix and rebuild
   (`campaign-245` / `client-145`).
2. Retest with `bash tools/start-coop-demo.sh`: player 2 equips and holsters
   while player 1 holds a weapon and watches: no sway on player 1's screen;
   player 1's engine log (`appdata/logs/xray_djb20.log` in the client
   runtime) shows `S07 K1 remote-hud-motion-blocked`. If the sway persists,
   the remaining suspects are the HUD layer's own update
   (`player_hud::update`, `attachable_hud_item`) for an item created for
   the other player's weapon (`HudItemData()` creates one on first use even
   for a remote weapon), and the `_G.CHudItem__PlayHUDMotion` script hook.
   Then move K1 and K5 to "Fixed" in known-issues.
3. Then **S08-T01** (durable item pickup): design sketch in the previous
   snapshot ([before K1 sway retest](./snapshot-before-K1-sway-retest.md)).

## Known issues (not to chase now)

[known issues](../../known-issues.md): K1 (sway residual, fix candidate
pending), K2–K4 unchanged, K5 (lean, fixed pending the written retest note).

## Run names

- Used today: through campaign-244 / client-144, proto-build-37, st-4,
  rec-4, mx3-1…5, death-1…4, timing-1, cam-1, patch checks 45–48.
- Next: `proto-build-38`, `st-5`, `campaign-245`, `client-145`, `pt-31`,
  `smoke-171`, `capture-62`, `cam-2`, patch check 49.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before K1 sway retest](./snapshot-before-K1-sway-retest.md).
