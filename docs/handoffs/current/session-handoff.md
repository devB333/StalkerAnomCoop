# Session handoff — S03-T04 in progress (participant-aware ALife activation); next: build, fixture, smoke

## State at the end of the session (2026-10-19)

**S03-T04 is In Progress.** The engine change is written but **not built and
not run**; it is exported as a work-in-progress patch 0024
(`0024-participant-activation-wip.patch`, `481d9bec…6162`, 17 files;
series reconstruction `patch-check-25b` applies cleanly). Nothing in this
patch has evidence yet. The in-progress report is
[S03-T04](../../validation/S03-T04.md); it holds the source reading and the
design, no results.

What is in patch 0024 (all in `builds/build_1/engine-work/src/xrGame`):

- `alife_switch_manager.{h,cpp,_inline.h}`, `alife_update_manager.cpp`: the
  activation set. `refresh_activation_anchors()` runs at the start of every
  `CALifeUpdateManager::update()`; on the server it snapshots the server
  entities of the *alive* participant actors (`o_Position`, the same state
  the engine measured for the ALife actor); `activation_distance(p)` is the
  distance to the nearest anchor. On a client, or with no alive participant
  actor, it is `graph().actor()->o_Position.distance_to(p)` read live, so
  the normal client is unchanged.
- The five switch rules now call `activation_distance`:
  `alife_dynamic_object.cpp` (`try_switch_online/offline`),
  `alife_online_offline_group.cpp` (both loops), `alife_group_abstract.cpp`.
  The DEBUG-only `[LSS]` messages in `alife_switch_manager.cpp:111,122` still
  print the ALife actor; they are not compiled.
- `CampaignActorAccess.{h,cpp}`: `participant_actor_entities(out)` (the
  participant actors' `CSE_ALifeCreatureActor` entities in slot order; empty
  on a client).
- Awareness: `ai/monsters/monster_enemy_memory.cpp` (the two `g_actor`
  "the player watches me" rules run over `participant_actors()`),
  `ai/stalker/ai_stalker.cpp:1694` (`LookAtActor` tests the nearest
  participant it already chose), `bloodsucker_predator_inline.h` (no
  predator state while any participant watches).
- Player-bound abilities made explicit no-ops while `Actor()` refuses (two
  participants on the server), each marked `S03-T04` and to be listed as
  uncovered for S10-T02: `anti_aim_ability.cpp` (`check_update_condition`
  dereferenced `Actor()` after a `g_actor` test and would fault),
  `controller_psy_hit.cpp` (`check_start_conditions`,
  `check_conditions_final`), `burer.cpp` (`StaminaHit`,
  `actor_is_reloading_weapon`), `poltergeist_telekinesis.cpp`
  (`update_schedule`), `monster_aura.cpp` (`calculate` returns 0).
  Not touched (comparisons or states reachable only with the actor as the
  enemy, recorded in the report): bloodsucker vampire/alien, poltergeist
  flame thrower (null-safe), burer shield, scanning, psy-dog aura, phantom,
  pseudogigant, snork (DEBUG).

## Next steps (in order)

1. **Build**: `Build-Stage2.ps1 -Run campaign-101 -Product Campaign`. Expect
   compile issues only in the new code; `CSE_ALifeCreatureActor::g_Alive`
   is on `CSE_ALifeCreatureAbstract` (`xrServer_Objects_ALife_Monsters.h:220`).
2. **Fixture** `xrGame/CampaignActivationFixture.cpp` (not written yet; the
   design is in the report): after `campaign-two-actors-passed` (participant
   2's third actor is alive beside actor 1), phases: baseline (settle until
   no switch for 50 ticks, then measure 250 ticks: online count, object-list
   and scheduler counters, `g_level_frame_timing` deltas); separate (teleport
   actor 2 the way `CScriptGameObject::SetActorPosition` does, to the
   farthest accessible level vertex found by a breadth-first walk from actor
   1's vertex; settle; checks: objects within `online_distance` of actor 2
   and beyond `offline_distance` of actor 1 came online, nothing within
   `online_distance` of actor 1 went offline, switchable objects beyond
   `offline_distance` of both are offline, exceptions logged by name);
   awareness (god mode on, a boar `6 m` from each actor, each boar's
   `EnemyMan.get_enemy()` becomes its actor within a budget; release them);
   return (teleport back; the far objects go offline again); disconnect
   (`detach_session` for session `0x53030002`, the retained actor stays an
   anchor, level and ALife intact, `attach_session` again). CSV
   `s03-t04-activation.csv`, stage `campaign-activation-passed`, exit 34,
   budget `g_campaign_activation_ticks = 5000`. Wire it in `x_ray.cpp` after
   the two-actor block, declare in `CampaignIdentityFixture.h`, add to
   `xrGame.vcxproj` (copy the `CampaignTwoActorFixture.cpp` entry) and to
   the expected stages in `tools/Run-Stage2Smoke.ps1` (line 96) with a
   capture copy block (line ~161). `switch_distance` on k00_marsh is 750 m
   (online 675, offline 825; smoke-117 activity meta); if the level cannot
   separate the actors by more than 825 m, add `-s03-switch-distance` and
   lower it for the fixture only, restoring it at the end.
3. **Smoke**: `python3 tools/prepare-stage2-smoke.py smoke-119` then
   `Run-Stage2Smoke.ps1 -Run smoke-119 -Scope GameState` (G02 regression
   rule; now 63 stages). Then `Build-Stage2.ps1 -Run client-27 -Product
   Client` for compile safety of the shared xrGame changes.
4. **Report and docs**: finish `docs/validation/S03-T04.md` (results, the
   player-bound ability table, tick cost), add the activation rule to
   `docs/participant-identity.md` and the new sites to
   `docs/actor-access-inventory.md`, re-export patch 0024 under its final
   name (`python3 tools/export-engine-patch.py 0024-participant-activation.patch 26`,
   remove the `-wip` file and its `series` line and manifest entry first),
   update `plan.md` §I, snapshot and rewrite this handoff, commit.
5. **Hearing variation** (S03-T01 open item, on this task's list): in the
   captures the walked stalker produced footsteps in phase 1 only when its
   object-handler action was `1272512514` (weapon `19416`, operator 2
   `eWorldOperatorHide`, smoke-117: 22 `material\human\step` events) and
   none when it was `1272512521` (operator 9 `eWorldOperatorIdle`, weapon in
   hands, smoke-114/115: 0 events, holster/draw only in smoke-116). So the
   footsteps depend on the legs animation the stalker walks with, which
   depends on whether the planner keeps the rifle out. Next check: whether
   `[stalker_step_manager]` (`configs/creatures/m_stalker.ltx:6`) lists the
   weapon-in-hands walk motions; `CStepManager::on_animation_start` disables
   steps for a motion without params. Not an identity or activation effect.
6. Carried: S03-T05 inputs and open items are unchanged from the
   [previous snapshot](../done/snapshot-before-S03-T04-fixture.md).

## Tools and runs

- Last run names: `campaign-100`, `client-26`, `smoke-118`, `capture-15`,
  `patch-check-25a/b`. Next: `campaign-101`, `client-27`, `smoke-119`,
  `capture-16`, patch check `26`.
- Run PowerShell scripts with the Windows path in single quotes from bash
  (`-File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Stage2.ps1'`).
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Identity|GameState` (GameState
  ~5 min; a hearing-stage `npc_walk_events=0` is recorded variation, rerun).
- Build: `Build-Stage2.ps1 -Run <name> -Product Campaign|Client` (~6 min
  after a project-file change, ~1.5 min incremental).
- The campaign clock runs `Device.seqParallel` on the main thread
  (`CampaignServerClock.cpp:151`), so the ALife update (`mtALife` is on by
  default) and the activation refresh run on the tick thread.
- Script files in the archives are cp1251/CRLF; use
  `python3 tools/xray-db-extract.py grep -g '*.script' <pattern> builds/build_2/test-install/source-client/db/configs/scripts.db0`
  (`-E` for a regex); configs are in `configs.db0`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02 carried limits are unchanged.
Previous snapshot: [before the S03-T04 fixture](../done/snapshot-before-S03-T04-fixture.md).
