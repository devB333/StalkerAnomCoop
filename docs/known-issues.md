# Known issues

Bugs found while testing that are not fixed yet, written so a session that has
not seen them can pick one up. Each has what was seen, the evidence, what was
already ruled out, where to look, and how to check a fix. Limits accepted at a
gate are in [carried limits](carried-limits.md); this list is for defects.

When an issue is fixed, move it to "Fixed" with the report and run names.

## Bug-squashing list (started 2026-10-30)

The user asked for a running list for a dedicated bug-fixing session. Each
row is a defect or a visible gap seen in a hands-on demo or a run; the
detailed entries (K…) below hold the evidence where there is more. Add new
rows as they are found; mark a row fixed with the patch and run.

| # | Seen | What | First lead | Status |
|---|---|---|---|---|
| B1 | user, demo-2 of S10-T04e (2026-10-30) | **The trader's portrait and name are missing in the trade window** (Sidorovich; the trade itself works) | the window is the installed Lua UI (`ui_inventory.script` `TMode_InitProfile` / `Reset(npc)`), which reads the NPC's `character_icon()` and `character_name()` on the client's replica trader; check what the replica's `CharacterInfo` holds (the specific character from the spawn state) and whether the talk window shows them | open |
| B2 | user, demo-2 of S10-T04e (2026-10-30) | **A PDA glitch** (details to confirm with the user: the partner's PDA on your screen, or your own?) | the engine has third-person PDA animations (`ActorAnimation.cpp:623-636`: draw, holster, idle, and a reading pose while zoomed); patch 0066 routes the server's state to weapons and grenades only, so a partner's PDA gets none of them, and the PDA's zoomed reading pose (`CPda::m_bZoomed`) is not sent at all; K1 (the other player's item driving the shared first-person hands) is the other suspect | open |
| B3 | every demo since S06-T08 | A `sound-volume` presentation flood (about 6 per tick per client) for sounds the clients do not have | S10-T05d1 found three causes: an unchanged volume (and one for a stopped sound) was sent every update; the level's ambient loops start before anyone joins, so no client had them; the surge's muted world run played the shared rumble/wave/earthquake sounds, so each player's pass only set volumes. Unchanged volumes are dropped, the loops are played to each joining client, a muted play is not routed and each pass has its own looped sounds | fixed (S10-T05d1, patch 0084, **presentation-4**: 0 absent records on either client, was 672) |
| B4 | user, 2026-10-22 and S10-T04a demos | NPCs slide in their idle pose while moving (K2, L57) | NPC movement type, body state and aim are not replicated (S10-T05d3 recorded it as not shown and left it to S10-T07) | open |
| B5 | upgrade-6, relations-1, stash-2, alife-5 (client 1, 0xC0000005, after a travel), sleep-2 (client 2), S10-T03f newgame-1 (client 1) | A client's process sometimes exits with an access violation after its clean stop (L59) | teardown order on a co-op client; the logs are complete, no gameplay effect | open |
| B6 | upgrade-5 | A long server-side move of a client's own actor (534 m) crashed that client (L58) | the client's prediction snap or a level-streaming step on a huge jump | fixed (S10-T07, patch 0087: a far snap is the engine's teleport; bugfix-1/-5: 525 m moves of player 2, client 2 stayed) |
| B7 | S10-T04c, S10-T02c reports; also L47 (the partner's use animation is not shown) | The partner's zoom (aim-down-sights) pose is not shown; a controller's camera pull during its psy attack is not shown (T02c-3) | replicate the zoom flag in the snapshot; present the controller's steering per frame | open |
| B8 | S10-T04f report | The upgrade window's tooltip shows the base price; the server charges the mechanic's discounted price (T04f-4) | send the discount in the upgrade table and use it in the tooltip | fixed (S10-T07, patch 0087: the table carries the server's percent; bugfix-7: tooltip 2.210 = server 2.210) |
| B10 | S10-T02d report | The bloodsucker's grab does not freeze the grabbed player or turn its camera toward the bloodsucker (T02d-3) | present the steering and an input hold to the grabbed client | fixed (S10-T07, patch 0087: the bloodsucker's CControlledActor on the client; bugfix-6: grab-hold, grab-released). The look of it: hands-on at G10 |
| B11 | S10-T02e fauna-1; S10-T03e sweep-3 (both clients, on arriving in Agroprom) | A psy dog phantom's spawn fails on a client (`refused by the entity`: the object binder, `bind_monster.script` `net_spawn`, refuses when `alife_object(id)` is nil); in sweep-3 the failed spawn's cleanup then crashed both clients in `CMonsterSquadManager::remove_member` (a squad the phantom never joined) | the cleanup is guarded (S10-T03e, patch 0080); the refusal now logs whether the client's ALife held the object (`alife=0|1`): read it when it recurs (it did not in sweep-4, -6 or -7) | open (both crashes fixed; the refusal's cause open) |
| B9 | S10-T04f report | The Lua windows show a bought, sold or taken item in its old list for a moment until the server's move arrives | expected latency; a local "pending" mark would hide it | open |
| B12 | S10-T02f report (T02f-5) | The companion hotkeys and the companion wheel (all-companion orders, per-squad settings, move-to-point) do nothing in co-op; orders through a companion's dialogue work | `axr_companions` `on_key_release` / `start_CW` run in the client's own Lua; route them as a server action in the presser's context, as the talk window does (S10-T04a) | fixed (S10-T07, patch 0087, action kind 28: the orders run on the server for the player's own companions; bugfix-4). The wheel lists the companions known at the join (limit) |
| B13 | S10-T03c sleep-1 (and every run since S05-T03: 350–520 steps per client per run) | Each client's game clock drifts more than 3 game seconds between the server's clock events (about one a second) and is stepped back into line almost every time; the weather and time-of-day on a client may jitter | `CoopClientLink.cpp` clock event (`clock-step`, 3 s tolerance): check whether the client's ALife clock advances at the time factor between events (a paused or differently scaled `CALifeTimeManager` on a replica), or widen the tolerance and slew instead of stepping | fixed (S10-T07, patch 0087: the client runs the clock between events, forward only, slower while ahead; bugfix-5/6/7: 1-4 steps per client, was about 400) |
| B14 | S10-T03e sweep-3 (every run since S08: 460–3300 lines per run) | The server sends both clients a `post-process-remove` for effects 2015 and 2020 about 5 times a second, when neither effect is on | the installed `actor_effects.script` removes them every update (`Update_Rad` 773–786 while radiation is under the threshold; `Update_Bleeding` 971–974, an `or`/`and` precedence slip); free in single-player, a network message per player in co-op: drop a remove for an effect the player does not have on the server (presentation layer) | fixed (S10-T05d1, patch 0084, presentation-4: 3,032 removes/factors for effectors not on held back on the server, none sent) |
| B15 | S10-T03e sweep-4 (1 of 437 identity checks in every run so far) | After a travel reload the server's identity check counted one online object without a server entity (`missing_entity=1`) and the server stopped; the stop then crashed in `log_status` (the level already gone) | sweep-6 named it (`burer_weak26892`: released by ALife in the last tick, its destroy event not yet processed); the check now skips an object queued for its destroy, names any other and is retried after 10 more ticks up to 3 times (a real mismatch stays); `log_status` reads the level's objects only while there is a level (S10-T03e, patch 0080; sweep-6 stopped cleanly; **sweep-7**: the retry fired once, for 3 monsters just released, and passed) | fixed (S10-T03e sweep-7) |
| B16 | S10-T03e sweep-4 | When the server stops or dies, both clients sit on the loading screen reconnecting for good, with no message | the client's reconnect loop (`status state=1 … connects=131`): after a while, say the server is gone and offer the main menu | fixed (S10-T07, patch 0087: the main menu and a message after 60 s, reconnects go on; bugfix-1: told at 60 s, back after the restart) |
| B17 | S10-T05d1 (reading `sound_ambient.script`) | The ambient wind, underground drone and rain-on-the-helmet loops follow **player 1's** state (indoors, underground, gas mask on) and both players hear them; player 2 indoors still hears the outdoor wind | `sound_ambient.script` `update_wind` / `update_rain_helm_sound` run from a time event outside any context, so `db.actor` is the host actor and the loops are broadcast; run them per player in that player's context with each player's own sound objects (as the surge passes do since S10-T05d1) | fixed (S10-T07, patch 0087: each player's wind, drone and rain loops in its context; every bugfix run: slot-loop for both slots) |
| B18 | S10-T05d1 (sweep-6, emission-2 server logs: `music\combat\theme*` presented to slot 0 and 1) | The **dynamic combat music** plays for both players when **player 1** is in combat; only the first part of a theme reaches the clients (the next parts are chained with `attach_tail`, which is not routed) and the music/ambient fades (`snd_volume_music` on the server) never reach them | `xrs_dyn_music.script` `main_loop` (an `AddUniqueCall`, no context) reads the host actor; run it per player in that player's context, route `attach_tail`, or let each client run its own dynamic music from its replica (the client has the NPC table it needs only partly) | fixed (S10-T07, patch 0087: the music per player, attach_tail routed; bugfix-5: music for player 2 only, 13 sounds) |
| B19 | S10-T07 bugfix-2 (a stalker fought player 2, the combat music never started) | **An NPC fighting player 2 is not counted as fighting "the actor"**, and player 2's hits do not end an NPC's combat ignore: the combat music, and the scripts that refuse fast travel or the backpack in combat, only see player 1's fights | `xr_combat_ignore` `enemy_callback` (`ene_id == AC_ID`) and `hit_callback` (`who:id() == AC_ID`) run from the engine outside any context, where `AC_ID` is player 1's: run them in the enemy's (the hitter's) player context | fixed (S10-T07, patch 0087: the callbacks run in the enemy's or hitter's player context; bugfix-5) |
| B20 | S10-T07 bugfix-8 (client 2, once in a run so far) | **A client hung while loading its level** after receiving the world: its log stops after `CLevel::CLevel()`, it ignored its stop file and was killed; client 1 and the server went on | the same build passed bugfix-5/6/7; rerun bugfix-9 to see whether it repeats; next: a dump of the hung process (needs the B5 permission) or a log step through `CLevel` construction | open |

## Open

### K1. The other player's knife bends your own first-person pistol

- **Seen** (user, demo-6 and demo-7, 2026-10-22): player 2 holds the pistol;
  when player 1 takes out the knife, player 2's **own first-person** pistol is
  held in a twisted, knife-like grip on player 2's screen. Player 2's body seen
  by player 1 is normal. Player 1's knife is not visibly drawn on player 2's
  screen.
- **Evidence:** `builds/logs/S06-T07/knife-glitch-evidence/`:
  `demo6-before-knife.jpg` (normal grip, right window), `demo6-after-knife.jpg`
  (twisted grip, 02:40:53 knife), `demo7-after-knife-fix1.jpg` (still twisted
  after the first fix attempt). Clients' `coop-combat-client.csv` in
  `builds/stage2/test-install/S06-T07/demo-6-c1|c2/appdata/logs/` (slot
  timeline: player 1 slot 1 at 02:40:53).
- **Ruled out:**
  - Running the remote knife's full state change (`OnStateSwitch`, which plays
    first-person motions on the shared hands): removed in patch 0039 (remote
    participant items now only show or hide, `HudItem.cpp`,
    `CHudItem::SwitchState`); the glitch stayed (demo-7).
  - The `actor_on_changed_slot` script callback: no installed script listens
    to it.
  - `player_hud::motion_length` / `get_hand_motions`: lookups only.
- **Where to look:** what touches `g_player_hud` (the local player's shared
  first-person hands) when a client switches the **other** actor's slot to the
  knife. That switch starts in `CoopClientLink.cpp` `apply_actor_outcomes`
  (`actor->inventory().Activate(slot)`) and runs through `CInventory::Update`
  (`Inventory.cpp:820-880`: `SendDeactivateItem`, `ActivateItem`,
  `_G.CActor_OnChangedSlot`). Then check `CWeaponKnife` (`WeaponKnife.cpp`;
  `switch2_Showing` attaches to the HUD when `ParentIsActor()`, which is true
  for **any** actor, not only the local one), any `HudItemData()` call on the
  remote item (it creates an `attachable_hud_item` on `g_player_hud`,
  `HudItem.cpp:976`), and the weapon's `UpdateCL`/`renderable_Render` paths.
  `ParentIsActor()` gates many first-person paths that assumed one actor.
- **Check a fix:** two clients side by side; player 2 draws the pistol (`2`),
  player 1 faces player 2 and takes out the knife (`1`); player 2's own pistol
  must keep its grip. `tools/Send-ClientActions.ps1` can press the keys
  (`key:0x03` = 2), but its `key:0x02` (1) did not draw the knife in act-3;
  doing it by hand worked.
- **Root cause found (2026-10-23, G07 play test):** on a co-op client the
  other player's weapon is owned by the client's own in-process server, so
  `object().Local()` is true and the S06-T07 guard in `CHudItem::SwitchState`
  (which only covered non-local objects) never applied: the state change went
  out as `GE_WPN_STATE_CHANGE`, came straight back through `CHudItem::OnEvent`
  and ran the full first-person switch (`CWeaponKnife::switch2_Showing` →
  `ParentIsActor()` true for any actor → `g_player_hud->attach_item`) on
  the shared hands. The same path made the other player's pistol re-draw
  when this player drew one.
- **Fix (patch 0043, campaign-240 / client-140):** `other_players_item()`
  (an actor parent that is not `local_view_actor()`) takes the show/hide-only
  branch in `SwitchState` and skips `OnStateSwitch` in `OnEvent`;
  `ParentIsActor()` means the local player's actor on a co-op client.
  **Pending the user's retest** (the K1 check: player 2 draws the pistol,
  player 1 takes out the knife; player 2's grip must not change; drawing a
  pistol must not re-draw the other player's).
- **Retest 2026-10-23 (campaign-240 / client-140):** the bent grip and the
  re-draw are gone; a **slight sway remains** when the other player equips,
  "like the sway from the equip animation that travels over". Measured in
  cam-1 (`builds/logs/S07-T08/run-cam-1/c1/coop-camera-client.csv`: the
  standing client's camera logged per frame for 2 s after each of the
  other player's two weapon changes): yaw, pitch, position and FOV did not
  change at all, so the sway is on the first-person hands/weapon layer, not
  the camera. **Fix candidate (patch 0044, campaign-244 / client-144,
  unverified):** `CHudItem::PlayHUDMotion` / `PlayHUDMotion_noCB` return
  early for the other player's item (its HUD motions, `anm_show`/`anm_idle`,
  carry the camera-like track the hands follow); the engine log prints
  `S07 K1 remote-hud-motion-blocked item= motion=` when it blocks one.
  **Check tomorrow:** the demo; player 2 equips while player 1 watches with a
  weapon drawn: no sway; the engine log of player 1's client shows the
  blocked motions.
- **Follow-up 2 (patch 0066, S10-T04c, 2026-10-29):** with every first-person
  motion of the other player's item blocked, its third-person body never
  swung, reloaded, drew or holstered either: `CActor::g_SetAnimation` picks
  the torso animation from the active weapon's `GetState()`
  (`ActorAnimation.cpp`), and on a replica that item is forced to idle
  (K1 + patch 0065). The server's sampled `weapon_state` (snapshot kWeapon)
  is now stored on the replica actor (`CActor::replica_set_weapon_state`)
  and read by the animation picker alone (`replica_anim_weapon_state`); the
  item's own state stays idle, because setting eFire on the weapon object
  would fire it on the client (`CWeaponMagazined::UpdateCL`). An attack
  state is held at least 350 ms. Proven by the combat program's knife leg
  (anim-1: `remote-weapon-state`, `remote-torso` in the logs); the user's
  hands-on check is pending.
- **Owner:** this fix; S10-T02 for the rest of the other player's presentation.

### K2. Stalker NPCs shuffle instead of walking

- **Seen** (user, 2026-10-22): every stalker NPC moves with a strange shuffle.
- **Known cause:** NPC movement type, body state and aim are not replicated;
  clients animate remote NPCs from their own idle/stand state, so a moving NPC
  slides in its standing animation ([S06-T02](validation/S06-T02.md) limits).
- **Owner:** S10-T02 (NPC animation).

### K3. Movement is still slightly choppy

- **Seen** (user, demo-5, 2026-10-22).
- **Evidence** (demo-5 clients' final status lines,
  `builds/stage2/test-install/S06-T07/demo-5-c1|c2/appdata/logs/`): 6.2 % /
  5.1 % of input ticks skipped (client frames over 60 ms); 34 % of presented
  entity frames extrapolated; own-actor corrections smoothed up to 0.59 /
  0.67 m, never snapped. Two clients and the server share one PC.
- **Where to look:** client frame time with two windows on one PC
  (`inputs_skipped`), the snapshot interpolation delay and extrapolation rate
  (`presented`/`extrapolated`, S06-T02), and own-actor prediction corrections
  (`pred_smoothed`, `view_max_offset`, S06-T03).
- **Owner:** S11 (S11-T01 latency and rates, S11-T03 performance).

### K4. A client keeps items an NPC held after the server released that NPC (fixed, S08-T03)

- **Seen** (rsession-1, 2026-10-22): client 1 had 5 items in its replica set
  but not in its level; a bandit (27062) received them and the server
  released the bandit while the items stayed in the world.
- **Known cause:** ownership changes are not replicated
  ([S06-T02](validation/S06-T02.md) limits); details in
  [S06-T07](validation/S06-T07.md).
- **Seen again** (rec-3, 2026-10-23, [S07-T05](validation/S07-T05.md)):
  after the server restart both clients kept one `sigaret_for_nps` (7228)
  an NPC consumed (server `despawn … Destroyed` at commit 3418) while every
  other of their 1,561 replica entities matched the server.
- **Narrowed** (S08-T01, patch 0045, 2026-10-24): the server now sends
  every owner change of a published entity to both clients (`Reparent`,
  protocol v1.6), NPC takes and drops included. The S08-T01 runs had no NPC
  take near the players, so whether this removes K4 (and what a client does
  with an NPC's items when the NPC goes offline) is untested.
- **Root cause of rec-3 found and fixed** (S08-T03, patch 0047): a client's
  entity table kept the previous world's tombstones across a rejoin and
  refused the new world's entities as recycled, so their despawns were
  dropped ([S08-T03](validation/S08-T03.md)). Since the fix the in-stream
  item-graph check (every item's owner on both clients, every 10 s) found no
  mismatch in six sessions with restarts (inv-7…12).
- **Owner:** closed by S08-T03 unless the item-graph check sees it again.

### K5. The other player's lean was not shown

- **Seen** (user, G07 play test, 2026-10-23): leaning tilts only the own
  view; the other player's body stays upright.
- **Cause:** the engine's lean is a torso roll (`r_torso.roll`, applied to
  the spine, shoulder and head bones by the `CActor` bone callbacks) that
  only `g_cl_Orientate` / `cam_Lookout` compute, for the controlled actor;
  the server sends the lean bits in the actor's state and the remote body
  received them (`replica_present`) but nothing computed its roll.
- **Fix (patch 0043, campaign-240 / client-140):** `CActor::replica_present`
  computes the remote actor's target roll from the lean bits with the same
  angle and inertia as the controlled actor (no wall check). The client logs
  `remote-lean … roll=` while the other player leans. **Pending the user's
  retest.**

## Fixed

- The other player's weapon was invisible; a player's weapon switch and reload
  stayed local: [S06-T07](validation/S06-T07.md) part 2 (patch 0038).
- Windows could not be dragged: [S06-T07](validation/S06-T07.md) part 1
  (patch 0037).
