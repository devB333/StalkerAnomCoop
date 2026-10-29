# Session handoff — S10-T04a and S10-T04b complete; next S10-T04c

## State (2026-10-29)

- **S10-T04a Completed** ([S10-T04a](../../validation/S10-T04a.md), patch
  0058 `talk-window`, `proto-build-49` v1.6 with action kinds 14–16 /
  reasons 33–36 / CampaignEvent 14, builds campaign-312 / client-209): a
  co-op client's talk window drives the server's dialogue, one speaker per
  NPC (T01-15), the party quest's accept and turn-in phrases through the
  S08 party path, inputs dropped while talking, the window's remote mode.
  Runs `talk-7` (all checks), `talk-8` (crash window at
  `quest:after-accept`), `talk-9` (the turn-in by the player who did not
  accept). L43 closed, L01 narrowed.
- **S10-T04b Completed** ([S10-T04b](../../validation/S10-T04b.md), patch
  0059 `party-info-portions` (check 67), builds **campaign-313 /
  client-210**): story info portions mirrored between the participant
  actors with a personal list per player (T04b-1), `has_alife_info` reads
  the context's player (T04b-2), an actor without the installed binder
  (participant 2's, a respawned body) gets an `alife_storage_manager`
  entry so its `pstor` is saved and restored (T04b-3). Run `info-1`
  (all five checks: 30 reads as expected across a kill and restart, both
  mirrors, the personal portion kept apart, both actor states restored).
  L44 closed; L11, L32 narrowed.
- **Decisions for G10**, not yet shown to the user:
  [Stage 10 decisions](../../stage10-decisions.md) T04a-1…4 and
  T04b-1…3. List them at the gate review with T01-1…19.
- **Limits to add as rows at G10** (each report's §Limits): talk: non-party
  tasks through the window are not given and the NPC's "accepted" line is
  still spoken (S10-T05b); dialogue preconditions other than info portions
  and the task (money, companion checks) read the talking player's actor.
  Info: the mirror follows online actors only; the binder-driven per-actor
  systems (status, surge cover, achievements) stay the script actor's
  (S10-T05).
- **Unverified**: the hands-on talk (two windows at Sidorovich: the window's
  look, the busy HUD hint, the voice lines); a talk ended by the NPC's
  death.
- The user's instruction for the session: keep going, ask on big decisions
  or blockers, stop at the gate.

## Next

1. **S10-T04c — trade, repair and upgrades** (L01 trade, L36 trade, L48;
   T01-7): each player's money is theirs; a trader's stock and restock are
   world; the trade window serves one buyer at a time (the other is told
   the trader is busy, as the talk's 33); repair and upgrades run on the
   server in the buyer's context. Read first: `CUITalkWnd::SwitchToTrade`
   (now off in remote mode: T04a-4), `UIActorMenuTrade.cpp`, `trade.cpp` /
   `trade2.cpp` (the server already has S08's transfer path for items:
   `take_transfer`, `kReasonNotContainer` refuses a living trader), the
   S08-T02 contract §6 `transfer`, `coop_client_ownership_event`
   (GE_TRADE_SELL/BUY halves), the [coverage matrix](../../campaign-coverage-matrix.md)
   F15 and the L48 row (an outfit with belt slots, then an artefact on the
   belt).
2. Then S10-T04d (factions and reputation per player, T01-6) and S10-T04e
   (stashes, T01-14).

## Run names

- Used here: `proto-build-49`, `st-24`, `st-25`, `campaign-306…313`,
  `client-203…210`, `talk-1…talk-9`, `info-1`, patch checks 65–67.
- Next: `campaign-314`, `client-211`, `proto-build-50`, `st-26`, patch
  check 68, patch 0060; session names per task (e.g. `trade-1…`).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T04c](../done/snapshot-before-S10-T04c.md).
