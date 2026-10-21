## Complete

# Session handoff — S06-T04 done; next: S06-T05

## State (2026-10-21)

Stage 6 **In Progress**: S06-T01–T04 **Completed**; S06-T05 Not Started (it
produces the G06 report). Engine `c57fe5b9` + patches 0001–0034
(reconstruction check 37). Protocol v1.4 (`proto-build-30`, the default in
`Build-Stage2.ps1` and `prepare-coop-session.py`). Design:
[co-op session](../../coop-session.md#authoritative-weapon-slice-s06-t04).

S06-T04 ([report](../../validation/S06-T04.md)):
- **Fire is an input.** The fire bit makes the server press or release
  `kWPN_FIRE` on the player's own actor, so the native weapon decides cadence,
  ammo and ballistics. Actions: activate slot (0 holsters) and reload.
  Outcomes: health, slot, rounds and a shot counter (`kWeapon`).
- **Fire ray per actor:** a non-view actor fires along its own camera
  (`CActor::g_fireParams`).
- **Hits between participants:** the installed pre-hit callback let only
  actor 0 hurt the other player. Hits between participants now skip it and
  follow one rule both ways, profile `friendly_fire` (default 1). **The final
  friendly-fire policy is a user decision for G06.**
- **Results:** combat-13 (nominal) and combat-12 (5 % loss, 40 ms lag) pass
  all 21 checks; display agreement 99.4–99.7 %. Regressions session-10,
  snap-16, pred-11, smoke-160/161 and capture-37 pass.
- **Smoke flakiness:** smoke-157 and smoke-159 failed the activity walk (L23)
  and smoke-158 the two-actors walk check (seen before in smoke-154). Watch
  the rate.
- **Carried limits:** L01, L08 and L22 updated (still open).

## Next: S06-T05 — NPC combat encounter and the G06 report

Read plan.md S06-T05. Notes:
- **Fixture:** extend the combat program with one hostile NPC that targets
  either participant. The server's AI already fights headless (S02-T06c,
  stalker against boar) and targets either player (S03-T04).
- **Deaths:** clients now keep actors alive whatever the snapshot health
  says (`apply_actor_outcomes`), and `combat_heal` keeps the fixture going.
  Death of a participant and of the NPC must replicate: decide the client
  presentation and what the server does with a dead participant (S03-T03
  killed and released actors on the server).
- **Measure:** the simulation and snapshot rates and bandwidth (snapshot CSVs
  as in S06-T02), plus controlled latency and loss and forged outcomes.
  Rerun the single-player combat regression (S02-T06c).
- **Fanat** (the companion at the base) pushes and strikes actors, and once
  put away actor 0's weapon (combat-10). Expect it in an NPC fixture near the
  base, or pick an NPC away from it.
- **G06:** write `docs/validation/G06.md` from S06-T01–T05, then stop for user
  acceptance.

## Tools and runs

- Combat:
  - `prepare-coop-session.py <run> --task S06-T04 --combat c1,c2 [--net-loss 5 --net-lag 40]`
  - `Run-CoopSession.ps1 ... -ClientStarts '0,10' -ClientSeconds '250,240'`
  - `compare-coop-combat.py <logs>`
- Last runs: `campaign-193`, `client-97`, `proto-build-30`, `pt-28`,
  `combat-13`, `session-10`, `snap-16`, `pred-11`, `smoke-161`,
  `capture-37`, patch check 37.
- Next: `campaign-194`, `client-98`, `proto-build-31`, `pt-29`, `smoke-162`,
  `capture-38`, patch check 38; co-op runs under `builds/logs/S06-T05/`.
- Run names used by stopped chains are taken (`session-9`, `combat-6`); pick
  new ones.
- Right after a build, client loads can run past the server's 15 s silence
  timeout (combat-11). Rerun under a new name.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before S06-T05](snapshot-before-S06-T05.md).
