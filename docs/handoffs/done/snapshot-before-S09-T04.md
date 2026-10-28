## Complete

# Session handoff — S09-T01 and S09-T02 completed; next S09-T03

## State (2026-10-28)

- **S09-T01 Completed** (2026-10-27; [S09-T01](../../validation/S09-T01.md),
  patch 0054, proto-build-45): the [travel contract](../../travel-contract.md)
  §1–§8, the consent state machine, kinds 11/12, CampaignEvent 13.
- **S09-T02 Completed** (2026-10-28; [S09-T02](../../validation/S09-T02.md),
  patch 0055 `travel-execution`, protocol `proto-build-46`, final builds
  **campaign-301 / client-198**): contract §9 — the barrier, the `transition`
  record (journal kind 13), both actors' entities moved, the destination
  generation as the commit point, the in-process reload (the serve loop in
  `x_ray.cpp`, `load_generation`, the identity registry restarted), the
  clients' rejoin. **The party travelled Cordon → Darkscape** (tv-11: commit
  192 ms after the agreement, the server on the new level 7 s later, the
  clients ~20 s after); crash before the commit → source level (tv-12),
  after it → destination (tv-13).
- Stage 9 In Progress. L14 touched (a coordinated level change exists;
  open until G09). Gameplay decisions for G09: T01-1…T01-9 and T02-1…T02-7
  in [Stage 9 decisions](../../stage9-decisions.md); T01-3 (no travel while
  the partner is away) is the one most likely to need the user's word.
- Still to do by hand: the user's two-window travel trial; the K1/K5
  retest.

## Next

S09-T03 (plan.md): synchronize both clients before resuming. Today each
client plays as soon as its own rejoin is in (tv-11: ~20 s after the
commit, independently). Start from `travel_after_reload` / the join path
(`request_world`, `stream_world`, `WorldLive` in `CoopServerHost.cpp`) and
the client's `world-live` point (`CoopClientLink.cpp`): hold the destination
world for the party until **both** clients have applied their checkpoint
and acknowledged (a validated readiness per transition id and generation:
a Ready intent or the first SnapshotAck after the join, bound to the
transition), keep the barrier (inputs ignored, actions refused 30) until
then, reset prediction baselines, refuse pre-transition inputs (the input
queues are reset on the join already), and send the destination/content
generation in the committed event. Validate different load durations,
delayed ready, duplicated packets and a wrong generation (plan.md S09-T03).
A long hold needs an explicit timeout policy (S09-T04 owns the failure
matrix; do not let a missing client hold the world for ever without a
stated rule).

## Run names

- Next: `campaign-302`, `client-199`, `proto-build-47`, `st-20`, patch
  check 62; session names `tv-14…` (tv-1…tv-13 used; tv-2, tv-9, tv-10
  prepared, not run).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S09-T03](../done/snapshot-before-S09-T03.md).
