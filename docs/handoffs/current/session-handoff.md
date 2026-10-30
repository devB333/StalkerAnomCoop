# Session handoff — S10-T04c and S10-T04f complete; next S10-T04d

## State (2026-10-29, end of session)

- **Completed this session, committed:** **S10-T04c** (patch 0066: the
  partner's attack/reload/draw/holster animations from the server's
  weapon state, the repaired item's condition in the layout event, L48
  closed; anim-1 23/23, trade-4 8/8) and **S10-T04f upgrades at mechanics**
  ([S10-T04f](../../validation/S10-T04f.md), patch 0067 `upgrades`, patch
  check 77; protocol **v1.8** `proto-build-53`, st-28 51/51; builds
  **campaign-341 / client-233**; **upgrade-7 pass 7/7**: the loner mechanic
  Xenotech, an install for 17680 RU, a repair for 1042 RU, both records
  replayed after the kill, the recovered helmet carries the upgrade).
- **Finding that matters for every window (T04f-5, T04f-6):** Anomaly's
  inventory, trade and repair/upgrade windows are the installed **Lua UI**
  (`ui_inventory.script`), opened from the dialogue
  (`dialogs.npc_is_tech` / `npc_is_trader` → `npc:start_upgrade` /
  `start_trade`); the engine's `mechanic_mode` flag is commented out of the
  character descriptions (a mechanic is a section of
  `misc\stalkers_upgrade_info.ltx`). The server now takes those two phrase
  actions over and opens the player's session; on a client the Lua window's
  script exports (`install_upgrade`, `set_condition`, `transfer_item`,
  `give_money`) become the intents while a session is open, the window is
  nudged after server-made changes, and the session ends when the window is
  gone. The S10-T04c engine-window hooks (`UIActorMenuTrade.cpp`) never run
  in Anomaly: the trade by hand goes through the same Lua path. **The
  fixtures are server-driven and do not click the Lua window: the user's
  hands-on is the only check of that path.**
- **Hands-on by the user, pending (list for G10):** the Lua repair window
  at Xenotech (install, repair, the partner refused busy), the Lua trade
  window at Sidorovich (buy, sell), the partner's weapon switch (0065) and
  attack animations (0066), the task's map spot and Sidorovich's voice
  (0062). Commands: `bash tools/start-coop-demo.sh S10-T04f 0 20 0` /
  `S10-T04c`.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, **T04f-1…6** (T04f-2 is a gameplay choice: the mechanic's
  lending of items for game hours follows the server's `mechanic_feature`
  option, binding both players).
- **New limit L58:** a 534 m server-side relocation of a client's own actor
  crashed that client (upgrade-5); fixtures move player 1 only.
- The user's standing directions (2026-10-29): trade is NPC-to-one-player
  (T01-7), the game's own drag-and-drop window (T04c-1, now the Lua window),
  quest rewards to both players in full (T05b-2). Keep the decisions list.

## Next

1. **S10-T04d factions and reputation per player** (T01-6: goodwill and
   reputation per player, faction-vs-faction relations world). Read the
   coverage matrix row, `game_relations.script`, the relation registry
   (`relation_registry.cpp`, keyed by character id), `CInventoryOwner::
   SetCommunityGoodwill/ChangeCommunityGoodwill` and the DoctorX dynamic
   relations; the server already runs dialogue and kills in the player's
   context (T05b-3), so most goodwill changes land on the right actor; check
   what reaches a client (the PDA's relations tab, the HUD's faction colours)
   and journal/checkpoint durability. Fixture idea: a kill of a neutral NPC
   by p2 lowers p2's goodwill only; a dialogue reward raises p1's only.
2. Then S10-T04e (stashes), S10-T02a–f, S10-T03a–f, S10-T05a/c/d, S10-T06;
   stop at G10 with the decisions list.

## Run names

- Used: `campaign-334…341`, `client-226…233`, `proto-build-52` (unused:
  headers stashed mid-build), `proto-build-53`, `st-28`, `upgrade-1…7`,
  patch check 77; the rest in the snapshots.
- Next: `campaign-342`, `client-234`, `proto-build-54`, `st-29`, patch
  check 78; sessions `faction-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T04d](../done/snapshot-before-S10-T04d.md).
