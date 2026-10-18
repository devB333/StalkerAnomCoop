## Complete

# Session handoff — G02 accepted for development; next: Stage 3, S03-T01

## State at the end of the session (2026-10-18)

The user accepted **G02 for development** on 2026-10-18 with its recorded
limits carried into Stage 3 as named risks ([G02](../../validation/G02.md),
"User decision and carried limits"). Stage 2 is Completed. No engine change
this session; last patch is still 0020 (`548dffb8…7d47`).

Each carried limit has a Stage 3 handling rule and a later owner (table in
G02.md; matching notes under Stage 3 and in S03-T02/T04/T05 in plan.md):

- **B21** → S03-T02: inventory and classify every `Level().CurrentEntity()` /
  `Actor()` use in `src/xrGame` (raw grep 2026-10-18: 126 `CurrentEntity()` in
  65 files, 372 `Actor()` in 85 files, debug/UI included); resolve or keep
  behind a recorded guard `phantom.cpp:86,149,266`, `ai_trader.cpp:99` and the
  psy dog aura guard (patch 0020). S03-T04 records the other player-bound
  monster abilities for S10-T02.
- **One quest** → S03-T05 reruns the S02-T06e quest fixture per participant.
- **One level/save, short runs, per-run variation, client quit fault** →
  recorded in each Stage 3 report; owners S09/S10, S11.
- **Regression rule:** every Stage 3 task that changes engine-work reruns the
  GameState smoke; a failure that is not recorded per-run variation reopens G02.

## Next steps

1. Read plan.md §B (D1–D8 binding), §D (G03) and S03-T01–T05.
2. S03-T01 (participants and authoritative contexts): define campaign/player/
   party/entity/item IDs, generation-aware mapping over recycled u16 engine
   IDs, a runtime participant registry with two slots as policy, and explicit
   action context and role boundaries. Validation: unit fixtures for slot
   reuse, disconnect identity retention and role/context misuse. No test
   framework exists yet; S03-T01 will create `tests/` (AGENTS.md).
3. Start S03-T02's B21 inventory early if it informs the context API.

## Tools and runs

- Last run names: `campaign-81`, `client-25`, `smoke-95`, `capture-13`
  (`patch-check-21a/b`). Next: `campaign-82`, `client-26`, `smoke-96`,
  `capture-14`; next patch export is `python3 tools/export-engine-patch.py <name> 22`.
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope GameState` (~75 s; 900 s timeout).
  The hearing stage occasionally reports `npc_walk_events=0`; rerun once and
  record it.
- Client captures, compare tools and quest fixture options: see the
  [previous snapshot](snapshot-before-S03-T01.md#tools-and-runs).
- Build: `Build-Stage2.ps1 -Run <name> -Product Campaign|Client` (~1–2 min).
  Do not chain a build on `grep -c` (it exits 1 on zero matches).
- Script files in the archives are cp1251/CRLF; use `grep -a`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous snapshot: [before S03-T01](snapshot-before-S03-T01.md).
