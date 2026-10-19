# Participant identity and authoritative contexts — S03-T01 to S03-T04

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

The privileged state-import bridge (E04) is `ServerInternal` by
construction and, since S03-T02, by policy: see "Local view and gameplay
access" below.

## Local view and gameplay access — S03-T02

The engine reaches the player two ways: `Actor()` returns `g_actor`
(`Actor_Network.cpp:60`) and `Level().CurrentEntity()` is the object the
local input and camera control (`xrEngine/IGame_Level.h:147`). Both are one
process's one player. The inventory of every use is
[actor-access-inventory.md](actor-access-inventory.md) (126 `CurrentEntity()`
sites, 372 `Actor()` sites). `xrGame/CampaignActorAccess.h` replaces them
with three kinds of access, by purpose:

| Access | Meaning | Server | Client |
|---|---|---|---|
| `local_view_actor()` | Presentation: cameras, HUD, post-process effectors, input, the map, pick-ups to draw | null (the server presents nothing, D1) | `g_actor` |
| `participant_actor(ref)`, `participant_of(object)`, `participant_actors(out)`, `nearest_participant_actor(from)`, `context_actor(context, scope)`, `dialog_actor(speaker1, speaker2)` | Gameplay: the actor a participant holds, the participant holding an actor, every participant actor, the nearest one to a point, the one an authorized action context names, the one among two dialogue speakers | Registry slots → `EngineEntityKey` → live object (`Objects.net_Find`; `g_actor` answers for its own id, which also covers its own `net_Spawn`) | The local player is the only known participant until Stage 4 delivers the roster: `participant_actors` is `{g_actor}`; `participant_actor(ref)` and `context_actor` are `WrongRole` |
| `sole_participant_actor(purpose)` and `Actor()` | The one participant actor while exactly one exists; null and a counted refusal with none (`NoActor`) or more than one (`Ambiguous`). Never a choice. `Actor()` is the compatibility accessor behind which every unconverted site sits; on the server each distinct caller is recorded by return address and reported with its symbol (`report_actor_access`) | sole actor or null, counted | `g_actor` |

Registry additions: `participant_actor_key`, `participant_of_engine_id`,
`live_actor_count`, `sole_actor_key`, `context_actor_key`,
`current_participant`, and `adopt_spawned_actor`: on the server every
`CActor::net_Spawn` (`OnServer()`) adopts the first participant without a
live actor, by the spawn packet's id (the object's own is set later in
`CGameObject::net_Spawn`). The harness binds fixture player 1 before the
level loads, so the saved actor is held by slot 0 from its spawn; a
checkpoint restart retires every binding and the re-spawned actor adopts
the same slot. An actor with no participant to hold it is logged and
counted (`unadopted`), never held by the global alone.

**The client's registry role (decided).** A normal client creates a
`Role::Client` registry when its `CLevel` is constructed and deletes it with
the level (`CampaignActorAccess.cpp`, `ensure_client_registry`). It mints no
authority, binds nothing, and knows one participant actor: its own. Stage 4
fills it from the server's roster. A process with no registry at all is a
client. The campaign server's registry is its identity service and exists
before the level.

**What was converted** (the inventory has every file): the three B21 sites
(the phantom hunts the nearest participant actor and goes idle with none;
the trader faces the nearest; the psy dog aura is the viewer's); dialogue
info checks use the actor among the speakers; stalker "player on the path"
and monster distributed-computation checks iterate every participant actor;
stalker look-at, crows and distant idle sounds use the nearest; "is my
enemy/hitter the player" became "is it an actor" (a cast of the entity in
hand); weapons, devices, the PDA and the flashlight use their holder; the
condition, animation, inventory, trade and dying-entity code use the object
itself; cameras, HUD, map, effectors and the per-frame camera update use the
local view actor. Tasks, map-location callbacks, script holders and
autosave name their single-actor assumption through
`sole_participant_actor(purpose)` until S03-T05/S08/Stage 7 give them a
context. The Lua `level.*` bindings (S03-T05) and the player-bound monster
abilities (S03-T04, S10-T02) are unchanged and go through the counted
accessor.

### The state-import bridge and remote outcome paths

The engine's server trusts one client with its private state: the direct
in-process client (`ClientID 1`, `xrServer::create_direct_client`), marked
`bLocal` and held as `SV_Client`. Before S03-T02 only `Process_update` and
`Process_save` checked it, as a fatal `R_ASSERT(bLocal)`, and
`xrServer::OnMessage` dispatched every other message without a sender check
(`xrServer.cpp:471-770`). `xrGame/CampaignMessagePolicy.h` classifies every
message and game event and `OnMessage`, `Process_event`, `Process_update`
and `Process_save` apply it before parsing:

| Class | Messages | From the bridge | From a remote or unknown sender |
|---|---|---|---|
| State import | `M_UPDATE` (→ `UPDATE_Read`: health, position, state), `M_SPAWN`, `M_SAVE_PACKET` (→ `load`) | accepted | refused |
| World control | `M_SAVE_GAME`, `M_LOAD_GAME`, `M_RELOAD_GAME`, `M_CHANGE_LEVEL`, `M_CHANGE_LEVEL_GAME`, `M_SWITCH_DISTANCE` | accepted | refused |
| Outcome | every `GE_*` game event carried by `M_EVENT`/`M_EVENT_PACK` (hit, die, destroy, ownership, trade, info transfer, zone and weapon state, upgrades, visuals, box status, delayed game events...), `M_GAMEMESSAGE`, `M_PLAYER_FIRE`, statistics, self-rename | accepted | refused (the pack is unwrapped and each inner event refused and counted by type) |
| Intent | `M_CL_INPUT`, `M_CL_UPDATE`, `M_MOVE_PLAYERS_RESPOND` | accepted | accepted (client-reported input and presentation state; Stage 4 defines the intent protocol) |
| Connection | handshake, auth, liveness, chat, map/digest, admin, files, secure messaging | accepted | accepted |
| Server-to-client, unknown | messages only the server sends; unknown types | accepted | refused |

Refusals are counted per type and the first 32 logged with the sender
(`report_message_policy`). The bridge is `ServerInternal` by policy as well
as by construction: a remote client can change nothing until Stage 4 gives
it an intent protocol, and even then only through intents.

## Per-participant control and the single-player script actor — S03-T03

Besides the view, the engine has one *control* entity:
`CActor::shedule_Update` (`xrGame/Actor.cpp:1825` before the change) runs the
control path (`g_cl_CheckControls` from `mstate_wishful`, `g_cl_Orientate`
from the actor's own camera yaw, `g_Physics`) only for
`Level().CurrentControlEntity()`; every other actor is interpolated from
network updates (`make_Interpolation`, `NET`). The campaign server adopts no
control entity (S02-T05d), so until S03-T03 no actor on it moved by input
(S02-T06b teleported). `xrGame/CampaignActorControl.h` replaces the test:

| Access | Meaning | Server | Client |
|---|---|---|---|
| `controls_actor(actor)` | Whether this process runs the actor's control path | every participant actor (`is_participant_actor`) | `Level().CurrentControlEntity() == actor` (unchanged) |
| `actor_input_hold/press/release/look(context, …)` | A participant's input context: the command goes to the actor the authorized `ActionContext` resolves (`Scope::Self`) through that actor's own `IR_*` handler; any other context moves nobody and is counted | the harness now, the client's intents in Stage 4 | — |
| `script_binding_allowed(object)` | Whether the object's section-declared Lua binder attaches | an actor's binder only for the single-player script actor (the ALife actor, `CALifeGraphRegistry::actor()`, the `M_SPAWN_OBJECT_ASPLAYER` entity); other participant actors get none, counted and logged | always |
| `assign_participant_actor(ref, engine_id)` (`CampaignActorAccess.h`) | The server names the participant an actor entity belongs to before the object exists; the adoption at `net_Spawn` then finds it held | explicit, refused when the id is not current, the reference stale, the participant already holding a live actor or another participant holding the entity | `WrongRole` |

**Converted control sites** (`controls_actor(this)`): the control branch of
`shedule_Update`; the sprint cut on a hit (`Actor.cpp:655`); the level-border
callback (`:1045`) and the contact-damage hit (`:1075`), which the
controlling side reports; the climb weapon-hide state
(`Actor_Movement.cpp:153`); the sprint-disable and weapon-hide game events
(`Actor_Weapon.cpp:150,161`). **View sites left on the view entity** (null on
the server): the explosion shock effector and heavy-breath sound, HUD FOV,
camera lookout, night-vision post-process, MP auto-pickup. The moving-camera
effector (`Actor_Movement.cpp:339`) is now the viewer's own actor
(`local_view_actor() == this`), and the single-player game-over sequence in
`CActor::Die` (`start_tutorial("game_over")`, a `CUISequencer` that grabs
input) runs for the viewer's own actor only: a participant's death on the
server is a gameplay outcome whose client presentation is S03-T05.

**Why the second actor gets no Lua binder yet.** The installed `actor`
section binds `bind_stalker.actor_init`; `actor_binder:__init` sets
`db.actor_binder`, `net_spawn` calls `db.add_actor` (`db.actor = obj`),
resets the weather manager and enables input, and `net_destroy` finalizes
the surge, psi-storm and weather managers and sets `db.actor = nil`
(`gamedata/scripts/bind_stalker.script`, `db.script`). Bound to a second
actor it would re-point the single-player globals at that actor and, on its
destruction, tear down services participant 1 still uses. That is the Lua
global-actor mutation D4 names and S03-T05 replaces with a participant
context; until then the binder attaches to the script actor only and the
refusal is counted (`binder_refused_actor`).

**The second actor's spawn packet.** The installed `se_actor.script`
(the `actor` section's server class, `class_registrator.script:24`) appends
a save marker and a flag to `cse_alife_creature_actor.STATE_Write` and
reads them back in `STATE_Read` only `if db.actor == nil`
(`se_actor.script:40-54`): in single player the one actor's packet is read
before its binder exists, so the gate never mattered. A second actor's
packet is read with `db.actor` set, its `STATE_Read` stops 3 bytes short,
and the update block that follows (`CSE_ALifeCreatureAbstract::UPDATE_Read`:
health, position) is parsed from the wrong offset: smoke-107/108 saw the
actor arrive with health 0 and a position of 10^29. `CSE_Abstract::Spawn_Read`
(`xrServerEntities/xrServer_Object_Base.cpp`) now resumes at the end of the
length-prefixed state frame it was already given, counts and logs the
mismatch (`g_spawn_state_resyncs`); the normal client is unchanged when the
sizes agree. The script gate itself is an S03-T05 input.

**The installed hit gate.** `CActor::Hit` asks `_G.CActor__BeforeHitCallback`
(`_g.script:1221`) before any actor takes damage; it ignores every hit for
7 s after the actor binder's first update (`bind_stalker_ext.script:74-76`,
one global window started by the single-player actor and applied to any
actor), refuses hits from stalkers friendly to `db.actor`, and lets the
`actor_on_before_hit` subscribers (`item_artefact.script`, `arszi_psy.script`)
scale the damage by `db.actor`'s artefacts. Its `IsStalker` (`_g.script:2838`)
includes `clsid.actor`, so a hit whose source is the other actor, or the
victim itself, is ignored as friendly fire against `db.actor` (smoke-116:
6 bridge events, 0 entity hits). The fixture sends its hits from an anomaly
zone, the engine's own non-creature hit source, after 9 s of server ticks;
the per-participant form of the gate (and the friendly-fire policy) is
S03-T05 / Stage 6.

**What is still single-player on the server** (recorded, not hidden): the god
mode flags `psActorFlags` (`AF_GODMODE`, `AF_GODMODE_RT`) are process-global
and apply to every actor (the fixture clears and restores them; a
per-participant form is Stage 6); the `Actor()` compatibility accessor
refuses while two participant actors live, so the presentation, script and
ability callers listed by `report_actor_access` get null then (S03-T04,
S03-T05, S10-T02).

## Activation and awareness — S03-T04

**Activation set.** One level stays active. On the campaign server the ALife
switch rules measure every object from the nearest **alive participant
actor**, not from the ALife actor: `CALifeSwitchManager::refresh_activation_anchors`
snapshots the participant actors' server entity positions at the start of
each ALife update, and `activation_distance` is the distance to the nearest
(`alife_switch_manager.cpp:60-89`; the five switch sites in
`alife_dynamic_object.cpp`, `alife_online_offline_group.cpp`,
`alife_group_abstract.cpp`). An object near either player stays online; an
object far from both goes offline; squads use their nearest member. The rules
are the same as the S02-T04c activity anchors (`CampaignServerStart.cpp:150`):

- a dead or released participant actor contributes nothing;
- a **detached** participant's retained actor still counts (its slot keeps
  the actor; releasing it on disconnect is S04-T05);
- with no alive participant actor the ALife actor is the anchor, the engine's
  own rule, so the world never loses its anchor;
- a client never changes: it reads the ALife actor live, as before.

The ALife actor itself (`graph().actor()`, the saved player entity) is
unchanged; level changes, travel and the surge still use it
(`alife_update_manager.cpp:181-235,360-368`, Stage 6).

**Awareness.** The monster rules that meant "the player" — a remembered
dangerous sound while the player watches, the player within feel distance
while watching — run over every participant actor
(`monster_enemy_memory.cpp:49-123`); the stalker's `LookAtActor`, the
bloodsucker predator check and the poltergeist's crow visibility likewise.
All other enemy selection was already generic (hit, sound and visual memory
filtered by `CEnemyManager::useful`). A server-driven participant's actor sees
along its camera, which keeps its last yaw until a client sends a view
(Stage 4).

**Player-bound abilities** (psy hit, auras, anti-aim, burer stamina hit,
poltergeist telekinesis and detection, bloodsucker vampire and alien vision)
are explicit no-ops while `Actor()` refuses; the table is in
[S03-T04](validation/S03-T04.md); their per-participant form is S10-T02.

**Measured cost** (k00_marsh, actors 1,098 m apart): 1,005 → 1,049 objects
online, object-update time 0.78 → 0.95 ms per tick; the rule itself is 1.3 ns
more per object per pass and 115 ns per refresh. The cost of a second player
is its area's objects.

**Server AI vision** has no light: the render object's luminosity is 0 on the
headless server, so AI sees as in darkness (S03-T04 Limits). Not a
participant effect; a server light model is an open item.

## Ownership table

| Thing | Owner (mints / mutates) | May resolve | Must never |
|---|---|---|---|
| `CampaignId`, `PlayerId`, `PartyId` | Server identity layer (S04-T04 for players; Stage 7 persists) | Server; clients receive their own | Be chosen or asserted by a client |
| `EntityId` | Registry, at engine-id assignment | Server; clients receive them in protocol (Stage 4) | Be reused within a campaign |
| `EngineEntityKey` | Registry | Server code holding references across frames | Be compared by `u16` alone |
| `ParticipantRef` | Registry | Server; a client receives its own | Be trusted as the actor of a client intent without the session check |
| Participant slot state | Registry under `ServerAuthority` | Anyone with the registry | Change hands without a generation bump |
| Bridge session | Registry (`set_bridge_session` from the live server) | — | Attach as a participant |
| `g_actor`, `Level().CurrentEntity()` | Engine | Client presentation only (`local_view_actor()`); the server's `Actor()` is the counted compatibility accessor (S03-T02) | Decide authority; choose a player when two exist |
| `Level().CurrentControlEntity()` | Engine | The client's own control path; the server answers `controls_actor` from the participant registry (S03-T03) | Decide which actor the server simulates by input |
| A participant's input | Its session (client intents, Stage 4) or the harness (`ServerInternal`) | `actor_input_*` under an authorized `Scope::Self` context | Reach another participant's actor |
| The single-player Lua binder (`bind_stalker.actor_init`) | The installed scripts, for the script actor only (S03-T03) | `db.actor` on the client and, on the server, for the ALife actor | Attach to a second participant's actor before S03-T05 |
| Server message dispatch | `xrServer::OnMessage` with the S03-T02 policy | The bridge client for state imports, world control and game events; remote clients for connection and input messages only | Accept a state import, world control or game event from a remote sender |

## Files

- `xrGame/CampaignIdentity.{h,cpp}` — the model and registry (no engine
  globals read; the only engine dependency is the `xrCore` types).
- `xrGame/CampaignIdentityHooks.h` — the five id events; `xrServer.h`
  `PerformIDgen(id, object)`/`FreeID`/`clear_ids`, the `xrServer` constructor
  and destructor call them; no-ops on the client, where `g_campaign_identity`
  is null until S03-T02. `CSE_Abstract::m_campaign_entity`
  (`xrServerEntities/xrServer_Object_Base.h`) carries the entity id.
- `xrGame/CampaignIdentityFixture.{h,cpp}` — the `identity` service of the
  campaign server (owns the process registry and binds fixture player 1
  before the level), the four unit fixtures
  (`Run-Stage2Smoke.ps1 -Scope Identity`), the bridge binding and the level
  identity check in the GameState smoke.
- `xrGame/CampaignActorAccess.{h,cpp}` (S03-T02) — the local view, participant
  and compatibility accessors, the `Actor()` caller audit, the client-role
  registry lifetime and the spawn adoption hook.
- `xrGame/CampaignMessagePolicy.{h,cpp}` (S03-T02) — the message and
  game-event classes, the bridge/remote policy and its counters.
- `xrGame/CampaignAuthorityFixture.cpp` (S03-T02) — the `actor-access` and
  `message-policy` unit fixtures and the level authority check
  (`campaign-authority-passed`).
- `xrGame/CampaignActorControl.{h,cpp}` (S03-T03) — `controls_actor`, the
  per-participant input context, the single-player binder rule and their
  counters; `assign_participant_actor` is in `CampaignActorAccess.{h,cpp}`.
- `xrGame/CampaignTwoActorFixture.cpp` (S03-T03) — the two-actor level
  fixture (`campaign-two-actors-passed`).
- `xrGame/alife_switch_manager.{h,cpp,_inline.h}` (S03-T04) — the activation
  anchors and `activation_distance`; `participant_actor_entities` is in
  `CampaignActorAccess.{h,cpp}`.
- `xrGame/CampaignActivationFixture.cpp` (S03-T04) — the activation and
  awareness level fixture (`campaign-activation-passed`).
