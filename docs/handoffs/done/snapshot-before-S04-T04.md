## Complete

# Session handoff — S04-T02 complete; next: S04-T03

## State at the end of the session (2026-10-20)

**Stage 4 In Progress.** S04-T01 and S04-T02 Completed. No engine change, no
patch since 0026. G03's carried limits are unchanged.

**S04-T02** — bounded transport adapter ([report](../../validation/S04-T02.md)).
`protocol/` builds two static libraries with the Stage 1 compiler (`/MD`,
C++17, 0 warnings): `coop_net` (`coop/net/transport.h`, shared policy,
in-memory `FakeNetwork`) and `coop_net_gns` (`src/net/gns_transport.cpp`, the
only file that includes GNS).
- Only the owner thread can use a transport; network input arrives only
  through `poll()`. Library callbacks just route notices to the owner's
  locked inbox.
- Per-channel classes: Critical overflow closes the connection (1002), Bulk
  refuses the send, Transient is dropped when stale or over budget.
- Peers that send on an unknown channel, with the wrong class or oversize are
  closed (1003). Shutdown flushes, then closes (1004).
- tr-4: 51/51 tests (25 scenarios on the fake and on GNS over loopback, 7
  race tests, 6 policy tests); tr-5, tr-6 and tr-race-1..8 repeated them.

## Found this session (inputs to later tasks)

1. GNS's simulated impairments survive `Kill`/`Init` (static globals); the
   test rig clears them.
2. Closing a listen socket destroys its connections without a close packet;
   the adapter closes each connection and waits 30 ms first.
3. A critical burst overloads before any acknowledgement returns. Producers
   must pace on `status()` below the send rate (S04-T03/S05 budgets).
4. Unverified: loopback on one host only, no sanitizer; the notice inbox is
   unbounded under a connection-attempt flood (S04-T04 rate limits); not yet
   linked into the engine (S05-T01, together with S04-T01 finding 5, power
   throttling).
5. Carried from G03 and S04-T01, unchanged: liveness heartbeat and reconnect
   binding (S04-T04/T05), unauthenticated encryption (S04-T04), protobuf
   3.21.12 age, and the G03 limits.

## Next

1. **S04-T03 — Define and validate the initial protocol codec.** First
   action: write `docs/network-protocol.md`'s wire section (byte order, the
   envelope from that doc, length/count limits, the channel layout that
   replaces the tests' illustrative one), then a `coop_protocol` library in
   `protocol/` with bounded readers/writers, hello/welcome, input/action
   envelopes, result, entity spawn/despawn, snapshot/baseline ack and resync
   framing. Tests: round trip, truncation, oversize, unknown version/type,
   stale epoch, recycled entity generation, and a deterministic fuzz pass.
   No native serializer behind an unchecked read.

## Tools and runs

- Build: `powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Protocol.ps1' -Run <name> [-Task S04-T03]`
  (full build about 1 min; uses the net-build-7 GNS install).
- Tests: `python3 tools/run-protocol-tests.py <run> --build <build-run> [--task S04-T03] [--filter a,b]`
  (full transport suite about 45 s).
- Last run names: `proto-build-4`, `tr-6`, `tr-race-8`, `net-build-7`, `net-8`;
  unchanged: `campaign-122`, `client-32`, `smoke-145`, `capture-24`,
  `patch-check-29a/b`. Next: `proto-build-5`, `tr-7`, `campaign-123`,
  `client-33`, `smoke-146`, `capture-25`, patch check `30`.
- Run PowerShell scripts with the Windows path in single quotes from bash.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02/G03 carried limits are unchanged.
Previous snapshot: [before S04-T03](./snapshot-before-S04-T03.md).
