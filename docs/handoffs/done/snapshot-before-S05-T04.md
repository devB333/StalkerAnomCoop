## Complete

Snapshot of the handoff that led into S05-T03 (history only; do not resume from it).

# Session handoff — S05-T02 done; next: S05-T03

## State (2026-10-20)

Stage 5 In Progress. **S05-T01 Completed** ([report](../../validation/S05-T01.md)),
**S05-T02 Completed** ([report](../../validation/S05-T02.md),
[design](../../coop-session.md#client-world-replica-s05-t02)): engine
`c57fe5b9` + patches 0001–0028; protocol v1.1 (`proto-build-15`). Clients
join a serving campaign server, receive its world as a native checkpoint plus
the lifecycle stream, and hold a passive replica; world-7: three replicas
(late join, rejoin, constant churn) equal the server's online set.
Regressions: smoke-149 (66 stages; smoke-148 hit the known intermittent walk
stop), capture-27 (single-player checkpoint match).

## Next: S05-T03 — display both participants with separate local views

- Each client's view is its own participant's actor (already the player
  entity on its client). Input → server: the client stops moving its actor
  locally (`controls_actor` false in the replica) and sends InputBatch
  (movement keys, look angles, stance) every frame; the server applies it to
  that participant's actor through the S03-T03 input context
  (`actor_input_*`), only for an attached session (detached: no input).
- Snapshots (protocol Snapshot, 20 Hz, transient): actor transforms, velocity,
  state bits; clients feed remote actors through the engine's MP path
  (`CActor::net_Import` / NET queue interpolation) and set their own actor's
  position (no prediction: S06-T03). Consider basic NPC transforms too, or
  leave NPCs to S06-T02 and say so.
- Evidence: both clients see both actors (screenshots via the `screenshot`
  console command, read with the image viewer), each moves only its own
  (scripted input in a test mode), positions compared server vs clients.

Then S05-T04 (bounded session, G05 report). Stop for G05 acceptance before
Stage 6.

## Tools and runs

- `python3 tools/prepare-coop-session.py <run> [--task S05-T0N] [--inject c1]`
  then `Run-CoopSession.ps1 -Run <run> -Task <task> -ClientStarts '0,15'
  -ClientSeconds '90,75' [-Restart 'c2:40:30']`; logs in
  `builds/logs/<task>/run-<run>/{server,c1,c2}`.
- Builds: `Build-Stage2.ps1 -Run <name> -Product Client|Campaign`
  (`-ProtocolBuild proto-build-15` default). Rebuild **both** products after
  any engine change (build ids must match).
- `python3 tools/compare-coop-world.py <server coop-lifecycle.csv> <client coop-world-final.csv>`
  compares a replica with the server; `--churn N` adds a server-side
  spawn/release every N s.
- Last runs: `proto-build-15`, `pt-13`, `tr-10`, `campaign-138`, `client-45`,
  `world-7`, `smoke-149`, `capture-27`, patch check 31. Next:
  `proto-build-16`, `pt-14`, `tr-11`, `campaign-139`, `client-46`,
  `view-1` (task S05-T03), `smoke-150`, `capture-28`, patch check 32.
- Don't build while a session or smoke runs. The user may click in client
  windows; refused starts are logged (`role-guard`).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. G01 waivers and G02–G04
carried limits unchanged.
Previous snapshot: [before S05-T03](snapshot-before-S05-T03.md).
