## Complete

# Session handoff — S10-T04 complete, S10-T02a/b complete; next S10-T02c

## State (2026-10-29, end of session)

- **Completed this session, committed:** S10-T04c/f/d/e (patches 0066–0069;
  **S10-T04 complete**), S10-T02a (0070) and **S10-T02b NPC loot and
  looting** ([S10-T02b](../../validation/S10-T02b.md), patch 0071 `npc-loot`,
  patch check 83; builds **campaign-354 / client-246**; **loot-1 pass 5/5**:
  a body filled in player 2's context, an NPC's take through the installed
  corpse scheme reaching both clients, a body within 75 m of a player kept
  from the release manager's release).
- **Findings that bind later work:** Anomaly's windows are the installed Lua
  UI (T04f-5/6, T04e-1); the engine does not save the faction table (T04d-4);
  NPC script logic decides against the nearest participant inside its own
  binder update (T02a-1) but global script logic (smart terrains, squads,
  managers) still reads actor 0 (L11's remaining part, S10-T03a/b); moves
  between non-participant objects are checkpoint-durable only (T02b-1).
  luabind: never assign a call proxy into or call `type()` on a
  default-constructed `luabind::object`; no `luabind::iterator`; a variable
  named `pair`; `T x(u8(a), u8(b))`; raw Python strings for the Lua chunks.
  `campaign::server_role()` is file-local (anonymous namespace): use
  `campaign::coop_server_prepared()` from `CoopServerHost.h` elsewhere.
- **Hands-on by the user, pending (list for G10):** an NPC looting a body on
  both screens and a body staying while the partner walks away (S10-T02b);
  a stalker greeting player 2 while player 1 is away (S10-T02a); the Lua
  loot window on a stash; the Lua repair window at Xenotech; the Lua trade
  window at Sidorovich; the partner's weapon switch and attack animations;
  the task map spot and Sidorovich's voice; the per-player relations tab.
  `bash tools/start-coop-demo.sh S10-T02b 0 20 0` (or S10-T02a/T04e/d/f/c).
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, T04d-1…4, T04e-1…3, T02a-1…2, **T02b-1…2**. Gameplay
  ones among the new rows: T02b-2 (a body near either player is never
  released by the installed manager).
- **Limits:** L58, L59 (see the snapshots); L11 narrowed (NPC part); L57
  routed to S10-T05d.
- The user's standing directions (2026-10-29): trade is NPC-to-one-player
  (T01-7), the game's own drag-and-drop window (T04c-1, the Lua window),
  quest rewards to both players in full (T05b-2). Keep the decisions list.

## Next

1. **S10-T02c controller and psy auras** (the controller's psy hit and the
   psy aura, `monster_aura.cpp`, `controller_psy_hit.cpp`: against either
   participant; the aura's per-player effect and the client presentation).
   Read the matrix row (F3?), `monster_aura.cpp` (patched since S06 for the
   nearest participant?), `controller_psy_hit.cpp` and the S06 mutant
   fixtures (`CampaignActivationFixture`, the encounter fixture) for the
   spawn-and-attack pattern; L12/L13 (global Actor() in mutant abilities).
2. Then S10-T02d–f, S10-T03a–f, S10-T05a/c/d, S10-T06; stop at G10 with
   the decisions list.

## Run names

- Used: `campaign-334…354`, `client-226…246`, `proto-build-52` (unused),
  `proto-build-53`, `st-28`, `upgrade-1…7`, `relations-1…3`, `stash-1…2`,
  `npc-1`, `loot-1`, patch checks 77–83; the rest in the snapshots.
- Next: `campaign-355`, `client-247`, `proto-build-54`, `st-29`, patch
  check 84; sessions `psy-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T02c](./snapshot-before-S10-T02c.md).
