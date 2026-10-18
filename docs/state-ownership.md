# Canonical state and recovery — preliminary proposal

These are required ownership rules, not claims about current engine behavior. Current code evidence: [actors](actor-findings.md), [network and persistence](network-findings.md), [simulation](simulation-findings.md).

| Category | Canonical owner | Examples |
|---|---|---|
| Globally shared | Dedicated simulation | World time, emissions, story flags, NPC life/death, ALife, spawns, stashes, trader stock, unique items |
| Party shared | Dedicated simulation | Main/selected side quests, objective completion, shared dialogue consequences, campaign transition decisions |
| Player specific | Dedicated simulation | Identity, health, inventory/equipment, ammunition, progression, position, personal reputation and personal task eligibility |
| Client-local presentation | Respective client | Camera, UI layout, audio mix, interpolation buffers, speculative movement/animation |

Quest ownership must be assigned per quest, especially repeatables. Faction reputation may have both a campaign relationship and a player-specific value; do not accidentally average or overwrite these. Presentation can cache canonical state but cannot commit it. Server scripts receive explicit player/party context; neither client's `db.actor` is the campaign authority. A compatibility layer for single-player scripts must not silently select whichever player most recently interacted.

## Conflicts and transactions

| Conflict | Proposed resolution |
|---|---|
| Both collect unique item | Tick-ordered transaction checks owner and revision; one success, second receives already-owned result |
| Both complete objective | Unique `(quest_instance, objective, completion_generation)` event; reward and flag mutation commit once |
| NPC dies during dialogue | Death and choice serialize against NPC revision; reject choice if death committed first; never revive through a stale dialogue response |
| Conflicting transfers/trade | Lock/serialize all affected item, inventory and balance aggregates; commit all effects or none |
| Repeat event or lost acknowledgement | Durable operation/event IDs return prior outcome; duplicate delivery makes no mutation |
| NPC dies during resync | Snapshot watermark plus ordered event suffix/tombstone delivers the death exactly once |
| Players request different levels | Initial policy is coordinated party travel; no speculative promise of concurrent online levels |
| Disconnect during travel | Server owns transition ID and prepare/commit state; bounded wait then explicit abort or agreed travel policy; reconnect uses committed destination |

Tie-break conflicting same-tick commands using the server's recorded enqueue order; no fairness guarantee beyond deterministic recorded ordering is claimed. Validate again at commit, not only at receipt. Creation, deletion, quest rewards and money movements share transaction boundaries where one causes another.

## Persistence and recovery

Prefer versioned native simulation checkpoints plus a durable journal of committed semantic mutations. Native ALife storage is a reuse candidate, not proof of an atomic multiplayer snapshot. Capture at a quiescent tick boundary including actor identities, ID mapping/generations, world clock, online/offline entities, quests, inventory, RNG state where available, transition state, dedupe outcomes and last committed sequence. Content manifest, engine revision, schema version and checksums accompany each checkpoint.

A single simulation writer owns canonical changes. A storage adapter stores them and acknowledges durability; the database does not decide damage, quest success or NPC behavior. For durable critical actions, persist an atomic transaction record before reporting committed success. Couple affected state updates and replication outbox/event IDs in the same logical commit. If persistence fails, stop accepting durable mutations rather than acknowledge success that cannot recover.

Write checkpoint to a new file/object, verify it, then atomically publish its manifest; retain the previous checkpoint. Journal records after its committed watermark contain resolved effects and sufficient state changes to restore them. Do not assume that replaying raw inputs will reproduce floating-point physics, Lua callbacks or scheduler order. Until deterministic replay or complete state-delta coverage is proven, crash recovery rolls continuous simulation back to the last consistent checkpoint. It must not regrant durable items/rewards or apply partial transactions. Define and test this reconciliation before promising zero-loss crash recovery.

On restart fence the old authority epoch, verify snapshot/schema/content identity, restore checkpoint, replay committed effects idempotently, rebuild runtime entity mappings, and only then admit players. Reconnection is a view of this authority, not a merge of client saves. Partial resync uses retained event history and acknowledged baselines; otherwise send a full checkpoint-derived view.

Remaining work: inspect native save ordering and every script serialization hook, make online and offline entity state checkpoint-consistent, define migration rules, and run crash injection before/after durable write, apply and acknowledgement. No persistence store is implemented.
