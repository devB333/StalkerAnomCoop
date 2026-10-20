## Complete

Snapshot of the handoff that led into S05-T01 (history only; do not resume from it).

# Session handoff — G04 accepted; next: S05-T01

## State at the end of the session (2026-10-20)

**Stage 4 Completed.** The user accepted [G04](../../validation/G04.md) for
development on 2026-10-20, including:
- the handshake's man-in-the-middle limit (a G11/R06 release item);
- the provisional liveness defaults;
- the v1.0 header without per-message session ids.

Stage 5 is Not Started. No engine change in Stage 4: the engine stays at
`c57fe5b9` + patches 0001–0026. G03's carried limits are unchanged.

Networking is project-owned code in `protocol/` (Stage 1 compiler, `/MD`,
C++17, 0 warnings), tested in `tests/`:

| Library | What | Report |
|---|---|---|
| `coop_net`, `coop_net_gns` | transport adapter: GNS in one file, owner-thread delivery via `poll()`, per-channel overload policy, in-memory fake | [S04-T02](../../validation/S04-T02.md) |
| `coop_protocol` | wire format v1.0: 17 types, 6 channels, bounded decoder, canonical encoder, stream guard | [S04-T03](../../validation/S04-T03.md), [spec](../../network-protocol.md#wire-format-v1) |
| `coop_session`, `coop-keytool` | ECDSA P-256 challenge-response, DPAPI key files, public-key roster, player-owned slots, rate limits, heartbeats, rotated-token resume, stale-connection fencing, shutdown | [S04-T04](../../validation/S04-T04.md), [S04-T05](../../validation/S04-T05.md), [design](../../session-authentication.md) |

Last runs: `proto-build-13`; `pt-9` 61/61; `pt-10/11` liveness repeats;
`tr-9` 51/51.

## Carried into Stage 5

- Owned by S05-T01:
  - linking `protocol/` into the engine;
  - computing `build_id` / `content_digest` (defined in
    [sessions](../../session-authentication.md#build-and-content-identity));
  - attaching each session to its participant slot (S03-T01 registry,
    `attach_session`/`detach_session`);
  - applying the detached-actor policy (no input, stays vulnerable);
  - the headless server's power throttling and `Sleep` granularity
    (S04-T01 finding 5).
- Wire entity generations ↔ the registry's `EngineEntityKey` (S05-T02).
- Loopback only so far (S05 is the first two-process, two-client run;
  S11-T01 for real networks). No sanitizer. No recovery across a restart
  (Stage 7).
- Later security fix (G11/R06): an application-level key exchange signed by a
  pinned server key and the player keys, with game messages encrypted under
  it.

## Next

1. **S05-T01 — Integrate matched client and dedicated roles** (plan.md
   Stage 5). First action: decide how the engine links `protocol/`. Options:
   - build it as static libraries with the same CMake/toolchain and add them
     to the engine projects' link inputs;
   - add its sources to the engine projects.

   Record the choice. Then:
   - the campaign server owns a `session::Server` on its tick (opting out of
     power throttling), mapping `Joined`/`Left` to `attach_session` /
     `detach_session`;
   - the client owns a `session::Client`;
   - role guards: no client authority over shared AI/ALife/campaign state,
     each process with its own isolated launch profile.

   Validation per plan: all three processes start and connect; only the
   server advances shared gameplay; deliberate client outcome injection is
   refused; the server runs without clients.

## Tools and runs

- Protocol build: `powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Protocol.ps1' -Run <name> -Task <task>`;
  tests: `python3 tools/run-protocol-tests.py <run> --build <build-run> --task <task> [--exe protocol-tests] [--filter a,b]`.
- Engine build and smoke: `Build-Stage2.ps1 -Run <name> -Product Client|Campaign`;
  `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Identity|GameState`.
- Last run names: `proto-build-13`, `pt-11`, `tr-9`, `keytool-1`,
  `net-build-7`, `net-8`, `campaign-122`, `client-32`, `smoke-145`,
  `capture-24`, `patch-check-29a/b`. Next: `proto-build-14`, `pt-12`,
  `tr-10`, `campaign-123`, `client-33`, `smoke-146`, `capture-25`, patch
  check `30`.
- Run PowerShell scripts with the Windows path in single quotes from bash.
  Don't build while a GameState smoke runs.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02–G04 carried limits are unchanged.
Previous snapshot: [before S05-T01](snapshot-before-S05-T01.md).
