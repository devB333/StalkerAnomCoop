## Complete

# Session handoff — S10-T05d in progress (d1 complete, next d2 HUD statics)

## State (2026-10-31)

- **S10-T05d1 presentation traffic: complete and committed**
  ([S10-T05d](../../validation/S10-T05d.md), patch 0084
  `presentation-traffic`, patch check 96; proto-build-55; builds
  **campaign-406 / client-292**; **presentation-4 pass 4/4, emission 6/6**,
  0 absent records on either client (was 672), 1,872 records presented
  (was 243,142), 12 evicted (was 14,481)). Three causes found for the old
  volume flood:
  1. the level's ambient loops start before anyone joins;
  2. the join replay was queued before the client's presentation start,
     which skips everything queued earlier (presentation-1/2's `loops`
     check was too weak to see it; now it counts looped plays applied
     within 10 s);
  3. the surge's looped sounds were one shared object, so neither player
     heard its own pass's rumble, wave or earthquake since S10-T05a.

  Now each pass has its own loops, a muted play is not routed, and a
  follow-up from another player's context is dropped. B3 and B14 are
  fixed. New bugs: **B17** (ambient loops follow player 1) and **B18**
  (combat music follows player 1 and is heard by both). L05 and L28
  narrowed. Decisions T05d-1…3.
- The installed scripts are packed in `scripts.db0`. Extract them with
  `python3 tools/xray-db-extract.py -q extract -o <scratch>/scripts -g '*.script' builds/build_2/test-install/source-client/db/configs/scripts.db0`.
  Grep extracted files with `grep -a`, because some are ISO-8859.
  `engine/xray-monolith/gamedata/scripts` holds only the Monolith's own 70
  scripts.
- **Plan for the rest of S10-T05d:** **d2** HUD custom statics: route
  `CUIGameCustom::AddCustomStatic` / `RemoveCustomStatic` on the server
  **only inside a player's context** (e.g. each player's emission psi
  indicators, `surge_manager` `displayIndicators`), the text set right
  after an add by a tick poll (new presentation kinds after
  `ControllerPsyHit`; client applies with `CurrentGameUI()->AddCustomStatic`).
  Statics added outside a context come from actor 0's binder
  (`actor_effects` helmet overlay, vignette) and are dropped: actor 2 has no
  binder, so record it as a limit. Statistics and achievements are the
  party's (one `alife_storage_manager` table; the clients' PDA shows the
  copy from their join): a decision plus a limit. **d3** body presentation
  (B4 NPC movement, B7 zoom, L47 use animation): the plan allows recording
  them as not shown; B4/B7 are already on the S10-T07 bug list.
- Earlier: S10-T03 (a…f), S10-T02 (a…f), S10-T04 (a…f), S10-T05a…c
  completed; see the [previous snapshot](../done/snapshot-before-S10-T05d2.md)
  and plan.md §I.
- **Bug-fixing stage S10-T07** (user, 2026-10-30) runs after S10-T06 and the
  G10 decision review. Keep adding rows to the
  [bug-squashing list](../../known-issues.md) (B1…B18 now; B3, B14, B15
  fixed).
- **Findings that bind later work:** Anomaly's windows are the installed Lua
  UI (T04f-5/6, T04e-1); the engine does not save the faction table
  (T04d-4); NPC script logic decides against the nearest participant
  (T02a-1), a companion against its recruiter (T02f-1); a remote creature on
  a client runs neither its scheduled update nor `UpdateCL`. A sound theme
  (`sound_theme.script`) holds **one `sound_object` shared by everyone**:
  per-player script runs need their own sound objects. Inside a context
  `AC_ID` and `db.actor` are that player's. luabind: never assign a call
  proxy into or call `type()` on a default-constructed `luabind::object`; no
  `luabind::iterator`. Use `campaign::coop_server_prepared()`, not the
  file-local `server_role()`. Fixtures move player 1 only (L58).
- **Hands-on by the user, pending for G10:** unchanged from the
  [previous snapshot](../done/snapshot-before-S10-T05d2.md). Add: **hearing
  the emission's rumble build up for each player**, and the wind on joining.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): as before, plus **T05d-1…3**.
  Open question: T05b-4.

## Next

1. **S10-T05d2** HUD statics and statistics (above): build, fixture run,
   report section in `docs/validation/S10-T05d.md`, patch 0085.
2. **d3**: record B4/B7/L47. Then S10-T06 (co-op menu). Then stop at G10
   with the decisions list and the bug list (B1…B18) for item-by-item
   approval. S10-T07 (bugs) follows.

## Run names

- Used: `campaign-334…406` (373, 374, 384, 385, 395 failed to compile,
  381 to link), `client-226…292`, `proto-build-52` (unused),
  `proto-build-53…55`, `st-28…30`, `sweep-1…7`, `newgame-1…4`,
  `emission-1…5`, `story-1…2`, `presentation-1…4`, patch checks 77–96.
- Next: `campaign-407`, `client-293`, `presentation-5` (or `hud-1`),
  `proto-build-56`, `st-31`, patch check 97.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T05d2](../done/snapshot-before-S10-T05d2.md).
