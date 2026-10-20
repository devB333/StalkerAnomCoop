# Session handoff — S05-T04 done; next: user's G05 acceptance (then S06-T01)

## State (2026-10-20, end of session)

Stage 5 In Progress; S05-T01–T04 **Completed**. Engine `c57fe5b9` +
patches 0001–0030 (exported, reconstruction check 33); protocol v1.1
(`proto-build-15`). Design: [co-op session](../../coop-session.md).
**G05 evidence is recorded and waits for the user's decision:
[G05](../../validation/G05.md).** Do not start Stage 6 before it is accepted.

| Task | Result | Report |
|---|---|---|
| S05-T04 session | session-2: server + two matched clients; c2 late join, quit, rejoin; 26 server medkit spawns/releases; c1, c2 first run and c2 rejoin replicas equal the server's online set; three own-only walks (≈2.25 m own, 0.00 m other); screenshots from both sides | [S05-T04](../../validation/S05-T04.md) |

Fixed this session (patch 0030): the server kept a player's last input
sequence across connections, so a rejoined player's inputs were refused as
old for about a minute (session-1: rejoin walk 0.00 m). The test driver also
takes an `other-joined` screenshot when the other player appears after its
facings. `compare-coop-actors.py` checks `cN-first-run` too.

Regressions on the final builds (campaign-154, client-61, build id
`6e9c8e7e…fb7c`): smoke-152 (66 stages), capture-30 (match). smoke-151
hit the intermittent activity-fixture walk stop (4th in ~95 runs).

## Not verified / carried (see G05)

- Movement only, no prediction, NPCs/items static on clients, no client
  gameplay scripts (S06, S08/S10).
- Player 2's client: a local "Mission complete: Training Day" news item and
  `CRandomTask … task_fanat_training_1 task is nil` from an unknown source
  (also in S05-T03); presentation only.
- Client ALife scheduler stall; rare client exit access violation; the
  smoke's intermittent activity walk stop.
- One host, loopback, short sessions, one level; G01 waivers and G02–G04
  limits unchanged.

## Next

1. Ask the user to accept G05 (or name what is missing). On acceptance:
   fill G05.md's "User decision", set its title to Completed, update plan.md
   (Stage 5 row Completed, G05 row in §I, the G00/G01 summary row) and commit
   `docs: record G05 acceptance`.
2. Then S06-T01 (validated movement inputs on the server); read its plan
   entry and the S05-T03/T04 input path first (`CoopServerHost.cpp`
   `take_input`/`apply_inputs`).

## Tools and runs

- Session: `prepare-coop-session.py` + `Run-CoopSession.ps1` (scenario in
  [coop-session.md](../../coop-session.md#bounded-shared-world-session-s05-t04)).
- Builds: rebuild both products after any engine change (build ids must match).
- Last runs: `campaign-154`, `client-61`, `session-2`, `smoke-152`,
  `capture-30`, patch check 33. Next: `campaign-155`, `client-62`,
  `session-3` (or a new S06 run series), `smoke-153`, `capture-31`, patch
  check 34.
- Don't build while a session or smoke runs.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before G05](../done/snapshot-before-G05.md).
