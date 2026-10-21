# Session handoff — G06 accepted; next: S06-T06 (Cordon fixture)

## State (2026-10-21)

- **G06 accepted** 2026-10-21 with five user decisions
  ([G06](../../validation/G06.md#user-decision)). Stage 6 stays In Progress
  for three tasks added at the gate: **S06-T06** Cordon fixture without Fanat
  (next), **S06-T07** jump and lean, **S06-T08** storm and emission visuals.
  **S07-T07** (respawn with a lootable corpse) was added after S07-T02/T03.
- Engine: patches 0001–0035; final build `campaign-202` / `client-105`;
  protocol v1.4 (`proto-build-30`). New carried limits L28–L31.
- A hands-on session `play-1` is prepared (not launched), with both players
  armed and no program. The command is in the last session summary:
  `Run-CoopSession.ps1 -Run play-1 -Task S06-T05 ... -ClientSeconds '1700,1690'`.

## S06-T06 first steps

1. A Cordon save. Only the marsh save exists
   (`builds/build_2/validation-evidence/20261017-user-gameplay/savedgames`).
   A new game takes its start from `axr_main.config` `[character_creation]`
   (`ui_mm_faction_select.script:1671-1760`: faction, map, name, loadout),
   then `start server(all/single/alife/new)` (`ui_main_menu.script:255`).
   Plan: an isolated client runtime with those settings written, a new-game
   start, then a native save, recorded with hashes like the marsh fixture. If
   that is not representative, ask the user for a save.
2. Remove Fanat (19409) from the co-op fixtures and programs; find every
   marsh-specific id and position in `CoopServerHost.cpp`,
   `CoopClientLink.cpp`, `prepare-*.py` and the compare tools.
3. Rerun the Stage 6 programs in Cordon.

## Open questions for the user

- S07-T07: what a respawned player carries (nothing, a starting kit, or the
  checkpoint's inventory).

## Run names

- Used: builds up to `campaign-202`, `client-105`; `enc-1…19`,
  `combat-14…16`, `session-11/12`, `snap-17/18`, `smoke-162/163`,
  `capture-38…43`, `play-1`; patch check 38.
- Next: `campaign-203`, `client-106`, `smoke-164`, `capture-44`, patch
  check 39.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before S06-T06](../done/snapshot-before-S06-T06.md).
