## Complete

Snapshot of the handoff that led into S05-T04 (history only; do not resume from it).

# Session handoff — S05-T03 done; next: S05-T04 (then G05)

## State (2026-10-20, end of session)

Stage 5 In Progress; S05-T01, T02, T03 **Completed**. Engine `c57fe5b9` +
patches 0001–0029 (exported, reconstruction check 32); protocol v1.1
(`proto-build-15`). Design: [co-op session](../../coop-session.md).

| Task | Result | Report |
|---|---|---|
| S05-T01 roles | serve mode, client link, local-start guard, build/content identity; injection 3/3 refused (coop-9) | [S05-T01](../../validation/S05-T01.md) |
| S05-T02 world | join = native checkpoint + Spawn/Despawn stream; client replica (no simulation, local outcomes refused); world-7: three replicas (late join, rejoin, churn) equal the server's online set | [S05-T02](../../validation/S05-T02.md) |
| S05-T03 views | own view per client; input → server → actor (session context only); 20 Hz actor snapshots, server-time interpolation; world clock + weather; view-11: each walk moved only its own actor, both clients within 0.15 m of the server, same sky | [S05-T03](../../validation/S05-T03.md) |

Regressions on the final builds: smoke-150 (66 stages), capture-28
(single-player checkpoint match).

User feedback during the session: walking felt "incredibly choppy" (eased
by the buffered interpolation; prediction is S06-T03); different weather
on the two clients (fixed: world clock). The user was asked not to play in
the test windows; their clicks earlier are visible in coop-9/view-2 logs.

## Not verified / carried

- No prediction, no actions (fire/use/jump), NPC/item movement not
  replicated (S06); no gameplay scripts on clients (presentation split S08/S10).
- Client ALife scheduler stalls after load (not needed; cause open).
- Rare client access violation at exit (2 of ~40 runs).
- smoke-148's intermittent activity-fixture walk stop (third time in ~90 runs).
- One host, loopback only; G01 waivers and G02–G04 limits unchanged.

## Next: S05-T04 — verify a bounded shared-world session (then G05)

One session that shows the G05 evidence together: two joins, both actors
visible, a server-controlled entity change seen by both (churn medkit),
late join and disconnect/rejoin, state comparison and screenshots.

```
python3 tools/prepare-coop-session.py session-1 --task S05-T04 --churn 10 --script c1,c2
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File 'C:\Users\djb20\StalkerAnomCoop\tools\Run-CoopSession.ps1' -Run session-1 -Task S05-T04 -ServerAloneSeconds 10 -ClientStarts '0,45' -ClientSeconds '220,80' -Restart 'c2:20:90' -AfterSeconds 10
python3 tools/compare-coop-world.py builds/logs/S05-T04/run-session-1/server/coop-lifecycle.csv builds/logs/S05-T04/run-session-1/<c1|c2-first-run|c2>/coop-world-final.csv
python3 tools/compare-coop-actors.py builds/logs/S05-T04/run-session-1 --lag-ms 120
```

Then write `docs/validation/S05-T04.md` and `docs/validation/G05.md` (the
G05 requirement table, carried limits), update plan.md, and **stop for the
user's G05 acceptance** before Stage 6.

## Tools and runs

- Builds: `Build-Stage2.ps1 -Run <name> -Product Client|Campaign` (rebuild
  both after any engine change: build ids must match).
- Last runs: `campaign-152`, `client-59`, `view-11`, `smoke-150`,
  `capture-28`, patch check 32. Next: `campaign-153`, `client-60`,
  `session-1`, `smoke-151`, `capture-29`, patch check 33.
- Don't build while a session or smoke runs.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before S05-T04](snapshot-before-S05-T04.md).
