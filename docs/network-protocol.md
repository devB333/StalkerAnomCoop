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
`protocol/src/proto/`. Version **1.6** (1.0 in S04-T03; 1.1 in S05-T01 raised the
Spawn state to 16 KiB for native spawn packets, lifecycle messages to 20 KiB
with a 1 MiB backlog, and the send buffer to 2 MiB; **1.2** in S06-T02 gave
Snapshot its own id, rotation and removal components and delta semantics
against acknowledged baselines, and SnapshotAck a missing-baseline field; **1.3** in S06-T03 added the aim
component; **1.4** in S06-T04 added the weapon component, the fire button and
the first two action kinds; 1.5 in S06-T07 the jump and lean buttons; **1.6** in S08-T01 the pickup action (kind 5), the named refusal reasons and the Reparent lifecycle message: both
peers come from one build, so the layout change is not negotiated). The code is the reference; this
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

**Ordering across channels (S06-T06).** Each channel is ordered only within
itself. After the Welcome the server sends nothing on channels 1–4 until the
client has sent a message inside the session (`session::PlayerView::confirmed`):
in cenc-14 (10 % loss, 100 ms) a resync chunk on the bulk channel arrived
before the resent Welcome and the client closed with 1003 ("message before the
session started"). The client sends a Heartbeat on its first update after the
Welcome, so the wait is one round trip.

### Messages and limits

| Message | Body | Bounds |
|---|---|---|
| Hello | magic `COOP`, major, minor, build id (32 B), content digest (32 B), player id, display name, client nonce (32 B), resume flag [+ session id, token, last event, last snapshot] | name 1–32; token 16–64; layout frozen across majors so a mismatch can be answered |
| AuthChallenge | server nonce (32 B), campaign id | campaign id non-zero ([sessions](session-authentication.md)) |
| AuthProof | ECDSA P-256 signature (64 B) over the transcript | fixed size; verified against the roster key |
| Welcome | major (must match), negotiated minor, campaign/session/player/party ids, slot, slot generation, authority epoch, server tick, tick and snapshot rates, resume token | ids non-zero; slot < 8; epoch ≥ 1; 1 ≤ snapshot rate ≤ tick rate ≤ 240; token 16–64 |
| Reject | reason, server major/minor, detail | reason ≠ 0 (unknown values kept); detail ≤ 128 |
| Heartbeat | nonce, echo, sent tick | nonce ≠ 0 |
| ActionIntent | operation id, kind, target ref, expected revision, params | operation id ≠ 0; kind 1–1023; params ≤ 256 (decoded per kind, with its own bounds). Kinds (v1.4): 1 activate slot (params: one byte, the slot: 0 holsters, 1–6 must hold an item), 2 reload (no params); 3 ledger (S07-T03, the synthetic critical effect of [persistence contract](persistence-contract.md) §10: params `s32 delta`, abs ≤ 1000, on the player's own participant actor's money; committed to the journal before apply and acknowledgement; a known operation id returns its stored outcome and commit; reason 5 `StoreFailed` when the store has failed closed); 4 respawn (S07-T07, contract §9: no params; the dead player's confirmed respawn; reason 6 while the actor is alive, 7 with no respawn point; a `respawn` journal record precedes the spawn; the result is followed by CampaignEvent kind 5); 5 pickup (v1.6, S08-T01, [contract](persistence-contract.md) §6 `pickup`: target the item, no params; the server validates on its state at that tick (attached session, own actor alive, the target a current online inventory item with no owner, one the engine lets an actor take, within 2 m + 0.5 m of the actor's feet-to-eyes axis), commits a `pickup` record, applies it through the engine's ownership event and acknowledges; a known operation id returns its stored outcome); 6 drop (S08-T02, contract §6 `transfer`: target an item in the player's own actor's inventory, no params; a quest item or another owner's item is reason 14); 7 transfer (S08-T02: target the item, params from ref + to ref, 12 bytes each; one side the player's own actor, the other an inventory box or a dead body within 2.5 m + 0.5 m, else 13 / 10; the item must be in `from` (else 9) and must not have changed owner after the intent's expected revision, the client's lifecycle cursor (else 12 `Stale`); 0 means no precondition); 8 quest (S08-T04, contract §8a: target the task giver, params `u8 op` (1 accept, 2 turn in), `u8 length`, the task id; one party task for both players; the answer comes only after a published generation holds the outcome; reasons 15–19); 9 equip (S08-T07, contract §6 `equip`: target an item in the player's own actor's inventory, params `u8 place` (1 slot, 2 belt, 3 ruck), `u8 slot` (place 1: the inventory slot, else 0), `u8 flags` (bit 0 do not activate); the engine's slot, belt and ruck rules and the installed inventory UI's slot table decide (reason 20); committed before it is applied); 10 use (S08-T07, contract §6 `use`: target an eatable item in the player's own actor's inventory, params none or `u8 flags` (bit 0: the client plays the use's animation; then the server ignores the player's fire key until the animation's timeline raises the weapon, S08-T09/T07-5); the installed scripts' before-use check (reason 22) and `CInventory::Eat` run on the server's actor inside the player's script context; the resolved result is committed before the answer; reason 21 for an item that is not eatable or has nothing left); any other kind is refused with reason 1. Named reasons (`ActionReason`): 1 unsupported, 2 malformed, 3 no actor, 4 nothing, 5 store failed, 6 alive, 7 no point, 8 item gone, 9 item taken (another player won it, or not where the client saw it), 10 out of reach, 11 not takeable, 12 stale (S08-T02), 13 not a container (S08-T02), 14 not droppable (S08-T02), 15 quest unsupported (not an adapted task, or the target is not a living giver), 16 quest active, 17 quest not offered, 18 quest not active, 19 quest not finished (S08-T04), 20 place refused, 21 not usable, 22 use refused (S08-T07). Kinds 6 and 7 need no wire change (params are decoded by the server per kind); the test drivers' deliberately undefined kinds moved to 1001–1003 (S08-T07: the S06-T01 teleport intent from 9 to 1004) |
| ActionResult | operation id, outcome, reason, commit id, tick, revision | outcome 1–3; accepted ⇔ reason 0 (duplicate: either) |
| Spawn | entity ref, archetype, level, transform, parent ref, commit id, state | archetype 1–64; parent ≠ self; state ≤ 16,384 (opaque, decoded later with bounds) |
| Despawn | entity ref, reason, commit id | reason 1–3 |
| Reparent (v1.6, 0x0023) | entity ref, parent ref (none: on the ground), transform, commit id | parent ≠ self; sent on the lifecycle channel in commit order with Spawn and Despawn for **every** parent change the server sees (a player's pickup, an NPC taking or dropping an item) |
| CampaignEvent | event id, kind, revision, payload | ids non-zero; payload ≤ 1,024. Kinds (application level, server to client, decoded with bounds by the client): 1 world clock (S05-T03: u64 game ms, f32 time factor, weather name), 2 environment (S06-T08: wfx flag, f32 wfx time, cycle, weather, the two blended descriptors as section + f32 exec time), 3 thunderbolt (S06-T08: collection, u32 entry, f32 life, longitude, distance, tilt ×3, second flag), 4 presentation (S06-T08: u8 kind, u16 object, u32 handle, f32 position ×3, f32 value, u32 flags, u32 serial, detail string), 5 rejoin (S07-T07: u8 slot, u16 new actor id; the client leaves its world and reconnects so its player entity is the new actor); 6, 7 the S08-T01/T02 test drivers' rounds; 8 item-graph point (S08-T03: u64 sequence, u64 hash, u32 items; S08-T07 adds u64 layout hash, u32 layout items: 32 bytes); 9 inventory layout (S08-T07: u64 actor entity, u32 serial, u8 part, u8 parts, u8 count, then count × (u64 item entity, u8 place, u8 slot, u16 amount: an eatable's remaining uses or, S08-T08, an ammunition box's rounds; 0xffff none), at most 80 per part; sent when a participant actor's layout changes and to a client entering the world); 10 amounts (S08-T08: u8 count, count × (u64 entity, u16 rounds): ammunition counts the server changed outside an actor's layout, a merge in a box; a client applies them when the object reaches its level, ≤ 10 s); 11 party tasks (S08-T09: u8 count, per task: id, u8 state 0 failed / 1 in progress / 2 completed, s16 count, s16 need, title, description, icon, map location, u16 map object, u32 priority); 12 survival values (S08-T09: f32 thirst, f32 sleep, f32 psy health of the receiving player, -1 when that module is off; then f32 hunger (satiety), stamina (power) and radiation of its actor as the server holds them, for the HUD; checked 5 times a second, sent on change, every 10 s and on entering the world); strings are u8 length + bytes; unknown kinds are ignored |
| Snapshot | server tick, snapshot id (v1.2), baseline, last input, part, part count, entities; each entity: ref, components, then transform / velocity / health / state bits / rotation (v1.2) / aim (v1.3: an actor's torso yaw and pitch) / weapon (v1.4: active slot, weapon state, rounds in the magazine, shot counter) as the bits say; a removal (v1.2) has no fields | snapshot id ≠ 0; baseline < snapshot id; part < count ≤ 16; ≤ 64 entities, each once; component bits known; removal alone; health ≤ 10,000 (S06-T05: 0 only when dead); rotation a unit quaternion (norm² 0.98–1.02); weapon slot ≤ 15 |
| SnapshotAck | snapshot id (newest complete, 0: none), missing baseline (v1.2, 0: none), event cursor | not both zero |
| InputBatch | 1–8 inputs, newest first (input *i* has sequence `header.sequence − i`) | ticks strictly decreasing; axes −1..1; angles bounded; buttons ≤ 16 bits (bit 0 walk, bit 1 fire in v1.4; bit 2 jump, bit 3 lean left, bit 4 lean right in v1.5); stance 0–3 |
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
  Exception: v1.6's Reparent (0x0023) is a required lifecycle type; an older
  peer cannot reach it because the build id must match (below).
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
  `Snapshot.last_input` is the last consumed tick; since S06-T03 it is
  `InputQueue::processed_tick`: the last consumed tick, or a newer one the
  server refused as invalid once nothing older waits, which a predicting
  client reconciles against ([co-op session](coop-session.md#own-actor-prediction-and-reconciliation-s06-t03)).
- Snapshots (v1.2, S06-T02, `snapshot.h`): one `SnapshotSender` and one
  `SnapshotReceiver` per connection. A snapshot (its parts share id, baseline,
  tick and last input; an entity appears in one part only) is a delta against
  the newest snapshot the client acknowledged with every part decoded, or full
  (baseline 0). Entries carry only the components that changed beyond a
  tolerance; a new entity or generation carries all of its components; an
  entity the baseline holds that is no longer wanted gets a removal. The
  sender keeps, per snapshot, the frame the client will hold (a change that
  did not fit the part budget keeps the baseline's state there and is sent
  later), so a decoded snapshot reproduces the sender's frame exactly. The
  sender keeps 32 snapshots, the receiver 64; an acknowledged baseline that
  left the sender's history, or a missing baseline the client reports, gives
  a full snapshot within the same part budget. The budget (8 parts at most)
  halves when the transport drops a part or the snapshot queue is older than
  50 ms and grows back by one part after 10 clean snapshots; a deferred entity
  moves ahead of the others, the pinned ones (the participant actors) always
  go first. The receiver hands a state on for presentation only when its tick
  is newer than the last one handed on for that entity. Presentation:
  `interpolation.h` (server clock as a 2-s windowed minimum of arrival
  offsets, 100 ms render delay, capped extrapolation of moving creatures).
