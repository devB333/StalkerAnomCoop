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

## Client world replica (S05-T02)

Validation: [S05-T02](validation/S05-T02.md). Code: `CoopServerHost.cpp`
(lifecycle, join stream), `CoopClientLink.cpp` (receive, apply),
`CoopReplica.{h,cpp}` (the client's guards and hooks).

**Join.** Once a joining player's actor is online, the server saves a native
checkpoint (`coop-join-N`) and streams it with the online set at the
lifecycle watermark and each participant's actor id (ResyncBegin / 60 KiB
chunks / ResyncEnd; a join keeps the epoch). The client writes it as its own
saved game and starts it (the one start the role guard allows), pumping the
link from the engine's loading loop. Lifecycle events that arrive meanwhile
are buffered; at live, those after the watermark apply in commit order.

**Lifecycle.** The server diffs its online ALife objects every tick:
`Spawn` (native spawn packet ≤ 16 KiB; parents first) / `Despawn`
OutOfRange (offline) or Destroyed (gone from ALife; children first), one
commit id each, paced below the lifecycle backlog. Clients keep
engine id ↔ entity id and generation maps and an `EntityTable`; an event at
or below the applied commit, a recycled generation or a stale reference is
dropped and counted.

**Replica rules on the client** (all off without a co-op world):

| Rule | Where |
|---|---|
| No ALife offline simulation; online/offline follows the server (object or squad switched when the event applies, 2-s reconcile) | `alife_update_manager.cpp`, `alife_switch_manager.cpp`, `CoopReplica.cpp` |
| Only the local participant's actor (and what it carries) is local; everything else spawns remote (no AI think) | `xrServer_process_spawn.cpp`, `xrServer_CL_connect.cpp` |
| The local participant's actor is the player entity and spawns first | `alife_simulator_base2.cpp` (`register_object`), `xrServer_CL_connect.cpp` |
| The client's in-process server refuses M_SPAWN, world control and every game event from its own level; ALife create/release refused unless replication asks | `xrServer.cpp`, `xrServer_process_event.cpp`, `alife_simulator_base.cpp` |
| The single-player actor binder only on the local actor; remote objects' binders do not update | `CampaignActorControl.cpp`, `script_binder.cpp` |

## Participant views (S05-T03)

Validation: [S05-T03](validation/S05-T03.md).

- Each client's player entity is its own participant's actor; the other
  participant's actor is drawn whole (third person) on it.
- A replica client moves no actor: it sends its player's control state
  (movement keys, stance, walk, camera angles) as `InputBatch` each frame
  (≤ 60 Hz); the server holds the newest input for 250 ms and writes it into
  that participant's actor's control state before every tick, only through
  the actor the session's context resolves (`Scope::Self`).
- Snapshots at 20 Hz carry both participant actors; clients interpolate
  100 ms behind the server's clock (`CActor::replica_present`). No prediction
  yet (S06-T03).
- World clock (`CampaignEvent` kind 1, payload `u64 game_ms, f32 factor, u8
  length, weather`): clients follow the server's game time and weather.
- No script binder updates on a replica; unattended test clients use
  `keypress_on_start off` and `g_always_active on`.
