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

0. **S10-T05b — every quest is a party quest** (reordered by the user on
   2026-10-29: before trade; decisions T05b-1 and T05b-2 in
   [Stage 10 decisions](../../stage10-decisions.md)): one adapter per
   progress family instead of the S08 allow list; the reward to both
   players in full; accept and turn-in by either player through the talk
   window or the kind-8 intent. Reading first: `task_manager.script`
   (`CGeneralTask`, `task_type`, `status_functor`), `task_status_functor.script`,
   the `tm_*.ltx` task sections (progress keys per type), the kill
   attribution in the installed death callbacks, the S08 adapter
   (`party_source` in `CoopServerHost.cpp`) to generalise.
1. **S10-T04c — trade, repair and upgrades** (after S10-T05b) (L01 trade, L36 trade, L48;
   T01-7). **Reading done (2026-10-29), no code yet.** What the source
   says: a price is `CTrade::GetItemPrice` (`trade2.cpp:154`): base cost ×
   condition factor × the relation factor (the trader's attitude to the
   buyer: per player) × the trader's `trade_parameters` (set by
   `trade_manager.setup_buy_sell_conditions` from `db.actor` condlists:
   per buyer) — so prices must come from the server, in the buyer's
   context, never from a client's replica. A trade is `OnBtnPerformTrade*`
   (`UIActorMenuTrade.cpp:547`) → `CTrade::TransferItem` per item
   (`GE_TRADE_SELL`, `set_money`, `GE_TRADE_BUY`); the client already turns
   the SELL/BUY halves into a transfer intent (`coop_client_ownership_event`)
   that the server refuses for a living trader (`kReasonNotContainer`).
   A co-op client's actor money only changes through a resync: the replica
   policy drops `GE_MONEY` (`CampaignMessagePolicy.cpp:127`), so the HUD
   money is stale after any server-side payment (check S08-T05's notes).
   Repair is a mechanic dialogue (`item_repair.script`:
   `give_repaired_items`, money through `db.actor`) and the field repair UI
   (`ui_workshop.script`, client-side on `db.actor`'s items). Upgrades are
   `CInventoryUpgradeManager` + `inventory_upgrades.script`
   (`effect_functor_a` takes the money, `precondition_functor_a` reads
   `actor:money()`), driven by the actor menu's upgrade mode.
   **Design to implement (my plan; T01-7 binds the policy):**
   (a) **trade enters from the remote talk** (the trade button on in remote
   mode: `trade start` intent 17 inside the open talk, one buyer at a time
   by the talk's one-speaker rule; the server answers with CampaignEvent 15
   `trade prices`: for each item of the trader's inventory and of the
   buyer's, the entity ref and its price for this buyer, plus both purses;
   the client opens the native `CUIActorMenu` trade mode against the
   replica trader and `CTrade::GetItemPrice` on a co-op client reads that
   table); (b) **perform trade** = intent 18 with the bought and sold entity
   refs and the client's expected delta; the server re-prices in the
   buyer's context, checks the owners and the purse, writes a `trade`
   journal record (contract §6: resolved post-state: each item's new
   owner, both purses, revision) before applying through
   `CTrade::TransferItem` and acknowledging; Reparent events follow; a
   concurrent purchase of the last item loses with 9 `ItemTaken`;
   (c) **money to the client**: a `money` field in the per-player survival
   event (kind 12) or a new kind, sent on change, so the HUD and the trade
   window's purse are the server's; (d) **repair**: the mechanic dialogue
   already runs on the server in the talking player's context (S10-T04a);
   its money and condition changes need (c) and an item-condition update
   to the client (a `condition` in the layout event 9); the field repair UI
   (`ui_workshop`) stays local to the client's own items and is a stated
   limit until an intent carries it; (e) **upgrades**: the actor menu's
   upgrade mode on the client sends an `upgrade` intent 19 (item ref,
   upgrade section); the server runs `upgrade_install` in the buyer's
   context, records it, and the client applies the upgrade to its replica
   item from the result; (f) fixture `--trade c1,c2`: both buy from
   Sidorovich, the last item contested, a sale, a repair through the
   dialogue, an outfit with belt slots then an artefact on the belt (L48),
   crash windows around the `trade` record; compare tool
   `compare-coop-trade.py`. Decide at the start whether (e) fits this child
   or moves to its own (the upgrade UI is large); ask the user if the
   trade UI should stay the native drag-and-drop window with server prices
   (my recommendation) or become a simpler list.
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
