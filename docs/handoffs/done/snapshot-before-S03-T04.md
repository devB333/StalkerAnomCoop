## Complete

# Session handoff — S03-T02 complete (local view vs gameplay authority); next: S03-T03

## State at the end of the session (2026-10-18)

**S03-T02 is Completed** and exported as patch 0022
(`c966e122…a61d`, 70 files; reconstruction `patch-check-23b`). Rules in
[participant-identity.md](../../participant-identity.md#local-view-and-gameplay-access--s03-t02),
every call site in [actor-access-inventory.md](../../actor-access-inventory.md),
evidence in [S03-T02](../../validation/S03-T02.md).

- `xrGame/CampaignActorAccess.{h,cpp}`: `local_view_actor()` (client only,
  null on the server), participant lookups (`participant_actor`,
  `participant_of`, `participant_actors`, `nearest_participant_actor`,
  `context_actor`, `dialog_actor`), `sole_participant_actor(purpose)`, and
  `Actor()` as the counted compatibility accessor (server: the one
  participant actor, null + counted with none or two, every caller recorded
  and symbolised from `xrGame.pdb`). A client gets a `Role::Client` registry
  with its `CLevel`; every actor spawned on the server adopts a participant
  (`adopt_spawned_actor`, by the spawn packet id); the harness binds fixture
  player 1 before the level.
- `xrGame/CampaignMessagePolicy.{h,cpp}`: `xrServer::OnMessage`,
  `Process_event`, `Process_update`, `Process_save` refuse state imports,
  world control and every game event from any sender but the bridge
  (direct client), counted by type.
- 54 files of call-site conversions (B21: phantom hunts the nearest
  participant actor and goes idle with none; trader faces the nearest; psy
  dog aura is the viewer's). Unchanged and recorded: 104 ability sites
  (S03-T04/S10-T02), 38 `level.*` script sites (S03-T05), HUD/UI/camera.
- Fixtures: `-Scope Identity` now runs `actor-access` (53 checks) and
  `message-policy` (259 checks); GameState adds `campaign-authority-passed`
  after `campaign-identity-passed` (63 checks: no view entity, lookups name
  the saved actor only, a second actor-less participant context never
  resolves it, forged remote `M_UPDATE`/events/world control refused with
  state unchanged, the bridge's accepted). smoke-104 (Identity) and
  smoke-106 (GameState, all 61 stages, hearing passed) pass from the final
  source; smoke-105 failed only on fixture accounting (fixed).
- Server-side `Actor()` callers observed (16): monster auras, IK legs, HUD
  item/player_hud, luabind script bindings, the fixture itself; no
  converted gameplay site.

## Next steps

1. **S03-T03** (two independent native actors), per plan.md: spawn a second
   genuine `CSE_ALifeCreatureActor` through the server harness on
   `k00_marsh`, let it adopt participant slot 1 (`on_actor_spawned`), give
   each actor its own input/control context, and verify condition,
   equipment, inventory, death and lifecycle independently. First action:
   decide how the fixture spawns actor 2 (a cloned actor spawn packet with
   a new section/position via `Process_spawn` from the bridge) and expect
   `Actor()` to refuse (`compat_ambiguous`) once both exist: the audit table
   in the log then lists every site that dereferences it; convert or guard
   those. `g_actor` will be whichever spawned last (server forces
   `M_SPAWN_OBJECT_LOCAL`); nothing gameplay-side should read it any more.
2. Open items carried: `EntityId`s do not survive a checkpoint reload
   (Stage 7); fixture `PlayerId`s only (S04-T04); detached-participant
   release policy (S04-T05); hearing-stage footstep cause (S03-T04);
   abilities list (S03-T04); script context (S03-T05); dropped script
   effectors on the server (S03-T05 presentation callbacks).

## Tools and runs

- Last run names: `campaign-89`, `client-25`, `smoke-106`, `capture-13`
  (`patch-check-23a/b`). Next: `campaign-90`, `client-26`, `smoke-107`,
  `capture-14`; next patch export is
  `python3 tools/export-engine-patch.py <name> 24`.
- Run PowerShell scripts with the Windows path in single quotes from bash
  (`-File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Stage2.ps1'`).
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Identity|GameState`. GameState
  ~4 min; a hearing-stage `npc_walk_events=0` is recorded variation, rerun
  once.
- Build: `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`
  (~6 min after a project-file change, ~1.5 min incremental). The client
  product was not rebuilt this session (S03-T02 changes compile in the
  campaign product; the client build is due before the next capture).
- Client captures, compare tools and quest fixture options: see the
  [S03-T01 snapshot](snapshot-before-S03-T01.md#tools-and-runs).
- Script files in the archives are cp1251/CRLF; use `grep -a`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02 carried limits are unchanged.
Previous snapshot: [before S03-T03](snapshot-before-S03-T03.md).
