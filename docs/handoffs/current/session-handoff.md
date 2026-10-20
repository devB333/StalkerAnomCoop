# Session handoff — G05 accepted; next: S06-T01

## State (2026-10-20, end of session)

Stage 5 **Completed**: S05-T01–T04 Completed and the user accepted
[G05](../../validation/G05.md) for development on 2026-10-20. Stage 6 Not
Started. Engine `c57fe5b9` + patches 0001–0030 (exported, reconstruction
check 33); protocol v1.1 (`proto-build-15`). Design:
[co-op session](../../coop-session.md).

The user asked that carried limits be tracked: they are in
**[carried limits](../../carried-limits.md)** (L01–L27, every open limit from
G01–G05 with an owner task). Stage 6 owns L02 (prediction, S06-T03), L03
(input validation, S06-T01), L04 (NPC/item snapshots, S06-T02), L08
(per-participant friendly fire/god mode, S06-T04), L22 (measured
convergence), L25 (client scheduler stall, check in S06-T02) and part of L01
(fire, S06-T04). Each Stage 6 report says whether its rows still hold.

S05-T04 ([report](../../validation/S05-T04.md)): session-2 showed two joins,
late join and rejoin converging under server medkit churn, both actors on
both clients, own-only walks; patch 0030 fixed rejoin input being refused.
Regressions: smoke-152, capture-30.

## Next: S06-T01 — process validated movement inputs on the server

Read plan.md S06-T01 first. Starting points: the S05-T03 input path
(`CoopServerHost.cpp` `take_input` / `apply_inputs`, the client's
`sample_input` in `CoopClientLink.cpp`), the protocol's `Input` fields
(`protocol/include/coop/proto/messages.h`) and the native control path the
server already drives (`CActor` movement / `g_cl_CheckControls`). The plan's
risk: native movement may depend on camera orientation or client timestep;
translate intention explicitly.

## Tools and runs

- Session: `prepare-coop-session.py` + `Run-CoopSession.ps1` (scenario in
  [coop-session.md](../../coop-session.md#bounded-shared-world-session-s05-t04)).
- Builds: rebuild both products after any engine change (build ids must match).
- Last runs: `campaign-154`, `client-61`, `session-2`, `smoke-152`,
  `capture-30`, patch check 33. Next: `campaign-155`, `client-62`, a new
  co-op run series for S06 (check `builds/logs/S06-T01/`), `smoke-153`,
  `capture-31`, patch check 34.
- Regression rule: every engine change reruns the GameState smoke and the
  single-player checkpoint capture.
- Don't build while a session or smoke runs.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before S06-T01](../done/snapshot-before-S06-T01.md).
