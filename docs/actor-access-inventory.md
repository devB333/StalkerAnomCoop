# Actor access inventory — S03-T02

Every `Level().CurrentEntity()` and `Actor()` use in `src/xrGame` of
`builds/build_1/engine-work` (`c57fe5b9` with patches 0001–0021), counted by
raw grep on 2026-10-18 before the S03-T02 changes: **126** `CurrentEntity()`
sites in 65 files and **372** `Actor()` sites in 85 files (the G02 carried
input, B21). Each file is classified and the S03-T02 change recorded. The
adapters are in [`xrGame/CampaignActorAccess.h`](../builds/build_1/engine-work/src/xrGame/CampaignActorAccess.h);
the rules are in [participant-identity.md](participant-identity.md#local-view-and-gameplay-access).

Classes:

| Class | Meaning | What S03-T02 did |
|---|---|---|
| Gameplay authority | The code decides something in the simulation for "the player" | Converted to an explicit participant lookup: the nearest participant actor, every participant actor, the actor among the dialogue speakers |
| Self or holder | The object already had its actor (the weapon's holder, the hit entity, the inventory's owner, the dying entity) and reached for the global anyway | Converted to the object's own reference or a cast of the entity in hand; works for any participant's actor |
| Single-actor assumption | A system with no participant context yet (tasks, map callbacks, script holders, autosave) | `sole_participant_actor(purpose)`: the one participant actor while exactly one exists, else null and a counted, named refusal; the owning task is recorded |
| Local view | Cameras, HUD, UI, post-process, input, map, render | `local_view_actor()` (the client's own actor, null on the server) or unchanged where the code already compares with the view entity and is null-safe |
| Script binding | Lua `level.*` and script-object bindings | Unchanged except the three self-acting bindings; S03-T05 owns the context contract; on the server these go through the counted compatibility accessor |
| Player-bound ability | Monster abilities that target *the* player | Unchanged, recorded as uncovered; on the server `Actor()` returns the sole participant actor and refuses (null) with two; S03-T04 lists them, S10-T02 owns the per-participant form |
| Debug / MP | DEBUG-only, commented-out, or multiplayer game-mode code | Unchanged |

Totals by class (files may hold sites of several classes; the file's dominant class is used):

| Class | Files | Sites |
|---|---|---|
| Gameplay authority: converted to a participant lookup | 8 | 24 |
| Self or holder: the object already knew its actor | 25 | 98 |
| Single-actor assumption, named and counted | 4 | 8 |
| Local view / presentation (client only) | 46 | 153 |
| Script binding (S03-T05) | 2 | 46 |
| Player-bound monster ability (S03-T04 / S10-T02) | 13 | 104 |
| Debug, test or commented-out code | 17 | 42 |
| Multiplayer game modes (not run by the campaign) | 9 | 17 |
| The accessors themselves | 3 | 6 |
| **All** | 127 | 498 |

## Gameplay authority: converted to a participant lookup

| File | `CurrentEntity()` | `Actor()` | S03-T02 |
|---|---|---|---|
| `PhraseScript.cpp` |  | 4 | dialogue info checks and transfers use the actor among the speakers (dialog_actor); a dialogue without an actor speaker falls back to sole_participant_actor, named |
| `ai/crow/ai_crow.cpp` |  | 2 | look-at and before-look distance use nearest_participant_actor(); crows drift to the nearest participant (one debug/commented line unchanged) |
| `ai/monsters/control_path_builder.cpp` |  | 2 | player-on-the-path and distributed-computation checks iterate every participant actor |
| `ai/phantom/phantom.cpp` | 3 |  | B21: the phantom's enemy is now nearest_participant_actor(); with none it faces north, goes idle and is destroyed; every dereference guarded |
| `ai/stalker/ai_stalker.cpp` | 1 | 7 | look-at and before-look distance use nearest_participant_actor(); crows drift to the nearest participant (one debug/commented line unchanged) |
| `ai/trader/ai_trader.cpp` | 1 |  | B21: the trader's head faces nearest_participant_actor(); with none the bone callback leaves the transform |
| `game_cl_single.cpp` |  | 1 | difficulty change applied to every participant actor |
| `stalker_property_evaluators.cpp` |  | 3 | player-on-the-path and distributed-computation checks iterate every participant actor |

## Self or holder: the object already knew its actor

| File | `CurrentEntity()` | `Actor()` | S03-T02 |
|---|---|---|---|
| `ActorAnimation.cpp` |  | 1 | the condition's/animation's own actor (m_object/this); IK first-person check against the local view actor |
| `ActorCondition.cpp` |  | 6 | the condition's/animation's own actor (m_object/this); IK first-person check against the local view actor |
| `Entity.cpp` | 1 | 3 | (see above) |
| `Flashlight.cpp` |  | 3 | the item's holder (smart_cast<CActor*>(H_Parent())); the auto-fire key check uses the local view actor; one bDebug line unchanged |
| `HudItem.cpp` |  | 3 | the item's holder (smart_cast<CActor*>(H_Parent())); the auto-fire key check uses the local view actor; one bDebug line unchanged |
| `IKLimbsController.cpp` |  | 4 | the condition's/animation's own actor (m_object/this); IK first-person check against the local view actor |
| `Inventory.cpp` |  | 3 | name translation, faction/rank/reputation refresh and eat callback use the owner itself; the trade callback the actor in the trade; the inventory box uses sole_participant_actor (the box does not record who opened it, S08) |
| `InventoryBox.cpp` |  | 1 | name translation, faction/rank/reputation refresh and eat callback use the owner itself; the trade callback the actor in the trade; the inventory box uses sole_participant_actor (the box does not record who opened it, S08) |
| `InventoryOwner.cpp` |  | 7 | name translation, faction/rank/reputation refresh and eat callback use the owner itself; the trade callback the actor in the trade; the inventory box uses sole_participant_actor (the box does not record who opened it, S08) |
| `WeaponAutomaticShotgun.cpp` |  | 1 | the item's holder (smart_cast<CActor*>(H_Parent())); the auto-fire key check uses the local view actor; one bDebug line unchanged |
| `WeaponFire.cpp` |  | 1 | the item's holder (smart_cast<CActor*>(H_Parent())); the auto-fire key check uses the local view actor; one bDebug line unchanged |
| `WeaponMagazined.cpp` |  | 9 | the item's holder (smart_cast<CActor*>(H_Parent())); the auto-fire key check uses the local view actor; one bDebug line unchanged |
| `WeaponMagazinedWGrenade.cpp` |  | 3 | the item's holder (smart_cast<CActor*>(H_Parent())); the auto-fire key check uses the local view actor; one bDebug line unchanged |
| `WeaponSSRS.cpp` |  | 1 | the item's holder (smart_cast<CActor*>(H_Parent())); the auto-fire key check uses the local view actor; one bDebug line unchanged |
| `WeaponShotgun.cpp` |  | 1 | the item's holder (smart_cast<CActor*>(H_Parent())); the auto-fire key check uses the local view actor; one bDebug line unchanged |
| `ai/monsters/basemonster/base_monster_feel.cpp` | 1 | 8 | attack effector on the viewer (local_view_actor()); hit effects on the hit entity itself (cast) |
| `ai/monsters/controller/controller.cpp` | 5 | 9 | "is my enemy/hitter the player" became "is it an actor" (cast of the enemy, hitter or hit entity); controller stamina hit and control effectors act on that entity; the poltergeist detection sites and the controller debug key stay (ability/debug) |
| `ai/monsters/dog/dog.cpp` |  | 2 | "is my enemy/hitter the player" became "is it an actor" (cast of the enemy, hitter or hit entity); controller stamina hit and control effectors act on that entity; the poltergeist detection sites and the controller debug key stay (ability/debug) |
| `ai/monsters/monster_enemy_manager.cpp` |  | 2 | "is my enemy/hitter the player" became "is it an actor" (cast of the enemy, hitter or hit entity); controller stamina hit and control effectors act on that entity; the poltergeist detection sites and the controller debug key stay (ability/debug) |
| `ai/monsters/poltergeist/poltergeist.cpp` |  | 12 | "is my enemy/hitter the player" became "is it an actor" (cast of the enemy, hitter or hit entity); controller stamina hit and control effectors act on that entity; the poltergeist detection sites and the controller debug key stay (ability/debug) |
| `ai/monsters/pseudodog/psy_dog.cpp` |  | 3 | "is my enemy/hitter the player" became "is it an actor" (cast of the enemy, hitter or hit entity); controller stamina hit and control effectors act on that entity; the poltergeist detection sites and the controller debug key stay (ability/debug) |
| `ai/monsters/pseudogigant/pseudo_gigant.cpp` | 1 | 4 | step shake on the viewer; threaten effectors and kick slow-down on the kicked enemy itself |
| `ai/monsters/states/monster_state_attack_inline.h` |  | 1 | "is my enemy/hitter the player" became "is it an actor" (cast of the enemy, hitter or hit entity); controller stamina hit and control effectors act on that entity; the poltergeist detection sites and the controller debug key stay (ability/debug) |
| `trade2.cpp` |  | 1 | name translation, faction/rank/reputation refresh and eat callback use the owner itself; the trade callback the actor in the trade; the inventory box uses sole_participant_actor (the box does not record who opened it, S08) |
| `weaponBM16.cpp` |  | 1 | the item's holder (smart_cast<CActor*>(H_Parent())); the auto-fire key check uses the local view actor; one bDebug line unchanged |

## Single-actor assumption, named and counted

| File | `CurrentEntity()` | `Actor()` | S03-T02 |
|---|---|---|---|
| `GameTask.cpp` |  | 3 | task state/info, map-location-added callbacks, script holder attach and autosave: sole_participant_actor(purpose), refused and counted with two actors; owners S03-T05 (script context), S08 (tasks), Stage 7 (autosave) |
| `HolderEntityObject.cpp` |  | 2 | task state/info, map-location-added callbacks, script holder attach and autosave: sole_participant_actor(purpose), refused and counted with two actors; owners S03-T05 (script context), S08 (tasks), Stage 7 (autosave) |
| `autosave_manager.cpp` |  | 1 | task state/info, map-location-added callbacks, script holder attach and autosave: sole_participant_actor(purpose), refused and counted with two actors; owners S03-T05 (script context), S08 (tasks), Stage 7 (autosave) |
| `map_manager.cpp` |  | 2 | task state/info, map-location-added callbacks, script holder attach and autosave: sole_participant_actor(purpose), refused and counted with two actors; owners S03-T05 (script context), S08 (tasks), Stage 7 (autosave) |

## Local view / presentation (client only)

| File | `CurrentEntity()` | `Actor()` | S03-T02 |
|---|---|---|---|
| `Actor.cpp` | 6 |  | the actor/object asks whether it is the local view or control entity (cameras, HUD pick, On_B_NotCurrentEntity); unchanged (the server adopts no entity, S02-T05d) |
| `ActorCameras.cpp` | 3 |  | the actor/object asks whether it is the local view or control entity (cameras, HUD pick, On_B_NotCurrentEntity); unchanged (the server adopts no entity, S02-T05d) |
| `Actor_Feel.cpp` | 1 |  | the actor/object asks whether it is the local view or control entity (cameras, HUD pick, On_B_NotCurrentEntity); unchanged (the server adopts no entity, S02-T05d) |
| `CustomDevice.cpp` | 1 | 1 | device visibility for the view entity (null-safe) and the holder's climb state (now the holder) |
| `CustomZone.cpp` | 6 |  | zone sound position relative to the control entity and the DOF pick effect; null-safe; unchanged |
| `Explosive.cpp` | 1 |  | camera shake, zone post-process and crosshair dispersion: converted to local_view_actor() |
| `GameObject.cpp` | 1 |  | the actor/object asks whether it is the local view or control entity (cameras, HUD pick, On_B_NotCurrentEntity); unchanged (the server adopts no entity, S02-T05d) |
| `GameObject.h` | 1 |  | the actor/object asks whether it is the local view or control entity (cameras, HUD pick, On_B_NotCurrentEntity); unchanged (the server adopts no entity, S02-T05d) |
| `GamePersistent.cpp` |  | 17 | the viewer's camera update each frame: converted to local_view_actor() (none on the server) |
| `HUDManager.cpp` | 3 |  | HUD and UI read the view entity; the server UI is inert (B20); unchanged (PDA item logic now uses its holder) |
| `HUDRecon.cpp` | 1 |  | HUD and UI read the view entity; the server UI is inert (B20); unchanged (PDA item logic now uses its holder) |
| `HUDTarget.cpp` |  | 3 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `Level.cpp` | 1 | 3 | render-time debug of the current stalker and the viewer's pick-up drawing (converted to local_view_actor()) |
| `Level_bullet_manager_firetrace.cpp` | 1 |  | no wallmark on the viewer's own actor; null-safe; unchanged |
| `Level_input.cpp` | 6 |  | input goes to the local control entity; unchanged |
| `Level_network_spawn.cpp` | 3 |  | the actor/object asks whether it is the local view or control entity (cameras, HUD pick, On_B_NotCurrentEntity); unchanged (the server adopts no entity, S02-T05d) |
| `PDA.cpp` | 1 | 9 | HUD and UI read the view entity; the server UI is inert (B20); unchanged (PDA item logic now uses its holder) |
| `UIDialogHolder.cpp` | 4 |  | HUD and UI read the view entity; the server UI is inert (B20); unchanged (PDA item logic now uses its holder) |
| `UIDosimeter.cpp` |  | 1 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `UIGameCustom.cpp` | 1 |  | HUD and UI read the view entity; the server UI is inert (B20); unchanged (PDA item logic now uses its holder) |
| `UIGameSP.cpp` | 2 | 1 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `Weapon.cpp` | 3 | 5 | HUD mode and DOF checks compare the holder with the view entity; unchanged (safemode and callbacks now use the holder) |
| `WeaponBinoculars.cpp` | 2 |  | HUD mode and DOF checks compare the holder with the view entity; unchanged (safemode and callbacks now use the holder) |
| `WeaponBinocularsVision.cpp` |  | 4 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `WeaponStatMgunFire.cpp` | 1 |  | HUD mode and DOF checks compare the holder with the view entity; unchanged (safemode and callbacks now use the holder) |
| `actor_input_handler.cpp` | 1 | 1 | input goes to the local control entity; unchanged |
| `ai/monsters/pseudodog/psy_dog_aura.cpp` | 1 | 1 | B21: the aura reads the local view actor (null on the server, update_schedule returns); the effector goes on that viewer; the ability is recorded for S03-T04 |
| `cameralook.cpp` | 1 | 3 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `fire_disp_controller.cpp` | 1 |  | camera shake, zone post-process and crosshair dispersion: converted to local_view_actor() |
| `map_location.cpp` | 3 | 6 | spots pinned to the viewer, the exit pointer path and the pointer distance: converted to local_view_actor() with null guards; the enemy-visibility check now uses the spot's own actor |
| `player_hud.cpp` |  | 7 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `player_hud_legs.cpp` |  | 1 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `pp_effector_custom.cpp` |  | 3 | post-process controllers and script effectors apply to the local view actor; with no viewer the effector is dropped (client presentation callbacks are S03-T05) |
| `script_effector.cpp` |  | 2 | post-process controllers and script effectors apply to the local view actor; with no viewer the effector is dropped (client presentation callbacks are S03-T05) |
| `ui/UIActorMenu.cpp` |  | 1 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `ui/UICharacterInfo.cpp` |  | 4 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `ui/UIInventoryUtilities.cpp` | 1 |  | HUD and UI read the view entity; the server UI is inert (B20); unchanged (PDA item logic now uses its holder) |
| `ui/UILogsWnd.cpp` |  | 3 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `ui/UIMMShniaga.cpp` |  | 2 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `ui/UIMapWnd.cpp` | 2 | 1 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `ui/UIMpTradeWnd.cpp` | 1 |  | HUD and UI read the view entity; the server UI is inert (B20); unchanged (PDA item logic now uses its holder) |
| `ui/UIPdaWnd.cpp` |  | 5 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `ui/UITalkDialogWnd.cpp` |  | 2 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `ui/UITalkWnd.cpp` |  | 3 | HUD, camera and UI of the local player; the server has no HUD (g_hud null) and an inert UI (B20); unchanged |
| `visual_memory_manager.cpp` |  | 2 | the HUD visibility indicator is fed for the local view actor only |
| `zone_effector.cpp` | 2 |  | camera shake, zone post-process and crosshair dispersion: converted to local_view_actor() |

## Script binding (S03-T05)

| File | `CurrentEntity()` | `Actor()` | S03-T02 |
|---|---|---|---|
| `level_script.cpp` | 1 | 38 | level.actor (deprecated) and the level.* camera/effector/statistic bindings; S03-T05 owns the Lua context contract; unchanged, counted on the server |
| `script_game_object_inventory_owner.cpp` |  | 7 | restore_weapon/hide_weapon/allow_sprint act on the script object itself (error if not an actor); give_game_news and the look-at-point camera stay on Actor() for S03-T05 (news) and are presentation (camera) |

## Player-bound monster ability (S03-T04 / S10-T02)

| File | `CurrentEntity()` | `Actor()` | S03-T02 |
|---|---|---|---|
| `ai/monsters/anti_aim_ability.cpp` |  | 10 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/bloodsucker/bloodsucker.cpp` |  | 8 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/bloodsucker/bloodsucker_alien.cpp` |  | 10 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/bloodsucker/bloodsucker_predator_inline.h` |  | 1 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/bloodsucker/bloodsucker_predator_lite_inline.h` |  | 2 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/bloodsucker/bloodsucker_vampire_execute_inline.h` |  | 7 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/burer/burer.cpp` |  | 8 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/burer/burer_state_attack_shield_inline.h` |  | 1 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/controller/controller_psy_hit.cpp` |  | 34 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/monster_aura.cpp` |  | 11 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/poltergeist/poltergeist_flame_thrower.cpp` |  | 1 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/poltergeist/poltergeist_telekinesis.cpp` |  | 9 | player-bound monster abilities (psy hit/tube, telekinesis, flames, auras, anti-aim, vampire/alien vision, gravi/shield): they target the one player; on the server Actor() is the sole participant actor and refuses with two; recorded uncovered for S03-T04 (list) and S10-T02 (per-participant form) |
| `ai/monsters/scanning_ability_inline.h` | 1 | 1 | scan effect shown to the viewer (local_view_actor()); the scan itself still picks that one player: recorded uncovered for S03-T04/S10-T02 |

### Resolved in S03-T04

Every row above is now either an explicit no-op while `Actor()` refuses, a
participant loop, or unreached on the server; the per-ability table is in
[S03-T04](validation/S03-T04.md#player-bound-ability-table-for-s10-t02). New
or changed sites:

| Site | Was | Now |
|---|---|---|
| `alife_dynamic_object.cpp:160,180`, `alife_online_offline_group.cpp:226,263`, `alife_group_abstract.cpp:164` | distance to the ALife actor | `activation_distance`: the nearest alive participant actor on the server |
| `ai/monsters/monster_enemy_memory.cpp:49-123` | `g_actor` watches / is near | every participant actor |
| `ai/stalker/ai_stalker.cpp:1694` | `Actor()` after choosing the nearest participant | the chosen participant |
| `ai/monsters/bloodsucker/bloodsucker_predator_inline.h:97` | `Actor()->memory()` (fault with two) | any participant watching |
| `ai/monsters/poltergeist/poltergeist.cpp:334` | `Actor()->memory()` every `UpdateCL` (fault with two) | any participant watching within 85 m |
| `ai/monsters/bloodsucker/bloodsucker_vampire_execute_inline.h:194` | any actor enemy, then `Actor()->` (fault) | only when the enemy is `Actor()` |
| `ai/monsters/bloodsucker/bloodsucker_alien.cpp:223` | `VERIFY(Actor())`, then `Actor()->` | none while `Actor()` refuses |
| `anti_aim_ability.cpp:265`, `controller_psy_hit.cpp:59,211`, `burer.cpp:247,539`, `poltergeist_telekinesis.cpp:56`, `monster_aura.cpp:56` | the one player | none while `Actor()` refuses (S10-T02) |

## Debug, test or commented-out code

| File | `CurrentEntity()` | `Actor()` | S03-T02 |
|---|---|---|---|
| `Car.cpp` | 2 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `CustomMonster.cpp` | 1 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `CustomRocket.cpp` |  | 2 | #if 0 or commented-out; unchanged |
| `HelicopterMovementManager.cpp` | 1 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `ai/monsters/basemonster/base_monster.cpp` | 1 | 1 | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `ai/monsters/basemonster/base_monster_debug.cpp` | 2 | 2 | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `ai/monsters/snork/snork.cpp` | 3 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `ai/monsters/snork/snork_jump.cpp` | 1 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `ai/monsters/states/state_test_look_actor_inline.h` | 2 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `ai/monsters/states/state_test_state_inline.h` | 1 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `ai/stalker/ai_stalker_debug.cpp` | 7 | 3 | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `console_commands.cpp` | 2 | 1 | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `enemy_manager.cpp` |  | 1 | #if 0 or commented-out; unchanged |
| `level_debug.cpp` | 1 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `level_graph_debug.cpp` | 4 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `level_graph_debug2.cpp` | 1 |  | DEBUG-only code, test states or commented-out lines; unchanged (console save command now uses sole_participant_actor) |
| `sight_manager.cpp` |  | 3 | #if 0 or commented-out; unchanged |

## Multiplayer game modes (not run by the campaign)

| File | `CurrentEntity()` | `Actor()` | S03-T02 |
|---|---|---|---|
| `Spectator.cpp` | 2 |  | multiplayer game modes and the spectator; not run by the campaign (GameID single) |
| `game_cl_artefacthunt.cpp` | 2 |  | multiplayer game modes and the spectator; not run by the campaign (GameID single) |
| `game_cl_base.cpp` | 1 |  | multiplayer game modes and the spectator; not run by the campaign (GameID single) |
| `game_cl_capture_the_artefact.cpp` | 2 |  | multiplayer game modes and the spectator; not run by the campaign (GameID single) |
| `game_cl_capturetheartefact_buywnd.cpp` | 1 |  | multiplayer game modes and the spectator; not run by the campaign (GameID single) |
| `game_cl_deathmatch.cpp` | 5 |  | multiplayer game modes and the spectator; not run by the campaign (GameID single) |
| `game_cl_deathmatch_buywnd.cpp` | 1 |  | multiplayer game modes and the spectator; not run by the campaign (GameID single) |
| `game_cl_mp.cpp` | 1 |  | multiplayer game modes and the spectator; not run by the campaign (GameID single) |
| `game_cl_teamdeathmatch.cpp` | 2 |  | multiplayer game modes and the spectator; not run by the campaign (GameID single) |

## The accessors themselves

| File | `CurrentEntity()` | `Actor()` | S03-T02 |
|---|---|---|---|
| `Actor.h` | 1 | 1 | the definition: Actor() is now the compatibility accessor (campaign::compat_actor) |
| `Actor_Network.cpp` | 1 | 2 | the definition: Actor() is now the compatibility accessor (campaign::compat_actor) |
| `CampaignServerStart.cpp` | 1 |  | the server's own no-view check (S02-T05d) |

## The three B21 sites

| Site | Before | After S03-T02 |
|---|---|---|
| `ai/phantom/phantom.cpp:86,149,266` (`CPhantom::net_Spawn`, `SwitchToState_internal`, `UpdateFlyMedia`) | `m_enemy = Level().CurrentEntity()`, dereferenced without a check; faults headless | `m_enemy = campaign::nearest_participant_actor(Position())`; with no participant actor the phantom spawns facing north, logs it, and its fly update switches it to idle, which destroys it; the contact hit and fly media are guarded |
| `ai/trader/ai_trader.cpp:99` (`CAI_Trader::LookAtActor`, a bone callback) | `Level().CurrentEntity()->Position()`; faults headless | faces `nearest_participant_actor(Position())`; with none the callback returns and the bone keeps its transform |
| `ai/monsters/pseudodog/psy_dog_aura.cpp:63,130` (`CPsyDogAura::reinit`, `update_schedule`) | guarded by patch 0020: no controlled entity, no aura | `m_actor = campaign::local_view_actor()` (the aura is a post-process on the viewer, driven by what that viewer's actor and the phantoms see of each other); the effector goes on `m_actor`; null on the server, so the guard stays and is now the rule, not a workaround; the ability is on the S03-T04 list |

## Remaining single-actor assumptions on the server

`campaign::report_actor_access` logs every distinct caller of `Actor()` on
the server with its symbol and call count (the campaign server build ships
`xrGame.pdb`). The GameState smoke's `campaign-authority-passed` stage and
the identity service's stop line print the table; the S03-T02 validation
report records the callers observed in the fixture run. A caller that is
not a script binding, a presentation site or a recorded ability is a
defect.

## Control sites — S03-T03

`Level().CurrentControlEntity()` names the actor the process drives by
input. 23 sites in `src/xrGame` (raw grep, 2026-10-19, before the change)
outside `Level*.cpp`. Classes as above; "control" is new: the site decides
something for the actor this process simulates by input.

| File | Sites | Class | S03-T03 |
|---|---|---|---|
| `Actor.cpp:1825` (`shedule_Update`) | 1 | control | `campaign::controls_actor(this)`: the control path runs for every participant actor on the server |
| `Actor.cpp:655,1045,1075` (sprint cut on hit, level-border callback, contact damage) | 3 | control | `controls_actor(this)` |
| `Actor_Movement.cpp:153`, `Actor_Weapon.cpp:150,161` (climb weapon-hide, sprint-disable and weapon-hide events) | 3 | control | `controls_actor(this)` |
| `Actor_Movement.cpp:339` (moving-camera effector) | 1 | view | the viewer's own actor (`local_view_actor() == this`), none on the server |
| `Actor.cpp:621,1944,2416`, `ActorAnimation.cpp:728`, `ActorCameras.cpp:484,554`, `ActorNightVision.cpp:31,46,60`, `Actor_Feel.cpp:282` | 11 | view / debug / MP | unchanged (null on the server; the night-vision effector and auto-pickup compare with the view entity) |
| `Level_input.cpp`, `Level.cpp`, `Level_network_spawn.cpp` | — | view | input and the spawn-time adoption of the local entity; unchanged (S02-T05d keeps the server from adopting one) |
| `CampaignAuthorityFixture.cpp:310`, `CampaignServerStart.cpp` | 2 | fixture | the server's own no-control-entity checks |

`CActor::Die` ran the single-player game-over sequence (`start_tutorial`)
for any dying actor; it now runs for the viewer's own actor only. The
installed actor Lua binder attaches to the single-player script actor only
on the server (`script_binder.cpp`, `campaign::script_binding_allowed`).

### Script bindings that dereferenced `Actor()` — S03-T03

With two participant actors `Actor()` returns null on the server, and
smoke-107 showed the first binding to dereference it:
`level.remove_pp_effector` faulted inside actor 1's Lua binder update and
the S02-T06 exception filter cleared that binder. The `level.*` and
game-object bindings below are now null-safe; their participant context is
still S03-T05.

| File | Sites | Class | S03-T03 |
|---|---|---|---|
| `level_script.cpp` `add_cam_effector` (5), `remove_cam_effector`, `set/get/check_cam_effector_*` (4), `set_cam_position_direction`, `remove_cam_position_direction`, `remove_hud_motion_cam_effectors`, `add/remove_complex_effector`, `add_pp_effector`, `remove/set/check/get_pp_effector_*` (6) | 23 | view | the viewer's own actor (`local_view_actor()`); with none the effector is dropped (an added one is deleted, a query returns 0/false), the rule S03-T02 set for `pp_effector_custom.cpp` |
| `level_script.cpp` `add_actor_points*`, `get_actor_points`, the Discord RPC updates, `get_pick(TT_ACTOR)`, `actor_safemode`, `actor_set_safemode`, `update_pda_news_from_uiwindow` | 10 | script (S03-T05) | null-guarded through the counted `Actor()`; a null returns 0/false/no-op |
| `script_game_object_inventory_owner.cpp` `_give_news` | 2 | script (S03-T05) | null-guarded: news for "the" actor are dropped until S03-T05 names the receiving participant |
| `script_game_object_inventory_owner.cpp` `ActorLookAtPoint`, `ActorStopLookAtPoint` | 2 | self | the game object's own camera (`smart_cast<CActor*>(&object())`) |

### The installed `se_actor.script` state gate — S03-T03

`se_actor:STATE_Read` reads its save marker and `start_position_filled`
flag only `if db.actor == nil`, so a second actor's spawn packet is read 3
bytes short and its update block (health, position) is misparsed.
`CSE_Abstract::Spawn_Read` now resumes at the end of the state frame and
counts the mismatch (`g_spawn_state_resyncs`, logged); see
[participant-identity.md](participant-identity.md#per-participant-control-and-the-single-player-script-actor--s03-t03).

### Installed script gates on the second actor — S03-T03

| Script | Site | Effect on a second actor | Owner |
|---|---|---|---|
| `se_actor.script:40-54` | `STATE_Read` reads its bytes only `if db.actor == nil` | the spawn packet's update block is misparsed; neutralised by the state-frame resync in `CSE_Abstract::Spawn_Read` | S03-T05 |
| `bind_stalker.script:29-142`, `db.script:102-112` | `actor_binder` sets `db.actor`, resets and finalizes the weather, surge and psi-storm managers | would re-point the globals at the second actor and tear them down on its destruction; the binder attaches to the script actor only (`script_binding_allowed`) | S03-T05 |
| `_g.script:1221,2838`, `bind_stalker_ext.script:74-76` | `CActor__BeforeHitCallback`: 7 s invulnerability after the first binder update, a friend check against `db.actor` whose `IsStalker` includes actors, artefact immunities from `db.actor`'s belt | any actor's hits are dropped during the window, hits sourced from an actor (the other participant, or the victim itself) are dropped as friendly fire, damage is scaled by actor 1's belt | S03-T05 (per-participant gate), Stage 6 (friendly fire) |
| `level_script.cpp` `show_indicators`/`hide_indicators` | set and clear the global `AF_GODMODE_RT` | god mode is process-wide; the fixture clears and restores it | Stage 6 |

## Script context sites — S03-T05

The sites S03-T02/T03 left for "the script context" now name their
participant ([participant-identity.md](participant-identity.md#native-and-lua-participant-script-context--s03-t05)).

| File | Site | S03-T05 |
|---|---|---|
| `GameTask.cpp` | `ChangeStateCallback`, `CheckInfo`, `SendInfo` | an owned task: the owner's actor; its state callback goes to the script host's handler inside the owner's context. An unowned task (client, checkpoint-loaded) keeps `sole_participant_actor` |
| `GametaskManager.cpp` | `UpdateTasks` | each owned task is updated inside its owner's context |
| `script_game_object_inventory_owner.cpp` | `GiveTaskToActor` | records the receiving actor's participant as the task's owner |
| `script_game_object_inventory_owner.cpp` | `GiveGameNews`, `AddIconedTalkMessage` | server: the receiving actor's participant's presentation (`News`, `TalkMessage`); client unchanged. `_give_news` stays client-only |
| `Actor_Communication.cpp` | `CActor::RunTalkDialog` | server: `campaign::dialogue_open` (the actor's context, `DialogOpen` presentation); client unchanged |
| `PhraseScript.cpp` | the five `CDialogScriptHelper` script calls | run in the actor speaker's participant context |
| `UsableScriptObject.cpp` | `CUsableScriptObject::use` | runs the use callback in the user's participant context |
| `Actor.cpp` | `CActor::Die` game-over sequence | server: `GameOver` presentation to the dying participant |
| `level_script.cpp` | `add_cam_effector` (5), `remove_cam_effector`, `add_pp_effector`, `remove_pp_effector` | server: the script context participant's presentation; dropped and counted without a context (S03-T03 dropped them all). An added camera effector returns its length when presented |
| `level_script.cpp` | `add_actor_points`, `add_actor_points_str`, `get_actor_points`, `actor_safemode`, `actor_set_safemode`, `get_actor_alcohol` | `gameplay_script_actor`: the context's actor, else the single-actor rule |
| `level_script.cpp` | Discord RPC, `get_pick(TT_ACTOR)`, `g_get_target_pos`, `update_pda_news_from_uiwindow`, `level.debug_actor` | unchanged: presentation, UI or debug (null with two actors on the server) |

### Installed-script forms of "the player" — S03-T05 audit

| Form | Uses (installed `scripts.db0`) | Inside a participant context | Outside |
|---|---|---|---|
| `db.actor` | 3,228 in 245 scripts | the context's actor | the binder's actor; reads counted by site while two actors live |
| `AC_ID` | 290 in 76 scripts | the context's actor's id | 0 |
| `has_alife_info` → `alife():has_info(0, …)` | 638 (+4 direct `has_info(0, …)` calls) | **actor 0** (not remapped; S08) | actor 0 |
| `alife():actor()` | 58 | **the ALife actor** (not remapped; S08) | the ALife actor |
| story object `"actor"` | 20 | not remapped (S08) | — |
| `CreateTimeEvent` | 114 calls | the creating context is captured and re-entered | no context |

The runtime audit (out-of-context `db.actor` reads while two actors lived)
is in [S03-T05](validation/S03-T05.md).
