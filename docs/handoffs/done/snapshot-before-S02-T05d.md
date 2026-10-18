## Complete

# Session handoff — S02-T03a–c and S02-T04a complete; level work next

G02 is In Progress. No headless gameplay, level, ALife tick or physics step has run.
S02-T03 and S02-T04 were split into suffix tasks on 2026-10-18 (see `plan.md`):

| Task | Status | Evidence |
|---|---|---|
| S02-T03a materials without pair media | Completed | [S02-T03a](../../validation/S02-T03a.md), patch 0004 |
| S02-T03b CPU-only model pool, real model load | Completed | [S02-T03b](../../validation/S02-T03b.md), patch 0005 |
| S02-T03c bone evaluation against a client capture | Completed | [S02-T03c](../../validation/S02-T03c.md), patch 0007 |
| S02-T03d skeleton collision, hit picking, physics shell | Not Started | needs a physics world, which needs a level |
| S02-T04a server clock and frame driver | Completed | [S02-T04a](../../validation/S02-T04a.md), patch 0006 |
| S02-T04b physics stepping, falling-body collision | Not Started | needs a level `CObjectSpace` |
| S02-T04c objects/AI/ALife and activity policy | Not Started | needs a level and ALife |

Start with the [internal context definition](../../campaign-server-context.md), which
gives the service order and the remaining blockers. The services start in this
order, and teardown runs in reverse:
1. `cpu-models`
2. `spatial-db`
3. `materials`
4. `script-engine`
5. `campaign-registries`
6. `frame-driver`

## What runs today (final binaries)

- **Builds.** `campaign-18` (exe `f14ccc26…`) and `client-11` (exe `def70d56…`),
  0 errors.
- **`smoke-35 -Scope Services` passes:**
  - all six services start
  - the model fixture loads the HUD arms and two stalkers on the CPU
  - the 300-tick frame-driver fixture runs, with a stall and a pause
  - the bone capture runs
  - graphics, window and input stay absent
- **Client match.** `capture-02` is the client's bone capture. It is
  bit-identical to the server's (`builds/logs/S02-T03c/compare-02.json`).
- **GameState** still stops at `persistent-context-required` (exit 3, not a pass).
- **Patches.** 0001–0007 rebuild all 28 changed files byte for byte. Use
  `python3 tools/export-engine-patch.py <name> <n>` for the next patch.

## Open dependency question

Every remaining Stage 2 item needs a loaded level: T03d, T04b, T04c, T05 and T06.
But `plan.md` lists S02-T03 and S02-T04 (the parents) as prerequisites of S02-T05.
Until that is resolved, start nothing that needs a level. The proposed fix is in
the session summary.

## Tools and runs

- **Last run names:** `campaign-18`, `client-11`, `smoke-35`, `capture-02`.
  `smoke-20` was suggested to the user for a manual run, so skip it.
  `client-10` was interrupted; see its `INTERRUPTED.txt`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`.
  Options: `-InjectFailure <service>`, `-Models a,b`, `-ExpectModelFailure`.
- **Client capture:** `python3 tools/prepare-stage2-smoke.py <run> S02-T03c`, then
  `Run-ClientBoneCapture.ps1 -Run <run>`. It opens a client window briefly.
- **Smart App Control** is off (the user turned it off on 2026-10-17). Newly built
  unsigned executables are blocked when it is on.

Preserve:
- the original installation
- the reference checkout
- the accepted source-client runtime
- the saved-game fixture

The two G01 waivers are unchanged.
Previous handoff: [before S02-T05](snapshot-before-S02-T05.md).
