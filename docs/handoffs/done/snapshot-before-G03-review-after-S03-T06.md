## Complete

# Session handoff — S03-T05 complete (participant script context); next: G03 review, then S04-T01

## State at the end of the session (2026-10-19)

Stage 3 / G03 in progress. **S03-T01..T05 Completed** — every Stage 3 task.
The [G03 evidence record](../../validation/G03.md) is written and **awaits the
user's review and acceptance**; S04-T01 needs G03.

**S03-T05** ([report](../../validation/S03-T05.md), patch
`0025-participant-script-context.patch`, `23fa820f…3f0a`, 20 files,
`patch-check-28b`, 197 files byte-equal; contract in
[participant identity](../../participant-identity.md#native-and-lua-participant-script-context--s03-t05),
sites and audit in [actor access inventory](../../actor-access-inventory.md#script-context-sites--s03-t05)):

- `xrGame/CampaignScriptContext.{h,cpp}`: a context stack over the registry
  (Self needs a live actor, Party the participant's party; innermost-only
  leave); scopes for an `ActionContext`, a captured handle, a task's owner,
  an actor's own participant; strict lookups (`campaign.*` in Lua returns
  nothing outside a context); `gameplay_script_actor` for the native
  bindings; task ownership (`CGameTask::m_campaign_owner`); `dialogue_open/close`;
  presentation outboxes; a server-only Lua compatibility layer: `db.actor` and
  `AC_ID` follow the context, `CreateTimeEvent` captures it, an owned task's
  `check_task` runs in its owner's context, out-of-context `db.actor` reads are
  counted by line while two actors live.
- Entry points converted: `UpdateTasks`, `ChangeStateCallback` (owned tasks go
  to the script host's handler in the owner's context), `give_task`,
  `RunTalkDialog` (server), phrase script calls, `CUsableScriptObject::use`;
  news, talk messages, game over and `level.*` effectors are presentation.
- smoke-133 (Identity, 18 stages; unit fixture 62 checks) and **smoke-136**
  (GameState, all 64 stages; level fixture 101 checks): probes in both
  contexts, nested, seven refused contexts, deferred events in their contexts;
  both participants run the installed mechanic quest **concurrently** (P2
  `task_1`, P1 `task_2`), each reward, goodwill, info and item removal on its
  own actor only. client-29 + capture-16/17 against smoke-136: `match: true`.
- smoke-134 (installed `tasks_fetch.script` fault: no talking partner) and
  smoke-135 (giver's talk disabled by his schedule) were fixture iterations;
  details in the report.

## Found this session (open items)

1. **Installed scripts outside a context decide for actor 0**: 320,729 reads
   from 167 lines in smoke-136 (NPC condlists `xr_logic.script:372`, meet/talk
   availability, disguise, corpse detection, warfare). The giver's talk
   availability follows actor 0 only. Bounded batches for S08/S10.
2. **Not remapped**: `has_alife_info` (`alife():has_info(0, …)`),
   `alife():actor()`, the story object. An info portion given in participant
   2's context lands on actor 2 but `has_alife_info` reads actor 0; party vs
   personal info portions are an S08 decision.
3. `axr_task_manager` offered/finished lists are per NPC, not per participant;
   the time-event key `delay_reward` is global (S08).
4. Carried: server AI vision has no light (**still awaiting the user's decision
   on where a server light model goes**); server-driven camera yaw (Stage 4);
   hearing-stage `npc_walk_events=0` (not seen in smoke-134..136); `EntityId`s
   and task owners across a checkpoint (Stage 7); fixture `PlayerId`s
   (S04-T04); detached release (S04-T05); friendly fire and god mode per
   participant (Stage 6); presentation delivery (Stage 4).

## Next

1. **G03 review by the user** ([evidence](../../validation/G03.md)). On
   acceptance: mark G03 Completed in plan.md (§I, roadmap table Stage 3) and
   record the decision in G03.md.
2. Then **S04-T01 — Validate GameNetworkingSockets in isolation** (plan.md
   Stage 4). First action: pin the GNS release and its dependencies
   (protobuf/OpenSSL or the bundled crypto option) against the recorded VS2022
   v143 toolchain and build an isolated listener + two peers under
   `tools/network-probe` (or `builds/`), without touching engine-work.

## Tools and runs

- Last run names: `campaign-115`, `client-29`, `smoke-136`, `capture-17`,
  `patch-check-28a/b`. Next: `campaign-116`, `client-30`, `smoke-137`,
  `capture-18`, patch check `29`.
- Run PowerShell scripts with the Windows path in single quotes from bash
  (`-File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Stage2.ps1'`).
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Identity|GameState` (GameState
  ~4 min; run it in the background).
- Client quest check: `prepare-stage2-smoke.py capture-N S02-T06e`,
  `Run-ClientCollisionCapture.ps1 -Run capture-N -Capture quest|quest-persisted -CheckpointRun <smoke>`;
  copy `builds/logs/S02-T06e/run-capture-N/client-quest*.csv` to
  `builds/logs/S02-T06/` before `compare-quest-capture.py`.
- Installed scripts for grepping: `python3 tools/xray-db-extract.py -q extract -o <dir>
  builds/build_2/test-install/source-client/db/configs/scripts.db0` (cp1251/CRLF).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02 carried limits are unchanged.
Previous snapshot: [before G03 review](snapshot-before-G03-review.md).
