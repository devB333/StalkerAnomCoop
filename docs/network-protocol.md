# Preliminary network protocol — investigation only

Status: design proposal, not implemented or wire-compatible with X-Ray. Engine evidence is in [network findings](network-findings.md). Exactly two authenticated active players initially; slot count is policy, not embedded in entity identifiers.

## Transport decision

**Validated 2026-10-20 ([S04-T01](validation/S04-T01.md)):** GameNetworkingSockets v1.6.1 (BCrypt crypto, static protobuf 3.21.12, no ICE) builds with the pinned v143 toolchain and passed connect, refusal, reliable-under-loss, unreliable, lane-priority, crash and reconnect tests on one Windows host; its findings on liveness, shared send buffers and per-lane ordering are inputs to S04-T02/T05. **Adapter implemented 2026-10-20 ([S04-T02](validation/S04-T02.md)):** `protocol/include/coop/net/transport.h` is the only network interface game code sees; GNS is confined to `protocol/src/net/gns_transport.cpp`. Each channel is one GNS lane with a delivery class (Critical: reliable, overflow closes the connection; Bulk: reliable, overflow refuses the send; Transient: unreliable, dropped when stale or over budget). Received data reaches the owner thread only through `poll()`. Original assessment: prefer GameNetworkingSockets for an Internet-facing dedicated server, subject to a Windows v143 integration/build experiment. It provides reliable/unreliable messages, encryption, connection statistics, loss simulation and independently prioritized lanes. Authentication/account binding still belongs to this application; do not assume Steam authentication or relay access in the open-source library. Entity encoding and deltas remain our responsibility. [Primary reference](https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/README.md), inspected 2026-10-15.

ENet is a viable simpler alternative, especially for a controlled LAN experiment: reliable and unreliable sequenced packets, independent channels, connection monitoring, fragmentation and bandwidth controls. Application authentication and an appropriate security layer would require separate design. [Primary reference](https://github.com/lsalzman/enet/blob/master/docs/design.dox), inspected 2026-10-15.

Retain useful X-Ray packet serializers and event meanings behind adapters, but replace DirectPlay as the external transport. Retained networking is not merely a stub; its local-client trust model and campaign direct-connect shortcuts need changes regardless of transport. Do not tunnel arbitrary legacy events from unauthenticated clients. No custom reliable UDP implementation is proposed.

## Identity and envelope

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

Cap message sizes, command rates, outstanding operations, entity counts and resync retries before allocation. Reject NaN/infinite coordinates, invalid IDs and malformed enums. Rate-limit requests per action and connection. Test loss, duplication, reordering, jitter, stale epochs, concurrent pickup and reconnect during death/transition. The transport adapter exists (S04-T02); the codec, sessions and liveness rules above are not implemented yet.
