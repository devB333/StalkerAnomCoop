# Persistence contract — recoverable state and critical transactions (S07-T01)

Status: specification, written 2026-10-22 for Stage 7 (plan.md §C, S07-T01).
Source citations are `builds/build_1/engine-work/src/` at `c57fe5b9` with
patches 0001–0041 applied; installed scripts are the ones in `scripts.db0`.
This document is the contract S07-T02…T07 implement and S07-T06 qualifies
(G07). It supersedes the "Persistence and recovery" proposal in
[state ownership](state-ownership.md), which stays as history. Decision D7
(plan.md §B) is binding: native checkpoints plus a durable journal of
validated effects; stable identities; commit critical progression durably
before acknowledging permanent success; clients discard stale state and
resynchronize after recovery; no deterministic replay and no zero-loss claim.

## 1. What exists today and what it does not guarantee

| Mechanism | Where | What it gives | What it does not give |
|---|---|---|---|
| Native save (`M_SAVE_GAME`) | `xrServer.cpp` `case M_SAVE_GAME` → `game_sv_single.cpp:242` → `alife_storage_manager.cpp:46-123` (`save`), `:235-250` (`save(NET_Packet&)`, `ClientSend`, `ClientSave`) | One synchronous write of header, time manager, spawns, every savable ALife object (`CALifeObjectRegistry::save`), registries; the actor's money, community, rank, reputation, name (`xrServer_Objects_ALife_Monsters.cpp:189-216`), health, position and holder (`:1504-1510`), inventory through the children; the Lua `.scoc` (`alife_storage_manager.script` `CALifeStorageManager_before_save`: `m_data` with every binder's `save_state`, `db.storage[id].pstor`) | Atomicity: `FS.w_open` (`LocatorAPI.cpp:1409-1420`, `CFileWriter`) writes the final path directly; the `.scoc` is a second file written by Lua `io.open`; no flush, no checksum, no manifest. A crash during the save leaves a truncated `.scop` whose header may still pass `valid_saved_game` (`saved_game_wrapper.cpp:36-48` checks 8 bytes) |
| Round-trip evidence | [S02-T06](validation/S02-T06.md) part 5 (one actor), [S05-T02](validation/S05-T02.md) (clients load two-actor join checkpoints) | ALife, clock, actor, NPCs, Lua marshal state come back exactly before the first tick | A **server** restart from a two-actor checkpoint has never run; a participant's actor dead at the save has never been loaded |
| Join checkpoints `coop-join-N` | `CoopServerHost.cpp:636-720` (`start_resync`) | Every join streams a fresh native save plus the online set at the lifecycle watermark (`g_host->commit`) | Not retained as recovery points; written through the same non-atomic path |
| Autosave manager | `autosave_manager.cpp:51-55` | — | Permanently disabled in this fork (`if (true) return;`): there is no autosave on any product |
| Identity registry | `CampaignIdentity.h`, [participant identity](participant-identity.md) | `EntityId` per object for the life of the process; `{u16, generation}` keys; participant slots | Nothing persists: a checkpoint load re-creates objects through `PerformIDgen(saved id)` with new `EntityId`s; slots are rebound from the roster in roster order and actors adopted in spawn order (`CampaignIdentity.cpp:538-561`, `CampaignActorAccess.cpp:370-388`) |
| Lifecycle commit ids, operation ids | `CoopServerHost.cpp` `g_host->commit`; `ActionResult.commit_id` | Ordered within one process | Restart at 0; no outcome index; a client's operation counter restarts with the process (`CoopClientLink.cpp:1836`) |
| Authority epoch | `session/server.cpp:459,469`; `proto/guard.h:37-42` | Every in-session message carries the epoch; an older one is dropped, a newer one is a violation; `ResyncBegin` can announce the next | Fixed at 1 (`CoopServerHost.cpp:2498`, `:702`); never persisted |
| Client on connection end | `CoopClientLink.cpp:2931-2948` | Logs `ended` | Stays in its stale world; no reconnect |

L15 ([carried limits](carried-limits.md)) names this gap: no crash or
journal guarantee; ownership, captured contexts, participant references and
actor 2's script state are not persisted.

## 2. Identities and ordering

| Name | Type | Scope | Where it lives |
|---|---|---|---|
| `CampaignId` | 128-bit | the campaign (save lineage) | profile today; the checkpoint manifest and journal header from S07-T02 |
| `PlayerId`, `PartyId` | 128-bit | across sessions and restarts | roster / profile; the supplemental record |
| `EntityId` | 64-bit serial, never reused | the campaign | the registry in memory; the supplemental record maps every saved object's u16 to it; journal records name entities only by `EntityId` |
| **Revision** `R` | u64, strictly increasing per campaign | every committed change the clients or the store can observe: lifecycle commits, action commits, journal records | `g_host->commit`; the checkpoint manifest carries the revision it reflects (`R_G`); every journal record carries its own |
| **Generation** `G` | u32, strictly increasing per campaign | one published checkpoint | file names `coop-gen-<G>.*`, the pointer file, the manifest |
| **Epoch** `E` | u64, strictly increasing per campaign | one server incarnation | the journal's first record at every start; the Welcome, every in-session message, `ResyncBegin.authority_epoch` |
| **Operation** | `(PlayerId, operation_id u64)` | one client request | the operation index: in memory, in the supplemental record up to `R_G`, in journal records after it |
| **Sequence** | u64 per journal file | one record | the record header; a gap or repeat invalidates the file from that point |

Rules:

- A revision is allocated at commit, in the server's tick order, and never
  reused. Transient state (snapshots) carries no revision; it is fenced by
  the epoch alone.
- `R_G` is the revision of the last change the checkpoint `G` reflects: the
  checkpoint is taken inside one tick with no commit between the take and the
  recorded `R_G`, so "record with revision ≤ `R_G`" means "already in the
  checkpoint" and "> `R_G`" means "must be replayed after loading it".
- An epoch is allocated once per server start, durably (journal record,
  flushed) **before** the server listens, so no client ever receives a
  Welcome under an epoch that a later restart could repeat.
- Operation ids are chosen by the client and must be unique for the player
  across its own restarts. From S07-T05 a client seeds its counter from the
  UTC clock at start (`(utc_ms << 16) | counter`); until then a restarted
  client's low ids could collide with its earlier accepted ones, which is why
  the operation index is per `(player, id)` and returns the stored outcome
  instead of re-executing.

## 3. State classes and the guarantee matrix

The owner column is canonical ownership from [state ownership](state-ownership.md);
the representation column is what survives, and where.

| Class | Examples | Representation | Durable when | After a crash |
|---|---|---|---|---|
| **A. Simulated world** | positions, velocities, AI plans, physics, bullets, NPC health between checkpoints, weather effect, bolts | native `.scop` + `.scoc` of generation `G` | at the publication of `G` | rolls back to `G` (transient rollback, D7); nothing in A is ever acknowledged as permanent |
| **B. Participant identity** | slot ↔ player ↔ party; slot generation; the participant's actor (u16 and `EntityId`); alive flag; last position | supplemental record `.sup` of `G` | with `G` | restored before the actors spawn, so each saved actor is assigned to its player, not adopted by spawn order |
| **C. Entity identity** | u16 → `EntityId` for every saved object; `next_entity` | `.sup` of `G`; journal `entity` records for durable entities created after `G` (a respawned actor), tombstones for durable entities destroyed after `G` | with `G` / with the record | restored at load; a journal entity record binds the recreated object to its recorded `EntityId` |
| **D. Script state** | actor 1's binder `pstor`, task manager state, info portions, story flags, `m_data` (S08-T09: also every other participant's thirst, sleep and psy health, `m_data.coop_needs`) | `.scoc` of `G` (unchanged native path) | with `G` | rolls back to `G`; actor 2 has no binder and no `pstor` (L15 remains for actor 2 until the presentation/script split; recorded as L32 at G07) |
| **E. Critical outcomes** | Stage 7: the synthetic ledger effect, a participant's death, a respawn; Stage 8+: pickup, transfer, equip, use, trade, reward (a party quest's accept and completion: **E′** below) | a **journal record** holding the *resolved* effect (absolute post-state, identities, revision, operation) | at the record's flush, **before** the acknowledgement | records with revision > `R_G` are re-applied through idempotent adapters; nothing acknowledged is lost or applied twice |
| **F. Operation outcomes** | accepted operations and their commit revision | the journal record that committed them; the index up to `R_G` in `.sup` | with the record | a retry after restart returns the stored outcome; a never-acknowledged operation is evaluated afresh |
| **G. Session state** | connections, resume tokens, snapshot baselines, input queues, prediction records | memory | never | fenced by the new epoch; every client rejoins and gets a full resync |
| **H. Client-local** | camera, UI, interpolation buffers, local sounds | the client | never | discarded with the stale world |

What "critical" means: an effect is critical when its success is reported to
a player as permanent (an `ActionResult` Accepted, a `CampaignEvent` that
says it happened, or anything the player would reasonably expect to keep:
a death, an item, money, a quest stage). A critical effect must have a
resolved representation that an adapter can re-apply without re-running
the gameplay that produced it. An effect whose only representation is "run
this Lua again" (dialogue side effects, random rewards) is **not** durably
recoverable and must not be acknowledged as permanent until its task gives
it a resolved record (R07; plan.md S07-T04 risk).

What rolls back and is allowed to: everything in A and D between `R_G` and
the crash, except what E re-applies on top. A player may find NPCs, items on
the ground and their own position where the last checkpoint left them,
while their acknowledged ledger value, death and respawn are where the
journal left them. Checkpoint cadence bounds the rollback (S07-T02: a
periodic checkpoint, profile `checkpoint_seconds`, plus one at every join
and one at a clean stop).

## 4. Transaction phases

Every critical operation goes through these phases in one server tick,
in this order, on the thread that owns the simulation (D8: one process, one
simulation writer):

| Phase | What happens | Can it fail? | Crash here means |
|---|---|---|---|
| 1 **Receive** | the session layer attributes the intent to its player; the operation index is consulted: a known `(player, id)` returns its stored outcome and nothing else happens | — | nothing happened |
| 2 **Prepare / validate** | the intent is checked against canonical state at this tick (the actor exists, is alive or dead as required, the parameters are bounded, the expected revision matches) | yes → `ActionResult` Rejected with a reason; not durable; kept in the in-memory index only (a retry after restart is evaluated again, which is correct because nothing was promised) | nothing happened |
| 3 **Resolve** | the effect is computed as absolute post-state (money after, the new actor's identity and position, the dead entity), a revision `R` is allocated, the record is built | — | the revision is lost with the process; the next incarnation continues from the last durable revision, so no gap is visible |
| 4 **Durable commit** | the record is appended to the journal and flushed (`WriteFile` on a handle opened with `FILE_FLAG_WRITE_THROUGH`, then `FlushFileBuffers`) | yes → **fail closed**: the record is not applied, not acknowledged; the store enters `failed`; every further critical intent is refused with reason `StoreFailed`; the server logs it, keeps the world running for the transient play already admitted and requests a clean stop (the journal is not written again in this incarnation). Recovery from the last generation plus the journal up to the last good record is intact | before the flush returned: the record may or may not be on disk; nothing was acknowledged, so either outcome is correct (a torn trailing record is discarded at recovery, §6) |
| 5 **Apply** | the adapter mutates the simulation from the record (idempotent: it sets absolute values and checks existence) | a failure here is a defect, not a storage condition; logged, counted, the operation is still committed and will be re-applied at recovery | the record is durable and unapplied: recovery re-applies it |
| 6 **Acknowledge** | `ActionResult` Accepted with `commit_id = R`; a `CampaignEvent` or lifecycle message to the other client where the effect is visible | the send can fail (connection gone) | the client never saw the result and retries after reconnecting; phase 1 returns the stored outcome; no second mutation |

The lifecycle stream (Spawn/Despawn, `commit_id`) keeps using revisions
from the same counter so that `ResyncBegin.event_watermark`, journal
revisions and lifecycle commits are one order.

Checkpoint publication is a transaction of its own (§5). It never runs
between phases 3 and 6 of an operation: all six phases complete inside the
tick that received the intent, and publication runs at the end of a tick.

## 5. Checkpoint generations and publication

Files in the server runtime's `$game_saves$`:

| File | Content |
|---|---|
| `coop-gen-<G>.scop`, `.scoc` | the native save of generation `G` (unchanged engine and Lua formats) |
| `coop-gen-<G>.sup` | the supplemental record (§5.1), CRC-32 trailer |
| `coop-gen-<G>.manifest` | JSON: schema versions, campaign id, `G`, `R_G`, epoch at the take, server tick, game time, level, build id, content digest, protocol major, every file's size and SHA-256, the journal file name for records after `R_G` |
| `coop-journal-<G>.log` | the journal opened at the publication of `G` (§6) |
| `coop-checkpoint.current` | one line: `<G>`; the only file whose content decides which generation is current |
| `coop-tmp-*` | work files of a publication in progress; deleted at the next start |

Publication, at the end of one tick, with no commit between steps 1 and 7:

1. **Take.** `R_G = commit`. The native save is requested under the temporary
   name (`M_SAVE_GAME`, the S02-T06d path). The supplemental record is
   written to a temporary file.
2. **Verify.** Each temporary file is read back: the `.scop` header
   (`valid_saved_game`), sizes, SHA-256 of each file.
3. **Flush** each temporary file (`FlushFileBuffers`).
4. **Rename** each temporary file to its `coop-gen-<G>` name
   (`MoveFileExW`, `MOVEFILE_WRITE_THROUGH`).
5. **Manifest.** Written to a temporary name, flushed, renamed to
   `coop-gen-<G>.manifest`.
6. **Journal rotation.** `coop-journal-<G>.log` is created with its header
   record (`G`, `R_G`, epoch), flushed. New records go there from now on.
   The previous journal is closed.
7. **Pointer.** `coop-checkpoint.current` is replaced atomically: the new
   content is written to a temporary file, flushed, and moved over the
   pointer with `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
   This is the commit point of the publication.
8. **Retention.** Generations older than `G-1` and their journals are deleted.
   `G-1` and `coop-journal-<G-1>.log` are kept: if `G` turns out unreadable,
   recovery uses `G-1` and replays journal `G-1` (which holds every record in
   `(R_{G-1}, R_G]`) and then journal `G`.

A failure at any step before 7 leaves the pointer at `G-1` and journal `G-1`
open; the temporary files are ignored. A failure at step 8 leaves extra
files. The join stream (`start_resync`) uses the same publication so every
join is also a recovery point, and the client's copy (`coop-join-N` as the
client writes it) is unchanged in format.

Windows guarantees relied on, and what they are not: `FlushFileBuffers`
sends the file's data and metadata to the volume and waits for the device to
report completion; `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` on one NTFS
volume replaces the target name atomically with respect to other openers,
and `MOVEFILE_WRITE_THROUGH` returns after the rename is on disk. These hold
against process termination (what S07-T06 tests) and against an OS crash on
a volume whose device honours flush. They are **not** a power-loss guarantee
on a device that lies about flush completion, and the tests do not cut
power: this is recorded as a limit, not claimed.

### 5.1 Supplemental record (`.sup`, schema 2, little-endian)

Schema 2 (2026-10-23, S07-T07 under G07 decision 2) adds each
participant's level entry point (`entry f32×3, entry vertex u32`) after its
position; schema 1 stores are refused as incompatible (none existed outside
tests).

```
'CSUP' u32, schema u32 = 2
campaign id 16 B, party id 16 B
generation u32, revision u64 (R_G), epoch u64, server tick u64, game time u64
level id u32, level name (u8 len + bytes)
next_entity u64
participants u8, each: slot u8, slot generation u32, player id 16 B,
    actor u16 (0xffff none), actor entity u64, alive u8, position f32×3,
    entry position f32×3, entry vertex u32 (0xffffffff unknown),
    name (u8 len + bytes)
entities u32, each: u16 id, u64 entity          (every saved ALife object)
operations u32, each: player id 16 B, operation u64, outcome u8, reason u16,
    revision u64                                 (accepted outcomes, per-player window)
CRC-32 u32 of everything before it
```

### 5.2 Compatibility

Recovery refuses a generation whose manifest has another supplemental or
journal schema, another campaign id, or another content digest (installed
content), and logs a build id that differs (a rebuilt server may read its
predecessor's checkpoint; the engine's own `ALIFE_VERSION` check guards the
`.scop`). A refused generation is never deleted and never "reset": the
server stops with a named reason (§7). A schema change in a later task adds
a converter or a refusal, never silent acceptance.

## 6. The journal

`coop-journal-<G>.log`, append-only, opened with `FILE_FLAG_WRITE_THROUGH`:

```
file header: 'CJRN' u32, schema u32 = 1, campaign id 16 B, generation u32,
             base revision u64 (R_G), CRC-32 u32
record:      length u32 (payload bytes), CRC-32 u32 (of the rest of the record),
             sequence u64 (1, 2, 3 … per file), kind u8, epoch u64, tick u64,
             revision u64, payload
```

Record kinds (Stage 7, and Stage 8 from S08-T01):

| Kind | Payload | Adapter (apply at commit; re-apply at recovery) |
|---|---|---|
| `epoch` | the new epoch | none: the recovery reads the highest epoch to fence the next one; the `epoch` record is the first record of each incarnation |
| `ledger` (synthetic, S07-T03) | slot, player id, operation id, actor entity, delta s32, money after u32 | sets the participant actor's money to `money after` on its live object and ALife entity; idempotent |
| `death` (S07-T06/T07) | slot, actor entity, killer entity (0 none), position | if the entity's live object is alive, kill it natively (`KillEntity`) without a killer when the killer is gone; if already dead, nothing |
| `respawn` (S07-T07) | slot, old actor entity, new actor entity, new u16, position, vertex, graph | if the slot's live actor is the old one or none, spawn the new actor as the server does at a join, bind it to the recorded entity id and the slot; if the recorded entity already exists, nothing |
| `pickup` (S08-T01) | slot, player id, operation id, owner (participant actor) entity, item entity, item u16 at commit, section, position, the item's native spawn state (`Spawn_Write`, M_SPAWN header and section) | **absolute post-state**: the item entity ends in the owner entity's inventory from wherever it is now. Already there: nothing. Held by another parent (an NPC, a corpse or a box after a rollback): `GE_OWNERSHIP_REJECT` from it, then `GE_OWNERSHIP_TAKE` to the owner. Only offline: the offline copy is released and the item is re-created. Absent (the checkpoint predates its spawn): re-created from the recorded spawn state in the owner's inventory under the recorded EntityId (`expect_entity`). The owner's level object not there yet (a respawned owner at recovery): deferred and retried every tick for up to 10 s. The owner entity missing: the adapter fails (the start stops) |
| `transfer` (S08-T02) | action (6 drop, 7 transfer), from entity (the owner it left), then the `pickup` layout with owner entity = the owner it ends in, **0 for the ground** | the `pickup` adapter: the item ends in the recorded owner, or on the ground (parent none); at replay a ground item is moved to the recorded position (`GE_CHANGE_POS`, the engine's `DropItemAndTeleport` path) and an absent one is re-created there (level vertex and game vertex from the position). The record is the whole post-state of the change: both affected containers are implied by the one item's new parent (the source loses it by the same event). **A container that is not in the recovered world** (created after the checkpoint: a stash a script spawned, a body killed since) cannot take the item back: at replay the item is kept **on the ground at the record's position** (`transfer-replay-owner-gone`) instead of being lost or stopping the start |
| `equip` (S08-T07) | slot, player id, operation id, actor entity, item entity, place u8 (1 slot, 2 belt, 3 ruck), inventory slot u8, flags u8 (bit 0 do not activate) | **absolute post-state**: the item is in that place of that actor's inventory (`CInventory::Slot/Belt/Ruck`). Committed **before** it is applied (an equip is deterministic). At replay: the item already there, gone (used up or released after the record) or moved out of the actor since (a later record says where): nothing; the actor's or the item's level object not there yet (a pickup that re-creates it is still pending): deferred and retried for up to 10 s; a slot still held by another item at replay: that item goes to the ruck first (a later record places it). Failure is counted, never stops the start |
| `use` (S08-T07) | slot, player id, operation id, actor entity, item entity, section, uses after u8, consumed u8, game time u64, the actor's health, power, radiation, psy health, satiety after (f32 each), then effects: count u8 × (op u8: 1 removed / 2 remaining uses / 3 created, item entity u64, op 2: uses u8, op 3: section and the item's spawn state) | **applied first, then committed before the player is answered**: only the engine and the installed scripts know what a use does (`CInventory__eat`, `CInventory::Eat`, the `actor_on_item_use` handlers inside the user's script context), so the server applies it, then records the resolved result: the item's remaining uses, or its release when used up, every other item of the actor's inventory the use created, removed or spent a use of, and the actor's condition. A termination between the apply and the commit loses an **unacknowledged** use with everything it did (the checkpoint comes back whole). At replay (absolute post-state, idempotent): created items are re-created in the actor under their EntityIds (the `pickup` adapter); a used-up or removed item is released wherever it is; remaining uses are set on the level object (the server entity holds none); the actor's five condition values are set and the item's boosters re-applied **from their full time**. Before the replay, every item a replayed `use` releases is collected: a `pickup` or `transfer` of such an item is not re-applied (`consumed-later`), so a used item never comes back with its pickup record |
| `entity-reserve` (S08-T02) | u64 limit | none at replay; the identity restore starts the EntityId serial past the highest limit in the replayed journals. Written for each block of 4,096 ids before the first is issued; a checkpoint's supplemental `next_entity` is the reserved limit. So an id the previous incarnation issued after its checkpoint is never issued again |
| `entity` (S08-T08: a container) | entity, kind u8 (1 inventory box, 2 body), section, position, the container's spawn state (a body's health 0) | written before the **first committed transfer that touches a box or a body after each publication** (a client's transfer, or a server-made move into a container): at replay a container the recovered world does not hold (created after the checkpoint: a stash a script spawned, a body of an NPC spawned since) is **re-created under its EntityId at its position** (a body dead), so the transfers that name it put their items back into it. Present: nothing |
| `tombstone` (S08-T08) | entity, the owner it was released from, section | a server-made release of an item from a participant actor's inventory (observed, below): at replay the item is released wherever it is; collected before the replay like a used-up item (an earlier `pickup`/`transfer` of it is not re-applied) |
| `stack` (S08-T08) | slot, player, operation, container entity, section, count u8 × (entity, rounds u16), count u8 × released entity | the result of an ammunition merge in a box (the installed `item_weapon.ammo_aggregation`, run on the server once the moved stack is in the box, read 10 ticks later because its changes land a frame later): at replay the counts are set (server entity and level object) and the released stacks released (and collected before the replay) |

**Server-made changes (S08-T08).** At the start of every host tick the
server compares each participant actor's children with the end of the
previous tick: what the server's own gameplay changed in the engine frame
between them (the installed scripts' deferred work such as the backpack
stash, an NPC taking from a player, a script creating or releasing an item)
is journaled as server-origin records, operation 0: a new item → `pickup`
(the item ends in the actor, with its definition), an item gone elsewhere →
`transfer` (its container's `entity` record first) or to the ground, an item
gone → `tombstone`. The host's own work in its tick (client actions and their
adapters, the quest's effects, the kit, fixtures) is journaled by itself; the
comparison is taken again at the end of every tick. **Combat consumables are
not observed** (ammunition boxes, grenades, bolts): they roll back with the
weapons and the fight (D7).

Reading rules: a record whose length runs past the end of the file, or
whose CRC does not match, **and which is the last record**, is a torn
append: discarded, logged, recovery continues (the effect was never
acknowledged). A bad record followed by readable records, a sequence gap or
repeat, or a header mismatch is corruption: the file is refused and the
previous generation is tried (§7). Records are applied in revision order;
a record with revision ≤ `R_G` of the generation being recovered is skipped
(already in the checkpoint).

Retention: a journal is deleted with its generation (§5 step 8). Truncation
inside a file never happens; rotation is the only compaction.

## 7. Recovery

At `-campaign-serve` start, before any service that reads the save
(`xrCoopServer_Prepare`, `x_ray.cpp:1674`):

1. Read `coop-checkpoint.current`. Absent and no `coop-gen-*` present: this
   is the campaign's first start; the profile's base save (`save =`) is
   loaded and generation 1 is published at the first join. Absent but
   generations present: refuse to start (`recovery-no-pointer`).
2. Verify generation `G`: the manifest parses, schema and campaign match,
   every file has the recorded size and SHA-256, the `.sup` CRC holds, the
   journal `G` header matches. On any failure, try `G-1` the same way. If
   neither verifies: refuse to start (`recovery-no-valid-generation`), never
   fall back to the base save.
3. Read every readable journal record of the chosen generation (and, when
   `G-1` was chosen, of journal `G` too): the highest epoch seen plus one is
   this incarnation's epoch; the records with revision > `R_G` are the replay
   list; `commit` continues from the highest revision seen or `R_G`.
4. Append the `epoch` record, flushed. Only now is the epoch allocated.
5. Load the native save of the chosen generation (the normal server start).
6. After the ALife load and before the client stages spawn anything
   (between `campaign-server-ready` and the client stages,
   `x_ray.cpp:1805-1830`): restore identities from `.sup` (the registry
   continues each saved u16's `EntityId`; `next_entity`), bind the roster's
   players to their saved slots and assign each saved actor to its
   participant by u16 (`assign_participant_actor`), restore the operation
   index.
7. After the level is loaded and the first ticks ran: apply the replay list
   through the adapters, in revision order, each idempotent; log each
   applied, skipped and failed record. A failed adapter (the record names an
   entity the checkpoint does not hold and no `entity` record defines)
   stops the start (`recovery-replay-failed`): a silent partial recovery is
   worse than a refusal.
8. Listen. Every client that connects gets a Welcome under the new epoch, a
   fresh session (a resume request for a pre-restart session is refused,
   `session/server.cpp` `resume_refused`), and a full resync from a new
   generation taken at its join, as today.

Nothing a client holds is merged: client saves never enter this path.

## 8. Resynchronization after restored authority (S07-T05)

- The client, on `Ended`, leaves its world (the stale world cannot continue
  to be played: inputs stop, the level is stopped, the replica is reset),
  keeps its unacknowledged operations, and reconnects with backoff (1 s
  doubling to 10 s) until its exit time.
- The Welcome's epoch is new; every message under the old epoch is dropped
  by the guard; a new connection also has new transport keys, so nothing
  from the old incarnation decodes at all. The resync carries
  `authority_epoch = E` and `event_watermark = R_join` so the client's
  applied revision starts at the checkpoint's.
- Once live, the client re-sends its unacknowledged operations with their
  original ids; the server answers each from the index (accepted before the
  crash: the stored outcome; never committed: evaluated afresh).
- Convergence check (S07-T05 validation): both clients' replica sets and
  actor states equal the server's after the restart; a delayed or
  duplicated intent produces one mutation; a client that does not reconnect
  cannot play on (no input accepted, no snapshot decoded).

## 8a. Party quest: checkpoint-backed outcomes (E′, S08-T05)

A party quest's accept and completion change state that only the native and
Lua save captures (the native task, the installed task manager's
`task_info`, the holder's portable store, the stash, the relation registry):
there is no adapter that could re-create it from a journal record without
re-running installed, non-deterministic code. These outcomes are therefore
**acknowledged only after a checkpoint generation that holds them is
published**:

1. the server applies the outcome (the accept phrases; or the completion and
   the adapter's resolved effects: patches released, each player paid,
   goodwill to both) and remembers the operation's outcome in the index;
2. it requests a publication; the generation's take holds the outcome and
   the index (supplemental operations);
3. when that generation is published, the waiting answers are sent
   (`answer_quests`); a failed publication answers `StoreFailed`.

A termination before step 3 rolls the outcome back with the world (nothing
was acknowledged); the client's retry is evaluated afresh, so the quest
completes once. A termination after step 3 recovers the generation that
holds it; the retry is answered from the index. The party's task always
lives on participant 1's actor, whose script state is the one the native
save holds (actor 2's is not, L32). Failpoints `quest:after-accept`,
`quest:after-apply`, `quest:before-ack`.

## 9. Respawn (S07-T07, under this contract)

Implementation note (2026-10-23, S07-T07): the `respawn` record carries the
new actor's **reserved** `EntityId` (`CampaignIdentityRegistry::reserve_entity`
before the commit; the object's `m_campaign_entity` carries it into the id
assignment at the spawn) and the point (position, level vertex, graph), and
it is itself the durable definition of the new entity (section `actor`); no
separate `entity` record is written, and the u16 is assigned at the apply.
The adapter is idempotent: if the slot already holds the recorded entity, or
an object with that `EntityId` exists in the checkpoint, nothing is spawned.

Death is a critical outcome (`death` record at the native `Die`); the
respawn is an operation: intent → validate (the slot's actor is dead, no
respawn pending, the client confirmed) → resolve (new entity id, point: the
participant's position in the latest published generation's `.sup`, or the
last position the server tracked for the living actor when the player
joined after the last generation) → commit (`respawn` + `entity` records)
→ apply (spawn the actor as at a join, bind it, the old actor stays as a
corpse with its inventory) → acknowledge. A client whose participant actor
changed gets a fresh world stream (the join path), so its player entity is
the new actor by construction.

## 10. Synthetic critical-effect fixture (defined here, built in S07-T03)

Purpose: a critical effect that is small, visible on both sides and captured
by the native checkpoint, so that every crash boundary of §4 and every
publication step of §5 can be exercised without inventory or quest code.

- **Intent:** `ActionIntent` kind 3 `ledger`, params `s32 delta`
  (|delta| ≤ 1000). No target: it acts on the player's own participant actor.
- **Validation:** a live participant actor (dead or alive), bounded delta.
- **Resolution:** `money_after = clamp(money + delta, 0, 2^31)`.
- **Commit:** `ledger` record (§6). **Apply:** `set_money(money_after)` on
  the live actor; the native save carries money (`CInventoryOwner::save`,
  `InventoryOwner.cpp:186`; S02-T06e carried it through two checkpoints).
  **Acknowledge:** `ActionResult` Accepted, `commit_id = R`.
- **Client program 6:** every 2 s sends a `ledger` with a known delta and a
  fresh operation id, retries an unacknowledged one every 3 s with the same
  id (across reconnects), and records `(operation, delta, sent, result,
  commit, money_after_expected)` in `coop-ledger-client.csv`.
- **Server:** `coop-ledger.csv` with every phase-4 commit and phase-5 apply
  (`utc_ms, tick, revision, slot, operation, delta, money_after, phase`).
- **Checks (`tools/compare-coop-recovery.py`):** for each participant, the
  server's money after recovery equals the last acknowledged `money_after`;
  the sum of acknowledged deltas equals the server's money change; every
  duplicate of an acknowledged operation returned the same outcome and
  commit; no revision appears twice in `coop-ledger.csv` across incarnations;
  nothing acknowledged is missing from the restored state; a transient
  rollback (positions) is reported, not failed.

## 11. Crash boundaries and expected recovery states (paper simulation)

`op` is one ledger operation with delta +10 from money 100; `G` is the
current generation at revision `R_G = 50`; the operation gets revision 51.

| # | Termination point | Durable state | Recovery result | Client sees |
|---|---|---|---|---|
| 1 | after receive, before prepare | journal: ≤ 50 | money 100 | no result; retries after reconnect; evaluated afresh: accepted at a new revision (money 110 once) |
| 2 | after validate, before append | same | money 100 | same as 1 |
| 3 | during append (torn record) | journal: ≤ 50 + torn tail | torn tail discarded; money 100 | same as 1 |
| 4 | after append, before flush returned | record may be present or torn | if present: replay sets money 110, index knows the op; if torn: as 3 | retry returns Accepted/51 (present) or is evaluated afresh (torn); in both cases money changes once |
| 5 | after flush, before apply | record 51 | replay sets money 110 | retry returns Accepted/51 |
| 6 | after apply, before ack | record 51; live money 110 (lost with the process) | replay sets money 110 (idempotent) | retry returns Accepted/51 |
| 7 | after ack | record 51 | money 110 | result already received; a stray duplicate returns Accepted/51 |
| 8 | checkpoint: after the temporary files, before rename | pointer `G`; journal `G` has 51 | `G` + replay 51 → money 110; temporaries ignored | — |
| 9 | checkpoint: after rename, before manifest | pointer `G`; `coop-gen-G+1.*` without manifest | `G` + replay 51; `G+1` files unverifiable, ignored, cleaned at the next publication of `G+1` (same name, overwritten through temporaries) | — |
| 10 | checkpoint: after manifest, before journal rotation | pointer `G`; manifest `G+1` valid; journal `G` has 51 | `G` + replay 51 (the pointer decides); `G+1` is a valid orphan the next publication replaces | — |
| 11 | checkpoint: after rotation, before pointer | pointer `G`; journal `G+1` exists (empty); journal `G` has 51 | `G` + replay journal `G` (51) and journal `G+1` (nothing) | — |
| 12 | checkpoint: after pointer, before retention | pointer `G+1`; extra old files | `G+1` (money 110 inside) + journal `G+1` (nothing); old files deleted at the next publication | — |
| 13 | `.scop` of `G+1` corrupted on disk after publication | manifest hash mismatch | `G` + journal `G` (51) + journal `G+1` | — |
| 14 | death of participant 2 committed (revision 52), then crash before `G+2` | journal `G+1`: 52 | replay 52: the actor is killed natively after the load (it was alive in `G+1`) | both clients see it dead after rejoining |
| 15 | respawn committed (53) with `entity` 54, crash before apply | journal: 53, 54 | replay spawns the new actor, binds entity 54 to slot 2; the corpse (old entity) stays | player 2 is alive at the respawn point |
| 16 | journal append fails (disk full) at op revision 55 | nothing new | the store fails closed; op 55 Rejected `StoreFailed`; no apply; clean stop requested | retry after restart is evaluated afresh |
| 17 | two incarnations: records from epoch 3 and 4 in journal `G` | both durable | all replayed in revision order; new epoch 5 | old-epoch packets dropped by the guard |

Rows 1–7 are the failpoints S07-T06 injects (`before-append`,
`after-append`, `after-flush`, `after-apply`, `after-ack`); rows 8–12 are
the publication failpoints; rows 13, 16 and 17 are store tests in
`tests/store/`; 14 and 15 run in S07-T07's restart scenario.

## 12. What this contract does not promise

- No deterministic replay: inputs are never re-executed; only resolved
  effects are.
- No zero-loss recovery of the simulated world: A and D roll back to the
  last generation.
- No power-loss guarantee: process termination is tested; device flush
  honesty is assumed, not tested.
- No durable second-actor script state (L32).
- No durability for effects outside E until their Stage 8/10 task gives
  them a record and an adapter under this contract; an effect produced only
  by re-running Lua is not acknowledged as permanent.
- No persistence service: everything runs inside the dedicated process
  (D8); a separate store would need a demonstrated limitation and approval.
