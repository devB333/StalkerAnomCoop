# Participant identity and authoritative contexts — S03-T01

This document defines the identities the co-op server uses, how they map onto
the engine's transient ids, who may create and resolve them, and the state a
participant keeps across a disconnect. It is the identity/context interface
and ownership table S03-T01 owes (plan.md, Stage 3). Source is
`builds/build_1/engine-work` at `c57fe5b9` with `patches/engine/series`
applied; engine citations are relative to `src/`. Runtime evidence is in
[S03-T01](validation/S03-T01.md). Persistence of these identities is Stage 7;
authenticated binding of a connection to a player is S04-T04.

## What the engine has, and why it is not enough

| Engine name | Where | What it is | Why it cannot be an identity |
|---|---|---|---|
| Entity id, `u16` | `xrServer.h:145-158` `CID_Generator<u32,u8,u16,u8,u16,0,u16(-2),256,u16(-1)>`; `id_generator.h` | The id of every server object (`CSE_Abstract::ID`, `xrServer_Object_Base.h:75`): online entities and all ~26,000 ALife objects draw from the same generator (`alife_simulator_base.cpp:107,165,209`, `alife_storage_manager.cpp:152`, `alife_switch_manager.cpp:95`, `xrServer_process_spawn.cpp:79-112`). | Recycled. A freed id returns to a 256-id block stamped with its free time; `tfGetID` takes from the block freed longest ago, last-freed first (`id_generator.h:97-104,106-125`). Script releases free with time 0 (`alife_simulator_script.cpp:199,318,451`), destroyed entities with the server timer (`xrServer.cpp:873`). A reference held across a release names a different object. Ids are stable across a checkpoint load (`PerformIDgen(saved id)`), which makes them look persistent; they are not unique over a session. |
| `ClientID`, `u32` | `xrCore/client_id.h` | A connection. The in-process direct client is `ClientID 1`, name `single_player` (`xrServer.cpp:1040-1048`); the server marks the client with its own process id `bLocal` and `SV_Client` (`xrServer.cpp:890-917`). | A connection, not a player: it changes on reconnect, and `1` is the private state bridge whose privileged imports are gated on `bLocal` (`xrServer_process_update.cpp:14,66`; E04), not on any role. |
| `g_actor` / `Actor()` | `Actor_Network.cpp:58-67,530-531` | The one player: whichever actor spawned last with `M_SPAWN_OBJECT_LOCAL` and `M_SPAWN_OBJECT_ASPLAYER`; `Actor()` asserts single-player. | One global for two players. Stage 3 replaces its gameplay uses (S03-T02); ALife's one `actor()` (`alife_graph_registry.cpp:70,90`) is S03-T04. |
| `game_PlayerState::GameID`, `xrClientData::owner` | `game_base.h:82`, `xrServer.h:33` | The actor a connection controls (`get_id_2_eid`, `game_sv_base.cpp:90-97`). | Derived from the two above; inherits both problems. |

## Identities

All identities are server-assigned. A client never chooses one; it receives
its participant reference after the server binds it (S04-T04 will do the
binding from an authenticated session; the Stage 3 harness does it directly).

| Identity | Type | Scope and lifetime | Allocated by | Notes |
|---|---|---|---|---|
| `CampaignId` | 128-bit (`Uuid128`) | One campaign instance (save lineage). | Server at new game (Stage 7 persists it). | Namespaces every `EntityId`. |
| `PlayerId` | 128-bit | One human player across sessions, reconnects and campaigns. | Server's identity layer (S04-T04). | Never derived from a `ClientID`. |
| `PartyId` | 128-bit | A party; Stage 3 uses exactly one. | Server. | Party-scoped actions must name it (see contexts). |
| `EntityId` | 64-bit serial, 0 = none | Unique within the campaign for the life of the registry; never reused. | Registry, at every engine-id assignment. | 128-bit per entity (the protocol draft's first suggestion) was rejected: ~26,000 ALife objects plus items flow at snapshot rate, and a campaign-scoped 64-bit serial cannot alias either. |
| `ItemId` | `= EntityId` | Items are server entities (`CSE_ALifeInventoryItem` draws its `ID` from the same generator). | — | One namespace, so inventory and world references compare directly. |
| `EngineEntityKey` | `{u16 id, u32 generation}` | A transient reference into the running server. | Registry. | The only form in which a u16 may be held across a frame. |
| `ParticipantRef` | `{u8 slot, u32 generation}` | A reference to a participant slot. | Registry. | Slot generation changes when the slot changes owner. |

Zero is never a valid identity: a zero `Uuid128`, `EntityId{0}`, generation 0
and `u16(-1)` are refused as `InvalidIdentity`.

## Engine-id mapping

Every `xrServer::PerformIDgen` result and every `xrServer::FreeID` call reports
to the registry (`xrGame/CampaignIdentityHooks.h`; `xrServer::entity_Destroy`
now frees through `FreeID`). `PerformIDgen` takes the object that receives the
id, and every assignment site passes it (`alife_storage_manager.cpp:152`,
`alife_switch_manager.cpp:95`, `alife_simulator_base.cpp:107,165,209`,
`alife_dynamic_object.cpp:273`, `alife_trader_abstract.cpp:263`,
`xrServer_process_spawn.cpp:79-112`; the actor-id reservation in
`alife_update_manager.cpp:257` has no object yet). A new `xrServer` and
`xrServer::clear_ids` (`game_sv_Single::restart_simulator`) start a new
**incarnation**: every binding is retired, generations continue; the server's
destructor retires every binding too (its entities go without `FreeID`).

- Assignment of `id` to an object → `generation[id] += 1`; the object's
  `CSE_Abstract::m_campaign_entity` is its `EntityId` if it has one, else a
  fresh one is allocated and stored on the object; the binding is live.
- Release of `id` → binding retired (the generation and last `EntityId` stay
  for the stale answer).
- `resolve({id, g})` → the bound `EntityId` only if the binding is live **and**
  `g` is its current generation; otherwise `StaleReference`.

**Why the id lives on the object.** The ALife switches free and re-request an
object's own u16 synchronously: going offline, `remove_online` destroys the
entity (`entity_Destroy` → `FreeID`) and at once re-requests the same id for
the same object (`alife_switch_manager.cpp:91-95`); going online, `add_online`
destroys the entity and re-spawns the same object through `Process_spawn`,
which re-requests `E->ID` (`alife_switch_manager.cpp:52-62`,
`xrServer_process_spawn.cpp:112`). The first GameState run with a per-
assignment id counted 1,074 such re-requests during one level load (smoke-97).
An identity per assignment would give an object a new identity every time it
crossed the switch distance; carrying it on the object keeps it, and the
generation still turns so references from before the switch are stale.

Consequences, observed by the `engine-id` fixture with the server's own
generator type: an id freed at time 0 is handed out again at once; the old
key and the new key differ in generation; the old key never resolves to the
new object; a thousand released and a thousand spawned objects produce no
equal keys and a thousand distinct `EntityId`s; the switch pattern keeps the
object's `EntityId` under a new generation while a different object taking
the freed id gets a different one. In the loaded campaign, every online
object and every ALife object holds a current binding whose `EntityId`
matches the one stored on the object, and no binding is orphaned
(`campaign-identity-passed`).

**Limit (Stage 7).** A checkpoint load re-creates every object through
`PerformIDgen(saved id)`, so the objects keep their u16s but get new
`EntityId`s in this process (the stored id is not in the save). Mapping saved
`EntityId`s back is the supplemental record S07-T01/T02 own; until then an
`EntityId` is stable for one process life, which is what Stage 3–6 need.

## Participants

A registry holds a fixed number of slots: **two as policy**, a construction
parameter, not a structural limit (a one-slot registry refuses the second
player, a two-slot registry the third). Slot generations start at 1.

| State | Holds | Enter | Leave |
|---|---|---|---|
| `Free` | nothing | construction; `release_participant` (generation += 1) | `bind_participant` |
| `Bound` | player, party | `bind_participant` (server token; player not already bound; a free slot) | `attach_session`; `release_participant` |
| `Attached` | + session (`ClientID` value), actor if bound | `attach_session` (session ≠ 0, not the bridge, not attached elsewhere) | `detach_session` → `Detached`; `release_participant` |
| `Detached` | player, party, actor; session 0 | `detach_session` | `attach_session` with any new session → `Attached`; `release_participant` |

Rules the fixtures hold:

- **Retention.** A detached participant keeps its slot, player, party and
  actor binding. A third player cannot take the slot. A new session
  re-attaches to the same slot and generation (`disconnect-retention`).
  Releasing is the server's decision (liveness policy is S04-T05).
- **No aliasing through slot reuse.** After a release, the slot's generation
  changes; a `ParticipantRef` from the previous owner is `StaleReference`,
  never the new owner (`slot-reuse`).
- **One session, one participant; one actor, one participant.** A session
  attached elsewhere is `SessionInUse`; an actor bound elsewhere is
  `AlreadyBound`; a stale actor key is refused at binding.
- **The bridge is not a participant.** The direct client's `ClientID` is
  recorded from the running server (`xrCampaignIdentity_BindBridge`,
  `GetServerClient()->ID`); attaching it or resolving it as a session is
  `BridgeNotParticipant`.

## Authority and contexts

| Concept | Definition |
|---|---|
| `Role::Server` / `Role::Client` | The process role the registry is built with. Only a server-role registry mints a `ServerAuthority`; a client-role registry returns none and authorizes nothing (`WrongRole`). |
| `ServerAuthority` | A capability token with no public constructor and no copy, owned by one registry. Every mutation (`bind_participant`, `attach_session`, `detach_session`, `bind_actor`, `release_participant`, `set_bridge_session`) takes it and refuses a missing or foreign token (`NotAuthoritative`). Client code cannot bind identities by construction. |
| `Origin::ServerInternal` | A context produced inside the authoritative process (session 0). |
| `Origin::ClientIntent` | A request that arrived on a session. It is authorized against the participant **that session is attached to**; the participant it names is a claim that must agree with that binding (`NotAuthoritative` otherwise). It can never act in `Scope::Identity` or `Scope::World`. |
| `Scope` | `Identity` (binding, attaching, releasing), `World` (shared simulation state), `Party` (party-shared state; the context's party must be the actor's), `Self` (the acting participant's own actor). |
| `ActionContext` | `{origin, session, actor: ParticipantRef, party: PartyId}`. `authorize(context, scope)` returns `None` or the first refusal: unknown or stale participant, unknown or bridge session, impersonation, server-only scope, wrong party. |

This is the "no client-supplied identity becomes authority by assertion"
acceptance in code: a client intent carrying another participant's reference,
a bridge session, an unknown session or a server-only scope is refused; the
server can still act for a detached participant (its actor keeps simulating)
and cannot act for a released one.

The privileged state-import bridge (E04: `Process_update`/`Process_save`
assert `bLocal`) is unchanged and remains `ServerInternal` by construction;
enumerating and closing the remote outcome paths is S03-T02.

## Ownership table

| Thing | Owner (mints / mutates) | May resolve | Must never |
|---|---|---|---|
| `CampaignId`, `PlayerId`, `PartyId` | Server identity layer (S04-T04 for players; Stage 7 persists) | Server; clients receive their own | Be chosen or asserted by a client |
| `EntityId` | Registry, at engine-id assignment | Server; clients receive them in protocol (Stage 4) | Be reused within a campaign |
| `EngineEntityKey` | Registry | Server code holding references across frames | Be compared by `u16` alone |
| `ParticipantRef` | Registry | Server; a client receives its own | Be trusted as the actor of a client intent without the session check |
| Participant slot state | Registry under `ServerAuthority` | Anyone with the registry | Change hands without a generation bump |
| Bridge session | Registry (`set_bridge_session` from the live server) | — | Attach as a participant |
| `g_actor`, `Level().CurrentEntity()` | Engine (unchanged in S03-T01) | Client presentation; server gameplay until S03-T02 replaces it | Decide authority (S03-T02) |

## Files

- `xrGame/CampaignIdentity.{h,cpp}` — the model and registry (no engine
  globals read; the only engine dependency is the `xrCore` types).
- `xrGame/CampaignIdentityHooks.h` — the five id events; `xrServer.h`
  `PerformIDgen(id, object)`/`FreeID`/`clear_ids`, the `xrServer` constructor
  and destructor call them; no-ops on the client, where `g_campaign_identity`
  is null until S03-T02. `CSE_Abstract::m_campaign_entity`
  (`xrServerEntities/xrServer_Object_Base.h`) carries the entity id.
- `xrGame/CampaignIdentityFixture.{h,cpp}` — the `identity` service of the
  campaign server (owns the process registry), the four unit fixtures
  (`Run-Stage2Smoke.ps1 -Scope Identity`), the bridge binding and the level
  identity check in the GameState smoke.
