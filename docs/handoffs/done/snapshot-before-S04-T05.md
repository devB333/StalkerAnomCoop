## Complete

# Session handoff — S04-T03 complete; next: S04-T04

## State at the end of the session (2026-10-20)

**Stage 4 In Progress.** S04-T01, S04-T02 and S04-T03 Completed. No engine
change, no patch since 0026. G03's carried limits are unchanged.

- **S04-T02** — bounded transport adapter
  ([report](../../validation/S04-T02.md)): `coop::net::Transport` in
  `protocol/`, GNS confined to `src/net/gns_transport.cpp`, owner-thread-only
  delivery through `poll()`, per-channel overload policy. tr-4..7: 51/51.
- **S04-T03** — protocol codec v1.0 ([report](../../validation/S04-T03.md),
  [wire format](../../network-protocol.md#wire-format-v1)): `coop_protocol`
  (`protocol/include/coop/proto/`) has 15 types on 6 channels
  (`transport_config()`) and a 24-byte header. The decoder bounds every
  field; the encoder refuses what the decoder refuses. The per-connection
  guard (`guard.h`) checks epochs, sequences, entity generations, resync
  and fresh inputs. pt-3: 20/20 with 2.0 M guarded fuzz mutations.

## Found (inputs to later tasks)

1. Critical producers must pace below the send rate (S04-T02 finding 3).
2. GNS impairments survive Kill/Init; closing a listen socket drops its
   connections silently (handled by the adapter).
3. The session's ids bind once in Hello/Welcome. Hello's layout is frozen
   across majors, so a mismatch can be answered.
4. Unverified or passed on: what build id/content digest cover, the auth
   proof and rate limits (S04-T04); heartbeat timing (S04-T05); wire entity
   generation mapping (S05-T02); one host, loopback, no sanitizer; the
   notice inbox is unbounded under a connection flood (S04-T04); engine
   linkage and power throttling (S05-T01).

## Next

1. **S04-T04 — Bind authenticated sessions to canonical players.** First
   action: decide the initial deployment/authentication scheme (plan.md R06;
   GNS encryption is unauthenticated, S04-T01) and write it down. Then build
   a session manager in `protocol/` on top of `Transport` + codec + guard:
   join state machine (Hello → check → auth → Welcome/Reject), two active
   slots bound to stable player ids, build/content match, per-connection and
   per-address rate limits, a bounded pending-handshake count, no privileged
   role for remote peers, and no logged tokens. Tests: two legitimate joins,
   impersonation, replay, a third client, mismatched builds, floods.

## Tools and runs

- Build: `powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Protocol.ps1' -Run <name> -Task <task>`
- Tests: `python3 tools/run-protocol-tests.py <run> --build <build-run> --task <task> [--exe protocol-tests] [--filter a,b]`
  (transport suite about 45 s, protocol suite about 3 s).
- Last run names: `proto-build-7`, `pt-3`, `tr-7`, `tr-race-8`; unchanged:
  `net-build-7`, `net-8`, `campaign-122`, `client-32`, `smoke-145`,
  `capture-24`, `patch-check-29a/b`. Next: `proto-build-8`, `pt-4`, `tr-8`,
  `campaign-123`, `client-33`, `smoke-146`, `capture-25`, patch check `30`.
- Run PowerShell scripts with the Windows path in single quotes from bash.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02/G03 carried limits are unchanged.
Previous snapshot: [before S04-T04](./snapshot-before-S04-T04.md).
