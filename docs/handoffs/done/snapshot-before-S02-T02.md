# Session handoff — 2026-10-17, S02-T01 complete

The user resumed `plan.md` at S02-T01. That source-mapping task is **Completed**;
Stage 2/G02 is **In Progress**, S02-T02–T06 are **Not Started**. Stage 0/G00 and
Stage 1/G01 remain completed for development with the two user-approved waivers:
completed quest/reward persistence and copied original-client comparison remain
untested. D1–D8 and stage order are unchanged. [plan.md](../../../plan.md) is authoritative.

## Current evidence

- [S02-T01 validation](../../validation/S02-T01.md) records exact commands and limits.
  [Service boundary](../../headless-service-boundary.md) maps startup, first tick,
  shutdown, service ownership and ten runtime assertions.
- [Fixture manifest](../../headless-fixture.json) pins the preserved real campaign
  autosave/log. No external clients; no dummy actor; CPU bones/collision required.
  Runtime actor/NPC/model/shot identity and level confirmation remain pending.
- `python3 tools/verify-headless-boundary.py` passed: 62 source ranges, 33 source
  files, accepted source/Optick revisions and three input hashes. Evidence lives
  in `builds/logs/S02-T01/static-verification.json`; document checks alongside it.
  No engine build or runtime test was performed. G02 has no behavioral pass.

## Source and changes to preserve

Use clean `builds/build_1/engine-work` at
`c57fe5b9fab4fe0506659db03137c52d2500eb2f`, Optick
`8abd28dee1a4034c973a3d32cd1777118e72df7e`. Root owns future engine patches under
`patches/engine`; no engine patch was created in S02-T01. Root files remain
uncommitted/untracked. Preserve all prior work.

Reference `engine/xray-monolith` has user changes: packaging project edit, dirty
Optick and untracked build logs. It is not the Stage 2 working tree. The accepted
runtime is `builds/build_2/test-install/source-client`; EXE SHA-256 is
`5b9b0719f196b7e86ee140a37ef8f0caa2561558bfd0d5b1e247a085e1363603`.
Do not replace that executable or write to the user's saves. Original installation
is read-only; Ghidra databases remain preserved.

This session added the map, fixture manifest, validation report and verifier;
updated `plan.md`, this handoff, README, AGENTS baseline status and the historical
roadmap's current-status pointer. Previous handoff preserved verbatim in
[snapshot-before-S02-T01.md](snapshot-before-S02-T01.md).

## Next: S02-T02

Start by reading the map's B01–B08 and lifecycle contract, then inspect the accepted
solution/project configuration to add a distinct campaign-server role/target.
Keep `NO_SINGLE` absent and native `CLevel`/`game_cl_Single`/ALife present. The old
runtime dedicated flag omits task/map/restriction/spawn managers and level Lua;
simply enabling it is insufficient. Retain task/map data, CPU models/materials and
hearing metadata; split UI/draw/audio playback through later T03–T05 work.

Add staged initialization/shutdown smoke logging, preserve private bridge ownership,
export reproducible patches, then build both client and dedicated configurations.
Check Windows toolchain paths after the earlier build-directory reorganization.
Stop on a concrete failed initialization boundary with evidence; a temporary
graphics dependency does not pass G02. The final server must run the real fixture
without graphics/view actor and join work before checkpoint/teardown. Full
two-participant context work remains Stage 3.
