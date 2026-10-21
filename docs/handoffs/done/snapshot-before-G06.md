## Complete

# Session handoff — S06-T05 in progress (uncommitted)

## State (2026-10-21)

Stage 6 **In Progress**: S06-T01–T04 **Completed** (last commit `0246232`,
patch 0034, protocol v1.4 `proto-build-30`). **S06-T05 In Progress.** Its
work is **not committed and not exported as a patch**:
- **Engine:** in `builds/build_1/engine-work`, 7 files: `Entity.cpp`,
  `entity_alive.cpp`, `Actor.cpp`, `CoopServerHost.cpp`, `CoopClientLink.cpp`,
  `CoopConfig.{h,cpp}`. Diff against the 0034 state (`patch-check-37b`):
  `builds/logs/S06-T05/wip-s06-t05-only.diff`. Full engine-work diff backup:
  `builds/logs/S06-T05/wip-engine-work.diff`.
- **Workspace (uncommitted):**
  - `tools/compare-coop-encounter.py` (new)
  - `tools/prepare-coop-session.py` (`--encounter`)
  - `docs/coop-session.md` (section "NPC encounter and deaths (S06-T05)")
  - `docs/network-protocol.md` (health 0 = dead)
  - `plan.md` row
- The last build is `campaign-199` / `client-102` (build id `5ba64072…`). It
  contains all of the above.

## What S06-T05 has so far (design: [co-op session](../../coop-session.md#npc-encounter-and-deaths-s06-t05))

- **Participant deaths are native on the server** (`CEntity::KillEntity`).
  Before this, the second participant's actor never died (health 0, no
  `Die`): the installed before-death callback only reaches `db.actor`.
- **Health 0 = dead in snapshots.** A living entity is sent at least 1.
- **Clients present deaths** with the native `Die`. On a replica,
  `CEntityAlive::Die` skips script death callbacks, relations and the killer
  event; `CActor::Die` skips the single-player game over. Dead bodies are not
  position-presented, and a dead own actor is not reconciled. Creature health
  is applied while alive. NPC deaths anywhere on the level now show on both
  clients.
- **The dead take no input:** consumed and counted as `dead_in<slot>` in the
  server status line.
- **Fixtures:**
  - `encounter`: a `boar_normal` 12 m beside participant 2, away from Fanat,
    20 s after both clients send input.
  - `lethal`: participant 2 takes a lethal wound hit 8 s after the boar died.
  - Record: `coop-encounter.csv` (actors, boar and Fanat every 100 ms).
- **Client program `script = 4`:** sprint 4 s away from Fanat, equip, reload,
  aim at the boar, hold fire until it hurts someone or 12 s pass, then fire and
  reload. After its own death: a dead walk at +5 s and a forged revive at +8 s.

## Results so far (`builds/logs/S06-T05/`)

- **enc-2 (campaign-196) passed all 22 checks.**
  - The boar hurt player 1; the players and Fanat killed it.
  - Both clients showed its death in 5–35 ms, and the lethal hit's death in
    under 70 ms.
  - Player 1 later bled to death (a native actor-0 death on the server; the
    server survived).
  - The dead players' walks moved 0.00 m and fired nothing; the forged revive
    was refused; agreement 99.9 %; the server ticked at a median 50 Hz.
- **enc-3…6:** deaths, agreement, the dead walk and the forged checks pass.
  `npc_damage` fails: **Fanat (19409), player 1's companion, follows actor 0
  and kills the boar** before it hurts anyone. enc-6 (10 % loss, 100 ms) also
  failed `dead_inert`: the client's ragdoll was still falling 3 s after death,
  so the dead walk now starts at +5 s.
- **enc-7 and combat-11 (S06-T04): invalid.** Clients loaded in about 40 s
  right after a build, and the server dropped them at its 15 s silence
  timeout. The encounter now waits for input.
- **enc-8** passed except damage (Fanat again: the players had run in
  opposite directions) → the boar now spawns beside **participant 2**.
- **enc-9 (campaign-199): the server crashed** (`0xC0000005`) before the boar
  spawned.
  - Stack: `CCC_MainMenu::Execute` ← `CConsole::ExecuteCommand` ←
    `CApplication::OnEvent` ← `CCampaignServerClock::Tick`
    (`builds/logs/S06-T05/run-enc-9/server/xray-campaign-server_djb20.log`,
    line 458).
  - Something sent the headless server the `main_menu` command at tick 2904,
    while player 1 was sprinting about 22 m west of the base (x ≈ 535).
  - **Next step:** find who issues `main_menu` (a script reacting to actor 0,
    such as a level-changer or no-go zone?). Guard it on the campaign server,
    or change the sprint direction.
- **Weather (user report):** a storm on screen differed between clients. The
  weather cycle names matched (`w_cloudy1`). Storm and emission effects are
  not replicated (L09, owner S10-T05). The user asked whether that will be
  implemented: yes, S10-T05. A smaller storm-visuals task earlier was offered.

## Next steps

1. Fix the enc-9 crash, rebuild (`campaign-200`, `client-103`), rerun the
   encounter until the boar hurts a player (`npc_damage`).
2. Final evidence on one build:
   - enc runs: nominal ×2, 5 % / 40 ms, and 10 % / 100 ms (a failure-limit
     probe; report whatever happens).
   - Regressions: combat (S06-T04 program), session (S05-T04 command),
     snapshots (snap), GameState smoke, checkpoint capture and combat capture.
   - Chain script to adapt:
     `/tmp/claude-1000/-mnt-c-Users-djb20-StalkerAnomCoop/764065e9-df87-4d90-a5d3-79e716fa30a3/scratchpad/final-t05.sh`
     (may be gone; it is a list of the commands below).
3. Export patch 0035 (`python3 tools/export-engine-patch.py
   0035-coop-npc-encounter-and-deaths.patch 38`), update
   `docs/source-manifest.json`, write `docs/validation/S06-T05.md` (rule
   changes so far: dead walk +2 s → +5 s; spawn beside participant 2;
   encounter clock from input), then `docs/validation/G06.md` from
   S06-T01–T05 (template: G05.md) and **stop for user acceptance of G06**.
4. Decisions to put to the user at G06:
   - friendly fire policy (on by default, `friendly_fire`)
   - what a dead player does next (respawn is unplanned)
   - the installed death game modes (Azazel, Ironman), which no longer run
     for participants
   - the Cordon fixture question
   - a possible early storm-visuals task

## Commands

```
python3 tools/prepare-coop-session.py <run> --task S06-T05 --encounter c1,c2 [--net-loss 5 --net-lag 40]
powershell.exe ... Run-CoopSession.ps1 -Run <run> -Task S06-T05 -ServerAloneSeconds 10 -ClientStarts '0,10' -ClientSeconds '200,190' -AfterSeconds 10
python3 tools/compare-coop-encounter.py builds/logs/S06-T05/run-<run> --out .../compare-encounter.json
# SP combat regression (S02-T06c): a client capture compared with a smoke's server-combat.csv
python3 tools/prepare-stage2-smoke.py capture-39 S02-T06c
powershell.exe ... Run-ClientCollisionCapture.ps1 -Run capture-39 -Capture combat -TimeoutSeconds 600
python3 tools/compare-combat-capture.py builds/logs/S02-T02/run-smoke-<n>/server-combat.csv builds/logs/S02-T06c/run-capture-39/client-combat.csv
```

## Run names

- Used:
  - builds: up to `campaign-199`, `client-102`
  - S06-T05 runs: `enc-1…9`, `combat-14` (interrupted)
  - S06-T04 runs: `combat-1…13`, `session-9/10`, `snap-15/16`, `pred-11`
  - smoke and capture: up to `smoke-161`, `capture-37`
- Next: `campaign-200`, `client-103`, `enc-10`, `combat-15`, `session-11`,
  `snap-17`, `smoke-162`, `capture-38` (checkpoint), `capture-39` (combat),
  patch check 38.
- Right after a build, wait about a minute before a session; clients can
  otherwise load past the server's 15 s timeout.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture.
Previous snapshot: [before S06-T05 continued](../done/snapshot-before-S06-T05-continued.md).
