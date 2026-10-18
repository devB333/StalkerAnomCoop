# Session handoff — Stage 2 engineering complete incl. the quest round-trip; next: the user's G02 decision, then Stage 3

## State at the end of the session (2026-10-18)

S02-T06e (a quest across two checkpoints) is **Completed** and committed as
patch 0020 (`548dffb8…7d47`, 9 files). On the campaign server the Clear Sky
mechanic's fetch task is given through the installed dialogue functions,
tracked by the native and the Lua task manager, carried through the checkpoint,
completed through the installed turn-in (money 1,000 → 10,700, goodwill 0 → 75,
info portion, item removed) and carried through a second checkpoint (smoke-95,
exit 0, 47 quest rules, 50 checkpoint rules); the normal client restores and
completes it from the first checkpoint (capture-12) and loads the completed
state from the second (capture-13); `compare-quest-capture.py` matches all
seven comparisons. Details: part 6 of [S02-T06](../../validation/S02-T06.md).

Every Stage 2 task (S02-T01–T06) is Completed. The **G02 evidence record**
([G02.md](../../validation/G02.md)) now shows every requirement observed within
its recorded limits (one quest, one giver, one level; dialogue window not
shown; B21 carried). G02 stays **In Progress** until the user accepts it. Do
not mark G02 Completed without that decision.

## Findings worth keeping

- **B21** (boundary doc): monster AI that takes the player from
  `Level().CurrentEntity()`. The psy dog aura crashed the server and is guarded
  in patch 0020 (presentation only); `CPhantom` (psi-storm phantoms) and
  `CAI_Trader::LookAtActor` still dereference it unguarded and will fault
  headless if reached. Stage 3 owns the per-participant form.
- A squad member online at a save is held under its online-offline group after
  the load (`CALifeGraphRegistry::update` registers members only under direct
  control): ALife total exact, level registry one lower. The checkpoint rule
  names each such object and accepts the difference; `compare-checkpoint-capture.py`
  uses the same tolerance for the level registry.
- Spawning a child of an **online** parent must go through the server
  (`CALifeSimulator__spawn_item2`, the `alife():create` binding); the offline
  `spawn_item` with a parent leaves a child without a server object and the
  level stop asserts `child registered but not found`.
- Reading Lua from C++: luabind class instances are not reliably indexable
  from C++; use a plain-table helper (defined per Lua state, which a level
  restart recreates) and never inspect a default-constructed `luabind::object`
  (it has no state and faults on `type()`).
- Script files in the archives are cp1251 with CRLF; the system `grep` (ugrep)
  treats them as binary and silently returns nothing. Use `grep -a`.
- The hearing stage's per-run variation (`npc_walk_events=0`) hit smoke-81,
  -84 and -94; rerun.
- capture-10's client exited `0xC0000005` after teardown (WER: `AddOne` in
  xrCore's log, logging after `g_pStringContainer` was destroyed); not
  reproduced since. Unexplained.

## Next steps

1. **Ask the user for the G02 decision** ("Decision requested" in G02.md).
   If accepted: set G02 Completed in plan.md (§D gate table row, §I table, the
   stage summary and "Roadmap at a glance"), record the acceptance and the
   named risks in G02.md the way G01.md does, commit `docs: accept G02`.
2. Then Stage 3, S03-T01 (participants and authoritative contexts), per
   plan.md. Read plan.md §D (D1–D8 binding) first. B21 is a concrete Stage 3
   input: every `Level().CurrentEntity()` / `Actor()` in gameplay code must
   become a participant lookup.

## Tools and runs

- Last run names: `campaign-81`, `client-25`, `smoke-95`, `capture-13`
  (`patch-check-21a/b`). Next: `campaign-82`, `client-26`, `smoke-96`,
  `capture-14`; next patch export is `python3 tools/export-engine-patch.py <name> 22`.
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope GameState` (~75 s; expects the quest
  stages and both checkpoints; 900 s timeout). Client captures:
  `prepare-stage2-smoke.py <run> <task>` then
  `Run-ClientCollisionCapture.ps1 -Run <run> -Capture collision|physics|activity|hearing|combat|checkpoint|quest|quest-persisted [-CheckpointRun <smoke>]`
  (quest captures are task `S02-T06e`). Compare tools: `tools/compare-*-capture.py`
  (quest: server CSV, client quest CSV, client persisted CSV).
- Quest fixture options: `-s02-quest-task <id>`, `-s02-quest-giver <profile>`.
- Build: `Build-Stage2.ps1 -Run <name> -Product Campaign|Client` (~1–2 min);
  build logs land in `builds/logs/S02-T02/<name>/`. Do not chain a build on
  `grep -c` (it exits 1 on zero matches).
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous snapshot: [before the G02 acceptance](../done/snapshot-before-G02-acceptance.md).
