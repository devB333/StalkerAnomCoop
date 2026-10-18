## Complete

# Session handoff — S03-T01 complete (participant identity); next: S03-T02

## State at the end of the session (2026-10-18)

**S03-T01 is Completed** and exported as patch 0021
(`7c9dd3fe…fda2`, 17 files; reconstruction `patch-check-22b`). The identity
model, engine-id mapping, participant registry and ownership table are in
[participant-identity.md](../../participant-identity.md); evidence in
[S03-T01](../../validation/S03-T01.md).

- `xrGame/CampaignIdentity.{h,cpp}`: 128-bit `CampaignId`/`PlayerId`/`PartyId`,
  64-bit `EntityId` (`ItemId` = `EntityId`), `EngineEntityKey {u16, generation}`,
  `ParticipantRef {slot, generation}`, `CampaignIdentityRegistry`
  (two slots as policy, `Free → Bound → Attached ⇄ Detached`, `ServerAuthority`
  token, `authorize(ActionContext, Scope)`), process registry `g_campaign_identity`
  (server only; the client has none until S03-T02).
- The entity id is carried on `CSE_Abstract::m_campaign_entity` because the
  ALife switches free and re-request an object's own u16 synchronously
  (`alife_switch_manager.cpp:52-62, 91-95`); `xrServer::PerformIDgen(id, object)`
  and `FreeID` report to the registry, every assignment site passes its object.
- Fixtures: `Run-Stage2Smoke.ps1 -Scope Identity` (4 unit fixtures, 111 checks,
  smoke-98/100 pass, ~3 s); GameState adds `campaign-identity-passed` after
  `campaign-ticked` (smoke-99 pass: 1,095 online + 26,248 ALife objects all
  bound and matching, 0 orphans, actor held by slot 0; smoke-103 from the final source: all 60 stages).
- **Hearing-stage variation explained** (S03-T01 report, "Hearing-stage variation"): smoke-101/102 failed `campaign-hearing-failed` with the pre-existing signature; the fixture's walk criterion is met by the stalker's rifle holster/draw sounds, not footsteps, and the server emits no footstep events during the scripted walk while the client does. Open item for S03-T04; rerun once on that failure until then.
- Findings: in one level load the engine re-requests objects' own ids ~1,075
  times and hands a freed id to a different object once; over a run with two
  checkpoint restarts: 3,505 continuations, 5 reuses, `freed_unbound=0`
  (every id flow passes the hooks).

## Next steps

1. **S03-T02** (separate local view access from gameplay authority), per
   plan.md and the G02 carried input: inventory and classify every
   `Level().CurrentEntity()` / `Actor()` use in `src/xrGame` (126 / 372 raw
   sites), gameplay uses become participant lookups through the registry;
   resolve `phantom.cpp:86,149,266`, `ai_trader.cpp:99` and the psy dog aura
   guard or keep each behind a recorded guard; decide the client's registry
   role; keep the `bLocal` import bridge server-only and enumerate remote
   outcome paths. Rerun the GameState smoke after engine changes.
2. Open limits for later tasks: `EntityId`s do not survive a checkpoint
   reload (Stage 7); fixture `PlayerId`s only (S04-T04); release of a
   detached participant is policy for S04-T05.

## Tools and runs

- Last run names: `campaign-84`, `client-25`, `smoke-103`, `capture-13`
  (`patch-check-22a/b`). Next: `campaign-85`, `client-26`, `smoke-104`,
  `capture-14`; next patch export is `python3 tools/export-engine-patch.py <name> 23`.
- Run PowerShell scripts with the Windows path in single quotes from bash
  (`-File 'C:\Users\djb20\StalkerAnomCoop\tools\Build-Stage2.ps1'`), or the
  backslashes are lost and the script does not run.
- Smoke: `python3 tools/prepare-stage2-smoke.py <run>` (no task argument for
  smokes) then `Run-Stage2Smoke.ps1 -Run <run> -Scope Identity|GameState`.
  GameState ~4 min; its hearing stage occasionally reports `npc_walk_events=0`;
  rerun once and record it.
- Client captures, compare tools and quest fixture options: see the
  [S03-T01 snapshot](../done/snapshot-before-S03-T01.md#tools-and-runs).
- Build: `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`
  (~6 min after a project-file change, ~1.5 min incremental).
- Script files in the archives are cp1251/CRLF; use `grep -a`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers and the
G02 carried limits are unchanged.
Previous snapshot: [before S03-T02](snapshot-before-S03-T02.md).
