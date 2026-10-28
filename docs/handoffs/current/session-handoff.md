# Session handoff — S09-T01…T03 completed; next S09-T04 (then G09)

## State (2026-10-28)

- **S09-T01 Completed** (2026-10-27, patch 0054, proto-build-45):
  [S09-T01](../../validation/S09-T01.md) — the [travel contract](../../travel-contract.md)
  §1–§8: request at a level changer, two-player consent, 30-s expiry,
  aborts on disconnect/death/combat, no one-player consent.
- **S09-T02 Completed** (2026-10-28, patch 0055, proto-build-46):
  [S09-T02](../../validation/S09-T02.md) — contract §9: barrier, the
  `transition` journal record, both actors moved, the destination
  generation as the commit point, the in-process reload, the clients'
  rejoin. **The party travelled Cordon → Darkscape** (tv-11); a crash
  before the commit recovers the source level (tv-12), after it the
  destination (tv-13).
- **S09-T03 Completed** (2026-10-28, patch 0056 `travel-resume`, protocol
  `proto-build-47`, final builds **campaign-302 / client-199**):
  [S09-T03](../../validation/S09-T03.md) — contract §10: the world held
  after the reload until both clients' validated readies (kind 13; wrong
  id/level refused 31, not-yet-live 32), 60-s deadline; tv-14 both ready
  (resume at 12.9 s), tv-15 delayed partner + wrong readies, tv-16 a silent
  partner → timeout; no input applied during any hold.
- Stage 9 In Progress; L14 touched (a coordinated level change works;
  close at G09 if the user accepts). Gameplay decisions for G09 are
  collected in [Stage 9 decisions](../../stage9-decisions.md): T01-1…9,
  T02-1…7, T03-1…2. The ones most likely to need the user's word: T01-3
  (no travel while the partner is away), T03-2 (60-s wait, then the
  present player goes on), T02-6 (vehicles not carried over).
- Still to do by hand: the user's own two-window travel trial (walk into
  a changer, USE, partner USE/DROP, arrive together); the K1/K5 retest.
  In tv-4 a few hands-on USE/DROP presses reached the clients and worked.

## Next

S09-T04 (plan.md): cancellation, failed load and interrupted travel — the
failure matrix and the G09 report. Most of the machinery exists; the task
is to define and test the remaining windows explicitly:
- kill the server at each transition state (done: after-propose tv-4,
  before-take tv-12, after-commit tv-13; **missing**: during the reload
  itself — between `travel-reload` and `travel-reloaded` — and during the
  hold; both should recover the destination generation);
- fail one client's load / a client that never rejoins (tv-16 covers a
  silent one; **missing**: a client that disconnects between the commit and
  its rejoin, and a client whose load fails outright);
- disconnect before/after consent (tv-3 round 4, tv-4) and **during the
  barrier** (`travel_execute_tick` rolls back on a party member leaving:
  untested by a run);
- the rolled-back-by-server path (phase 3: a failed take/publication) —
  needs a failpoint in `travel_take` or a store failure (`:fail` mode);
- retry and recover: the players request again after each failure.
Then write `docs/validation/G09.md` (gate conditions: both confirm, the
barrier and migration preserve state, both load before resuming;
refusal, cancellation, timeout, failed load, client disconnect and server
crash recoverable) with the decisions list for the user, and **stop at
the gate** for the user's item-by-item review.

## Run names

- Next: `campaign-303`, `client-200`, `proto-build-48`, `st-22`, patch
  check 63; session names `tv-17…` (tv-1…tv-16 used; tv-2, tv-9, tv-10
  prepared, not run).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S09-T04](../done/snapshot-before-S09-T04.md).
