# Co-op session integration (Stage 5)

How the campaign server and the rendering clients run a shared session. Code:
`builds/build_1/engine-work/src/xrGame/Coop*.{h,cpp}` (patch series from 0027),
the project-owned networking in `protocol/` ([protocol](network-protocol.md),
[sessions](session-authentication.md)). Validation:
[S05-T01](validation/S05-T01.md) onwards.

## Roles

| Process | Command | Owns |
|---|---|---|
| Campaign server | `AnomalyCampaignServer.exe -campaign-serve -snd_cpu -fsltx <fsgame> -coop-config <profile>` | the only simulation: level, xrServer, ALife, AI, physics, scripts, participants; the co-op host (transport, session server) |
| Client | `AnomalyDX11.exe -fsltx <fsgame> -coop-config <profile> -nointro` | rendering, input, UI; the co-op link (transport, session client); a replica of the server's world (S05-T02) |

A client never simulates the shared world. It has no authority by
construction (no client message is an outcome; the server's decoder refuses
server-only types, S04-T03) and by role guards in the client (below).

## Linking the protocol into the engine (decision, S05-T01)

**Static libraries.** `tools/Build-Protocol.ps1` builds `coop_net`,
`coop_net_gns`, `coop_protocol` and `coop_session` with the Stage 1 pinned
compiler (`/MD`, C++17, `/W4 /WX`), the same objects the protocol tests run.
The engine links them:

- `src/Coop.props` (imported by `Common.props`) adds the protocol headers to
  xrGame and xrEngine, the library directories to the link, and delay-loads
  `GameNetworkingSockets.dll`, so single-player never loads it.
- `xrGame/CoopIdentity.cpp` names the libraries (`#pragma comment(lib)`).
- `tools/Build-Stage2.ps1 -ProtocolBuild <run> -GnsFrom <run>` passes their
  locations and copies the DLL beside the executable.

Why not add the sources to the engine projects: the engine compiles without
C++ exceptions, with its own precompiled headers and warning set; the
protocol code is tested under `/EHsc /W4 /WX`. Linking the tested libraries
keeps the shipped objects identical to the tested ones.

## Build and content identity

- `build_id` (`CoopIdentity.cpp`): `Build-Stage2.ps1` hashes the engine pin,
  every engine-work file that differs from it (path and SHA-256), every
  protocol source file and the protocol major, writes the list to
  `builds/logs/S02-T02/<run>/build-identity.txt`, and compiles the SHA-256 in
  (`coop_build_id.h`, generated in the intermediate directory; only one file
  includes it). Both products built from one tree carry the same id.
- `content_digest`: computed by the process itself at start from what its
  file system mounted under `$game_data$`: every archive entry by name, sizes
  and the archive's CRC-32, every loose file by name, size and SHA-256 of its
  bytes. Profiles prepared from the same accepted content give equal digests.

## Launch profiles

`tools/prepare-coop-session.py <run> [--clients N] [--inject c1]` makes one
isolated runtime per process (`prepare-stage2-smoke.py`: own appdata, logs,
saves and temp; archives read in place), a `coop.ltx` profile per process,
one DPAPI key file per client player (`coop-keytool`) and the server's
roster (public keys, client order: client 1 is roster player 1 and holds the
saved actor). `tools/Run-CoopSession.ps1 <run>` runs a timed session and
collects every process's logs under `builds/logs/<task>/run-<run>/`.

Profile `[coop]` keys: `role`, `campaign_id`, `log`, `stop_file`; server:
`listen`, `roster`, `party_id`, `save`, `serve_seconds`; client: `server`,
`key`, `name`, `exit_after_seconds`, `inject`. Each process writes a co-op
event log (UTC milliseconds, never keys, nonces or tokens).

## Server: serve mode (S05-T01)

`-campaign-serve` runs the GameState start (services, the level from the
profile's save, the first ticks, the identity check) and then serves in real
time until the stop file or the time limit:

- Before the services start, the profile and roster are read; the identity
  service binds the roster's players to participant slots in roster order
  (instead of the fixture player), so the saved actor is adopted by roster
  player 1 at its spawn.
- Each tick: `xrCoopServer_Tick` polls the session server first (the only
  place network input reaches the world), then the simulation tick runs.
- `Joined` → `attach_session` (registry session numbers from `0x00C00001`); a
  participant without an actor gets one (`actor` section, ALife
  `spawn_item`, assigned before it exists), beside a participant actor.
- `Left` → `detach_session`. **Detached-actor policy:** the actor stays in
  the world, vulnerable, keeps its area online, and receives no input (input
  is only taken from an attached session).
- Client intents: every action kind is refused (`ActionResult` Rejected,
  reason 1) until a later task defines one.
- The process opts out of Windows power throttling and asks for 1 ms timers
  (S04-T01 finding 5).

## Client: link and role guard (S05-T01)

The link starts in `CGamePersistent::OnAppStart` when the profile says
`role = client`, connects, and is polled once per frame
(`CGamePersistent::OnFrame`). While it is active, `CApplication::OnEvent`
refuses every local game start (main menu new game or load, console
`start`/`load`); the refusal is counted and logged.
