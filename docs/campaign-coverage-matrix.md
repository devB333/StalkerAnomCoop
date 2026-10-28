# Campaign coverage matrix — S10-T01

The bounded inventory of what the installed Anomaly 1.5.3 campaign contains,
which parts the co-op server already covers with evidence, which parts the
release must cover (as child tasks with stable IDs), and which parts are
excluded and must not be represented as compatible (R12, G10). Written
2026-10-28 against the accepted test installation
(`builds/build_2/test-install/source-client/db/configs/{scripts,configs,spawns}.db0`,
hashes in [S10-T01](validation/S10-T01.md)) and engine `c57fe5b9` with patches
0001–0057. The scope decisions this matrix depends on are listed for the
user's approval in [Stage 10 decisions](stage10-decisions.md) (T01-1…T01-16);
until they are approved the "required" column is a proposal.

Method: targeted search of the extracted installed scripts (413 `.script`
files), configs (3,802 files) and the spawn file (`all.spawn`, a string scan
for `level_changer` entries), the engine source in `builds/build_1/engine-work`,
and the earlier audits ([actor access inventory](actor-access-inventory.md),
[S03-T04 ability table](validation/S03-T04.md#player-bound-ability-table-for-s10-t02),
[S03-T05 runtime audit](validation/S03-T05.md)). No whole-binary analysis, no
indiscriminate extraction beyond those three archives, no runtime test: this
task plans; its counts are inventory, not coverage claims.

## 1. State classes and the owner/context policy

Every family below is classified with the four state classes of R09; the
policy names which actor a native or Lua "the player" means in that family.

| Class | Meaning | Authority and durability | Examples |
|---|---|---|---|
| **World** | one value for the one world | the server; checkpoint and, where critical, the journal | ALife, squads, smart terrains, game time and weather, trader stock, dynamic faction relations, emission schedule, stash contents |
| **Party** | one value shared by both players | the server; journaled when it is progression | info portions that are story progress, party tasks (S08 policy), map spots, stash markers, dialogue commits that change the world |
| **Per player** | each participant actor has its own | the server, in that participant's context; journaled when critical | health and survival values, inventory, money, goodwill and reputation, personal info portions, portable store (pstor), companions, sleep state, task rewards |
| **Client-local** | presentation only | the owning client; a late join gets a replay or nothing | HUD, news, PDA tabs, camera and sound effects, statistics display |

Rules that follow from the accepted work and are applied to every family:

- Code that runs for an actor (a use, a dialogue, a task callback, a hit)
  runs inside that participant's script context (S03-T05); `db.actor` and
  `AC_ID` are that actor.
- Code that runs for an NPC or a world object with no actor in hand (NPC
  condlists, meet logic, squad relationship condlists, smart-terrain
  condlists) evaluates against the **nearest alive participant actor** to
  that object, as the engine's ALife switching already does (S03-T04,
  decision T01-5 below). Presence checks (`actor_in_zone`,
  `dist_to_actor_le`) mean **any** participant unless the content names a
  player.
- `has_alife_info`, `alife():actor()` and the story object `"actor"` resolve
  to the **party** info set for story progress and to the context actor for
  personal portions (decision T01-4); the personal list is explicit.

## 2. Inventory of the installed campaign

### 2.1 Levels and passages (L50, L51, L52)

33 levels are installed (`db/levels/level_*.db0`). The spawn file holds
**122 named level changers**: 118 parsed with their destination, forming
**95 directed passages** between the 33 levels, plus 4 whose destination did
not parse and is inferred from their names (`mar_level_changer_to_agroprom_1`,
`yan_level_changer_to_rostok_1`, `aes2_level_changer_to_gen`,
`agru_level_changer_to_agroprom_6`), which add 3 passages: **98 directed
passages** in all (string scan by `tools/list-passages.py`; two binary-named
entries and the marsh `fake_level_changer` are not passages).
The full table is in `builds/logs/S10-T01/passages.tsv`. Travelled so far:
Cordon → Darkscape and Cordon → Garbage (S09-T02/T04). Beside the changers,
four script paths call `ChangeLevel` and skip game time: guides
(`ka_travel.script:62`), fast travel (`game_fast_travel.script:121`), the
backpack travel (`game_backpack_travel.script:93`) and the MLR utilities
(`mlr_utils.script:71`); the same-level case uses `set_actor_position`.

| Level | Out-passages (to) |
|---|---|
| `k00_marsh` (Great Swamp; fixture save 1) | `l01_escape` ×2, `l03_agroprom` |
| `l01_escape` (Cordon; fixture save 2) | `k00_marsh` ×2, `l02_garbage` ×2, `k01_darkscape`, `y04_pole` |
| `l02_garbage` | `l01_escape` ×2, `l03_agroprom` ×2, `l04_darkvalley` ×2, `l05_bar`, `l06_rostok` |
| `l03_agroprom` / `l03u_agr_underground` | `k00_marsh`, `l02_garbage` ×2, `l08_yantar`, underground ×6 / agroprom ×6 |
| `k01_darkscape`, `k02_trucks_cemetery`, `y04_pole` | escape, darkvalley / darkvalley, bar, military / escape, darkvalley |
| `l04_darkvalley` / `l04u_labx18` | darkscape, trucks cemetery, garbage ×2, labx18, pole / darkvalley |
| `l05_bar`, `l06_rostok` | trucks cemetery, garbage, rostok, military / garbage, bar, yantar |
| `l07_military`, `l08_yantar` / `l08u_brainlab` | trucks cemetery, bar, dead city, radar, red forest / agroprom, brainlab, dead city, rostok / yantar ×2 |
| `l09_deadcity`, `l10_limansk`, `l11_hospital` | military, yantar, limansk / dead city, red forest, hospital / limansk, stancia 2 |
| `l10_radar` / `l10u_bunker`, `l10_red_forest` | military, red forest, bunker, pripyat / radar; jupiter, military, limansk, radar |
| `l11_pripyat`, `l12_stancia`, `l12_stancia_2` | jupiter, jupiter underground, radar, stancia, CoP pripyat / pripyat, stancia 2, zaton / hospital, stancia, control monolith, sarcofag, generators |
| `l12u_control_monolith`, `l12u_sarcofag`, `l13_generators`, `l13u_warlab` | stancia 2 / stancia / stancia 2, warlab / sarcofag, generators ×2 |
| `zaton`, `jupiter`, `jupiter_underground`, `pripyat`, `labx8` | jupiter ×2, stancia, pripyat / zaton ×2, pripyat, jupiter underground ×2, red forest, soc pripyat / jupiter, pripyat ×2 / jupiter, soc pripyat, labx8, zaton / pripyat |

A new game starts through `start server(all/single/alife/new)`
(`ui_main_menu.script:255`) after the faction/loadout dialog
(`ui_mm_faction_select.script`); the co-op server has never run that path
(every session starts from a save, L51).

### 2.2 Scripts and configs by family

| Family | Installed content (counts) | Key sites of "the player" |
|---|---|---|
| Stalker NPC logic | `xr_logic` (1,329 lines), `xr_meet` (829), `xr_danger`, `xr_combat_ignore` (10 `AC_ID`), `xr_motivator`, `xr_corpse_detection`, `xr_gather_items`, `gameplay_disguise` (26 `db.actor`), 18 `state_*` scripts, 14 `sr_*` restrictor scripts | `xr_logic.script:372` (every NPC condlist, 82,301 out-of-context reads in the S03-T05 audit), `xr_meet` (13 lines, ~95,000) |
| Mutants | 17 engine monster classes (`class_registrator.script:39-56`: bloodsucker, boar, dog, flesh, pseudodog, burer, cat, chimera, controller, fracture, poltergeist, pseudogigant, zombie, snork, tushkano/rat, psy dog, psy-dog phantom) behind 26 `creatures/m_*.ltx` kinds (the Anomaly extras karlik, lurker, psysucker, bibliotekar, rotan, gigant jumper, borya are config variants of those classes) | the S03-T04 ability table: anti-aim, controller psy hit, auras, burer, poltergeist, bloodsucker vampire/alien/predator, scanning; `controller_psy_hit.cpp` 37 `Actor()` uses, `monster_aura.cpp` 13, `bloodsucker_alien.cpp` 13 |
| Companions | `axr_companions.script` (1,965 lines, 22 `db.actor`), `dialogs_axr_companion`, `xr_companion`, `txr_paid_companions`, `ui_companion_inv` | `companion_<n>` slots in `db.actor`'s pstor (`:238-250`), follow/trade/talk against `db.actor` |
| Offline ALife and spawning | `sim_board` (429), `sim_squad_scripted` (1,442; 20 `db.actor`, 3 `alife():actor()`), `smart_terrain` (1,787), `simulation_objects`, `sim_offline_combat`, 39 squad descriptions, `misc/simulation.ltx`, `dynamic_spawn.ltx` | online exclusion by distance to `db.actor` (`sim_squad_scripted.script:1079-1093`), `is_on_the_same_level(alife():actor())` (`simulation_objects.script:193`), relationship/target condlists with `db.actor` (`:61,116`) |
| World time, sleep, weather | `ui_sleep_dialog` (356; `level.change_game_time` at `:243`), `actor_status_sleep`, `level_weathers`; time skips also in guides, fast travel, backpack travel, the emission managers | `actor_is_sleeping`/`sleep_active` info portions (`ui_sleep_dialog.script:199-260`) |
| Emissions and psi-storms | `surge_manager` (1,650; 57 `db.actor`, 29 `AC_ID`), `psi_storm_manager` (717; 11 `db.actor`), `misc/surge_manager.ltx` | cover check and hits on `db.actor` (`surge_manager.script:956-977, 1025-1045`), `surge_immuned` pstor flag, psi-storm vortex hits (`psi_storm_manager.script:432-452`) |
| Dialogue | 25 `gameplay/dialogs*.xml`, 15 `dialogs_*.script`, `dialog_manager`, `dialogs.script`; engine `PhraseDialog`, `PhraseScript` | the talk window (L43), `dialogs.who_is_npc` by `AC_ID` |
| Trade and economy | 36 `items/trade/trade_*.ltx`, `trade_manager.script` (239; 10 `db.actor`), `axr_trade_manager`, `item_upgrades.ltx`, `stalkers_upgrade_info.ltx`, `inventory_upgrades`; engine `CTrade`, `UITradeWnd` | trade availability and price condlists against `db.actor` (`trade_manager.script:26-227`) |
| Factions and reputation | `game_relations.script` (1,044; 18 `db.actor`, 4 `alife():actor()`), `creatures/game_relations.ltx`, `faction_expansions`, `bind_faction`; engine `RELATION_REGISTRY` (`relation_registry.h:47-57`: goodwill keyed by character id) | dynamic faction relations saved in `db.actor`'s pstor (`drx_df_*`, `game_relations.script:202-213`), goodwill set on `db.actor` (`:716-828`), community checks on `alife():actor()` |
| Stashes | `treasure_manager.script` (778), `misc/secrets.ltx`, `secret_{zaton,jupiter,pripyat}.ltx`, `tasks_stash` | toolkit reward counters in `db.actor`'s pstor (`treasure_manager.script:379-393`) |
| Tasks | 29 `misc/task/tm_*.ltx` (610 task sections: 68 dynamic, 89 warfare, 453 level- and story-bound; 218 `storyline = true`), 22 `tasks_*.script` types (fetch, multifetch, delivery, assault, defense, bounty, clear map, dominance, faction control, fate, guide, measure, stash, anomaly scanner, chimera scan, agent rescue, recover item/mutant data, pump station, smart control, top 10, vehicle destroy), `task_manager`, `axr_task_manager`, `task_status_functor` (91 `has_alife_info`) | S08 covered one fetch task with the party policy; `task_manager.script:123,147` (tasks with no owner) |
| Storyline chains | DoctorX storyline (`drx_sl_*` per faction honcho, `tm_<level>.ltx`), the four Lost Zone chains (`tm_lostzone_hb/ll/ms/oa.ltx`: 14, 26, 47, 10 sections; `dialogs_lostzone`, `tasks_fate`), MLR (`tm_mlr.ltx`, `dialogs_mlr` 42 `db.actor`), `story_objects.script`, `game_achievements` (55 `has_alife_info`) | info-portion gates everywhere; `dialogs_bar.script` 49 `db.actor`, 35 `has_alife_info` |
| Presentation scripts | `news_manager`, `dynamic_news_manager`, `pda*`, `ui_*` (37 scripts), `actor_effects` (57 `db.actor`), `game_statistics`, `actor_menu` | client-local by L05; the server drops out-of-context effectors (206 counted in S03-T05) |
| Optional game modes | `warfare*` (6 scripts, 89 tasks), `gamemode_azazel`, `gamemode_ironman`, `simulation_survival_mode.ltx`, `gamemode_timer` | `alife():actor()` 9 in azazel; `warfare_faction_control` 43 `db.actor` |
| Holders | vehicles (`se_car`, `vehicles.ltx`; `Car.cpp` 15 `Actor()`), mounted guns (`WeaponStatMgun.cpp` 34), helicopters (8 `heli_*` scripts; engine `helicopter*.cpp` 0 `Actor()`) | the holder's owner is "the actor" (L52) |
| Runtime overlay | 70 scripts in `gamedata/scripts` of the test install (Modded Exes options, `aaaa_script_fixes_mp`, `LuaPanda`, `bind_monster`, `item_weapon`) | `aaaa_script_fixes_mp.script:852` in the S03-T05 audit; the rest is options UI |

### 2.3 Where the remaining global-player sites are (ranked)

Static counts over the installed scripts (per file, 2026-10-28) and the
runtime audit of S03-T05 (out-of-context `db.actor` reads while two actors
lived) agree on the order. Engine sites outside the monster code are
presentation (`player_hud`, `PDA`, `HUDTarget`, cameras), holders (above) or
already converted ([inventory](actor-access-inventory.md)).

| Rank | Site | Count | Family and child |
|---|---|---|---|
| 1 | `xr_logic.script:372` and the condlist evaluators (`xr_conditions`, 3,310 lines) | 82,301 reads at runtime | NPC logic → S10-T02a |
| 2 | `xr_meet.script` (13 lines) | ~95,000 reads | NPC logic → S10-T02a |
| 3 | `surge_manager.script` | 57 `db.actor`, 29 `AC_ID` | emissions → S10-T05a |
| 4 | `actor_effects.script` | 57 | presentation → S10-T05d |
| 5 | `ui_inventory.script` | 51 | client UI (covered by S08: the server validates) |
| 6 | `tasks_fetch`, `tasks_defense`, `tasks_fate`, `tasks_delivery`, `tasks_measure`, `task_status_functor` | 49, 28, 25, 22, 19, 27 (+91 `has_alife_info`) | tasks → S10-T05b |
| 7 | `dialogs_bar`, `dialogs_mlr`, `dialogs_lostzone`, `dialogs_devushka`, `dialogs_yantar`, `dialogs_agr_u` | 49, 42, 28, 19, 15, 15 | story dialogue → S10-T04a/b, S10-T05c |
| 8 | `warfare_faction_control` (43), `gamemode_azazel` (22) | excluded modes | — |
| 9 | `xr_meet`/`gameplay_disguise` (26), `xr_combat_ignore` (10 `AC_ID`), `xr_danger`, `xr_motivator` (15) | NPC logic → S10-T02a |
| 10 | `axr_companions` (22), `axr_task_manager` (24), `bind_stalker_ext` (24) | companions → S10-T02f; tasks → S10-T05b |
| 11 | `sim_squad_scripted` (20 + 3), `game_relations` (18 + 4), `simulation_objects` (2) | ALife → S10-T03a/b; factions → S10-T04d |
| 12 | `itms_manager` (34), `item_weapon` (25), `item_device` (20), `item_*` | per-item scripts in the user's context (S08-T07 covers use; equip of devices and weapons' script work is re-checked in S10-T04c) |
| 13 | `info_portions.script` (16; 10 `has_alife_info`), `game_achievements` (55 `has_alife_info`), `game_statistics` (13) | party info and per-player statistics → S10-T04b, S10-T05d |
| 14 | `trade_manager` (10), `treasure_manager` (5 + 1), `death_manager` (1) | economy → S10-T04c/e; loot → S10-T02b |
| 15 | engine: `controller_psy_hit.cpp` (37), `monster_aura.cpp` (13), `bloodsucker_alien.cpp` (13), `poltergeist_telekinesis.cpp` (11), `burer.cpp` (10), `anti_aim_ability.cpp` (9) | mutants → S10-T02c/d/e |
| 16 | engine: `WeaponStatMgun.cpp` (34), `Car.cpp` (15), `HolderEntityObject.cpp` (8), `CarCameras`/`CarInput` (10) | holders: excluded (T01-12) |

## 3. The matrix

Status values: **Covered** (evidence exists, linked), **Required** (a child
task below must deliver it before G10), **Untested** (installed, runs as is,
no fixture; not claimed), **Excluded** (not part of the release; must not be
represented as compatible). The "evidence required" column is what the
child's validation report must show for the family to count as covered.

| # | Family | Owner / context policy | Fixture | Evidence required | Child | Status |
|---|---|---|---|---|---|---|
| F1 | Stalker NPC logic: condlists, meet/greet/usability, danger, combat-ignore, motivator, disguise | NPC code evaluates against the nearest alive participant; presence checks mean any participant; disguise per player | two actors at different distances from a trader, a guard squad and a hostile squad in Cordon; one disguised | both players can talk to a usable NPC; greeting and usability decided per approaching player; a hostile squad engages either; disguise fools NPCs only for the disguised player; the out-of-context audit count for these lines falls to 0 | S10-T02a | Required |
| F2 | NPC death, loot and looting: `death_manager`, `xr_corpse_detection`, `xr_gather_items`, `release_body_manager` | world (loot generated once on the server); an NPC's take from a body is a server transfer (T08-2) | a stalker loots a body both players saw; a body released; restart in between | both clients' item graphs match the server's after an NPC loot and a release; no duplicate loot after a restart (T08-7 stays) | S10-T02b | Required |
| F3 | Controller: psy hit, possession, aura; psy-dog aura | the ability targets its chosen enemy (either participant); auras affect every participant in range by its own distance | a controller and a psy dog beside the two actors; one player out of range | either player is psy-hit; the effect and the camera go to the hit player only; the other is unaffected; both converge under reconnect | S10-T02c | Required (L12) |
| F4 | Bloodsucker (vampire, alien vision, invisibility, predator), burer (telekinesis, stamina hit, shield), poltergeist (telekinesis, flames, detection) | the ability's target is its enemy participant; shield reacts to the shooter | each kind spawned beside the two actors in Cordon (the S03-T04 guards are unexercised) | each ability lands on the targeted player, not on actor 0 by default; no fault with two actors (the S03-T04 guards become real behaviour); deaths converge | S10-T02d | Required (L12) |
| F5 | Generic mutants (boar, dog, flesh, pseudodog, cat, chimera, fracture, pseudogigant, zombie, snork, tushkano/rat, phantoms) and AI lighting | the enemy manager picks either participant; threaten/scan abilities target the chosen enemy; AI lighting per participant view (L13) | a smoke stage that spawns each class beside two actors, one in the dark | each class attacks either player; lighting from the second player's view counts; the Anomaly config variants (karlik, lurker, psysucker, bibliotekar, rotan, gigant jumper, borya) run their base class and stay **Untested** until a fixture spawns them | S10-T02e | Required (L12, L13); variants Untested |
| F6 | Companions (`axr_companions`) | per player: a companion follows and obeys its recruiter; its inventory transfers are the recruiter's server actions | each player recruits one companion; travel; restart | each companion follows its own player; commands from the other player are refused; survives travel and restart | S10-T02f | Required if T01-10 approved, else Excluded |
| F7 | Online/offline switching, squads, smart terrains, respawn | world; activation by the union of participants (S03-T04); online exclusion and same-level checks use the nearest participant, never actor 0 | players far apart; one disconnects; a travel; a restart | no duplicate squad or resurrected NPC after switch, travel or restart; identity reuse stays correct; tick cost recorded | S10-T03a | Required (L53 watch) |
| F8 | Offline combat, squad targets and relationship condlists | world; `assigned_target = actor` means the nearest participant; condlists per rule 2 | a squad sent against "the actor"; offline combat between squads near one player | the squad finds either player; offline combat results match single player's rules | S10-T03b | Required |
| F9 | World time and sleep | the server owns the clock; sleep is a party consent (T01-8); forced blackout per player, no skip | both sleep; one declines; one passes out from exhaustion | one time skip for the world after both agree; emission and weather follow; a blackout takes one player's control only | S10-T03c | Required (L49) |
| F10 | Guides, fast travel, backpack travel, MLR teleports | party travel under the S09 contract (T01-9); fee from the requester; same-level teleport moves both | a guide dialogue; a fast-travel marker; the backpack | both consent, both arrive, time skipped once, fee paid once; refusals as in S09 | S10-T03d | Required |
| F11 | Passages: 98 directed, 33 levels | the S09 travel contract | the travel harness driven over every passage both ways from a scripted party position at each changer | every passage travelled headless with both clients rejoining; every destination level loads; failures listed by passage | S10-T03e | Required (L50) |
| F12 | New game | the server starts the native new game for the party faction (T01-2/T01-3); each player's loadout per player | start a new game as two factions' starts | both actors spawn at the start with their loadouts; the first checkpoint publishes; the first quest giver works | S10-T03f | Required if T01-2 approved, else Excluded (sessions start from a host save) |
| F13 | The talk window | the client's talk UI drives the server's `PhraseDialog`; one speaker per NPC at a time (T01-15) | both players at Sidorovich; both press talk | the second gets "busy"; accept and turn-in of the S08 party quest by talking; dialogue commits once | S10-T04a | Required (L43, L01) |
| F14 | Info portions and actor-2 script state | story portions party-shared, personal list per player (T01-4); actor 2's pstor and binder state persisted (L32, L44) | a story portion given to one player; a personal one; restart; participant 1 dead | `has_alife_info` answers the same for both players for story portions and per player for personal ones; actor 2's pstor survives a restart | S10-T04b | Required (L11, L32, L44) |
| F15 | Trade, repair and upgrades | money per player; trader stock and restock world; the trade window one buyer at a time; repair/upgrade on the server in the buyer's context (T01-7) | both buy from one trader; concurrent purchase of the last item; a repair; an outfit with belt slots then an artefact on the belt (L48) | one winner for the last item; money and stock converge; no duplicate item or money through every crash window; the artefact-belt path exercised | S10-T04c | Required (L01 trade, L36 trade, L48) |
| F16 | Factions and reputation | goodwill and reputation per player (native per-character registry); faction-vs-faction relations world (T01-6) | one player angers a faction; the other trades with it; a dynamic relation change | goodwill differs per player; the faction's NPCs treat each player by his own goodwill; the world relation change applies to both | S10-T04d | Required |
| F17 | Stashes and treasure | contents world (first come, journaled as S08 transfers); markers party (T09-3) | a stash rewarded to one player; both open it; a restart | one stash, one marker on both maps, contents taken once | S10-T04e | Required |
| F18 | Emission and psi-storm gameplay | schedule world; cover check, damage, immunity and death camera per player (T01-11) | an emission with the players apart, one in cover; a psi-storm | the covered player lives, the exposed one dies; the camera and sounds go to the right player; a late join sees the right phase | S10-T05a | Required (L28, L29) |
| F19 | Dynamic task types (22) | party task under the S08 policy (anchor, pooled progress, per-player rewards), extended to every type; timed, repeatable, failed and cancelled transitions on the server | one task of each type from `tm_dynamic.ltx` and the level files; owner death; restart | each type accepts, progresses and completes/fails once for the party; rewards per the S08 policy; survives restart and travel | S10-T05b | Required (L10) |
| F20 | Storyline chains | party progression (F14 policy); either player may contribute; the chain's info gates are party | the DoctorX chain in Cordon and one Lost Zone chain start to a travel and a restart | both players see the same chain state; a step done by either advances it once; recovery keeps the step | S10-T05c | Required for the DoctorX and Living Legend chains; the other chains **Untested** unless T01-16 widens it |
| F21 | Presentation: news, PDA, HUD scripts, statistics, achievements, use animations | client-local, driven by per-player presentation records; late join replays news only (T09-4) | the S06/S08 fixtures with the audit of dropped presentations | dropped-presentation count 0 for the covered families; the partner's use animation (L47) shown or recorded as not | S10-T05d | Required (L05, L06, L33, L47) |
| F22 | Warfare, Azazel, Ironman, survival mode, timer modes | — | — | — | — | Excluded (T01-12, L31) |
| F23 | Vehicles, mounted guns, helicopters | — | — | — | — | Excluded (T01-12, L52); helicopters Untested |
| F24 | Debug tools, multiplayer game modes, the Modded Exes options overlay | the overlay runs as installed; options are client-local | — | — | — | Excluded from claims; the overlay's one audited site (`aaaa_script_fixes_mp.script:852`) is checked with F1 |

Already **Covered** before this matrix (not repeated above): movement,
combat and deaths (G06); pickup, drop, transfers, equip, use, containers,
stacks (G08); one fetch quest with the party policy and its presentation
(G08); coordinated level change on two passages (G09); storm and emission
visuals (S06-T08); the ALife activation union (S03-T04); per-player survival
values (S08-T07 revision).

## 4. Child tasks

Stable IDs; each is fixture-sized and gets its own `docs/validation/<ID>.md`.
Prerequisites inside Stage 10 are listed; everything requires the approved
decisions of [Stage 10 decisions](stage10-decisions.md).

| Child | Title | Prerequisites | Owns limits | Families |
|---|---|---|---|---|
| S10-T02a | NPC logic per participant (condlists, meet, danger, combat-ignore, disguise) | — | L11 (NPC part) | F1 |
| S10-T02b | NPC loot and looting as server transfers | S10-T02a | — | F2 |
| S10-T02c | Controller and psy auras per participant | — | L12 (part) | F3 |
| S10-T02d | Bloodsucker, burer, poltergeist abilities per participant | — | L12 (part) | F4 |
| S10-T02e | Generic mutants, phantoms and AI lighting with two actors | — | L12 (rest), L13 | F5 |
| S10-T02f | Companions per recruiter | S10-T02a, T01-10 | — | F6 |
| S10-T03a | ALife switching and respawn across travel, disconnect and restart | — | L53 (watch) | F7 |
| S10-T03b | Offline combat and squad targets with two participants | S10-T03a | — | F8 |
| S10-T03c | World time and sleep in co-op | — | L49 | F9 |
| S10-T03d | Guides, fast travel and backpack travel as party travel | S10-T03c | — | F10 |
| S10-T03e | Passage sweep over every level changer | — | L50 | F11 |
| S10-T03f | New game on the campaign server | T01-2 | L51 | F12 |
| S10-T04a | The talk window | — | L43, L01 (dialogue) | F13 |
| S10-T04b | Party info portions and actor-2 script state | S10-T04a | L11 (info part), L32, L44 | F14 |
| S10-T04c | Trade, repair and upgrades | S10-T04a | L01 (trade), L36 (trade), L48 | F15 |
| S10-T04d | Factions and reputation per player | S10-T04b | — | F16 |
| S10-T04e | Stashes and treasure | S10-T04c | — | F17 |
| S10-T05a | Emission and psi-storm gameplay per participant | S10-T03c | L28, L29 | F18 |
| S10-T05b | Dynamic task types under the party policy | S10-T04a, S10-T04b | L10 | F19 |
| S10-T05c | Storyline chains (DoctorX, Living Legend) | S10-T05b, S10-T03e | — | F20 |
| S10-T05d | Presentation split: per-player news, PDA, HUD scripts, statistics | — | L05, L06, L33, L47 | F21 |

Suggested order: S10-T04a → S10-T04b (they unblock most script families),
then S10-T02a, S10-T03a, S10-T03e in any order; S10-T02c/d/e are
independent; S10-T05 children last. The parent tasks S10-T02…T05 are
Completed when their children are.

## 5. Release scope and exclusions (proposal, pending approval)

- **Required for release:** F1–F5, F7–F11, F13–F21 as listed; with F6 and
  F12 if T01-10 and T01-2 are approved.
- **Explicitly excluded** (the release notes name them; the server refuses
  or ignores them, as it refuses level changes it does not know): Warfare,
  Azazel, Ironman, survival mode (F22); vehicles, mounted guns (F23); debug
  tools and multiplayer game modes (F24).
- **Untested, not claimed:** the Anomaly mutant config variants (F5),
  helicopters (F23), the Lost Zone chains other than Living Legend and the
  MLR chain (F20), the levels no fixture visits beyond the passage sweep's
  load check (F11).

## 6. Carried limits touched

| Row | This task |
|---|---|
| L48 | still holds; routed to S10-T04c (an outfit with belt slots bought from a trader, then an artefact on the belt) |
| L50 | still holds; the release needs every passage (T01-1): the 98 directed passages of §2.1 are the scope; owner S10-T03e |
| L51 | still holds; a new game is proposed as S10-T03f (T01-2); if declined it stays a product limit owned by S11 |
| L52 | still holds; vehicles are proposed as excluded (T01-12); if approved the row stays open as a product limit and is listed in the release notes |
| L10, L11, L12, L13, L28, L29, L32, L33, L43, L44, L47, L49, L53 | owners confirmed or narrowed to the children in §4 |
