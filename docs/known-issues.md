# Known issues

Bugs found while testing that are not fixed yet, written so a session that has
not seen them can pick one up. Each has what was seen, the evidence, what was
already ruled out, where to look, and how to check a fix. Limits accepted at a
gate are in [carried limits](carried-limits.md); this list is for defects.

When an issue is fixed, move it to "Fixed" with the report and run names.

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

### K4. A client keeps items an NPC held after the server released that NPC

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
- **Owner:** S08 (inventory and ownership).

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
