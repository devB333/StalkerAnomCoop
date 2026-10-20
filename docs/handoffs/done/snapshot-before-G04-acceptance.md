## Complete

# Session handoff — S04-T04 complete; next: S04-T05

## State at the end of the session (2026-10-20)

**Stage 4 In Progress.** S04-T01..T04 Completed; S04-T05 (then G04) remains.
No engine change, no patch since 0026. G03's carried limits are unchanged.

- **S04-T02** transport adapter ([report](../../validation/S04-T02.md)):
  `coop::net::Transport` with GNS and fake backends; owner-thread delivery;
  per-channel overload policy.
- **S04-T03** codec v1.0 ([report](../../validation/S04-T03.md),
  [wire format](../../network-protocol.md#wire-format-v1)): bounded decoder,
  canonical encoder, per-connection guard; 2.0 M guarded fuzz mutations.
- **S04-T04** sessions ([report](../../validation/S04-T04.md),
  [design](../../session-authentication.md)): `coop_session` (`Server`,
  `Client`) with ECDSA P-256 challenge-response (BCrypt), DPAPI key files
  from `coop-keytool`, a public-key roster, player-owned slots (kept across
  reconnects; a reconnect replaces the stale session; `release` advances
  the generation), rate limits and handshake timeouts. pt-6 43/43, tr-8 51/51.

## Found (inputs to later tasks)

1. Stated limit: an active man in the middle relaying a live handshake keeps
   the session (GNS encryption is unauthenticated; open-source GNS takes its
   CA key at compile time). Recorded for G11/R06.
2. The transport's `max_connections` (default 4) refuses peers with 1001
   before the session layer sees them; it is the first bound.
3. Critical producers must pace below the send rate (S04-T02).
4. Passed on: build/content digest computation and session-to-participant
   attachment (S05-T01); wire entity generation mapping (S05-T02); the
   notice inbox under a connection flood (bounded by the transport cap and
   the address limit in practice; S04-T05 to recheck); power throttling
   (S05-T01); one host, loopback, no sanitizer.

## Next

1. **S04-T05 — Handle liveness and in-memory reconnect.** First action:
   write the policy in `docs/session-authentication.md`. It covers:
   - a reliable heartbeat (S04-T01 finding 1: GNS needs traffic that expects
     a reply), suspect and disconnect timeouts and the grace period;
   - what a detached player's actor does (participant model: it stays in
     its slot);
   - resume with the Welcome's token (stored hashed) and token rotation.

   Then implement in `session::Server/Client`: heartbeats, timeouts, resume,
   rotation, and refusing a stale session's late messages. Tests: dropped
   connection, delayed old messages, repeated reconnect, server shutdown. Then
   the G04 report.

## Tools and runs

- Build: `powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Protocol.ps1' -Run <name> -Task <task>`
- Tests: `python3 tools/run-protocol-tests.py <run> --build <build-run> --task <task> [--exe protocol-tests] [--filter a,b]`
  (transport suite about 45 s; protocol+session suite about 10 s).
- Last run names: `proto-build-11`, `pt-6`, `tr-8`, `keytool-1`; unchanged:
  `net-build-7`, `net-8`, `campaign-122`, `client-32`, `smoke-145`,
  `capture-24`, `patch-check-29a/b`. Next: `proto-build-12`, `pt-7`, `tr-9`,
  `campaign-123`, `client-33`, `smoke-146`, `capture-25`, patch check `30`.
- Run PowerShell scripts with the Windows path in single quotes from bash.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02/G03 carried limits are unchanged.
Previous snapshot: [before S04-T05](./snapshot-before-S04-T05.md).
