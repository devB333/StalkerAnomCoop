## Complete

# Session handoff — S10-T04 complete, S10-T02a complete; next S10-T02b

## State (2026-10-29, end of session)

- **Completed this session, committed:** S10-T04c (0066), S10-T04f (0067,
  protocol v1.8), S10-T04d (0068), S10-T04e (0069) — **S10-T04 is
  complete** — and **S10-T02a NPC logic per participant**
  ([S10-T02a](../../validation/S10-T02a.md), patch 0070 `npc-logic`, patch
  check 82; builds **campaign-350 / client-242**; **npc-1 pass 5/5**: while
  an NPC's script binder update runs on the server, `db.actor` and `AC_ID`
  resolve to the nearest alive participant (`campaign::NpcEvaluationScope`
  in `CScriptBinder::shedule_Update`, Lua `campaign.npc_actor` /
  `npc_actor_id`); a participant context keeps its own actor; 15,000 NPC
  updates ran under the rule in a two-minute session).
- **Findings that bind later work:** Anomaly's inventory, trade, repair and
  loot windows are the installed Lua UI (T04f-5/6, T04e-1); the engine
  does not save the faction table (server-kept in the checkpoint, T04d-4);
  script logic *outside* any NPC's update and outside a participant context
  (smart terrains, squads, global managers) still reads actor 0 (L11's
  remaining part: S10-T03a/b). luabind pitfalls: never assign a call proxy
  into or call `type()` on a default-constructed `luabind::object`; no
  `luabind::iterator`; a variable named `pair` under `using namespace
  std`; `T x(u8(a), u8(b))` parses as a function declaration; C string
  literals in the Lua chunks need raw Python strings when edited by script.
- **Hands-on by the user, pending (list for G10):** a stalker greeting and
  talking to player 2 while player 1 is away (S10-T02a); the Lua loot window
  on a stash; the Lua repair window at Xenotech; the Lua trade window at
  Sidorovich; the partner's weapon switch and attack animations; the task
  map spot and Sidorovich's voice; the per-player relations tab.
  `bash tools/start-coop-demo.sh S10-T02a 0 20 0` (or S10-T04e/d/f/c).
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, T04d-1…4, T04e-1…3, **T02a-1…2**. Gameplay ones among
  the new rows: T02a-1 (each NPC decides against the player nearest to it,
  so two NPCs may treat the two players differently in one tick).
- **Limits:** L58, L59 (see the snapshots); L11 narrowed (NPC part), L57
  routed to S10-T05d.
- The user's standing directions (2026-10-29): trade is NPC-to-one-player
  (T01-7), the game's own drag-and-drop window (T04c-1, the Lua window),
  quest rewards to both players in full (T05b-2). Keep the decisions list.

## Next

1. **S10-T02b NPC loot and looting as server transfers.** Read the matrix
   row and `death_manager.script` (loot generation at death: runs where? on
   the server at `CEntityAlive::Die` in the killer's context, T05b-3),
   `ui_inventory` loot mode (the Lua window's `transfer_item` from a body:
   S08-T02's GE_TRADE path or the new T04f export hook — check which path a
   body take uses now and that it becomes a kind-7 transfer), the S08-T02
   body-transfer evidence, and L38 (a body created after the checkpoint).
   Likely small: verify by a fixture that a client's take from an NPC body
   through the Lua loot window is one journaled transfer, that the body's
   loot is generated once, and that both clients see the body's inventory.
2. Then S10-T02c–f, S10-T03a–f, S10-T05a/c/d, S10-T06; stop at G10 with
   the decisions list.

## Run names

- Used: `campaign-334…350`, `client-226…242`, `proto-build-52` (unused),
  `proto-build-53`, `st-28`, `upgrade-1…7`, `relations-1…3`, `stash-1…2`,
  `npc-1`, patch checks 77–82; the rest in the snapshots.
- Next: `campaign-351`, `client-243`, `proto-build-54`, `st-29`, patch
  check 83; sessions `loot-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T02b](./snapshot-before-S10-T02b.md).
