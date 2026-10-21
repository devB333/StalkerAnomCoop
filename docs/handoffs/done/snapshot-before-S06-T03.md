## Complete

Snapshot of the handoff written after S06-T01, before S06-T02 began (history only; do not resume from it).

# Session handoff — S06-T01 done; next: S06-T02

## State (2026-10-20, end of session)

Stage 6 **In Progress**: S06-T01 **Completed**; S06-T02–T05 Not Started.
G05 accepted earlier the same day. Engine `c57fe5b9` + patches 0001–0031
(exported, reconstruction check 34); protocol v1.1 with the input queue
(`proto-build-18`, the default in `Build-Stage2.ps1` and
`prepare-coop-session.py`). Design:
[co-op session](../../coop-session.md#validated-movement-input-s06-t01).
The user asked to stop after S06-T01 to change models.

S06-T01 ([report](../../validation/S06-T01.md)):
- **Input path:** each player's inputs go through a `coop::proto::InputQueue`.
  They are sequenced by tick and validated, at most 5 wait, and exactly one is
  consumed per server tick; an empty queue reuses the last input for at most
  10 ticks. The client samples once per server tick. The native control path
  decides speed, stance and collision.
- **move-8**, the final evidence: all 24 checks of
  `tools/compare-coop-movement.py` pass. Batches sent four times, a shifted
  tick counter with a +100000 jump, and a full replay moved no faster or
  further than a normal walk. A Prone stance, a teleport intent and a forged
  snapshot moved nothing; the forged snapshot closed the connection with code
  1003. Both actors walk, crouch at about 45 % and sprint at about 2× normal
  speed, and their bodies never overlap.
- **Defect fixed (patch 0031):** two actors within about 1.5 m pinned each
  other for good. `CActor::UpdateCL` ran the NPC box fitting
  (`UpdateObjectBox`) on the other actor, and that zeroed its control
  acceleration every frame. It is now skipped for actors; single-player is
  unchanged. The S05-T03 "collision at 2 m" was most likely this pin.
- **Rule changes:** three movement rules were revised after move-2, move-6 and
  move-7 data. The report lists each change and why. move-8 ran afterwards
  under the final rules.
- **Regressions:** session-3 (the S05-T04 scenario: late join, rejoin, churn;
  all replicas match, own-only walks), smoke-153 (66 stages) and capture-31
  (match) all pass.
- **Carried limits:** L03 closed. L24, the client crash at exit, was seen
  twice (move-3, session-3), both after `client-stopped`.

Not verified: a sprint until stamina runs out, the pitch clamp in a co-op leg,
and jump, lean, climb and vehicles (not in the input model). How far one actor
can push another has no single-player reference. See the report's limits.

## Next: S06-T02 — snapshots, interpolation and baselines

Read plan.md S06-T02 first. It owns L04 (NPCs and items move on clients) and
checks L25 (client ALife scheduler stall). Starting points: `send_snapshots`
in `CoopServerHost.cpp` (20 Hz, participant actors only, always full),
`on_snapshot` and `present_actors` in `CoopClientLink.cpp` (100 ms render
delay), and `Snapshot`/`SnapshotAck` in `protocol/include/coop/proto/messages.h`.
`last_input` is now the last consumed input tick, which S06-T03 will need.

## Tools and runs

- Movement program: `prepare-coop-session.py <run> --movement c1,c2`, then
  `Run-CoopSession.ps1 ... -ClientStarts '0,10' -ClientSeconds '180,170'` and
  `compare-coop-movement.py`. The exact commands are in the report.
- Rebuild both products after any engine change; their build ids must match.
  An `Actor.h` change makes each build take about 10 minutes.
- Last runs: `campaign-161`, `client-68`, `proto-build-18`, `pt-16`, `tr-12`,
  `move-8`, `session-3`, `smoke-153`, `capture-31`, patch check 34. Next:
  `campaign-162`, `client-69`, `proto-build-19`, `smoke-154`, `capture-32`,
  patch check 35, and new co-op run names under `builds/logs/S06-T02/`.
- Regression rule: every engine change reruns the GameState smoke and the
  single-player checkpoint capture. Don't build while a session or smoke runs.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before S06-T02](../done/snapshot-before-S06-T02.md).
