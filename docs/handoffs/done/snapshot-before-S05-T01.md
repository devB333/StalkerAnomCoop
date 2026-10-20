## Complete

# Session handoff — Stage 4 complete; G04 awaits user acceptance

## State at the end of the session (2026-10-20)

**Stage 4 In Progress: every task Completed (S04-T01..T05); G04 evidence
complete, awaiting the user's acceptance** ([G04](../../validation/G04.md)).
Stage 5 does not start before that. No engine change in Stage 4: the engine
stays at `c57fe5b9` + patches 0001–0026. G03's carried limits are unchanged.

Networking is project-owned code in `protocol/` (built with the Stage 1
compiler, `/MD`, C++17, 0 warnings) with tests in `tests/`:

| Library | What | Report |
|---|---|---|
| `coop_net`, `coop_net_gns` | transport adapter: GNS in one file, owner-thread delivery via `poll()`, per-channel overload policy, in-memory fake | [S04-T02](../../validation/S04-T02.md) |
| `coop_protocol` | wire format v1.0: 17 types, 6 channels, bounded decoder, canonical encoder, stream guard | [S04-T03](../../validation/S04-T03.md), [spec](../../network-protocol.md#wire-format-v1) |
| `coop_session`, `coop-keytool` | ECDSA P-256 challenge-response (BCrypt), DPAPI key files, public-key roster, player-owned slots, rate limits; heartbeats, suspect/timeout, rotated-token resume, stale-connection fencing, shutdown | [S04-T04](../../validation/S04-T04.md), [S04-T05](../../validation/S04-T05.md), [design](../../session-authentication.md) |

Final runs: **proto-build-13**; **pt-9** 61/61 (protocol, session, liveness;
fake and GNS loopback); **pt-10/11** liveness repeats 18/18; **tr-9**
transport 51/51.

## Decisions the user should review with G04

1. **Authentication scheme** (R06): per-player keys, server holds public keys
   only. Stated limit: an active man in the middle relaying a live handshake
   keeps the session, because GNS encryption is unauthenticated and
   open-source GNS certificates need a CA key compiled into the library.
2. **Liveness defaults** 1 s heartbeat / 5 s suspect / 15 s timeout,
   10-minute resume window, disconnected players keep their slots until
   released (provisional; measured only with short test values).
3. **Protocol v1.0 header** carries no session ids (bound once in the
   handshake), which departs from the original envelope draft.

## Carried (owners named in G04)

Loopback on one host only (S05, S11-T01); no sanitizer; engine integration,
build/content digests, session-to-participant attachment, the detached-actor
policy and power throttling (S05-T01); entity generation mapping (S05-T02);
no cross-restart recovery (Stage 7).

## Next

1. **User: accept or amend G04.** On acceptance: mark G04 Completed in
   plan.md §I, record the decision in `docs/validation/G04.md`, and start
   **S05-T01 — Integrate matched client and dedicated roles**. First action:
   decide how the engine links `protocol/`'s static libraries (a CMake
   library with the same compiler and runtime, or its sources added to the
   engine projects). Then run the session server on the campaign server's
   tick, opting out of power throttling (S04-T01 finding 5).

## Tools and runs

- Build: `powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Protocol.ps1' -Run <name> -Task <task>`
- Tests: `python3 tools/run-protocol-tests.py <run> --build <build-run> --task <task> [--exe protocol-tests] [--filter a,b]`
  (transport suite about 45 s; protocol suite about 30 s).
- Last run names: `proto-build-13`, `pt-11`, `tr-9`, `keytool-1`; unchanged:
  `net-build-7`, `net-8`, `campaign-122`, `client-32`, `smoke-145`,
  `capture-24`, `patch-check-29a/b`. Next: `proto-build-14`, `pt-12`,
  `tr-10`, `campaign-123`, `client-33`, `smoke-146`, `capture-25`, patch
  check `30`.
- Run PowerShell scripts with the Windows path in single quotes from bash.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02/G03 carried limits are unchanged.
Previous snapshot: [before G04 acceptance](./snapshot-before-G04-acceptance.md).
