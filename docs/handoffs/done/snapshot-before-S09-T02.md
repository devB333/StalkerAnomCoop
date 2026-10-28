## Complete

# Session handoff — Stage 8 completed (G08 condition confirmed); next S09-T01

## State (2026-10-27)

- **G08 condition confirmed** ("they all work You can close the gate",
  [G08](../../validation/G08.md#condition-review--ready-for-the-users-confirmation-2026-10-26)):
  L37–L42 closed by S08-T07…T09; every gameplay decision approved
  ([Stage 8 closing decisions](../../stage8-closing-decisions.md)), T07-5
  (no firing during a use animation) and T07-8 (thirst, sleep, psy health per
  player; each HUD shows the server's hunger, stamina, radiation) as the user
  revised them; new carried limits **L44–L49** added with owners. **Stage 8
  Completed.** L43 (the talk window) stays with S10-T04.
- Engine patches 0001–0053, protocol v1.6 `proto-build-44`; final builds
  campaign-292 / client-191. Reports: [S08-T07](../../validation/S08-T07.md),
  [S08-T08](../../validation/S08-T08.md), [S08-T09](../../validation/S08-T09.md).
- Open from earlier: the user's K1/K5 hands-on retest.

## Next

S09-T01 (plan.md): party travel request and two-player consent. Start from
E11 (actor-based level travel), the level changer interaction (the server and
clients refuse level changes since S06-T05: `CLevelChanger` / the client's
refusal) and the session/party state; define the transition id, the state
machine, expiry, refusal, cancellation, combat restriction and disconnect
policy. A policy that would accept one player's consent after a disconnect
needs the user's approval (plan.md risk). L49 (sleep and its time skip) is
related world-time policy (S10-T03).

## Run names

- Next: `campaign-293`, `client-192`, `proto-build-45`, `st-16`, patch
  check 60; session names `tv-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S09-T01](../done/snapshot-before-S09-T01.md).
