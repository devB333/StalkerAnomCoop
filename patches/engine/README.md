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
the talk fixture (`talk_party`) and the driver's actions 14–16. Single player
is unchanged (the hooks return false off the campaign server).

Root commit identity plus source manifest plus ordered patches defines project
source identity. Root currently has no commits; these artifacts are pending
version control, not a published or committed baseline.
