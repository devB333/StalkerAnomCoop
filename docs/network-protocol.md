# Network protocol

Status: the transport adapter (S04-T02), the v1 wire format and codec (S04-T03, [below](#wire-format-v1)), authenticated sessions (S04-T04, [sessions](session-authentication.md)) and liveness/reconnect (S04-T05) are implemented and tested, and run inside the engine since S05-T01 ([co-op session](coop-session.md)); replication is Stage 5/6 work. The sections before the wire format are the original design proposal; it is not wire-compatible with X-Ray. Engine evidence is in [network findings](network-findings.md). Exactly two authenticated active players initially; slot count is policy, not embedded in entity identifiers.

## Transport decision

**Validated 2026-10-20 ([S04-T01](validation/S04-T01.md)):** GameNetworkingSockets v1.6.1 (BCrypt crypto, static protobuf 3.21.12, no ICE) builds with the pinned v143 toolchain and passed connect, refusal, reliable-under-loss, unreliable, lane-priority, crash and reconnect tests on one Windows host; its findings on liveness, shared send buffers and per-lane ordering are inputs to S04-T02/T05. **Adapter implemented 2026-10-20 ([S04-T02](validation/S04-T02.md)):** `protocol/include/coop/net/transport.h` is the only network interface game code sees; GNS is confined to `protocol/src/net/gns_transport.cpp`. Each channel is one GNS lane with a delivery class (Critical: reliable, overflow closes the connection; Bulk: reliable, overflow refuses the send; Transient: unreliable, dropped when stale or over budget). Received data reaches the owner thread only through `poll()`. Original assessment: prefer GameNetworkingSockets for an Internet-facing dedicated server, subject to a Windows v143 integration/build experiment. It provides reliable/unreliable messages, encryption, connection statistics, loss simulation and independently prioritized lanes. Authentication/account binding still belongs to this application; do not assume Steam authentication or relay access in the open-source library. Entity encoding and deltas remain our responsibility. [Primary reference](https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/README.md), inspected 2026-10-15.

ENet is a viable simpler alternative, especially for a controlled LAN experiment: reliable and unreliable sequenced packets, independent channels, connection monitoring, fragmentation and bandwidth controls. Application authentication and an appropriate security layer would require separate design. [Primary reference](https://github.com/lsalzman/enet/blob/master/docs/design.dox), inspected 2026-10-15.

Retain useful X-Ray packet serializers and event meanings behind adapters, but replace DirectPlay as the external transport. Retained networking is not merely a stub; its local-client trust model and campaign direct-connect shortcuts need changes regardless of transport. Do not tunnel arbitrary legacy events from unauthenticated clients. No custom reliable UDP implementation is proposed.

## Identity and envelope

**Superseded on the wire by [Wire format v1](#wire-format-v1) (S04-T03):** the session's 128-bit identities travel once in Hello/Welcome, not in every message. Original proposal, kept for its rules:

Proposed logical envelope: `protocol_major:u16, protocol_minor:u16, message_type:u16, payload_bytes:u32, campaign_id:128bits, session_id:128bits, authority_epoch:u64, stream_id:u16, sequence:u64, server_tick:u64` plus a typed payload. Negotiate supported minor features; reject incompatible major/content/schema versions before spawning. Define byte order, length limits and schema encodings before implementation; never transmit native struct layouts.

Use server-assigned persistent entity IDs (128-bit) with incarnation/generation, mapped to engine transient IDs. A recycled engine u16 ID must not identify a different item to an old packet. Connection identity binds to a canonical player UUID; clients cannot select another actor by supplying its ID. Epoch fencing rejects packets from previous server incarnations. Server tick is authoritative; client timing is only a bounded estimate.

## Logical messages and streams

| Message | Essential payload | Delivery |
|---|---|---|
| Hello / Welcome | versions, content manifest digest, authentication proof; selected session, player, epoch, tick rate | Reliable control |
| InputBatch | input sequence, intended tick, bounded movement/aim/buttons, recent redundant inputs | Unreliable, discard obsolete sequences |
| ActionIntent | player operation ID, action kind, target ID+generation, expected entity revisions, action parameters | Reliable actions |
| ActionResult | operation ID, accepted/rejected, reason, transaction/commit ID, resulting revisions | Reliable actions |
| Spawn / Despawn | stable ID, generation, archetype, level, initial state or tombstone, commit dependency | Reliable lifecycle |
| Snapshot | snapshot ID, server tick, baseline ID, last processed input, changed entity components | Unreliable sequenced |
| SnapshotAck | latest fully decoded snapshot/baseline and event cursor | Piggyback or periodic acknowledgement |
| CampaignEvent | unique event ID, affected aggregate revisions, committed effects, causal dependencies | Reliable campaign |
| ResyncBegin / Chunk / End | checkpoint ID, epoch, content/schema hash, event watermark, chunk hash/count | Reliable bulk, low priority |
| Resume / ResumeAccepted | identity proof, expiring resume token, last acknowledged event/snapshot | Reliable control |
| TransitionPrepare / Ready / Commit | transition ID, destination, roster, checkpoint watermark | Reliable control |

Use separate control, gameplay, campaign and bulk lanes. Order inventory/quest mutations at the simulation transaction boundary, not by globally blocking every reliable message on bulk transfer. Cross-lane dependencies carry commit/entity revisions; buffer a snapshot for an unknown spawn within a bound, otherwise request repair. Tombstones and generation checks prevent resurrection.

## Server action pipeline

Receive -> authenticate and bound/decode -> reject stale epoch/duplicates/rate excess -> enqueue for tick -> validate against canonical state -> serialize conflicting mutations -> resolve simulation -> commit state and durable transaction record where required -> replicate outcomes. Dedupe key is `(campaign, player, operation_id)` with durable stored outcome. A retry returns that outcome, not a second mutation. Reliable transport alone supplies neither idempotency nor authority.

Movement: consume inputs for the bound actor; validate elapsed time, speed, stance, collision, stamina and transition state. Correct impossible movement. Attacks: validate alive/equipped/ammo/fire cadence and input timing; resolve muzzle, projectile/raycast, hit and damage on the simulation server. Never accept client health or damage values. Pickup/transfer/trade: validate distance, visibility where needed, item existence/generation, owner, capacity, eligibility, price and expected revisions; atomically change ownership and balances. Quest/dialogue: validate prerequisites, NPC alive, interaction range, campaign revisions and permitted choice; execute server-side consequences.

## Replication and responsiveness

Initial experiment: 30 Hz input processing/simulation scheduling and 15–20 Hz snapshots; retain native physics substeps until measured stability supports changing them. These are benchmark starting points, not established engine rates. At 30 Hz the tick budget is 33.3 ms; select a lower replication rate than simulation to reserve CPU/network capacity for AI and campaign events. Measure p95/p99 tick time, AI cost, reconciliation error and bytes per player before freezing rates.

Predict only local movement and reversible presentation. Reconcile using authoritative transform/velocity and last accepted input, then replay unacknowledged inputs. Interpolate remote actors/NPCs across a jitter-adjusted buffer initially spanning 2–3 snapshots; cap extrapolation. Irreversible inventory and quest outcomes wait for commit. Prediction parity of native movement/physics remains unproven.

Deltas reference only acknowledged complete baselines, with bounded retention and periodic full keyframes. Missing/expired baseline triggers repair; never apply an undecodable delta. Prioritize both players, nearby threats and interactables; replicate distant ALife as coarse authoritative summaries rather than full online physics. Estimate bandwidth as entities × component bytes × snapshot rate, then enforce per-connection byte and CPU budgets; no measured bandwidth claim yet. Keep transient messages below the transport's measured safe payload and avoid fragmentation for frequent snapshots. Chunk bulk resync separately.

Begin with server-present-time combat. If latency testing requires hitscan rewind, retain bounded server history, clamp requested time to verified clock/latency limits, and rewind only validated collision state. Projectile simulation and death/ownership commits are not rolled back casually. No client-reported hit is authoritative.

## Disconnect, recovery and abuse bounds

Provisional liveness policy: 1-second heartbeat, suspect after 5 seconds, disconnected after 15 seconds; tune with loss tests. Grace period retains player identity/slot, but defines server behavior for the unattended actor (neutral input, normal vulnerability unless game policy says otherwise). Reconnect authenticates, rotates token, and reconstructs from canonical checkpoint plus committed event suffix. If history/baseline has expired, use a full snapshot. Pause client action acceptance until spawn, state and event watermark agree. Client saves never enter this path.

Cap message sizes, command rates, outstanding operations, entity counts and resync retries before allocation. Reject NaN/infinite coordinates, invalid IDs and malformed enums. Rate-limit requests per action and connection. Test loss, duplication, reordering, jitter, stale epochs, concurrent pickup and reconnect during death/transition. The transport adapter (S04-T02) and the wire codec (S04-T03) exist; the session, liveness and replication rules above are not implemented yet.

## Wire format v1

Implemented 2026-10-20 (S04-T03, [validation](validation/S04-T03.md)) in
`protocol/include/coop/proto/` (`wire.h`, `messages.h`, `guard.h`) and
`protocol/src/proto/`. Version **1.1** (1.0 in S04-T03; 1.1 in S05-T01 raised the
Spawn state to 16 KiB for native spawn packets, lifecycle messages to 20 KiB
with a 1 MiB backlog, and the send buffer to 2 MiB). The code is the reference; this
section is its summary.

### Encoding

- Little-endian integers; `f32` is IEEE-754 binary32 and must be finite and
  inside the field's range; no padding, no native struct layouts.
- Strings: `u8` length then bytes, with a per-field character set (display
  names and details are printable ASCII; archetypes are `[a-z0-9_.-]`).
  Byte blobs: `u16` or `u32` length then bytes, each with its own maximum.
- Identities: `Uuid128` (two `u64`), `EntityRef {u64 id, u32 generation}`.
  Zero is never a valid identity; a reference is either fully zero ("none",
  where allowed) or fully set.
- One canonical form: anything `decode()` accepts re-encodes to the same
  bytes, and `encode()` refuses anything `decode()` would reject.

### Header (24 bytes, every message)

| Field | Type | Rule |
|---|---|---|
| type | u16 | known type, or bit `0x8000` set (optional: skipped if unknown) |
| flags | u16 | 0 in v1 |
| payload_bytes | u32 | exactly the bytes after the header |
| epoch | u64 | 0 for the handshake (Hello, AuthChallenge, AuthProof, Welcome, Reject); the session's authority epoch otherwise |
| sequence | u64 | per channel and direction, from 1 |

The session (campaign, session, player ids) is bound to the transport
connection by the handshake, so later messages do not repeat it.

### Channels

Each type has one channel and one sending role; anything else is a protocol
violation (the receiver closes with transport code 1003).

| # | Channel | Class | Max message | Backlog | Carries |
|---|---|---|---|---|---|
| 0 | control | Critical, p0 | 1,024 | 64 KB | Hello, AuthProof (C), AuthChallenge, Welcome, Reject, ResyncBegin, ResyncEnd (S), Heartbeat (both) |
| 1 | actions | Critical, p0 | 512 | 64 KB | ActionIntent (C), ActionResult (S) |
| 2 | lifecycle | Critical, p0 | 20,480 | 1 MB | Spawn, Despawn, CampaignEvent (S) |
| 3 | bulk | Bulk, p2 | 65,536 | 192 KB | ResyncChunk (S) |
| 4 | snapshot | Transient, p1, 50 ms | 1,200 | 16 KB | Snapshot (S), SnapshotAck (C) |
| 5 | input | Transient, p0, 50 ms | 512 | 8 KB | InputBatch (C) |

`coop::proto::transport_config()` returns this layout for the transport.

### Messages and limits

| Message | Body | Bounds |
|---|---|---|
| Hello | magic `COOP`, major, minor, build id (32 B), content digest (32 B), player id, display name, client nonce (32 B), resume flag [+ session id, token, last event, last snapshot] | name 1–32; token 16–64; layout frozen across majors so a mismatch can be answered |
| AuthChallenge | server nonce (32 B), campaign id | campaign id non-zero ([sessions](session-authentication.md)) |
| AuthProof | ECDSA P-256 signature (64 B) over the transcript | fixed size; verified against the roster key |
| Welcome | major (must match), negotiated minor, campaign/session/player/party ids, slot, slot generation, authority epoch, server tick, tick and snapshot rates, resume token | ids non-zero; slot < 8; epoch ≥ 1; 1 ≤ snapshot rate ≤ tick rate ≤ 240; token 16–64 |
| Reject | reason, server major/minor, detail | reason ≠ 0 (unknown values kept); detail ≤ 128 |
| Heartbeat | nonce, echo, sent tick | nonce ≠ 0 |
| ActionIntent | operation id, kind, target ref, expected revision, params | operation id ≠ 0; kind 1–1023; params ≤ 256 (decoded per kind later, with its own bounds) |
| ActionResult | operation id, outcome, reason, commit id, tick, revision | outcome 1–3; accepted ⇔ reason 0 (duplicate: either) |
| Spawn | entity ref, archetype, level, transform, parent ref, commit id, state | archetype 1–64; parent ≠ self; state ≤ 16,384 (opaque, decoded later with bounds) |
| Despawn | entity ref, reason, commit id | reason 1–3 |
| CampaignEvent | event id, kind, revision, payload | ids non-zero; payload ≤ 1,024 |
| Snapshot | server tick, baseline, last input, part, part count, entities | baseline < this sequence; part < count ≤ 16; ≤ 64 entities, each once; component bits known; health ≤ 10,000 |
| SnapshotAck | snapshot id, baseline, event cursor | baseline ≤ snapshot |
| InputBatch | 1–8 inputs, newest first (input *i* has sequence `header.sequence − i`) | ticks strictly decreasing; axes −1..1; angles bounded; buttons ≤ 16 bits; stance 0–3 |
| ResyncBegin | resync id, checkpoint id, next epoch, content hash, event watermark, total bytes, chunk size, chunk count | total ≤ 256 MiB; chunk ≤ 60 KiB; count = ⌈total/chunk⌉ ≤ 65,536 |
| ResyncChunk | resync id, index, CRC-32, data | 1–60 KiB; CRC checked on decode |
| ResyncEnd | resync id, chunk count, CRC-32 of all data | count ≥ 1 |

Positions are within ±100 km of the level origin, speeds within ±1 km/s and
angles within ±4 rad (pitch ±2): wide enough for any level, narrow enough to
refuse garbage.

### Compatibility

- **Major** must match. A server answers a Hello of another major with
  Reject `VersionMismatch` (1); a client refuses a Welcome of another major.
- **Minor**: both sides use the lower one. Additions in a later minor are
  optional message types (bit `0x8000`), which older peers skip.
- **Build and content**: the Hello's build id and content digest must equal
  the server's (`check_hello`), else Reject `BuildMismatch` (2) or
  `ContentMismatch` (3). What the digests cover:
  [sessions](session-authentication.md#build-and-content-identity).

### Stateful rules (`guard.h`, one set per connection)

- Sequences: reliable channels count 1, 2, 3, ... with no gap or repeat (a
  violation); transient channels only move forward, and older or repeated
  ones are dropped.
- Epoch: only handshake messages before the session, and never inside it.
  After the Welcome every message carries the session's epoch; an older one
  (in flight across a resync or recovery) is dropped; a newer one is a
  violation. A resync announces the next epoch in ResyncBegin's body, sent
  under the current one, and both sides then advance.
- Entities: a generation only moves forward. A spawn that does not move it
  forward is a recycled generation and is refused; a reference to an older
  generation, or to a despawned entity, is stale; a newer one not yet
  spawned is unknown (buffer within a bound, then repair). Tombstones are
  kept up to a capacity, oldest forgotten first; live entities are never
  evicted.
- Resync: chunks in order with the announced sizes, then the end with the
  matching count and CRC; data is streamed, never buffered whole.
- Inputs: only inputs newer than the last processed one are applied, oldest
  first. Since S06-T01 the server keeps one `InputQueue` per player
  (`input_queue.h`, [co-op session](coop-session.md#validated-movement-input-s06-t01)):
  inputs are sequenced by their tick, validated (finite, axes −1..1, |pitch|
  ≤ 1.6, a stance the actor has, known buttons), queued in tick order with a
  bounded backlog (5, oldest dropped), refused when stale (≤ the last consumed
  tick, or a repeat) or ahead (> newest + 250), and exactly one is consumed
  per server tick; an empty queue reuses the last input for at most 10 ticks.
  `Snapshot.last_input` is the last consumed tick.
