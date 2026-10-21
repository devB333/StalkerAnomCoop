## Complete

Snapshot of the handoff written after S06-T02, before S06-T03 began (history only; do not resume from it).

# Session handoff — S06-T02 done; next: S06-T03

## State (2026-10-21, end of session)

Stage 6 **In Progress**: S06-T01 and S06-T02 **Completed**; S06-T03–T05 Not
Started. Engine `c57fe5b9` + patches 0001–0032 (exported, reconstruction
check 35). Protocol v1.2 (`proto-build-26`, the default in `Build-Stage2.ps1`
and `prepare-coop-session.py`). Design:
[co-op session](../../coop-session.md#snapshots-interpolation-and-baselines-s06-t02).

S06-T02 ([report](../../validation/S06-T02.md)):
- **Snapshots:** each player's set (participant actors pinned, other online
  creatures, physics objects while they move or until their rest pose is
  acknowledged) goes as deltas against the snapshot that player acknowledged
  (`coop::proto::SnapshotSender`/`SnapshotReceiver`). There are bounded full
  resyncs, an adaptive part budget, and causality with the lifecycle stream.
  87/87 protocol tests and 51/51 transport tests pass, including loss,
  reorder, eviction and lifecycle beside a snapshot flood.
- **Clients:** NPCs, crows, objects and actors are interpolated from a server
  clock that follows drift (`interpolation.h`). Moving creatures are carried
  on for at most 500 ms.
  - snap-11 (nominal) and snap-12 (5 % loss, 40 ms lag): all 12 checks pass.
    Moving NPCs p95 0.11–0.14 m, objects resting within 1.4 mm.
  - Bandwidth: 11–14 KB/s per client steady.
- **Defects fixed:**
  - NPCs had run AI on replica clients since S05-T02 (`cl_Process_Spawn`
    forced spawns local). Now they stay remote.
  - Once remote they stood in the bind pose; they animate again. The user
    saw Fanat T-posing, and it is fixed and checked by blend count and
    screenshot.
- **Regressions:** session-6 passes, smoke-155 passes, capture-34 matches.
  - smoke-154 failed `campaign-two-actors` (actor 2 pushed).
  - capture-32/33 timed out (cause unknown; capture-34 took 189 s against
    56 s before).
  - Movement program: move-12 24/24, move-13 22/24, move-14 23/24. The
    failures coincide with Fanat pushing player 1's standing actor on the
    server.
- **Carried limits:** L04 closed, L25 closed (not reproduced).

Not verified: replicated equipment (player 2's actor seen by player 1 shows no
weapon and an odd stance, owner S06-T04), NPC movement animations (they slide
in idle, S10-T02), ownership and death replication (S08, S06-T05).

User questions this session: the T-pose (above). Moving the test area from
Marsh to Cordon was recommended against for now: add Cordon as a second
fixture at S08/S09/S10-T01, or after G06 if the user wants it sooner. The
user has not decided.

## Next: S06-T03 — predict and reconcile the local actor

Read plan.md S06-T03 first. It owns L02. Starting points:
- `sample_input` in `CoopClientLink.cpp`: input ticks, the last 8 resent.
- `Snapshot.last_input`: the last consumed tick per player, already acked.
- `CActor::replica_present`, which places the local actor from snapshots
  today.
- The `InterpolationBuffer` extrapolation, which is creatures only.

Watch the single-player capture timing in the regressions, and account for
Fanat near player 1 in movement legs.

## Tools and runs

- Snapshot program:
  - `prepare-coop-session.py <run> --task S06-T02 --script c1,c2 --drop 6 [--net-loss 5 --net-lag 40]`
  - `Run-CoopSession.ps1 ... -ClientStarts '0,10' -ClientSeconds '150,140'`
  - `compare-coop-snapshots.py <logs> [--lag-ms 140]`
- Rebuild both products after any engine change; their build ids must match.
- Last runs: `campaign-174`, `client-80`, `proto-build-26`, `pt-24`, `tr-13`,
  `snap-12`, `session-6`, `move-14`, `smoke-155`, `capture-34`, patch check
  35.
- Next: `campaign-175`, `client-81`, `proto-build-27`, `pt-25`, `tr-14`,
  `smoke-156`, `capture-35`, patch check 36, and new co-op run names under
  `builds/logs/S06-T03/`.
- Regression rule: every engine change reruns the GameState smoke and the
  single-player checkpoint capture. Don't build while a session or smoke runs.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before S06-T03](../done/snapshot-before-S06-T03.md).
