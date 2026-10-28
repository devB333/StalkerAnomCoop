# Session handoff — Stage 9 tasks complete; G09 waits for the user's review

## State (2026-10-28)

- **S09-T04 Completed** (2026-10-28, patch 0057 `travel-failures`, protocol
  `proto-build-48`, final builds **campaign-305 / client-202**):
  [S09-T04](../../validation/S09-T04.md) — the
  [failure matrix](../../travel-contract.md#11-the-failure-matrix-s09-t04):
  a disconnect during the barrier rolls back naming who left, retry travels
  (tv-17); a failed publication rolls back with a phase 3 record, both told,
  retry travels (tv-18); crashes during the reload (tv-19) and the hold
  (tv-20) recover the destination, a recovered server holds nothing; a
  client back after the hold's timeout is told the party went on (tv-21); a
  client that crashed at the destination load restarts, is told the hold
  (CampaignEvent 13 state 10 `held`) and the party resumes together (tv-26;
  two defects in that notice found by tv-22/tv-25 and fixed); after a crash
  before the take the party asked again and travelled, to Garbage (tv-23).
  tv-24 was an invalid run (server rebuilt alone: `game build differs`).
- **Every Stage 9 task is Completed.** Stage 9 stays In Progress until
  **[G09](../../validation/G09.md)** is accepted.
- Gameplay decisions for the review: [Stage 9 decisions](../../stage9-decisions.md)
  T01-1…9, T02-1…7, T03-1…2, T04-1…6. Most likely to need the user's word:
  T01-3 (no travel while the partner is away), T03-2 (60-s wait, then the
  present player goes on), T02-6 (vehicles not carried over), T04-4 (a
  recovered server does not hold).
- L14 closure is proposed at G09 with new rows for what remains (two
  passages only, no new game, vehicles, the hold's running world, the
  untuned 60-s/10-s policies, the untested death-in-barrier and duplicated
  ready) — add them to `docs/carried-limits.md` only when the user accepts.
- Still to do by hand: the user's two-window travel trial (walk into a
  changer, USE, partner USE/DROP, arrive together) and the K1/K5 retest.

## Next

**Stop at the gate.** Walk the user through G09 item by item (the
requirement table, the decisions T01-1…T04-6, L14's closure and the new
limits), implement any revision they ask for (new run names below), then
record the decision in `docs/validation/G09.md`, close L14 and add the new
limit rows in `docs/carried-limits.md`, mark Stage 9 Completed in
`plan.md` (§A table, §I) and commit. After acceptance: **S10-T01** (the
bounded campaign coverage matrix; it should name the passages the release
needs).

## Run names

- Next: `campaign-306`, `client-203`, `proto-build-49`, `st-24`, patch
  check 65; session names `tv-27…` (tv-1…tv-26 used; tv-2, tv-9, tv-10
  prepared, not run; tv-24 invalid).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the G09 review](../done/snapshot-before-G09-review.md).
