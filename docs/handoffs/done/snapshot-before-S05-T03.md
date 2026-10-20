## Complete

Snapshot of the handoff that led into S05-T02 (history only; do not resume from it).

# Session handoff — S05-T01 done; next: S05-T02

## State (2026-10-20)

Stage 5 In Progress. **S05-T01 Completed** ([report](../../validation/S05-T01.md),
[design](../../coop-session.md)): engine `c57fe5b9` + patches 0001–0027;
protocol v1.1 (`proto-build-15`). A serving campaign server
(`-campaign-serve`) and two real clients connect over GNS loopback with
matched build and content; clients refuse local game starts; injected
outcomes are refused; detached actors stay. Regressions: smoke-146 (66
stages), capture-25 (single-player checkpoint match).

## Next: S05-T02 — entity lifecycle and initial world snapshot

Design (in progress, write it into `docs/coop-session.md` as it lands):

- **Initial world = a native checkpoint.** On a player's join (once their
  actor is online) the server saves `coop-join-N` (`M_SAVE_GAME`, as
  `xrCampaignLevel_CheckpointSave`), records the lifecycle watermark W and
  streams a blob (scop + scoc + manifest: online set u16→EntityRef, each
  participant's actor id, the player's slot) as ResyncBegin/Chunk/End
  (epoch unchanged for a join). Clients hold ResyncEnd until all chunks
  arrive (different lanes).
- **Lifecycle:** each tick the host diffs the server level's ALife objects
  against what it published: Spawn (EntityRef from the registry, section,
  transform, state = `Spawn_Write`+`UPDATE_Write`) / Despawn (OutOfRange if
  the ALife object remains, else Destroyed), commit id increasing; sent to
  every player whose resync started; paced by `Server::backlog()`.
- **Client replica:** writes the checkpoint as `coop-world`, starts it through
  the role guard (one permitted start), pumps the link during loading
  (`device.cpp` loading branch), applies buffered events with commit > W.
  Hooks: ALife `update_scheduled` off; `switch_object` follows the replica
  online set; `register_object` gives `M_SPAWN_OBJECT_ASPLAYER` to the
  local participant's actor only; `spawn_item`/`release`/Lua create refused
  except by replication; the local xrServer spawns everything except the
  local actor (and its items) as non-local, so NPCs take the engine's
  `Remote()` path (no Think, NET interpolation); M_EVENT/M_SPAWN from the
  client's own level refused; script binders: no update for remote objects,
  actor binder only on the local actor.
- **Validation:** server/client online-set summaries at the same commit;
  late join; a server-side churn (spawn/release on a timer) during joins.

Then S05-T03 (local view per participant, input to the server, actor
snapshots through `CActor::net_Import`), S05-T04 (bounded session, G05).
Stop for G05 acceptance before Stage 6.

## Tools and runs

- `python3 tools/prepare-coop-session.py <run> [--task S05-T0N] [--inject c1]`
  then `Run-CoopSession.ps1 -Run <run> -Task <task> -ClientStarts '0,15'
  -ClientSeconds '90,75' [-Restart 'c2:40:30']`; logs in
  `builds/logs/<task>/run-<run>/{server,c1,c2}`.
- Builds: `Build-Stage2.ps1 -Run <name> -Product Client|Campaign`
  (`-ProtocolBuild proto-build-15` default). Rebuild **both** products after
  any engine change (build ids must match).
- Last runs: `proto-build-15`, `pt-13`, `tr-10`, `campaign-131`, `client-39`,
  `coop-9`, `smoke-146`, `capture-25`, patch check 30. Next:
  `proto-build-16`, `pt-14`, `tr-11`, `campaign-132`, `client-40`,
  `coop-10`, `smoke-147`, `capture-26`, patch check 31.
- Don't build while a session or smoke runs. The user may click in client
  windows; refused starts are logged (`role-guard`).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. G01 waivers and G02–G04
carried limits unchanged.
Previous snapshot: [before S05-T02](snapshot-before-S05-T02.md).
