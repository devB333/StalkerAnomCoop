# Session handoff — S03-T03 complete (two independent native actors); next: S03-T04

## State at the end of the session (2026-10-19)

**S03-T03 is Completed** and exported as patch 0023
(`1b69e2df…689c`, 16 files; reconstruction `patch-check-24b`). Rules in
[participant-identity.md](../../participant-identity.md#per-participant-control-and-the-single-player-script-actor--s03-t03),
sites and the installed-script gates in
[actor-access-inventory.md](../../actor-access-inventory.md#control-sites--s03-t03),
evidence in [S03-T03](../../validation/S03-T03.md).

- `xrGame/CampaignActorControl.{h,cpp}`: `controls_actor(actor)` (the
  server runs `CActor::shedule_Update`'s control path for every participant
  actor; a client for its local control entity), the participant input
  context `actor_input_hold/press/release/look(context, …)` (an authorized
  `Scope::Self` context, forwarded to the actor's own `IR_*` handler;
  refusals counted), `script_binding_allowed` (the installed `actor` Lua
  binder attaches to the ALife actor only on the server; S03-T05 owns the
  participant script context). `CampaignActorAccess`:
  `assign_participant_actor(ref, engine_id)` names an entity's participant
  before it spawns.
- Two findings in the installed scripts, both recorded, neither hidden:
  `se_actor:STATE_Read` reads its bytes only `if db.actor == nil`, so a
  second actor's spawn packet was misparsed (health 0, position 10^29);
  `CSE_Abstract::Spawn_Read` now resumes at the end of the length-prefixed
  state frame and counts the mismatch (`g_spawn_state_resyncs`; also fires
  once per `smart_terrain` at save load, 1 byte short, harmless there).
  `_G.CActor__BeforeHitCallback` drops hits for 7 s after the binder's first
  update and drops hits sourced from any actor as friendly fire against
  `db.actor` (`IsStalker` includes actors); the fixture hits from an anomaly
  zone after 9 s.
- `level.*` camera/post-process bindings use the local view actor (23
  sites); 10 `level.*` gameplay bindings and `_give_news` are null-guarded
  (`level.remove_pp_effector` had faulted on the null `Actor()` and cleared
  actor 1's binder); `ActorLookAtPoint` uses the object's own camera; the
  moving-camera effector and the game-over sequence are the viewer's own.
- Fixture `campaign-two-actors-passed` (GameState, after
  `campaign-quest-persisted`): spawn, identity, condition, inventory,
  control, death, lifecycle, slot reuse; 116 checks. smoke-117 (campaign-100)
  passes all 62 stages; smoke-118 passes the Identity scope. The capture is
  `server-two-actors.csv` (state rows per phase, the participant map,
  every check).
- Client build client-26: 0 errors; checked against the server's smoke-117
  checkpoints (capture-14 completes the quest from the first, capture-15
  loads the completed state from the second; `compare-quest-capture.py`
  matches all rules).

## Next steps

1. **S03-T04** (participant-aware ALife activation and basic AI), per
   plan.md: the switch manager measures from the ALife actor alone
   (`alife_switch_manager.cpp:111,122`, `CALifeGraphRegistry::actor()`);
   make the online/offline policy use the participant set (the activity
   anchors already do, `CampaignActivityRefreshAnchors`), check NPC
   enemy selection and awareness for either actor, record each
   player-bound ability (phantom, psy dog aura, controller, snork,
   pseudogigant, scanning; the `Actor()` callers in the audit table are the
   list) as participant-aware or uncovered for S10-T02, measure the tick
   cost with the union. First action: extend the two-actor fixture (or a
   new one) to walk actor 2 beyond the switch distance from actor 1 and
   observe what switches offline; the hearing stage's `npc_walk_events=0`
   variation (twice in a row in smoke-114/115) is on this task's list.
2. **S03-T05** inputs collected this session: `se_actor` state gate,
   the actor binder's `db.actor` globals, the pre-hit callback (window,
   friendly fire, `db.actor` artefact immunities), news to "the" actor,
   the `level.*` gameplay bindings behind null guards, god mode flags
   (`psActorFlags`, process-wide).
3. Open items carried: `EntityId`s do not survive a checkpoint reload
   (Stage 7); fixture `PlayerId`s only (S04-T04); detached-participant
   release policy (S04-T05); dropped script effectors on the server
   (S03-T05 presentation callbacks); friendly-fire policy (Stage 6).

## Tools and runs

- Last run names: `campaign-100`, `client-26`, `smoke-118`, `capture-15`
  (`patch-check-24a/b`). Next: `campaign-101`, `client-27`, `smoke-119`,
  `capture-16`; next patch export is
  `python3 tools/export-engine-patch.py <name> 25`.
- Run PowerShell scripts with the Windows path in single quotes from bash
  (`-File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Stage2.ps1'`).
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Identity|GameState`. GameState
  ~5 min now (the two-actor stage adds ~20 s); a hearing-stage
  `npc_walk_events=0` is recorded variation, rerun.
- Build: `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`
  (~6 min after a project-file change, ~1.5 min incremental).
- Two-actor fixture options: `-s03-actor-section`, `-s03-weapon-section`,
  `-s03-consumable-section`, `-s03-actor-distance`.
- Client captures, compare tools and quest fixture options: see the
  [S03-T01 snapshot](../done/snapshot-before-S03-T01.md#tools-and-runs).
- Script files in the archives are cp1251/CRLF; use `grep -a`, or
  `python3 tools/xray-db-extract.py extract -m <name> -o <dir> <db>` /
  `grep -g '*.script' <pattern> <db>` on
  `builds/build_2/test-install/source-client/db/configs/scripts.db0`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02 carried limits are unchanged.
Previous snapshot: [before S03-T04](../done/snapshot-before-S03-T04.md).
