## Complete

# Session handoff — S10-T05 complete; next S10-T06 (co-op menu and session control)

## State (2026-10-31)

- **S10-T05d presentation split: Completed and committed**
  ([S10-T05d](../../validation/S10-T05d.md); proto-build-55). With it,
  **S10-T05 is complete** (a…d).
  - **d1** traffic (patch 0084, campaign-406 / client-292,
    **presentation-4 pass 4/4**). 0 absent records (was 672) and 1,872
    records per run (was 243,142). Changes:
    - the level's ambient loops are played to a joining client once its
      presentation starts;
    - a muted play is not routed;
    - each surge pass has its own looped sounds.
  - **d2** HUD statics (patch 0085, campaign-408 / client-294,
    **presentation-5 pass 5/5, emission 6/6**). Inside a player's context,
    `Add/Get/RemoveCustomStatic` work on that player's own set on the
    server and are shown on its HUD with their text. The exposed player saw
    the psi indicator change colour (5 adds / 5 removes); the covered player
    saw none. Statistics stay the party's (T05d-5). **Not exercised by a
    run:** the statics replay at a join and the text update. A hands-on
    `sr_timer` or disguise message would show them.
  - **d3**: B4/L57, B7 and L47 recorded as not shown, routed to S10-T07.
  - B3 and B14 fixed. New bugs B17 (ambient loops follow player 1) and B18
    (combat music follows player 1, heard by both). L05 and L28 narrowed.
    Decisions **T05d-1…5**.
- The installed scripts are packed in `scripts.db0`. Extract them with
  `python3 tools/xray-db-extract.py -q extract -o <scratch>/scripts -g '*.script' builds/build_2/test-install/source-client/db/configs/scripts.db0`
  and grep with `grep -a`. `engine/xray-monolith/gamedata/scripts` holds
  only the Monolith's own 70 scripts.
- **Bug-fixing stage S10-T07** (user, 2026-10-30) runs after S10-T06 and the
  G10 decision review. The [bug-squashing list](../../known-issues.md) has
  B1…B18 (B3, B14, B15 fixed).
- **Findings that bind later work:**
  - Anomaly's windows are the installed Lua UI (T04f-5/6, T04e-1).
  - The engine does not save the faction table (T04d-4).
  - NPC script logic decides against the nearest participant (T02a-1); a
    companion decides against its recruiter (T02f-1).
  - A remote creature on a client runs neither its scheduled update nor
    `UpdateCL`.
  - A sound theme holds one `sound_object` shared by everyone, and the
    server has one HUD for everyone. Per-player script runs need their own
    objects (S10-T05d).
  - Inside a context, `AC_ID` and `db.actor` are that player's.
  - luabind: never assign a call proxy into, or call `type()` on, a
    default-constructed `luabind::object`; no `luabind::iterator`.
  - `xr_sprintf` is `sprintf_s`, which aborts on overflow: use
    `_snprintf_s(..., _TRUNCATE)` for text from scripts.
  - Use `campaign::coop_server_prepared()`, not the file-local
    `server_role()`.
  - Fixtures move player 1 only (L58).
- **Hands-on by the user, pending for G10:** unchanged from the
  [S10-T05d2 snapshot](../done/snapshot-before-S10-T05d2.md). Add:
  - the emission's rumble building up for each player;
  - the wind on joining;
  - the psi indicator on the exposed player's HUD only.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…T05c-3 as before, plus
  **T05d-1…5**. Open question: T05b-4.

## Next

1. **S10-T06 co-op menu and session control** (T01-17…19, approved
   2026-10-28; matrix row F25; L51 closed by S10-T03f for the server side).
   It has no section of its own in plan.md yet. Write one and split it into
   children:
   - **a** a protocol session-control message (create / load / list saves;
     owner-only; a protocol change, proto-build-56);
   - **b** the server carrying out a create (S10-T03f's new game, in
     process) or a load (another generation or save) and both clients
     re-attaching as after a travel;
   - **c** the client's main-menu Co-op page (installed Lua main menu,
     `ui_main_menu.script`) with the standard creation screen for the owner
     and loadout-only for the joiner;
   - **d** the owner role (the first key, T01-18).
2. Then stop at **G10** with the decisions list and the bug list (B1…B18)
   for item-by-item approval. S10-T07 (bugs) follows.

## Run names

- Used: `campaign-334…408` (373, 374, 384, 385, 395, 407 failed to compile;
  381 failed to link), `client-226…294` (293 failed to compile),
  `proto-build-52` (unused), `proto-build-53…55`, `st-28…30`,
  `sweep-1…7`, `newgame-1…4`, `emission-1…5`, `story-1…2`,
  `presentation-1…5`, patch checks 77–97.
- Next: `campaign-409`, `client-295`, `proto-build-56`, `st-31`, patch
  check 98.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T06](../done/snapshot-before-S10-T06.md).
