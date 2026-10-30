## Complete

# Session handoff — S10-T04 complete (all six children); next S10-T02a

## State (2026-10-29, end of session)

- **Completed this session, committed:** S10-T04c (patch 0066), S10-T04f
  (patch 0067, protocol v1.8), S10-T04d (patch 0068) and **S10-T04e
  stashes and treasure** ([S10-T04e](../../validation/S10-T04e.md), patch
  0069 `stashes`, patch check 81; builds **campaign-349 / client-241**;
  **stash-2 pass 7/7**: a box made a stash in player 1's context, its spot
  on both maps, four items spawned on the USE with a checkpoint right
  after, one taken by a journaled transfer with the box's take callback
  fired in the taker's context (spot removed on both maps), the restart
  kept it all). **S10-T04 (dialogue, factions, economy) is complete.**
- **Findings that bind later work:** Anomaly's inventory, trade, repair and
  loot windows are the installed **Lua UI** (T04f-5/6, T04e-1): the server
  takes the dialogue phrases `dialogs.npc_is_tech` / `npc_is_trader` over,
  a box's USE reaches the server while the client opens its own loot
  window (since patch 0063 a client could not open a box: fixed in 0069),
  and the Lua windows' script exports become the intents on a client. The
  engine does not save the faction table (server-kept in the checkpoint,
  T04d-4). luabind pitfalls: never assign a call proxy into or call
  `type()` on a default-constructed `luabind::object` (two server
  crashes); no `luabind::iterator`; `using namespace std` makes a variable
  named `pair` a type; `T x(u8(a), u8(b))` is a function declaration.
- **Hands-on by the user, pending (list for G10):** the Lua loot window on
  a stash (both maps show it, one empties it, the partner finds it empty,
  the marker gone), the Lua repair window at Xenotech, the Lua trade window
  at Sidorovich, the partner's weapon switch and attack animations, the task
  map spot and Sidorovich's voice, the per-player relations tab.
  `bash tools/start-coop-demo.sh S10-T04e 0 20 0` (or S10-T04d/f/c).
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, T04d-1…4, **T04e-1…3**. Gameplay ones among the new
  rows: T04f-2 (the mechanic's lending follows the server's option),
  T04d-1 (goodwill per player, no party goodwill), T04e-1/2 (a stash's
  contents spawn on the first USE and get a checkpoint; first come).
- **Limits:** L58 (a long server-side relocation of a client's own actor
  crashed it), L59 (a client's process sometimes exits with an access
  violation after its clean stop: upgrade-6, relations-1, stash-2).
- The user's standing directions (2026-10-29): trade is NPC-to-one-player
  (T01-7), the game's own drag-and-drop window (T04c-1, the Lua window),
  quest rewards to both players in full (T05b-2). Keep the decisions list.

## Next

1. **S10-T02a NPC logic per participant** (T01-5: condlists, meet, danger,
   combat-ignore, disguise against the nearest alive participant; L11 NPC
   part; L57 NPC walking animation on clients is routed here too). Start
   from the coverage matrix §4 and the S10-T01 report's NPC rows; the
   installed `xr_conditions`/`xr_logic` evaluate `db.actor` (outside a
   context the host actor): the likely work is a "nearest participant"
   actor for NPC logic evaluation (`campaign::gameplay_script_actor` has the
   rule) plus fixtures (an NPC's meet/greet toward player 2, a danger
   reaction to player 2). L57 (NPC movement/animation replication) is a
   presentation child: body state, movement type and aim of NPCs to clients.
2. Then S10-T02b–f, S10-T03a–f, S10-T05a/c/d, S10-T06; stop at G10 with
   the decisions list.

## Run names

- Used: `campaign-334…349`, `client-226…241`, `proto-build-52` (unused),
  `proto-build-53`, `st-28`, `upgrade-1…7`, `relations-1…3`, `stash-1…2`,
  patch checks 77–81; the rest in the snapshots.
- Next: `campaign-350`, `client-242`, `proto-build-54`, `st-29`, patch
  check 82; sessions `npc-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T02a](./snapshot-before-S10-T02a.md).
