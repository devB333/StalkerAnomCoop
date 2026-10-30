# Session handoff — S10-T04c, S10-T04f and S10-T04d complete; next S10-T04e

## State (2026-10-29, end of session)

- **Completed this session, committed:** **S10-T04c** (patch 0066; anim-1
  23/23, trade-4 8/8), **S10-T04f upgrades at mechanics** (patch 0067,
  protocol v1.8 `proto-build-53`, st-28 51/51; upgrade-7 7/7) and
  **S10-T04d factions and reputation per player**
  ([S10-T04d](../../validation/S10-T04d.md), patch 0068 `relations`, patch
  check 79; builds **campaign-347 / client-239**; **relations-3 pass 8/8**:
  player 2's goodwill alone through the installed export with character 0,
  a stalker/bandit relation on both clients, player 1's reputation on its
  client, all three recovered after the kill).
- **Findings that bind later work:** (1) Anomaly's inventory, trade and
  repair windows are the installed **Lua UI** (T04f-5/6): the server takes
  the dialogue phrases `dialogs.npc_is_tech` / `npc_is_trader` over, the
  Lua window's script exports become the intents on a client, and the
  fixtures never click that window (the user's hands-on is its only check).
  (2) The engine does **not save the faction-vs-faction table**; the
  installed script re-applies its own changes from `db.actor`'s pstor (one
  player's inside a context), so the server keeps the table in its
  checkpoint state (`m_data.coop_relations`, T04d-4). (3) luabind pitfalls
  met twice: never assign a call proxy into a default-constructed
  `luabind::object` and never call `type()` on one (both crashed the
  server); `luabind::iterator` is not in this engine's luabind.
- **Hands-on by the user, pending (list for G10):** the Lua repair window at
  Xenotech (install, repair, partner busy), the Lua trade window at
  Sidorovich, the partner's weapon switch (0065) and attack animations
  (0066), the task map spot and Sidorovich's voice (0062), and per-player
  relations (one player kills a neutral stalker: his PDA relations tab
  falls, the partner's does not). `bash tools/start-coop-demo.sh S10-T04d 0
  20 0` (or `S10-T04f`, `S10-T04c`).
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, **T04d-1…4**. Gameplay ones among the new rows:
  T04f-2 (the mechanic's lending of items for game hours follows the
  server's option), T04d-1 (goodwill per player, no party goodwill), T04d-3
  (relation changes durable through the checkpoint only).
- **New limits:** L58 (a 534 m server-side relocation of a client's own
  actor crashed that client), L59 (client 2's process sometimes exits with
  an access violation after its clean stop; logs complete).
- The user's standing directions (2026-10-29): trade is NPC-to-one-player
  (T01-7), the game's own drag-and-drop window (T04c-1, the Lua window),
  quest rewards to both players in full (T05b-2). Keep the decisions list.

## Next

1. **S10-T04e stashes and treasure** (T01-14: contents world, first come,
   journaled as transfers; a stash reward's marker to both players' maps as
   S08-T09's map spots). Read the matrix row, `treasure_manager.script`
   (stash creation, `give_treasure`, the marker), the inventory-box path of
   S08-T02 (transfers to and from boxes are already journaled) and the
   S08-T09 map-spot event; the likely work is the stash *marker* and the
   stash's `treasure_manager` state (per `db.actor` pstor?) for both players,
   plus a fixture (a stash given by a dialogue/reward, both maps show it,
   one player empties it, the other sees it empty, a restart keeps it).
2. Then S10-T02a–f, S10-T03a–f, S10-T05a/c/d, S10-T06; stop at G10 with the
   decisions list.

## Run names

- Used: `campaign-334…347`, `client-226…239`, `proto-build-52` (unused),
  `proto-build-53`, `st-28`, `upgrade-1…7`, `relations-1…3`, patch checks
  77–79; the rest in the snapshots.
- Next: `campaign-348`, `client-240`, `proto-build-54`, `st-29`, patch
  check 80; sessions `stash-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T04e](../done/snapshot-before-S10-T04e.md).
