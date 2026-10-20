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
- Input sequences are per connection. Every join is a new connection, so the
  server forgets the player's last input sequence and held input on join
  (S05-T04: before this, a rejoined player's inputs were all refused as old).

## Bounded shared-world session (S05-T04)

Validation: [S05-T04](validation/S05-T04.md), gate [G05](validation/G05.md).
The repeatable minimal scenario (one host, loopback, level `k00_marsh` from
the fixture save):

```
python3 tools/prepare-coop-session.py <run> --task S05-T04 --churn 10 --script c1,c2
powershell.exe ... Run-CoopSession.ps1 -Run <run> -Task S05-T04 -ServerAloneSeconds 10 -ClientStarts '0,45' -ClientSeconds '220,80' -Restart 'c2:20:90' -AfterSeconds 10
python3 tools/compare-coop-world.py <logs>/server/coop-lifecycle.csv <logs>/<c1|c2-first-run|c2>/coop-world-final.csv
python3 tools/compare-coop-actors.py <logs> --lag-ms 120
```

Server alone 10 s; c1 joins; c2 joins 45 s later (late join), quits after
80 s, rejoins 20 s later for 90 s; the server spawns a medkit every 10 s and
releases it 5 s later (server-owned entity changes). Each client faces the
other player, takes screenshots and walks back 1.5 s (the test driver,
profile `script = 1`; a client that was alone at its facings faces the other
player 3 s after it appears and takes one more screenshot).

Supported in the session: joining (native checkpoint + lifecycle stream),
seeing both participant actors, walking one's own actor through the server,
the server's world clock and weather, server-owned spawn/release, leaving
and rejoining. Not supported yet: any action other than movement, moving
NPCs and items on clients, gameplay scripts on clients (see S05-T04 limits).

## Validated movement input (S06-T01)

Validation: [S06-T01](validation/S06-T01.md). Patch 0031; protocol
`input_queue.h` (proto-build-18).

- **What a client sends is intent, never state:** movement axes, stance, the
  walk button and the camera's yaw and pitch, numbered by input tick. No
  message a client can send carries a position, speed or outcome; a Snapshot
  from a client is a wrong-direction violation (the connection closes) and
  every ActionIntent is refused (no action kind is defined).
- **One input per server tick:** the client samples its control state once
  per server tick of wall time (the Welcome's `tick_rate_hz`, 50 Hz), counts
  ticks per connection and repeats the last 8 inputs in every batch. The
  server keeps one `coop::proto::InputQueue` per player: every received input
  is validated (finite, axes −1..1, |pitch| ≤ 1.6, a stance the actor has, a
  known button), refused when stale (≤ the last consumed tick, or a repeat) or
  ahead (> newest accepted + 250), else queued in tick order with a backlog of
  at most 5 (oldest dropped). Each tick consumes exactly one input; an empty
  queue reuses the last for at most 10 ticks, then the actor gets no input.
  So duplicates, replays, floods and a shifted counter cannot move an actor
  further than the server's ticks do, and a backlog adds at most 100 ms.
  Refusals are counted per player (`in<slot>=` in the status line) and the
  first 24 are logged with their reason.
- **Native validation of the intent:** the consumed input only sets the
  actor's control bits and camera angles (the pitch clamped to the camera's
  own `lim_pitch`); the server's own control path then decides what the actor
  can do with them and moves it with its own timestep and collision:
  `CActor::shedule_Update` → `g_cl_CheckControls` (`CanMove`, the crouch box
  check, `CanSprint`/`CanAccelerate`, stamina through `DisableSprint`, the
  walk/run/sprint/crouch/strafe speed factors) → `g_cl_Orientate` (the camera
  yaw becomes the body's) → `g_Physics` (the character controller against the
  level and other characters). Nothing of the client's timestep or camera
  reaches the server except the yaw and pitch it chose.
- **Identity:** input is applied only to the actor the session's context
  resolves (`Scope::Self`); a detached player's queue is reset and its actor
  gets no input. Snapshots acknowledge the last consumed tick (`last_input`).
- **Two actors near each other** (patch 0031, `CActor::UpdateCL`): an actor
  no longer runs the NPC box fitting (`UpdateObjectBox`) on another actor.
  That function recalculates the touched character's movement with zero
  acceleration. Between two actors it pinned both for good within about
  1.5 m (S06-T01 move-1..5). Single-player has one actor and is unchanged.
- **Rate:** the session layer's input channel limit (120/s, burst 240) drops
  the excess on the transient channel (`rate_drops=` in the status line).
- **Trace:** after each tick that applied an input, the server logs what the
  native step did (`input-after-tick`: control bits, `CanMove`, environment,
  acceleration, the physics body's exist/enabled/active/frozen flags,
  velocity and the living creatures within 3 m; `CActor::control_trace`).
- **Test driver (profile `script = 2`):** the movement validation program
  (legs A–K in `CoopClientLink.cpp`, measured by
  `tools/compare-coop-movement.py`): baseline legs, duplicated batches, a
  shifted counter with a jump, a replay while standing, crouched legs, an
  unsupported stance, a sprint into the other player's actor, a teleport
  intent and a forged snapshot. Player 2's program starts 60 s after player
  1's, and player 1's connection-closing leg K waits until player 2's
  program is over.

```
python3 tools/prepare-coop-session.py <run> --task S06-T01 --movement c1,c2
powershell.exe ... Run-CoopSession.ps1 -Run <run> -Task S06-T01 -ServerAloneSeconds 10 -ClientStarts '0,10' -ClientSeconds '180,170' -AfterSeconds 10
python3 tools/compare-coop-movement.py <logs> --out <logs>/compare-movement.json
```
