## Complete

# Session handoff — S10-T04 complete, S10-T02a–d complete; next S10-T02e

## State (2026-10-30)

- **Completed and committed:** **S10-T02d bloodsucker, burer, poltergeist**
  ([S10-T02d](../../validation/S10-T02d.md), patch 0073 `mutant-abilities`,
  patch check 85; builds **campaign-362 / client-254**; **mutant-2 pass
  6/6**: the vampire grab, the anti-aim with the burer's stamina hit and the
  poltergeist's telekinesis all land on the monster's enemy, player 2, never
  on player 1; the grab's HUD and camera effects and the anti-aim's camera
  kick reach client 2 only; presentation `MonsterEffect`). Before it:
  S10-T04c/f/d/e (patches 0066–0069; S10-T04
  complete), S10-T02a (0070), S10-T02b (0071) and **S10-T02c controller and
  psy auras** ([S10-T02c](../../validation/S10-T02c.md), patch 0072
  `controller-psy`, patch check 84; builds **campaign-357 / client-249**;
  **psy-3 pass 6/6**: the controller's tube attack targets its participant
  enemy on the server, its camera, HUD and sound effects reach only that
  player's client through the new presentation `ControllerPsyHit`; monster
  auras are each viewer's own, driven by the client link's frame because a
  remote creature's scheduled and `UpdateCL` updates do not run on a client).
- **The user's hands-on of demo-2 (S10-T04e, 2026-10-30):** trade at
  Sidorovich works; the weapons work. Two bugs went to the new
  **bug-squashing list** at the top of `docs/known-issues.md` (B1 the
  trader's portrait and name missing in the Lua trade window; B2 a PDA
  glitch, details to confirm with the user — the partner's PDA gets no
  third-person animation because patch 0066 routes the server state to
  weapons and grenades only). The user asked for that list to grow until a
  dedicated bug session; add rows as bugs are found and keep building.
- **Findings that bind later work:** Anomaly's windows are the installed Lua
  UI (T04f-5/6, T04e-1); the engine does not save the faction table
  (T04d-4); NPC script logic decides against the nearest participant inside
  its own binder update (T02a-1); a remote creature on a client runs neither
  its scheduled update nor `UpdateCL` — presentation that lives there must be
  driven from the client link (`present_actors`), as the auras now are.
  luabind pitfalls: never assign a call proxy into or call `type()` on a
  default-constructed `luabind::object`; no `luabind::iterator`.
  `campaign::server_role()` is file-local: use
  `campaign::coop_server_prepared()` (`CoopServerHost.h`).
- **Hands-on by the user, pending (list for G10):** a controller attacking
  one player (effects on that screen only), each player feeling its aura by
  distance; an NPC looting a body; an NPC greeting player 2; the Lua loot,
  repair and trade windows; the partner's weapon animations; the relations
  tab. `bash tools/start-coop-demo.sh S10-T02c 0 20 0`.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8, T04f-1…6, T04d-1…4, T04e-1…3, T02a-1…2, T02b-1…2, T02c-1…3,
  **T02d-1…3**.
  Open question for the user: T05b-4 (who pays a task's money cost).
- The user's standing directions (2026-10-29): trade is NPC-to-one-player
  (T01-7), the game's own drag-and-drop window (the Lua window), quest
  rewards to both players in full (T05b-2). Keep the decisions list and the
  bug list.

## Next

1. **S10-T02e generic mutants, phantoms and AI lighting** (matrix F5; L12's
   rest: phantoms, psy dog, snork, pseudogigant threaten, scanning; L13 AI
   lighting per participant view). Same pattern; the mutant fixture
   (`mutant_fixtures`, stages, forced triggers, player 2's health held) is
   the template. Watch: an ability's trigger may need the player armed or a
   loose object nearby (mutant-1 lesson).
2. Then S10-T02f, S10-T03a–f, S10-T05a/c/d, S10-T06; stop at G10 with the
   decisions list and the bug list.

## Run names

- Used: `campaign-334…362`, `client-226…254`, `proto-build-52` (unused),
  `proto-build-53`, `st-28`, `upgrade-1…7`, `relations-1…3`, `stash-1…2`,
  `npc-1`, `loot-1`, `psy-1…3`, `mutant-1…2`, demos `demo-1…2` of S10-T04e,
  patch checks 77–85; the rest in the snapshots.
- Next: `campaign-363`, `client-255`, `proto-build-54`, `st-29`, patch
  check 86; sessions `mutant-3…` (S10-T02e).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T02e](./snapshot-before-S10-T02e.md).
