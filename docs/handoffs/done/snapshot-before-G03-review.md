## Complete

# Session handoff — S03-T04 complete (participant activation and awareness); next: S03-T05

## State at the end of the session (2026-10-19)

Stage 3 / G03 in progress. **S03-T01..T04 Completed.** S03-T05 Not Started.

**S03-T04** ([report](../../validation/S03-T04.md), patch
`0024-participant-activation.patch`, `f70dd04d…513c`, 24 files,
`patch-check-27b`, 191 files byte-equal; it replaced the unbuilt `-wip`
patch):

- The ALife switch rules measure from the nearest alive participant actor
  (`CALifeSwitchManager::refresh_activation_anchors` / `activation_distance`,
  `alife_switch_manager.cpp:60-89`, five call sites). Rules recorded in
  [participant identity](../../participant-identity.md#activation-and-awareness--s03-t04).
- `CampaignActivationFixture.cpp` (stage `campaign-activation-passed`, exit
  34) passes 35 checks in smoke-128, 129 (campaign-110) and smoke-132
  (campaign-111, final source); all 63 GameState stages. With the actors
  1,098 m apart: 0 rule violations over 1,061 objects, every switchable object
  near actor 2 online, nothing near actor 1 dropped, the far area offline on
  return; a boar takes each actor as its enemy; a detached participant's
  retained actor keeps its area.
- Cost: +44 online objects, object update 0.78 → 0.95 ms/tick; the rule
  itself +1.3 ns/object.
- `Actor()->` sweep of `xrGame/ai/`: three more faults guarded
  (`poltergeist.cpp:334` participant-aware, vampire attack and alien vision
  no-ops). These three are compile- and regression-checked only (no
  poltergeist or bloodsucker in the fixture).
- smoke-130 (Identity) and client-27/28 pass.

## Found this session (open items)

1. **Server AI vision has no light.** The render object's luminosity is 0 on
   the headless server (`LightTrack.cpp:23`, never updated without a render
   pass); the visibility value scales with it
   (`visual_memory_manager.cpp:322-328`), so every AI sees as in darkness
   (30–60× slower). Affects all server AI since Stage 2. Needs a server-side
   light model (sun/hemisphere from weather and time, sky occlusion from the
   collision model). **Proposed as a new task; awaiting the user's decision on
   where it goes in the plan.** Do not substitute a constant.
2. A server-driven participant's camera keeps its last yaw (the fixture
   turns it as `SetActorDirection` does); real views arrive with Stage 4.
3. Hearing-stage `npc_walk_events=0` recurred in 3 of 13 GameState runs
   (smoke-120, 122, 131). Candidate: `[stalker_step_manager]`
   (`m_stalker.ltx:6-66`) has walk entries for every direction at index `_0`
   but only `norm_walk_fwd_1` at index `_1`; next check: log the walked
   stalker's legs motion name.
4. Carried unchanged: `EntityId`s across a checkpoint (Stage 7), fixture
   `PlayerId`s (S04-T04), detached-participant release (S04-T05), dropped
   script effectors (S03-T05), friendly fire and god mode per participant
   (Stage 6).

## Next: S03-T05 — native and Lua participant context contract

Inputs already collected (S03-T03 report and the
[earlier snapshot](snapshot-before-S03-T04-fixture.md)): the
`se_actor` state gate, the actor binder's `db.actor` globals, the pre-hit
callback (window, friendly fire, `db.actor` immunities), news to "the" actor,
the `level.*` gameplay bindings behind null guards, `psActorFlags`.
First action: inventory the script entry points the fixtures need (start
from `report_actor_access`'s caller list in smoke-132's server log and the
S03-T03 script gates), then define the context API.

## Tools and runs

- Last run names: `campaign-111`, `client-28`, `smoke-132`, `capture-15`,
  `patch-check-27a/b`. Next: `campaign-112`, `client-29`, `smoke-133`,
  `capture-16`, patch check `28`.
- Run PowerShell scripts with the Windows path in single quotes from bash
  (`-File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Stage2.ps1'`).
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Identity|GameState` (GameState
  ~2.5 min; run it in the background — a crash leaves the process waiting on
  the crash handler; a hearing-stage `npc_walk_events=0` is the recorded
  variation, rerun).
- Build: `Build-Stage2.ps1 -Run <name> -Product Campaign|Client` (~1–3 min
  incremental, ~4 min for the client).
- Export: `python3 tools/export-engine-patch.py <name> <check>` appends to
  `series`; update `docs/source-manifest.json` by hand.
- Activation fixture options: `-s03-switch-distance <m>`,
  `-s03-monster-section <section>` (default `boar_normal`).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02 carried limits are unchanged.
Previous snapshot: [before S03-T05](snapshot-before-S03-T05.md).
