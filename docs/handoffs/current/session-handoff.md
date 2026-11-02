# Session handoff — S10-T07 part 3 checked hands-on; part 4 open (NPCs walk backwards, fires, trader portrait)

## State (2026-11-02, late)

- **Part 3 is built and committed** (patch 0089, campaign-422 / client-305,
  commit 9f55d24; [S10-T07](../../validation/S10-T07.md) part 3). The user's
  hands-on check **demo-3** (`builds/stage2/test-install/S10-T07/demo-3-*`,
  logs under its `appdata/logs`) gave:
  - **B22 jump: no complaint.** `jump-followed` fired 5 times on client 1;
    `jump-refused` lines are present (count every held frame, expected).
  - **B23 campfires: frame rate fine (`fps=60`), but the user wants the
    fires back.** With the server now turning campfires as single-player
    does, only 10-11 of 46 Cordon fires burn (the NPC-lit ones). The user's
    direction: "I want the fires to remain but better performance/lighting."
    Plan (not coded): the campaign server never turns a campfire (every one
    burns, as in demo-1/2; `campfire_turn_allowed()` false on the server,
    `ZoneCampfire.cpp`), and on a co-op client a campfire's idle light casts
    a shadow only within ~20 m of the camera (`CCustomZone::UpdateIdleLight`,
    flag `eIdleLightShadow`, `Device.vCameraPosition`; a member set in
    `CZoneCampfire::Load` when `coop_client_active()`). Record it as
    decision T07-10 and a limit (fires cannot be put out in co-op).
  - **B4 NPCs: animated now, but "definitely walking backwards"** (the
    user's words, twice). What is established:
    - Server data is consistent: for moving stalkers in demo-3's
      `coop-snapshots.csv`, velocity direction = position progression
      (3625/3626 rows) and = the sent yaw (90 % within 0°). The sent yaw is
      `yaw_of(XFORM) = atan2(k.x, k.z)`; the client applies `rotateY(yaw)`
      (`CCustomMonster::replica_present`, and the engine's own remote path
      `XFORM().rotateY(NET_Last.o_model)`, upstream CustomMonster.cpp:547),
      which reproduces the server's matrix exactly. So on paper the body
      faces its motion. **The user sees the opposite; the paper model is
      wrong somewhere.** Not yet checked: where a *local* stalker's XFORM
      rotation is built from `m_body.current.yaw` (grep
      `m_body.current.yaw` in CustomMonster.cpp / movement_manager*.cpp /
      stalker_movement_manager_base.cpp; monsters: control_direction.cpp:97
      `setHPB(-yaw, -pitch, 0)`), whether the stalker model's forward is
      +k, and the client-side spine callback
      (`stalker_animation_callbacks.cpp:145`, `sight().current_spine_rotation()`
      is never updated on a replica: a stale upper-body twist). Also compare
      on the **client**: `coop-snapshots-client.csv` (10 Hz) heading vs.
      position progression, and the other player's body (CActor applies the
      same `rotateY(transform.yaw)`; the user has not complained about it).
    - `replica-anim` lines: real stalkers walk with legs idx 5 =
      `norm_walk_fwd_1` (a forward cycle; names from
      `meshes/actors/stalker_animation.omf`, parser in the session's
      scratchpad, re-derivable with `tools/xray-db-extract.py`), so the leg
      animation is forward. One creature (4135, speed 0.3-0.8 m/s, 108 of
      127 lines, legs idx 238 / torso 70) plays what slot 0 names
      `norm_torso_12_draw_0`: identify it (likely a different motion slot or
      a zombie visual); add the motion *names* (`LL_MotionDefName_dbg`), the
      visual and the slot to the `replica-anim` line.
  - **B1 trader portrait and name still missing** in the Lua trade window
    (`ui_inventory.script` `UpdateCharacter()`: `npc:character_name()` /
    `npc:character_icon()` on `GetPartner()`). Facts: the trader 15398 is a
    `local-creature` on the client; the client's own `trade-open` line prints
    `name=Sidorovich` from `CInventoryOwner::Name()`, so `m_game_name` is set
    on that object; the icon is `CharacterInfo().IconName()`. A read-only
    subagent was investigating (Lua partner lookup, `CCharacterInfo::Init`
    on the client's spawn state, Lua errors in the engine log around utc
    1793596534) when the session ended; **its result is not recorded**.
    Extracted scripts for reading are session-scratch only.
- **Still open from demo-1:** B21's remaining part (a Smooth correction on
  62-84 % of moving snapshots, mean 11-14 cm).
- **Still waiting on the user:** B5 and B9 (decisions), B11 (to recur),
  B20 (a dump needs B5).
- Engine-work equals patch 0089 (no edits since the commit).

## Next

1. Part 4 in code: B4 facing (find the real cause first, see above), B23
   fires per the user's direction, B1 portrait. Then export patch 0090,
   build `campaign-423` / `client-306`, update the report (part 4),
   known-issues (B1, B4, B23 rows: the demo-3 observations above are not
   yet written there), decision T07-10 in
   [Stage 10 decisions](../../stage10-decisions.md), handoff, commit.
2. The user's hands-on check: `bash tools/start-coop-demo.sh S10-T07 0 30 120 0`.
3. B5/B9 decisions; the G10 review, limits L60…, close G10, Stage 11.

## Run names

- Used: `campaign-334…422`, `client-226…305`, `proto-build-53…59`,
  `st-28…34`, `bugfix-1…16`, `demo-1…3` (S10-T07), patch checks 77–101.
- Next: `campaign-423`, `client-306`, `proto-build-60`, `st-35`,
  `bugfix-17`, patch check 102, `demo-4`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T07 part 4](../done/snapshot-before-S10-T07-part4.md).
