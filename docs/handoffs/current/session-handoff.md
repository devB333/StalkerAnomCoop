# Session handoff — S06-T03 done; next: S06-T04

## State (2026-10-21)

Stage 6 **In Progress**: S06-T01–T03 **Completed**; S06-T04 and T05 Not
Started. Engine `c57fe5b9` + patches 0001–0033 (reconstruction check 36).
Protocol v1.3 (`proto-build-29`, the default in `Build-Stage2.ps1` and
`prepare-coop-session.py`). Design:
[co-op session](../../coop-session.md#own-actor-prediction-and-reconciliation-s06-t03).

S06-T03 ([report](../../validation/S06-T03.md)):
- **Prediction:** each client predicts its own actor through the native
  control path (`controls_actor` on a predicting replica) and reconciles it
  after every complete snapshot against the server's position for the last
  processed input (`processed_tick`, which includes refused inputs).
  Corrections go to the physics controller at once; a camera effector smooths
  the view.
  - pred-8 (nominal) and pred-9 (5 % loss, 40 ms lag) pass all checks.
  - Deliberately wrong predictions (legs G and P) stay under 0.28 m and
    converge.
  - The own actor moves in 83–100 ms, against 250 ms without prediction.
- **Fixed on the way (user reports):**
  - The other actor's arms now follow its aim (`kAim`, protocol 1.3).
  - The own HUD arms no longer snap when turning: two actors shared
    `cam_Update` statics. Measured: 6.6 M → 0.000 rad/s with a still camera,
    and the user confirmed.
- **Regressions:** session-8, snap-14, smoke-156 and capture-35 all pass.
  pred-10 (no prediction) gave 22/24 on the movement program: Fanat pushes.
- **Carried limits:** L02 closed.

## Next: S06-T04 — one authoritative weapon/combat slice

Read plan.md S06-T04. Design notes from this session:
- Fire as an input button. The server presses and releases `kWPN_FIRE` on
  the participant's actor through `actor_input_press` / `actor_input_release`
  (`Scope::Self`).
- `CActor::g_fireParams` takes the fire ray from the global HUD pick
  (`g_player_hud`, `HUD().GetPick()`). A non-view actor on the server needs
  its own camera instead.
- The replica refuses every game event from its own level, including
  `GE_WPN_STATE_CHANGE` for the local weapon (`CHudItem::SwitchState` waits
  for the echo), so a client's own weapon cannot change state. Allow it for
  items the local actor carries, for presentation.
- Equip: a validated action (activate slot). Outcomes: ammo, active slot and
  actor health replicated to the clients. Presentation of the other player's
  shots: a separate event.
- Target: the other participant's actor.

## Tools and runs

- Prediction:
  - `prepare-coop-session.py <run> --task S06-T03 --movement c1,c2 [--net-loss 5 --net-lag 40] [--no-predict c1,c2]`
  - `compare-coop-movement.py` and `compare-coop-prediction.py`
- Last runs: `campaign-182`, `client-87`, `proto-build-29`, `pt-27`,
  `pred-10`, `session-8`, `snap-14`, `hud-2`, `smoke-156`, `capture-35`,
  patch check 36.
- Next: `campaign-183`, `client-88`, `proto-build-30`, `pt-28`, `tr-14`,
  `smoke-157`, `capture-36`, patch check 37; co-op runs under
  `builds/logs/S06-T04/`.
- The host was slow at times while the user ran the game alongside (pred-2's
  42-s loads, capture-32/33 timeouts). Rerun under a new name when timings
  look off.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before S06-T04](../done/snapshot-before-S06-T04.md).
