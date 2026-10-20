## Complete

# Session handoff — S04-T01 complete; next: S04-T02

## State at the end of the session (2026-10-20)

**Stage 4 In Progress.** S04-T01 Completed
([report](../../validation/S04-T01.md)); no engine change, no patch. G03's
carried limits are unchanged.

**S04-T01** — GameNetworkingSockets validated in isolation. Pinned GNS v1.6.1
(commit `d534c19a`), BCrypt crypto (no OpenSSL), static protobuf 3.21.12 (the
last release without Abseil), ICE off, CMake 4.3.5 and Ninja 1.13.1, all in
`tools/network-probe/pins.json`. Built with the Stage 1 pinned `cl.exe`, /MD,
0 warnings (**net-build-7**, 55 s). The DLL (1.6 MB) imports only what the
campaign server already loads plus Windows' `bcrypt.dll`. On one host over
loopback (**net-8**, all pass): third peer refused with an app reason; 12,000
reliable messages intact and in order under 10% loss, 10% reordering, 5%
duplication and 20 ms lag; unreliable delivery 89–90% at 10% loss, no
duplicates, stale ones discarded; lane priority plus a send-side discard rule
keep critical pings within 9 ms of baseline on a 3× oversubscribed link;
crash detection and reconnect traced; GNS `test_crypto` and
`test_connection suite-quick` pass.

## Found this session (inputs to S04-T02/T05)

1. Dead-peer detection needs traffic that expects a reply: a silent server
   took 15.5 s with a 5 s timeout, an unreliable heartbeat 12.5 s, a reliable
   heartbeat 5.5 s. S04-T05 needs a reliable heartbeat or its own timer.
2. A dead session holds its slot until it times out; a reconnect is a new
   connection. S04-T04/T05 must bind reconnects to the player and replace the
   stale session.
3. The send buffer (512 KB) is shared per connection: when transient data
   fills it, reliable critical sends are refused. The adapter must bound
   unreliable data itself and budget it below the send rate (under 1% loss on
   a saturated link the critical tails grew even with the discard rule).
4. Reliable order is per lane; independent reliable streams go on separate
   lanes.
5. Windows 11 power throttling made `Sleep(1)` last 15.5 ms in a windowless
   process (fixed in the probe with `SetProcessInformation(ProcessPowerThrottling)`).
   The headless campaign server is such a process. Check its frame clock and
   future network polling.
6. Open: the encryption is unauthenticated (no certificates), so identity is
   S04-T04's job. protobuf 3.21.12 is from 2022, so revisit before release.
   Separate-machine, NAT and firewall tests weren't possible.
7. Carried from G03 (unchanged): installed scripts outside a context still
   decide for actor 0; the smoke-143 stalker stop (watch); k00_marsh has no
   static point lights; capture-21 start-up hang; checkpoint identity
   (Stage 7); presentation delivery and server-driven camera yaw (Stage 4,
   later tasks).

## Next

1. **S04-T02 — Implement a bounded transport adapter.** First action: decide
   where the adapter lives (a project-owned library linked by the campaign
   server and client, keeping GNS behind it), and write its interface first:
   listen/connect/send/poll/close, separate critical-reliable and
   transient-unreliable classes with bounded queues (finding 3), lanes per
   independent stream (finding 4), received intents marshalled to the
   simulation thread through `RunCallbacks`/poll on that thread. Then a fake
   transport and the loopback, shutdown-race and queue-limit tests. Use the
   GNS DLL from `builds/network-probe/install/net-build-7/gns`.

## Tools and runs

- Probe build: `powershell.exe ... -File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-NetworkProbe.ps1' -Run <name> [-ReuseLibrariesFrom <run>]`
  (full build about 1 min). Probe run: `python3 tools/run-network-probe.py <run> --build <build-run> [--only a,b] [--skip-selftest]`
  (full set about 6 min, 3.5 of it GNS's `test_connection`).
- Last run names: `net-build-7`, `net-8`; unchanged: `campaign-122`,
  `client-32`, `smoke-145`, `capture-24`, `patch-check-29a/b`. Next:
  `net-build-8`, `net-9`, `campaign-123`, `client-33`, `smoke-146`,
  `capture-25`, patch check `30`.
- Clone third-party repositories that Windows tools build with Windows git: a
  WSL clone makes Linux symlinks that Windows can't read.
- Run PowerShell scripts with the Windows path in single quotes from bash.
  Don't build while a GameState smoke runs (the server is real-time).
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Identity|GameState`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02/G03 carried limits are unchanged.
Previous snapshot: [before S04-T02](./snapshot-before-S04-T02.md).
