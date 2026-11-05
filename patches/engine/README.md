# Engine source ownership

The root repository owns `docs/source-manifest.json`, this file, `series`, and
the ordered patches. The ignored reference checkout is immutable investigation
evidence. `0001-campaign-server-startup.patch` adds the S02-T02 campaign role,
separate executable name, core/game-factory smoke path and conditional teardown.
`0002-campaign-core-smoke.patch` adds command-only console initialization,
persistent stage checks and explicit Core/GameState smoke scopes. GameState
reports the missing native level prerequisite; it is not a runtime pass.
`0003-campaign-services-smoke.patch` adds the server-owned CPU campaign services
(spatial DBs, script engine, character registries) with reverse-order owner
teardown, a Services smoke scope and a test-only `-campaign-fail-at` unwind check.
Its xrGame functions are additive; the client's game-globals functions are unchanged.
`0004-materials-without-media.patch` (S02-T03a) adds a material-library switch
that keeps pair media names but creates no pair sounds or wallmarks; it defaults
on, so the client is unchanged. The campaign server loads materials with it off.
`0005-cpu-only-models.patch` (S02-T03b) attaches the renderer module with no
device, creates only its model pool, skips and counts shader, geometry and GPU
buffer creation, and registers bone materials without the persistent object.
`0006-campaign-frame-driver.patch` (S02-T04a) adds `CCampaignServerClock`, the
server's tick-based clock and serial frame driver, with pause and bounded catch-up.
`0007-bone-capture.patch` (S02-T03c) adds the shared bone capture used by the
campaign smoke and by the client's opt-in `-s02-bone-capture` flag, and stops
view-based bone throttling in a CPU-only process.
`0008-persistent-without-device.patch` (S02-T05a) constructs the real game
persistent object with no device: shader/texture creation and resource
destructors are skipped in CPU-only mode, environment ambience sounds are
skipped without a sound manager, and the campaign branch of CGamePersistent
skips UI, menu, fonts and prefetch.
`0009-campaign-level-start.patch` (S02-T05b/c) constructs CApplication, CLevel,
xrServer, game_sv_Single and ALife from the fixture save with no device: loading
stages are pumped explicitly, the particle library loads on the CPU, the engine's
-nosound manager stands in for sound, and the presentation calls on that path are
guarded (render-resource dump, camera reset, input release, logo, wallmarks).
`0010-campaign-level-load.patch` (S02-T05d) runs the client loading stages and
the first server ticks with no device: CFORM, physics world, client game state
and level scripts load for real while the HUD, render-level load, texture upload
and precache are skipped; spawns stay inline; the campaign server adopts no
control or view entity (including the stalker fallback); map locations keep
their data without spot widgets; the actor stores its PDA task filters; HUD-pick
users and light shader creation are guarded.
`0011-collision-capture.patch` (S02-T03d) adds the shared level collision
capture (`xrGame/CampaignCollisionCapture.{h,cpp}`): skeleton collision forms,
their elements, character controllers, physics shells and object-space ray picks
for every object on the loaded level, written as CSV by the campaign server after
its first ticks (stage `campaign-collision-passed`) and by the normal client with
the opt-in `-s02-collision-capture` flag one second after the actor spawned. The
native spawn already builds all of that without the renderer; no engine
behaviour changes.
`0012-physics-fixture.patch` (S02-T04b) adds the shared falling-body physics
fixture (`xrGame/CampaignPhysicsCapture.{h,cpp}`): one existing physics shell
is lifted a metre and released, and every native physics step is sampled
through the world's step-time callback (the level's callback keeps running)
and an added contact callback, by the campaign server after its collision
capture (stage `campaign-physics-passed`) and by the normal client with the
opt-in `-s02-physics-capture` flag. The engine's stepping, gravity, per-step
velocity scale, contacts and sleeping are observed, not changed.
`0013-activity-policy.patch` (S02-T04c) adds the server activity policy
(`SActivityPolicy g_activity_policy`, `xrEngine/xr_object.{h,cpp}`): while the
campaign level runs, crow selection and the scheduling scale use the distance
to the nearest activity anchor (one per online actor, refreshed every frame
from `CLevel::OnFrame`) instead of the camera; disabled, the client is
unchanged. `CSheduler::dwUpdated` counts dispatches per frame. The shared
activity fixture (`xrGame/CampaignActivityCapture.{h,cpp}`) observes the object
list, the scheduler, a stalker's planner, ALife switching, offline scheduling
and the ALife clock, then walks the stalker through the native script-control
path (stage `campaign-activity-passed`, client flag `-s02-activity-capture`).
`0014-cpu-sound-events.patch` (S02-T05c) adds the CPU sound core
(`xrSound/SoundRender_CoreCPU.{h,cpp}`, `-snd_cpu`): the native sound core with
no OpenAL device and no listener, so sources, emitters and AI sound events run
on the server and 2D events are dropped rather than placed at a fabricated
listener; `has_listener()` skips the listener-distance start delay. Material
pairs create their sounds on the server (`SetPairSounds`/`SetPairMarks`), the
footstep and impact range gates use `activity_range_distance()` instead of the
camera, the campaign clock pauses emitters, and hearing counters
(`IGame_Level::snd_Stats`/`snd_Trace`, `CSoundMemoryManager::s_feel_calls`)
feed the shared hearing fixture (`xrGame/CampaignHearingCapture.{h,cpp}`, stage
`campaign-hearing-passed`, client flag `-s02-hearing-capture`).
`0015-actor-binder-diagnostics.patch` (S02-T06) makes the script binder's
exception handling visible: xrGame has no C++ exceptions, so its `catch (...)`
blocks caught native faults and deleted the Lua binder silently; the scheduled
update now uses an SEH filter that logs the fault with symbols (dbghelp) and
the other catches log the object and phase. `ActorMenu.get_*` and
`grab_menu_mode` return nil / `mmUndefined` with no game UI instead of
dereferencing null; `CActor::s_schedule_updates` counts actor scheduled
updates; the campaign smoke's tick report prints the actor's scheduling and
Lua binder state (`S02-T06 actor …`). The client's behaviour is unchanged.
`0016-inert-server-game-ui.patch` (S02-T06a, decision B20) gives the campaign
server the UI core, fonts and the real game UI (`CUIGameSP` with its actor
menu, PDA, in-game window and messages) as inert CPU objects: created by the
native path with GPU resources skipped and counted, laid out at the UI base
resolution (`Device.dwWidth/dwHeight` 1024×768 when no window set them), never
updated (`CHUDManager::OnFrame` returns on the server; the game UI joins no
frame sequence) and never drawn. The HUD manager is created on the server too.
The smoke's loaded report expects `hud=1 game_ui=1 ui_in_frame=0`, and its
tick report prints the script binder update time and the level frame's pump
times (`SLevelFrameTiming`, measured on both products). The client is unchanged.
`0017-alife-actor-position-check.patch` (S02-T06b) adds the smoke's ALife
actor position check: the actor is teleported 3 m as the script API does, 50
ticks run, and the live and ALife positions are compared (stage
`campaign-actor-position-passed`). Server smoke only; no behaviour changes.
`0018-combat-fixture.patch` (S02-T06c) adds the shared combat fixture
(`xrGame/CampaignCombatCapture.{h,cpp}`): one monster is spawned through the
native ALife spawn path near the fixture stalker, the switch manager brings it
online, the actor's god mode is set for the fixture's duration, and the fight
is observed through new observation hooks (every bullet the bullet manager
adds, every hit event it commits, every `CEntityAlive::Hit` and `Die`) plus
the stalker's visual memory and enemy selection and the monster's enemy
manager; by the campaign server after its actor position check (stage
`campaign-combat-passed`) and by the normal client with the opt-in
`-s02-combat-capture` flag. The hooks only count and record; no combat
behaviour changes.
`0019-checkpoint-round-trip.patch` (S02-T06d) adds the native checkpoint
round-trip (`xrGame/CampaignCheckpoint.{h,cpp}`): the campaign server sends
the `M_SAVE_GAME` packet the console `save` command and the autosave manager
send (handled synchronously by the in-memory server: `game_sv_Single::save_game`
→ `CALifeStorageManager::save`, the Lua storage-manager callbacks and the
`.scop`), checks the written `.scop`/`.scoc` against the live state, stops the
level and starts it again from the checkpoint the way the engine reconnects
(stages `campaign-checkpoint-saved`, `campaign-restart-server-ready`,
`campaign-restart-level-loaded`, `campaign-checkpoint-passed`), comparing the
ALife registries, clock, actor and fixture NPCs with the state the checkpoint
was written from. The normal client takes the same snapshot from a server
checkpoint with the opt-in `-s02-checkpoint-capture` flag. Observation only;
no save, load or spawn behaviour changes.
`0020-quest-round-trip.patch` (S02-T06e) adds the quest round-trip
(`xrGame/CampaignQuestCapture.{h,cpp}`): the campaign server moves the actor
beside the Clear Sky mechanic, uses him (the engine use path and the stalker
binder's use callback), starts the talk state, offers and accepts a fetch task
through the installed dialogue functions, lets the task manager run, carries
the given task through the checkpoint round-trip, spawns the fetch item the way
`alife():create` does, turns the task in through the dialogue functions and
carries the completed task, its info portion, money and goodwill through a
second round-trip (stages `campaign-quest-given`, `campaign-quest-restored`,
`campaign-quest-completed`, `campaign-quest-checkpoint-*`,
`campaign-quest-persisted`); the normal client does the same from the server's
checkpoints with `-s02-quest-capture` and `-s02-quest-persisted`. Observation
counters in `CGameTaskManager::UpdateTasks` and `CGameTask::UpdateState`. The
checkpoint check names level-registry objects the reload holds under their
group. One behaviour change: the psy dog's aura (`psy_dog_aura.cpp`) no longer
dereferences `Level().CurrentEntity()` when there is none (blocker B21); the
aura is a camera post-process and is skipped with no controlled entity.
`0021-participant-identity.patch` (S03-T01) adds the participant identity
model and registry (`xrGame/CampaignIdentity.{h,cpp}`): 128-bit campaign,
player and party ids, 64-bit entity ids (items are entities), engine-id
references `{u16, generation}` that go stale when the server recycles the
u16, two participant slots as policy with slot generations, session
attach/detach with identity retention, a server-only authority token and
action-context authorization. `xrServer::PerformIDgen`, `FreeID` and
`clear_ids` and the `xrServer` constructor report to the registry through
`xrGame/CampaignIdentityHooks.h` (no-ops on the client, where no registry
exists yet); `xrServer::entity_Destroy` frees through `FreeID`. The campaign
server gets an `identity` service, an `Identity` smoke scope with four unit
fixtures (`xrGame/CampaignIdentityFixture.{h,cpp}`), the bridge-session
binding after the server is ready and a level identity check after the first
ticks (stage `campaign-identity-passed`). No gameplay behaviour changes.
Export later patches with `python3 tools/export-engine-patch.py <name> <check-number>`.

`0022-actor-access-authority.patch` (S03-T02) separates local view access
from gameplay authority. `xrGame/CampaignActorAccess.{h,cpp}` adds
`local_view_actor()` (the client's own actor, null on the server), the
participant lookups (`participant_actor`, `participant_of`,
`participant_actors`, `nearest_participant_actor`, `context_actor`,
`dialog_actor`), `sole_participant_actor(purpose)` and the `Actor()`
compatibility accessor, which on the server returns the one participant
actor, refuses with none or two, and records every caller; a client gets a
`Role::Client` registry with its `CLevel`; every actor spawned on the server
adopts a participant. The registry gains the actor lookups and adoption.
`xrGame/CampaignMessagePolicy.{h,cpp}` classifies every server message and
game event; `xrServer::OnMessage`, `Process_event`, `Process_update` and
`Process_save` refuse state imports, world control and game events from any
sender but the direct in-process client. Call sites in 54 engine files
use the explicit lookups (70 files in the patch with the five new ones and the wiring) (the phantom, trader and psy dog aura B21
sites among them); the inventory is `docs/actor-access-inventory.md`.
`xrGame/CampaignAuthorityFixture.cpp` adds the `actor-access` and
`message-policy` unit fixtures and the level authority check
(`campaign-authority-passed`).

`0023-two-independent-actors.patch` (S03-T03) adds per-participant actor
control. `xrGame/CampaignActorControl.{h,cpp}`: `controls_actor` (the
server runs the control path of every participant actor; a client its
local control entity), the participant input context (`actor_input_*`
under an authorized `Scope::Self` context, forwarded to the actor's own
`IR_*` handlers), the single-player binder rule (`script_binding_allowed`:
on the server the installed `actor` binder attaches to the ALife actor only)
and their counters; `assign_participant_actor` names an entity's participant
before it spawns. `CActor::shedule_Update` and six control sites use
`controls_actor`; the moving-camera effector and the game-over sequence are
the viewer's own. The `level.*` camera and post-process bindings use the
local view actor, the remaining `level.*`/game-object bindings that
dereferenced `Actor()` are null-guarded. `CSE_Abstract::Spawn_Read` resumes
at the end of the length-prefixed state frame when a `STATE_Read` consumed a
different number of bytes (the installed `se_actor.script` skips its bytes
once `db.actor` exists), counted and logged. `xrGame/CampaignTwoActorFixture.cpp`
is the level fixture (stage `campaign-two-actors-passed`).

Reconstruct in a new, absent workspace directory (never overwrite an existing tree):

```sh
git clone --no-hardlinks engine/xray-monolith builds/engine-work
git -C builds/engine-work checkout --detach c57fe5b9fab4fe0506659db03137c52d2500eb2f
git -C builds/engine-work submodule update --init --recursive
```

Verify HEAD and submodule SHAs against the manifest before building. The submodule
download requires network access. This procedure was executed on 2026-10-15:
the independent working checkout began clean at the pinned revision and Optick
was initialized at its pinned gitlink. Stage 2 source edits are now exported by
the patch series. The reference checkout remains unchanged.
The source commit also pins bundled SDK inputs; record hashes of externally
installed compiler/SDK/dependency inputs when S01-T01 resolves them.

Apply patches in `series` order with `git apply --check`, then `git apply`, from
the working tree. Use relative patch filenames, one per line. Before ending each
editing session, export reviewed source changes as ordered text patches here,
including newly created files; verify reconstruction in a fresh scratch clone.
Never rely on ignored working-tree edits as the only copy. Update the manifest
and task report with patch names and SHA-256 hashes in the same root change.
Do not export proprietary assets or generated outputs. Revert by reconstructing
the pinned source with only the preceding patches in a new directory, preserving
the old tree until its changes are accounted for.

`0024-participant-activation-wip.patch` (S03-T04, work in progress, not yet
built) adds the ALife activation set: on the campaign server the five switch
rules (`CSE_ALifeDynamicObject::try_switch_online/offline`, the online-offline
groups, the monster groups) measure from the nearest alive participant actor
(`CALifeSwitchManager::activation_distance`, refreshed once per ALife
update) instead of the one ALife actor; a client still reads the ALife actor.
The monster enemy memory and the stalker look-at run over every participant,
and the player-bound abilities that dereferenced `Actor()` on reachable paths
(anti-aim, controller psy hit, burer stamina hit, poltergeist telekinesis,
monster aura strength) are explicit no-ops while it refuses. To be re-exported
under its final name once built and validated.

`0042-checkpoint-generations-and-journal.patch` (S07-T02/T03/T04/T05) adds the
campaign server's persistence (`xrGame/CoopPersistence.{h,cpp}`, the
project-owned `coop_store` library linked through `Coop.props`): recovery at
`xrCoopServer_Prepare` (the store's pointer, generation verification with
fallback to G−1, journal reading, the durable `epoch` record before anything
else), identity restore in the registry (`restore_entities`: the saved
u16 → EntityId map consumed by the load's `PerformIDgen`; `restore_participant`:
saved slots and slot generations) and the new `xrCoopServer_AfterServerReady`
hook in `x_ray.cpp` that assigns each saved actor to its participant by its
saved id (stage `coop-server-recovered`); replay of committed records at
`xrCoopServer_Start`; checkpoint generations published at every join, every
`checkpoint_seconds` and at a clean stop (`publish_generation`: the native save
under the store's temporary name plus the supplemental record); the synthetic
`ledger` action (kind 3) through the six transaction phases with the operation
index and fail-closed reason 5; `coop-ledger.csv` and `coop-checkpoints.csv`;
profile keys `checkpoint_seconds`, `failpoint`, `failpoint_count`. The client
gets the ledger program (profile `script = 6`) and, on a connection end, leaves
its stale level and reconnects with backoff (S07-T05). The Stage 2 tick check
accepts more than one actor on a serving server. S07-T06 adds the `death`
record: every `CEntityAlive::Die` of an ALife object on the serving server is
committed in its tick (`coop-deaths.csv`) and re-applied at recovery by
killing the entity natively when the checkpoint still holds it alive
(`apply_death`); profile key `failpoint_fail` makes an armed failpoint fail
the store call instead of terminating the process (the fail-closed test).
S07-T07 adds the respawn: `ActionIntent` kind 4 validated on the server (a
living actor is refused), the `respawn` record with a reserved `EntityId`
committed before the spawn, the slot rebound to the new actor (the corpse
stays), `CampaignEvent` 5 telling the client to rejoin, the client's
prompt through the installed `actor_menu.set_msg` and the use key
(`CLevel::IR_OnKeyboardPress` asks `coop_client_key_press` first), the
`auto_respawn` driver key; under the G07 decisions the respawn point is the
participant's level entry point (recorded where its actor is first seen,
kept in supplemental schema 2) and the new actor gets the faction kit
(`items\settings\new_game_loadouts.ltx`, the always-given lines of
`[<faction>_loadout]` plus a weapon's ammo) with the dead actor's community,
name, rank, reputation, profile and money copied onto its server entity.
S07-T08 moves publication off the simulation thread: `CALifeStorageManager::save`
hands the serialized world to `g_alife_save_sink` during a take (and
reports its phase timings on the campaign server), `CoopPersistence` runs
compression, the `.scop` write and the store's steps on a worker thread
(`publish(…, nullptr)`: the journal writer stays on the previous file until
`poll()` switches it; the store's recovery reads that journal too), joins
wait in `WorldPublishing` for the generation they requested, and the clean
stop waits for the worker. `xrCampaignLevel_ReportTicks` accepts a dead
participant actor on a serving server. No gameplay behaviour changes outside
co-op.

`0043-remote-lean-and-hud-ownership.patch` (G07 play test, 2026-10-23) fixes
two presentation defects on a co-op client: the other player's lean
(`CActor::replica_present` computes the remote body's torso roll from the
server's lean bits with the controlled actor's angle and inertia; K5) and
the other player's weapons attaching to this player's first-person hands
(K1: `CHudItem::SwitchState` and `OnEvent` take the show/hide-only path for
an item whose actor parent is not `local_view_actor()`, which the client's
in-process server owns as a local object; `ParentIsActor()` means the local
player on a co-op client). The client logs `remote-lean` while the other
player leans. No single-player behaviour changes.

`0044-remote-hud-motion-guard-and-camera-watch.patch` (G07 play test,
2026-10-23, **unverified by the user yet**): `CHudItem::PlayHUDMotion` and
`PlayHUDMotion_noCB` play nothing for the other player's item on a co-op
client (the residual K1 sway: the hands followed the other weapon's HUD
motion track), logging `S07 K1 remote-hud-motion-blocked`; the client logs
its own camera per frame for 2 s after the other player's weapon change
(`coop-camera-client.csv`, `remote-slot-change`), which showed in cam-1 that
the camera itself did not move. No single-player behaviour changes.

`0054-travel-consent.patch` (S09-T01, 2026-10-27): party travel request and
two-player consent (docs/travel-contract.md): ActionIntent kinds 11 (travel
request at a level changer) and 12 (answer) on the server with the
`coop::proto::TransitionCoordinator` state machine (30-s consent, abort on a
party member's disconnect, death, combat or the changer's loss), CampaignEvent
13 to both players; on a co-op client a level changer touch is a prompt (USE
asks the partner, DROP stays; the partner's USE/DROP answers; DROP cancels),
nothing loads (an agreed transition ends as the S09-T02 hand-over); the
travel fixture (`travel_party`, `travel_variant`) and the step driver's
actions 11/12. Single-player level changers are unchanged.

`0055-travel-execution.patch` (S09-T02, 2026-10-28): the agreed travel is
carried out (docs/travel-contract.md §9): the barrier (inputs ignored, actions
refused with reason 30), the `transition` journal record (kind 13, phases
prepared / committed / rolled back), both participant actors' ALife entities
moved to the changer's destination as `CALifeUpdateManager::change_level`
moves one, the destination generation taken through the ALife save (not
`M_SAVE_GAME`) and published as the commit point, the level reloaded in
process by the serve loop (`x_ray.cpp`: level and identity registry stopped
and restarted from the destination generation, `CoopPersistence::load_generation`,
the saves folder re-indexed) while the clients leave and rejoin (CampaignEvent
13 state 8); recovery logs the record's verdict. Single player is unchanged.

`0056-travel-resume.patch` (S09-T03, 2026-10-28): the coordinated resume
(docs/travel-contract.md §10): after the reload the destination world is held
(each player's input consumed and discarded, actions refused 30) until every
client's ready (ActionIntent 13: transition id and level; refused 31 for
another transition or level, 32 before the server sees that world live)
or the 60-s deadline; CampaignEvent 13 state 9 `resumed`; the committed event
carries the destination generation; the drivers' `travel_ready_delay` and
`travel_ready_wrong` knobs. Single player is unchanged.
`0057-travel-failures.patch` (S09-T04, 2026-10-28): the failure matrix
(docs/travel-contract.md §11): a barrier rollback names who left or died;
the `held` notice (CampaignEvent 13 state 10) to a client in the world during
the hold that was not told the transition (a new session is told afresh);
a right ready refused with no hold ends the client's wait; a new authority
epoch voids a client's hold; failpoints `travel:reloading` and
`travel:after-ready`; harness knobs `travel_barrier_hold` (server),
`travel_rejoin_delay` and `travel_load_fail` (client); fixture variants 3
and 4; the step driver's connection drop on a travel answer waits for the
answer's result. Single player is unchanged.

`0058-talk-window.patch` (S10-T04a, 2026-10-29): a co-op client's talk window
drives the server's dialogue (docs/validation/S10-T04a.md): ActionIntent 14
talk start / 15 talk say / 16 talk end (reasons 33 busy, 34 not talking, 35
wrong phrase, 36 no talk), CampaignEvent 14 (open, answer, list, closed); the
server runs CUITalkWnd's logic in the player's script context with one
speaker per NPC (T01-15); hooks in `CDialogScriptHelper::Action` and
`CAI_PhraseDialogManager::AnswerPhrase` route the party quest's accept and
turn-in phrases through the S08 party path (`party_task_accepted` factored
out of `take_quest`) and the NPC's replies to the player; a talking actor's
inputs are dropped; the client window's remote mode (trade and upgrade off);
the talk fixture (`talk_party`, `talk_variant`) and the driver's actions 14–16. Single player
is unchanged (the hooks return false off the campaign server).

`0059-party-info-portions.patch` (S10-T04b, 2026-10-29): party info portions
and the script state of an actor without the installed binder
(docs/validation/S10-T04b.md): a story portion given to or taken from one
participant actor is mirrored to the other participant actors
(`CActor::OnReceiveInfo` / `OnDisableInfo` → `coop_server_info_changed`;
the personal list `ui_*`, `*tutorial*`, the sleep flags,
`start_equippment_handled` stays per actor); `alife():has_info(0, ...)`
reads the context's player (`campaign::info_actor_id`); participant 2's
actor and a respawned body get an `alife_storage_manager` entry so their
`pstor` is saved with the checkpoint and restored (`actor_state_tick`); the
info fixture (`info_party`). Single player is unchanged.

`0060-party-tasks-general.patch` (S10-T05b, replacing the work-in-progress
export of 2026-10-29 in place): every task the installed task ini knows is
the party's (`party_task_supported` asks `task_manager.task_ini`; the S08
allow list is gone; `rebuild_party_tasks` registers every in-process native
task after a restart; `party_accept_task` / `party_turn_in_task` shared by
the talk window and the fixture); the adapter resolves the general reward
paths while a party turn-in is pending (`dialogs.relocate_money` "in",
`dialogs.relocate_item_section` "in", `xr_effects.reward_item`,
`CGeneralTask.give_reward` as the completion marker) and the server applies
them to every participant actor (`apply_party_effects` item gives, the same
sum each); a task scope sees the party's inventory
(`campaign::party_inventory_view`; `CScriptGameObject::GetObjectByName` and
`IterateInventory` on a participant actor also see the other participant
actors' items); the death and hit callbacks run in the participant
attacker's context (`CEntityAlive::Die`, `CHitMemoryManager::add`); the
fixture `task_party` and `CoopConfig` `task_party`. Single player is
unchanged.

`0061-demo-place-giver.patch` (a hands-on aid for the S10-T04a talk check,
2026-10-29): the server setting `place_giver = N` (`CoopConfig`,
`prepare-coop-session.py --place-giver`, `start-coop-demo.sh`'s fifth
argument) places both participant actors in front of Sidorovich once, N
seconds after both players are in the world (`demo_place_tick`), because a
client's USE on his bunker door has no co-op path yet (S10-T04c). No
gameplay change; off by default; single player is unchanged.

`0062-party-task-map-npc-sounds.patch` (after the S10-T04a hands-on check,
2026-10-29): the party-task event (CampaignEvent 11) is resent when a
task's map location or map object changes (the installed `check_task` sets
the target after the give) and a co-op client relocates its presentation
copy (`CGameTask::ChangeMapLocation`), so the PDA shows the task's spot and
centres on it; a script sound played on a living NPC or monster
(`CScriptSound::Play` / `PlayAtPos`, e.g. Sidorovich's `xr_sound` greeting)
is delivered to the players as a sound at its position (L05 narrowed).
Single player is unchanged.

`0063-use-object-money.patch` (S10-T04c, first slice, 2026-10-29): a
client's USE on a script-usable world object (a door, a switch; not an
item, a living thing or a level changer) is the use-object intent (kind 17,
protocol v1.7 / `proto-build-50`): `take_use_object` runs the object's use
callback on the server in the player's context (`CUsableScriptObject::use`),
so the installed `ph_door` logic decides; a scripted joint drive
(`cphysics_joint_scripted::SetForceAndVelocity`, the door's swing) is
presented to every client (`Presentation::JointDrive`), which applies it to
the same joint of its copy; the per-player needs event (12) carries the
player's money, which the client applies to its actor (the HUD and the
trade window's purse). Single player is unchanged.

`0064-trade.patch` (S10-T04c, the trade slice, 2026-10-29; protocol v1.7
`proto-build-51`: kinds 18 trade open, 19 trade, 20 trade close, reasons
37–39, journal record 14 `trade`): the talk window's Trade button in a
co-op talk asks the server to open the trade; the server starts the engine's
trade session for the buyer in his context (`CTrade::StartTradeEx`,
`StartTrading`, the installed `trade_manager.update` and
`ActorMenu_on_trade_started` hooks), prices the stock and the buyer's items
with `CTrade::GetItemPrice` and sends the table as CampaignEvent 15; the
client opens the native `CUIActorMenu` trade mode and `CTrade::GetItemPrice`
on a co-op client reads the table; buy and sell go to `take_trade`, which
re-prices, checks and journals one `trade` record (`TradeRecord`:
transfer records plus both purses) before `CTrade::TransferItem` applies
it; the clients rebuild the window's lists from the Reparent events
(`CUIActorMenu::CoopTradeRefresh`); recovery replays the record. Also: the
party task's map spot is placed only on an object the client holds and
`CMapLocation::UpdateSpot` tolerates a missing owner (demo-1 crash); remote
weapon-switch diagnostics (`remote-slot-applied` / `remote-slot-watch`);
the door and trade fixture `trade_party` and `compare-coop-trade.py`.
Single player is unchanged.

`0065-remote-weapon-switch.patch` (after the S10-T04c hands-on check,
2026-10-29; the K1 follow-up): on a co-op client the other player's weapon
switch is instant presentation. `CHudItem::OnStateSwitch` maps a showing
or hiding state of another player's item to shown (eIdle) or hidden and
sets its visibility, and `CHudItem::SendHiddenItem` hides such an item at
once instead of sending the state event (whose motion is blocked for
other players' items, K1), so `CInventory::Update` on the replica actor
completes the switch and the next weapon shows (trade-2 diagnostics: the
partner's knife stayed in eHiding and its pistol never appeared). Single
player is unchanged.
`0066-remote-weapon-anim-repair-belt.patch` (S10-T04c, the K1 follow-up 2 and
the repair and belt slices) stores the server's sampled weapon state on a
replica actor (`CActor::replica_set_weapon_state`) and reads it in the
third-person animation picker alone (`ActorAnimation.cpp`), so the other
player's swing, reload, draw and holster play while its weapon object stays in
K1's idle; the combat program gets a knife leg. The inventory layout event
(kind 9) carries a condition byte per item, applied by the client; the trade
fixture wears and repairs a weapon and puts an artefact on the belt of an
outfit with artefact slots (L48). Single player is unchanged.
`0067-upgrades.patch` (S10-T04f) adds upgrades and repairs at a mechanic: the
server takes the dialogue phrases `dialogs.npc_is_tech` / `npc_is_trader` over
and opens the player's upgrade or trade session, sends the upgrade table
(CampaignEvent 16), checks and journals an install or a repair (kind 22, the
`upgrade` record), applies it through the engine's own path in the player's
context and tells every client (CampaignEvent 17); a mechanic is the engine's
flag or a section of `misc\stalkers_upgrade_info.ltx`. On a co-op client the
installed Lua windows' script exports (`install_upgrade`, `set_condition`,
`transfer_item`, `give_money`) become the intents while a trade or upgrade
session is open, and the Lua window is nudged after a server-made change; the
engine's own upgrade window reads the table too. The upgrade fixture
(`upgrade_party`). Single player is unchanged.
`0068-relations.patch` (S10-T04d) maps a character id of 0 in the Lua goodwill
exports to the context's player (server) or the client's own actor (client),
and feeds each co-op client's relation registry from the server (CampaignEvent
18: the actor's community goodwill, the faction relation table and changes,
the actor's reputation; hooks in the registry's setters and SetReputation).
The relations fixture (`relations_party`). Single player is unchanged.
`0069-stashes.patch` (S10-T04e) lets a client's USE on an inventory box reach the
server (the treasure manager spawns a stash's contents there) while the client's
own loot window opens; fires the box's take callback on the server in the
taker's context for a participant's take; publishes a checkpoint after a stash's
contents appear. The stash fixture (`stash_party`). Single player is unchanged.
`0070-npc-logic.patch` (S10-T02a) makes the installed scripts' `db.actor` and
`AC_ID` mean the nearest alive participant while the campaign server runs an
NPC's script binder update (`campaign::NpcEvaluationScope` in
`CScriptBinder::shedule_Update`; the Lua exports `campaign.npc_actor` and
`npc_actor_id`); a participant context keeps its own actor. The NPC logic
fixture (`npc_party`). Single player is unchanged.
`0071-npc-loot.patch` (S10-T02b) keeps a dead stalker's or monster's body from
the installed release manager's ALife release while the nearest participant is
within 75 m (`CALifeSimulator__release`), and adds the NPC loot fixture
(`loot_party`). Single player is unchanged.
`0072-controller-psy.patch` (S10-T02c) makes the controller's tube attack and
control hit target the controller's participant enemy on the campaign server
and present their camera, HUD and sound effects to that player's client (a new
presentation kind, `ControllerPsyHit`, applied by the client with the
controller replica's own sounds and particles); logs each viewer's aura
post-process; adds the psy fixture (`psy_party`). Single player is unchanged.
`0073-mutant-abilities.patch` (S10-T02d) makes the bloodsucker's vampire grab,
the anti-aim (with the burer's stamina hit and shield) and the poltergeist's
detection and telekinesis target the monster's participant enemy (the
nearest participant for the poltergeist) on the campaign server; the vampire's
HUD, inventory and camera effects go to the grabbed player's client (a new
presentation kind, `MonsterEffect`), the anti-aim's camera kick through the
existing camera-effector presentation; the poltergeist's detection
post-process runs on each client for its own actor. The mutant fixture
(`mutant_party`). Single player is unchanged.

`0074-fauna.patch` (S10-T02e) presents the pseudogigant's threaten kick to
the kicked player's client (`MonsterEffect` phase 4), drives the psy dog's
aura on co-op clients, fixes a client crash when a psy dog phantom's spawn
fails (its particle names were uninitialized), logs which part of a custom
monster's spawn refused, and adds the fauna fixture (`fauna_party`). Single
player is unchanged.

`0075-companions.patch` (S10-T02f) gives each companion to the player who
recruited it: a companion registry (`campaign.set_companion`,
`clear_companion`, `companion_slot`, `slot_actor_id`) filled by compat-layer
wrappers of `axr_companions.add_to_actor_squad` / `remove_from_actor_squad`
(saved in the NPC's script vars), its squad's target and its `db.actor` /
story `actor` answer for the recruiter, the companion command dialogue is
refused to the other player, and a recruit asks for a checkpoint. The NPC
evaluation scope (S10-T02a) now also covers a stalker's and a monster's
planner (`Think`, `ProcessScripts`), where scheme actions such as `axr_beh`'s
walk run. The companion fixture (`companion_party`). Single player is
unchanged.

`0076-alife-respawn.patch` (S10-T03a) makes the installed ALife scripts'
distance to "the actor" the nearest participant's on the campaign server
(`campaign.nearest_participant_distance`): a smart terrain's respawn guard
(`se_smart_terrain.try_respawn`) and a squad's `dist_to_actor`
(`sim_squad_scripted.check_invulnerability` wrap), so no squad respawns
beside player 2; and adds the ALife identity fixture (`alife_party`).
Single player is unchanged.

`0077-squad-targets.patch` (S10-T03b) sends a squad whose script target is
"the actor" (bounty hunters, task squads: `sim_squad_scripted.get_script_target`
returns 0) at the nearest player instead of actor 0
(`campaign.nearest_participant_id`, the existing compat-layer wrap), and adds
the squad-target fixture (`squad_party`). Single player is unchanged.

`0078-sleep.patch` (S10-T03c) makes sleeping a party consent: the client's
installed sleep window asks the server (ActionIntent 24) and the partner
answers with USE or DROP (25) under the travel consent's rules; agreed, the
server skips the world's time once and rests both players (CampaignEvent 19
presents it); passing out (`ui_sleep_dialog.sleep_forced`, wrapped on the
server) is that player's blackout with no time skip. A client logs every
clock jump over 10 minutes. The sleep fixture (`sleep_party`). Single player
is unchanged.

`0079-relocate.patch` (S10-T03d) makes guides, fast travel and the backpack
party travel to a point: a script's time skip is held (`change_game_time`
gate: noted on a client, held for the tick on the server), a guide's
`ChangeLevel` in a player's context (server) and a client's `ChangeLevel` or
own-actor `set_actor_position` (ActionIntent 26) become a travel proposal
without a changer, carried out by the S09 executor (same level too); the trip's
time skips once when agreed and a guide's fee is refunded otherwise. The
relocation fixture (`relocate_party`) and its client driver (`relocate_test`).
Single player is unchanged.

`0080-passage-sweep.patch` (S10-T03e) adds the passage sweep fixture
(`sweep_party`): the server reads every level changer of the spawn data and
travels the party through each passage by the S09 consent path and executor
(its own requests skip the changer and combat checks, god mode on). It guards
a monster's squad cleanup when its spawn failed (a psy dog phantom on a
client), logs whether a refused monster's replica is in the client's ALife,
skips destroy-pending objects in the identity check and retries the check
after 10 more ticks up to 3 times, and reads the level's objects in the stop
status only while there is a level. Single player is unchanged.

`0081-new-game.patch` (S10-T03f) starts a new game on the campaign server
when the profile asks (`new_game = 1`) and the campaign has no generation
yet: the level starts from the spawn data (`all/single/alife/new`), the
installed `on_game_load` applies the creator's choices from the server's
`axr_options.ltx`, and the new-game script's `ChangeLevel` to the start
(in a player's context or as `M_CHANGE_LEVEL`) is carried out as an agreed
party travel by the S09 executor. Player 2's actor, created when it joins,
takes the creator's community, its own name and money, and its loadout from
the profile (`new_game_loadout_2`) or the faction's kit. Single player is
unchanged.

`0082-emission-per-player.patch` (S10-T05a) makes emissions and psi-storms
each player's own: the compat layer runs the installed surge update once for
the world (muted, its cover check answered "in cover") and once per player in
that player's context with its own stage flags, waves and the surge file's
timers (`campaign.enter_slot`, `campaign.mute`); a psi-storm's vortices come
down around each player in turn and hit each player near one by that player's
cover and protection. A player rejoining during a surge gets its current
phase. The emission fixture (`emission_party`, `emission_variant`). Single
player is unchanged.

`0083-story-chains.patch` (S10-T05c) keeps the DoctorX storyline's state the
party's: the compat layer's `save_var` / `load_var` keep a `drx_sl_*` variable
of a participant actor on the host actor whoever's context writes or reads it,
and each change asks for a checkpoint (`story_tick`). The story fixture
(`story_party`). Single player is unchanged.

`0088-bug-pass-2.patch` (S10-T07 part 2, 2026-11-01) fixes the rows the user
answered: a task's or script map spot on an object a client does not hold
stands at the server's place (`CMapLocation::SetFixedPosition`, the place
sent with CampaignEvent 11 and every map-spot presentation); a dead player's
map icon goes at death; the partner's zoom (input button bit 5 → actor
state bit 15 → the aim pose and walking) and a stalker's or monster's
animation state in the creature's state bits (`CAI_Stalker::
replica_present_animation`, a monster's override motion); the controller's
hold on the player during its psy glide; one input per tick period and a
record at the tick's time, a held snapshot skipped (B21); a jump committed
only when the physics can take it and latched on the server (B22); a zone's
state as presentation on a client (`GE_ZONE_STATE_CHANGE` allowed through
the replica, `ZoneState` presentation, the states sent at a join: every
campfire burned on every client before, B23). Single player: the jump change
only.

`0090-bug-pass-4.patch` (S10-T07 part 4, 2026-11-02): a creature replica
faces where the server turns it (`CCustomMonster::replica_present` sets
`m_body`; a stalker's `CSightManager::Exec_Look` and a monster's
`CControlDirection::update_frame` no longer turn a replica themselves: they
rebuilt its rotation from a yaw never set on a client, B4), and the
`replica-anim` line gains the motion names and the drawn, sent and travel
headings; the campaign server never turns a campfire and a co-op client's
campfire casts its shadow only within 20 m of the camera (B23, decision
T07-10); a hidden trade partner panel in the Lua trade window is shown and
filled when the window opens, with a `panel=` probe on `trade-open` (B1).
Single player is unchanged.

`0091-bug-pass-5.patch` (S10-T07 part 5, 2026-11-02): a stalker replica runs
no sight action or aiming of its own; its upper body turns by the server's
head aim from the body only, with the torso share of the server's mental
state (B4: the hips twisted toward the replica's own stale look). A co-op
client's campfire casts no shadow by default; the game-console settings
`coop_campfire_shadow <m>` and `coop_campfire_light 0/1`, and a `frame-trace`
line near fires (B23). Single player is unchanged.

`0092-blend-list-guard.patch` (S10-T07 part 6, 2026-11-02): a model part's
blend list holds 16 blends in a fixed array whose bound is checked in debug
builds only; when it is full, `CKinematicsAnimated::LL_PlayCycle` retires the
part's oldest blend instead of writing past the end (B24: a co-op client
crashed animating the other player), and logs `[B24] blend list full`. The
per-bone lists already did this. Single player: the same guard, which only
replaces a memory overwrite.

`0093-g10-revisions.patch` (S10-T07, the user's G10 review, 2026-11-02): the
nine revisions. The clicking player's purse for a party task's money
(`CampaignScriptContext` `PartyPurseScope`, the Lua `money` / `give_money` /
`transfer_money` bindings); the server's G10 Lua adapter (`g10_source`: item
rewards and a failed task's goodwill penalty to both players, bounty squads by
the worse standing, the sleep shelter check, the repair kit's rules, mechanic
lending always on with a per-player ledger); parking an absent player after 5
minutes (`set_participant_parked`: no ALife anchor, not seen by the AI, no hits
or condition change); the sleep request and the partner's answer refused
without shelter (reason 47); story mode only (the creation screen and the
server's check); the trade window's donate as a free transfer and the repair
kit's window on the client with the repair on the server (action kind 29).
Fixtures: the sleep fixture's shelter round, the party-task fixture's purse,
item and fail probes. Single player: unchanged (every path is the campaign
server's or a co-op client's).

`0094-world-item-pickup.patch` (S10-T07, demo-6, 2026-11-02): B26, a client
crashed picking up one of the randomly placed world items: the installed
`game_setup` pickup handler read its spawn table, which a client's replica
actor never filled (`init_settings` runs at the actor's first update). The
client's Lua layer runs `init_settings` (ini reads only) and makes its
`try_spawn_world_item` a no-op (the server spawns the replacement). Also the
demo kit (profile `demo_kit`, with `arm`): a pistol repair kit and a tent, the
pistol worn to 80 % and the player sleepy, for the hands-on checks. Single
player: unchanged.

`0095-local-hosting.patch` (S11-T00, 2026-11-02): local hosting over
Tailscale and the host and join menu. The client's co-op menu (only with app
data `coop-menu.ltx`): New game offers Host (a new or the last campaign: the
key in the server's roster, a join code and the choice in its app data, the
launcher started hidden, a join at 127.0.0.1) and Join (the installed
direct-IP box: address and join code; the key made on first use); the link
starts at run time from a profile the menu writes (values quoted: CInifile
drops spaces outside quotes; the config reader unquotes). The campaign
server: the join code from its app data, the host's choice served by a
relaunch from the prepare stage (`x_ray.cpp`: exit 75), a code-joined player
kept in the roster file and bound at run time (never the owner), every
binding pass on a fresh registry, no join while a travel replaces the world
(ServerBusy). Single player: unchanged without `coop-menu.ltx`.

`0096-dedicated-server.patch` (S11-T00, 2026-11-02): a dedicated server (the
user: a third PC hosts, both players join from theirs). A server with a join
code may start with an empty roster; its first player to join owns the
session; that player takes over the start-up identity check's stand-in
participant (slot 0) and the saved actor (`stand-in-released`,
`stand-in-actor-taken`). The server's own scripts: `tools/Host-CoopServer.ps1`
(a join code, the Tailscale address, the launcher) and
`tools/Stop-CoopServer.ps1`. Single player: unchanged.

`0097-demo7-fixes.patch` (S10-T07, 2026-11-02): the user's demo-7 bugs. B27:
a monster's override animation index past its motion's variants is ignored
(`select_animation`; the server's motion and index can pair across a
change). B28: `level.add_call` re-enters the caller's script context (the
tent is placed by the player who used it), the server's shelter check scans
every placed tent, a client's placed sleeping bag opens the installed sleep
window locally with its shelter check left to the server, the demo kit adds
a sleeping bag. B29: a client moves a settled body to the server's position
(loot reach). Single player: `select_animation`'s guard only.

`0098-physics-present-fire-gate.patch` (S10-T07, 2026-11-02): the user's
demo-8 bugs. B31 (and B28's unseen tent): a client places a physics object's
shell with `SetGlTransformDynamic` (rigid, `m_object_in_root` kept) instead
of `SetTransform` (every element at the object's transform: a dropped item
drawn off the ground, a several-bone tent folded). B30: the fire key is sent
only while the world has the input (no dialog, console, menu or pause).
Single player: unchanged.

`0099-sleepbag-corpse-grab.patch` (S10-T07, 2026-11-02): the user's demo-9
bugs. B28: a client's F on a placed sleeping bag opens the installed sleep
window itself (the installed handler is registered by a binder update a
replica skips). B33: a body is moved to the server's position once (over
1 m), again only when the server's body moves; `CPHCapture` takes a bone's
callback parameter only when it is one of the shell's elements (a dead
actor's spine, shoulder and head keep the actor's callbacks). Single
player: the capture guard only (no dead actor is grabbed there).

`0100-tent-shelter-rifle-kit.patch` (S10-T07, 2026-11-02): the user's
demo-10. B34: a placed tent shelters within 3 m of its origin in co-op (the
installed 1.5 m holds one player, not two and a sleeping bag); a refused
sleep request shows only its own words. The demo kit adds an AK-74 and three
boxes of 5.45x39 (the user's rifle animation check). Single player: unchanged.

`0101-effects-per-player-body-loot.patch` (S10-T07, 2026-11-02): the user's
demo-11. B35: the installed `actor_effects` hit flash, bleeding, blood and
radiation effects run on the server for each player in its context with its
own state (they ran for player 1 outside a context and reached both
screens); the server's effect de-duplication is keyed by audience and id.
B36: on a client `get_object_by_id(0)` is the real object unless it is the
client's own actor (the loot window moved player 1's body's items "from the
player to the player"); a same-owner move is not sent; malformed transfers
are logged with their check. Single player: unchanged.

`0102-npc-weapons-status-hud.patch` (S10-T07, 2026-11-03): the user's
demo-12. B38: the server sends each NPC stalker's active slot, weapon state
(bit 7: strapped), ammo and shot counter (`kWeapon`, no wire change); a
client gives the replica's object handler the item as its goal (idle or
strapped, never fire) and shows each shot's flash and sound. B39: a client
makes the installed `actor_status` icon window (its first-update handler
never runs on a replica); the needs event carries the actor's bleeding speed
(8th value) for the HUD's blood overlay (`UIMainIngameWnd`). Single player:
unchanged (the overlay hook answers false).

`0103-npc-body-loot.patch` (S10-T07, 2026-11-03): the user's demo-13. B40: a
client sends the use of a dead NPC's body to the server instead of running
its use callback (the installed `xr_motivator` makes the body's loot, which
needs the server's ALife); the server uses the body in the player's context
(3 m reach); the client's loot window refreshes while the loot arrives.
Single player: unchanged.

`0104-host-actor-fallback-spot-text.patch` (S10-T07, 2026-11-03): the user's
demo-14. B41: on the server, outside a context, `db.actor` is a living
player's actor when the binder's actor (player 1's first) is dead, so the
installed actor update (task targets, weather, `actor_on_update`) goes on
after player 1's respawn. B42: a map spot's long detail is truncated, not
fatal (`record_presentation`, `present_map_spot`). Single player: unchanged.

`0105-travel-resume-fallback-itemswap.patch` (S10-T07, 2026-11-03): the
user's demo-15. B43: B41's fallback holds no object across frames (it looks
the binder's actor and the living one up by id, actors only): a level
reload destroyed the held actor and the next `db.actor` read crashed the
server (0xC0000005 at the travel); the installed weapon draw/holster camera
nudge (`actor_effects.Update_ItemSwap`) runs per player. B44: travel events
(the resume among them) go to every live player, inputs or not (the clients
never got the resume after a reload). Single player: unchanged.

`0106-slot-resync-after-travel.patch` (S10-T07, 2026-11-03): the user's
demo-16. B45: a client's refused slot intent (sent at world-live inside the
travel's hold, refused 30) resets its record of the slot sent to the
server's slot, so the local slot is asked for again; the travel's resume
does the same. Diagnostics: `actor-weapons` (the server's inventory slots at
a travel's commit and resume and at each activation; the travel fixture's
weapon probe with profile `arm`), `player-slot-sent` / `player-slot-refused`
on the client. Single player: unchanged.

`0107-stalled-inventory-kick.patch` (S10-T07, 2026-11-03): B45, second part
(demo-17: after a travel both actors kept a weapon change pending, active 0
and next 2/3, through every later accepted slot intent). The server updates
a participant's inventory itself when its actor's own update (`UpdateCL`,
which runs `CInventory::Update`) has not run for 10 frames with a change
pending (`actor-weapons tag=kick`, once per actor); `actor-weapons` gains the
update age, processing, view and hand-animation state. Single player:
unchanged.

`0108-travel-combat-vanilla.patch` (S10-T07, 2026-11-03): B46 (after a travel
to the Marsh every request back to Cordon was refused 27, in combat, for over
a minute: a renegade kept player 2 as its enemy). By the user's choice
(T07-11, revising T01-5) the combat rule now follows the installed game:
only a PDA fast travel or a backpack jump is refused in combat
(`travel_combat_rule`); a level changer, a guide's trip and another script's
`ChangeLevel` are taken (`travel-combat-ignored` noted). The travel fixture's
round 8 asks and agrees with the boar hunting; its new variant 5 runs round 8
alone and carries the travel out (variant 0's nine rounds predate S09-T02). On a client a refused travel,
fast travel, session or repair keeps its own words (the generic "You can't
pick that up." no longer overwrites them). Single player: unchanged.

`0109-server-never-paused.patch` (S10-T07, 2026-11-03): B47 (demo-16, 17, 19:
after player 1 walked into a Cordon or Marsh passage, the players' actors and
the NPCs near them got no `UpdateCL` for the rest of the session: shots left
from 0,0,0, NPCs stood still; the root cause of B45). The passage's space
restrictor runs the installed `ui_sr_teleport` travel box on the server; its
`ShowDialog` read a null `MainMenu` (no dialog holder there), and on the way
the device was paused (`set_device_paused_script`), which nothing undid; a
paused device runs no object's update (`CObjectList::Update`). Now
`CRenderDevice::Pause` refuses a pause on the campaign server
(`device-pause-refused`), `CurrentDialogHolder` returns none there and
`ShowDialog` skips the window. `update-stall` (the server, once a second)
logs any actor or creature near a player with no update for 100 frames, with
its crow state and the device's pause. Fixture options `travel_changer` and
`travel_approach` take the travel fixture through that passage's approach.
Single player: unchanged.

`0110-torch-demo-quest.patch` (S10-T07, 2026-11-03): B48 (demo-20: the L key
did nothing). The installed L key (`actor_effects.Hit_TorchToggle` →
`item_device.toggle_torch`) runs on the client; the light is item_device's
binder's, from one script-wide `torch_active`, and on the server it kept
every player's torch off. Now the client's `toggle_torch` (wrapped) asks the
server (ActionIntent 30, protocol `proto-build-64`), the server switches the
torch in the player's own torch slot (`torch slot=`), every client shows it
(Presentation `TorchState`, replayed at a join), and scripts no longer switch
a player's torch (`torch-script-ignored`) except off at the battery's
critical level (`torch-battery-off`). The demo kit adds a headlamp in the
torch slot; the hands-on aid `demo_quest` (`start-coop-demo.sh` 7th
argument) gives the party Sidorovich's "Mobbing the Mob" with its bandit
patches, ready to turn in; the client driver `torch_test`. Single player:
unchanged.

`0111-menu-new-game-click.patch` (S11-T00, 2026-11-03): B49 (the user, with
kit host-7: New game showed no co-op choice). The installed main menu binds
its New game button to `OnButton_new_game` when it is built
(`ui_main_menu.script:73`), before the co-op menu is installed, so the
replaced function was never reached by a click (the menu test driver had
called the host flow directly). The choice now comes from `ShowFactionUI`,
which the button's handler looks up when called; `Update` records the live
menu, and the driver's `host_new` clicks New game through it, picks "Host
co-op: a new campaign" as its double-click does and starts. Single player:
unchanged.

`0112-menu-join-box.patch` (S11-T00, 2026-11-04): B50 (the user's friend,
with kit client-7: Join co-op crashed). The installed `message_box_direct_ip`
template has no `message_text`, so `CUIMessageBox::SetText` dereferenced a
null text window when `ui_dyn_msg_box.msg_box_ui` set its text; it is now
skipped when the template has none (`GetText` gives ""). The join box's script
window is kept (its buttons send to it), its captions read *Host address* and
*Join code* (the template's are multiplayer strings Anomaly lacks) and the
code is shown as typed; `menu_join` undoes `GetHost`'s `host/port=N` form. The
menu test driver's `join` now clicks New game, opens Join co-op, types into
the box and presses its OK. Single player: unchanged.

`0113-smooth-movement.patch` (S10-T07, 2026-11-04): B51 (the user: both
players' movement "choppy and jittery", on one PC too). A co-op client's
correction of its predicted actor now moves the physics character by the
offset (`CPHSimpleCharacter::ShiftPosition`, through
`CPHMovementControl::ShiftPosition`: the body, its safe and death positions
and both render interpolation points) instead of `SetPosition`, which placed
the body at the shown position plus the offset (losing its lead over the shown
position, up to a physics step) and restarted the interpolation (a frame
without movement): the next snapshot found that loss and corrected again,
7-9 times a second at a run. The client's reconciler bleeds errors of 5 mm to
15 cm off at 30 % per snapshot (proto-build-66 `PredictionConfig::blend`); its
server clock slews 1 ms per snapshot within 30 ms (5 ms steps had moved the
other player 1.3 cm at a run); the render time uses the timer's full
resolution. Test tooling: `motion_log = 1` writes `coop-motion-client.csv`
(a row per frame: both actors' motion), `script = 9` is the smoothness walk
(`prepare-coop-session.py --smooth`, `compare-coop-smoothness.py`). Single
player: unchanged (`ShiftPosition` has no other caller).

`0114-minimap-option.patch` (S10-T07, 2026-11-04): B52 (the user: the minimap
is always on though it is off in the settings). The installed option
(`video/hud/show_minimap`) is applied by `actor_effects`'
`actor_on_first_update` (`actor_effects.script:1830`), which a co-op client's
actor never fires; the client now applies it at every world-live
(`UIZoneMap.disabled`, the motion icon) and logs `minimap hidden|shown`.
Single player: unchanged.

`0115-travel-after-respawn.patch` (S10-T07, 2026-11-04): B53 (the user, kits
host-13 / client-9: a level travel stayed on the same level, no loading
screen). A reload loads the ALife player's level (the object flagged
`M_SPAWN_OBJECT_ASPLAYER`, `alife_graph_registry.cpp:55-70`): object 0,
participant 1's first actor and the server's script actor (`db.actor`; the
installed scripts key the player on `AC_ID = 0`). After participant 1's death
and respawn object 0 is the corpse, left where it died: the travel moved the
two living actors, the reload loaded the corpse's level and found neither
(`arrived=0`), and the server stopped. The travel's take now also moves the
corpse (beside the arrival point, its items with it) when it is not
travelling (`travel-take-corpse`). Moving the flag to the living actor
instead loaded the destination but its NPCs' scripts ran before `db.actor`
existed (objects come online in id order) and the scripts' `AC_ID` keys would
have pointed at the corpse. Fixture `travel_kill_first = 1`
(`--travel-kill-first`): participant 1 is killed and respawned before the
travel rounds. Single player: unchanged.

Root commit identity plus source manifest plus ordered patches defines project
source identity. Root currently has no commits; these artifacts are pending
version control, not a published or committed baseline.
