# Session handoff — S09-T01 completed; next S09-T02

## State (2026-10-27)

- **S09-T01 Completed** ([S09-T01](../../validation/S09-T01.md), patch 0054
  `travel-consent`, protocol `proto-build-45`, final builds **campaign-298 /
  client-195**): the [travel contract](../../travel-contract.md) (transition
  ids `(epoch << 32) | serial`, the consent state machine
  `coop::proto::TransitionCoordinator` with 7 unit tests, request kind 11 at
  a level changer, answer kind 12, CampaignEvent 13, 30-s expiry, aborts on
  disconnect / death / combat / changer loss, no one-player consent, no
  load). Validated: tv-3 and tv-6 (nine rounds incl. combat), tv-4 (server
  crash with a proposal live). Gameplay decisions **T01-1…T01-9** listed for
  G09 in [Stage 9 decisions](../../stage9-decisions.md); T01-3 (a request
  while the partner is away is refused) is the one the user may want to
  change (one-player travel would need approval, R10).
- Stage 9 is In Progress. L14 still holds (no level change yet).
- Hands-on: in tv-4 a few USE/DROP presses reached the client windows and
  the prompts, keys and HUD lines worked for a real player; the user's own
  two-window trial (walk into a changer, USE, partner USE/DROP) is still to
  be done. Open from earlier: the K1/K5 hands-on retest.

## Next

S09-T02 (plan.md): the authoritative travel state and the destination load.
Start from `end_travel` in `CoopServerHost.cpp` (the Agreed hand-over:
`travel-agreed … executor=none`), `CALifeUpdateManager::change_level`
(`alife_update_manager.cpp:166`, refused since S06-T05) and the checkpoint
coordinator (`request_publication`, S07-T08): define the barrier (quiesce or
order gameplay, refuse inputs and actions), journal the transition and the
source recovery generation durably at the barrier (contract §8 says nothing
is durable before Agreed), adapt the native one-actor level change for two
participant identities and one online world, and define the commit point
with rollback before it and roll-forward after it. The destination level
and arrival position are already resolved per transition
(`travel_destination`); the respawn entry point (`PlayerLink::entry`) should
become the arrival on the new level (G07 decision 2).

## Run names

- Next: `campaign-299`, `client-196`, `proto-build-46`, `st-18`, patch
  check 61; session names `tv-7…` (tv-1…tv-6 used; tv-2 prepared, not run).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S09-T02](../done/snapshot-before-S09-T02.md).
