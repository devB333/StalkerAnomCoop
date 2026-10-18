## Complete

# Session handoff — S02-T06 complete, G02 evidence recorded; next: the user's G02 decision, then Stage 3

## State at the end of the session (2026-10-18)

S02-T06d (native checkpoint round-trip) is **Completed** and committed as patch
0019 (`02d97fd5…50bdd`, 7 files). The campaign server saves through the host's
own `M_SAVE_GAME` path, restarts the level from the checkpoint and gets its
ALife, clock, actor and NPC state back (smoke-83, exit 0, 54 s wall, 25 rules
ok); the normal client loads the same checkpoint and
`compare-checkpoint-capture.py` matches (capture-11, exit 0). Details: part 5
of [S02-T06](../../validation/S02-T06.md).

Every Stage 2 task (S02-T01–T06) is Completed. The **G02 evidence record**
([G02.md](../../validation/G02.md)) maps each gate requirement to its evidence
and names the one gap: task (quest) execution is observed only as the
installed task scripts running without error through the actor's binder. No
task was given, advanced or completed headless. G02 stays **In Progress**
until the user accepts it (with the gap as a named risk) or asks for a quest
fixture first. Do not mark G02 Completed without that decision.

## Findings worth keeping

- The client's checkpoint snapshot must get the fixture ids from the command
  line (`-s02-checkpoint-npc`, `-s02-checkpoint-monster`): the activity
  fixture's id accessor returns 0 (the actor) when no fixture ran
  (capture-10 described the actor in the stalker columns).
- A normal client's ALife clock runs through its level load (~75–85 s of game
  time), so its loaded object count can be a few lower than the checkpoint's;
  the compare tool's rules for rows taken after the level ran are the C++
  ticked rules (counts within max(10, 0.1 %), stalker health within 0.05).
- The Lua `.scoc` is written with `io.open`; `FS` never sees it, check with
  `_stat64`.
- capture-10's client exited `0xC0000005` after its full teardown log;
  capture-11 (final source) exited 0. Unexplained; watch for it.
- smoke-81 failed its hearing stage (`npc_walk_events=0`) and passed on rerun:
  per-run variation, not a code change.

## Next steps

1. **Ask the user for the G02 decision** (see "Decision requested" in G02.md).
   If accepted: set G02 Completed in plan.md (§D gate table row, §I table, the
   stage summary and "Roadmap at a glance"), record the acceptance and the
   named risk in G02.md the way G01.md does, commit `docs: accept G02`.
   If a quest fixture is required: new sub-task S02-T06e; the task manager
   runs through the actor binder, so give a task through the native script
   path and compare its state across the checkpoint on both products.
2. Then Stage 3, S03-T01 (participants and authoritative contexts), per
   plan.md. Read plan.md §D (D1–D8 binding) before designing.

## Tools and runs

- Last run names: `campaign-69`, `client-24`, `smoke-83`, `capture-11`
  (`patch-check-20a/b`). Next: `campaign-70`, `client-25`, `smoke-84`,
  `capture-12`; next patch export is `python3 tools/export-engine-patch.py <name> 21`.
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope GameState` (~55–80 s; expects the
  checkpoint stages too). Client captures: `prepare-stage2-smoke.py <run> <task>`
  then `Run-ClientCollisionCapture.ps1 -Run <run> -Capture collision|physics|activity|hearing|combat|checkpoint [-CheckpointRun <smoke>]`.
  Compare tools: `tools/compare-*-capture.py`.
- Build: `Build-Stage2.ps1 -Run <name> -Product Campaign|Client` (~1m45s each
  for game-module changes); build logs land in `builds/logs/S02-T02/<name>/`.
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous snapshot: [before the G02 decision](./snapshot-before-G02-decision.md).
