## Complete

# Session handoff — S08-T01 (durable item pickup) In Progress: built, not yet run

## State (2026-10-23, paused by the user)

- **K1/K5 retest build is ready:** campaign-244 (0 errors) / **client-146**
  (client-144 was cut off mid-link; client-145 linked the stale
  `proto-build-35` and failed with `LNK2001 CheckpointStore::publish`).
  `Build-Stage2.ps1 -ProtocolBuild` is now **mandatory** (CLAUDE.md and the
  stage2-run skill updated). The user's retest is still open (steps in the
  [previous snapshot](snapshot-before-S08-T01-first-run.md)); note
  campaign-246 / client-148 below contain 0044 too and can be used instead.
- **S08-T01 code written and built, no gameplay run yet:**
  - Protocol **v1.6** (`protocol/`): action kind 5 `pickup` (target = item),
    `ActionReason` enum (8 ItemGone, 9 ItemTaken, 10 OutOfReach,
    11 NotTakeable), lifecycle message `Reparent` 0x0023 (an entity's owner
    changed), journal kind `kRecordPickup` 7. **proto-build-39**, tests
    **st-6 108/108 pass** (proto-build-38/st-5: the fixture count 22→24 fix).
  - Server (`CoopServerHost.cpp`, `take_pickup` …): six phases; validation
    (alive, online item with no owner, `CanTakeItem`, reach 2 m + 0.5 m from
    the actor's feet-to-eyes axis); `pickup` record = owner/item entities +
    section + the item's native spawn state; apply = the bridge's
    `GE_OWNERSHIP_TAKE` (REJECT first if it has another parent); replay is
    absolute post-state, re-creating an item the checkpoint lacks under its
    EntityId (`CampaignIdentityRegistry::expect_entity`), deferring until
    level objects exist (respawned owner); `pickup-confirmed` check of the
    level actor; `coop-pickups.csv`; `pickup-state` dumps (recovered/final);
    failpoints `pickup:before-append|after-flush|after-apply|after-ack`.
    `track_lifecycle` now emits `Reparent` for **any** parent change (also
    NPC pickups — the K4 class). Race fixture: profile `pickup_race`,
    `pickup_rounds` (CampaignEvent 6 names the claim tick).
  - Client (`CoopClientLink.cpp`, `game_cl_base.cpp`): every pickup path
    (`SendPickUpEvent`) becomes the intent; retries every 3 s; `Reparent`
    applied through REJECT/TAKE events under `ReplicaPermit`; script 7 race
    driver (claim at tick, same-id repeat, new-id repeat after losing);
    `coop-pickup-client.csv`; world summary gains `parent_entity`,
    `level_parent_entity`.
  - Tools: `prepare-coop-session.py --pickup c1,c2 [--pickup-rounds 8]
    [--pickup-start 20]` (default protocol build now 39),
    `tools/compare-coop-pickup.py` (one winner, one commit per item,
    duplicates, refusal rounds, acknowledged held, clients agree,
    `--expect-restart`).
  - Builds: **campaign-246 / client-148** (0 errors; campaign-245/client-147
    failed on two forward declarations, fixed).
  - Patch **0045-durable-item-pickup-wip.patch** export started with patch
    check 49 (`tools/export-engine-patch.py … 49`); **confirm it finished**
    (`builds/logs/patch-reconstruction-49.json`, `patches/engine/series`,
    `docs/source-manifest.json` hashes). If it did not, rerun with check 50.
    The engine-work tree is the authoritative copy until then.
- No `docs/validation/S08-T01.md` yet.

## First thing next session

1. Check the patch export (above); commit it if it is not committed.
2. Run the prepared session **pick-1** (already prepared, campaign-246 /
   client-148 bins copied):
   ```
   powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File 'C:\Users\djb20\StalkerAnomCoop\tools\Run-CoopSession.ps1' -Run pick-1 -Task S08-T01 -ServerAloneSeconds 10 -ClientStarts '0,15' -ClientSeconds '300,285' -AfterSeconds 15
   python3 tools/compare-coop-pickup.py builds/logs/S08-T01/run-pick-1 --out builds/logs/S08-T01/run-pick-1/compare-pickup.json
   ```
   Expect first-run defects (the client Reparent path and the replay
   re-creation are untested).
3. Then crash runs (`--failpoint pickup:after-flush:2`, `after-apply`,
   `after-ack`, `pickup:before-append:2:fail`, each with
   `Run-CoopSession … -ServerKill 0 -ServerRestartAfter 10` and
   `compare-coop-pickup.py --expect-restart`), an impaired run (5 %/40 ms),
   a hands-on use-key check, then `docs/validation/S08-T01.md`, the contract
   §6 `pickup` row (absolute post-state replay instead of "owned by someone
   else → fail"), `docs/network-protocol.md` v1.6, carried limits (client-side
   scripts' item callbacks run on the client; drops/consumption are not
   durable until S08-T02).

## Run names

- Used today: campaign-245/246, client-145…148, proto-build-38/39, st-5/6,
  pick-1 (prepared only), patch check 49.
- Next: `campaign-247`, `client-149`, `proto-build-40`, `st-7`, `pick-2`,
  patch check 50.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S08-T01 first run](snapshot-before-S08-T01-first-run.md).
