# Carried limits

The open limits each accepted gate carried forward, in one place, so they can
be tracked to closure. Created 2026-10-20 at the user's request when G05 was
accepted (L28–L31 added at G06, 2026-10-21; L32–L36 at G07, 2026-10-23); it collects the limits recorded in [G01](validation/G01.md),
[G02](validation/G02.md), [G03](validation/G03.md), [G04](validation/G04.md)
and [G05](validation/G05.md). The gate records stay the source of what was
accepted; this file is the working list.

## Rules

- Status is `Open` or `Closed`. Close a row only with evidence: a validation
  report link and the run names. Never delete a row; closed rows stay as
  history.
- The owner is the plan.md task expected to remove the limit. "No task yet"
  means a child task must be added (R12) before the stage that needs it.
- A task that touches a row's area checks the row at its end and says in its
  validation report whether the limit still holds.
- A new limit found during a task gets a row when its gate is accepted (or
  sooner if it blocks work).
- Risk IDs (R01–R14) are in [plan.md §H](../plan.md#h-decision-and-risk-register).

## Gameplay and presentation

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L01 | Movement is the only player action that reaches the server: no fire, use, jump, lean, inventory, trade or dialogue from a client | G05 | S06-T04 (combat), S06-T07 (jump, lean), S08-T01–T03 (items), S10-T04 (dialogue, trade) | Open. 2026-10-21: fire, reload and activate-slot now reach the server ([S06-T04](validation/S06-T04.md), combat-12/13); use, jump, lean, inventory moves, trade and dialogue remain |
| L02 | No client prediction or reconciliation: a player's own movement waits for the server (input → tick → snapshot → 100 ms render delay); the user found walking choppy | G05 (S05-T03) | S06-T03 | Closed 2026-10-21: [S06-T03](validation/S06-T03.md), runs pred-8, pred-9 and session-8. The own actor is predicted natively and reconciled; it responds in 83–100 ms (250 ms without); wrong predictions converge |
| L03 | Server movement is native physics but inputs are not validated (rates, speeds, stance changes) | G05 | S06-T01 | Closed 2026-10-20: [S06-T01](validation/S06-T01.md), runs move-8 and session-3. Inputs are sequenced by tick and validated, one is consumed per tick, and forged streams, teleports and positions are refused. The stamina-exhaustion and pitch-clamp limits are listed in the report |
| L04 | NPCs and items do not move on clients after spawning; only the two participant actors have snapshots; snapshot rates/baselines provisional | G04, G05 | S06-T02 | Closed 2026-10-21: [S06-T02](validation/S06-T02.md), runs snap-11 and snap-12 (5 % loss, 40 ms lag). Creatures and moving physics objects are sent as deltas against acknowledged baselines and presented within p95 0.11–0.14 m; objects rest where the server has them; 11–14 KB/s per client. NPC animation state, ownership changes and deaths are not replicated (report limits) |
| L05 | No gameplay scripts on clients (HUD scripts, news, PDA updates); presentation outboxes are not delivered to clients. Narrowed by [S06-T08](validation/S06-T08.md) (patch 0041): camera and post-process effectors, script sounds on an actor or at a position, and script particles are delivered to clients (a late join misses ongoing ones); HUD statics, news, dialogs and game over still are not | G03, G05 | No task yet: a presentation-split task before S08 | Open |
| L06 | Player 2's client shows a local "Mission complete: Training Day: Boar Hunting" news item and logs `CRandomTask:task_fail/task_complete task_fanat_training_1 task is nil` after it goes live; source not found (not in `scripts.db0` or the engine). Presentation only | G05 (S05-T03, S05-T04) | With L05 | Open |
| L07 | Player 2's actor has the engine's default name ("Zhora Squire"), faction and look, not the player's own | G05 | No task yet: player profile/appearance before S08 | Open |
| L08 | Friendly fire and god mode are not per participant | G03 | S06-T04 | Open. 2026-10-21: hits between participant actors follow one co-op rule both ways (profile `friendly_fire`, [S06-T04](validation/S06-T04.md)); god mode (`AF_GODMODE_RT`) is still process-wide, and other hits on a second participant still pass the installed callback's `db.actor` checks (L11) |
| L09 | Weather effects (storms, emission visuals) not replicated, only the weather section and game time | G05 (S05-T03) | S06-T08 (visuals, user decision at G06); S10-T05 (emission gameplay) | Closed 2026-10-22: [S06-T08](validation/S06-T08.md), runs cweather-2, cweather-3 (5 % loss, 40 ms) and cweather-4. The server's cycle, blended descriptors, weather effect and its time left, its bolts, and the scripts' effectors, sounds and particles reach both clients (patch 0041). The emission's gameplay stays S10-T05 |

| L28 | Deaths are in memory only; a dead player stays dead (no respawn); the killer is not replicated to clients (a body names itself) | G06 | S07-T07 (respawn, durable death); killer presentation: with L05 | Open |
| L29 | Hits are resolved at the server's present time; no lag compensation | G06 | No task yet (decide from play tests; S11-T01 latency work) | Open |
| L30 | Fixture NPCs decide the tests' course: Fanat (companion) follows actor 0, fights, and his melee (`xrs_facer`) drops a punched player's weapon; the boar's AI chooses whether to attack (8 of 10 runs) | G06 | S06-T06 (Cordon, no Fanat) | Open. 2026-10-22: Fanat's part removed: the co-op programs run in Cordon with no companion and no program names him ([S06-T06](validation/S06-T06.md), cenc-19…22, ccombat-5, cpred-5, csession-5, csnap-5). The boar's choice remains; a fixture lethal hit at 60 s makes sure it dies |
| L31 | Installed death game modes (Azazel, Ironman) do not run for participants | G06 | None: accepted scope (user decision 2026-10-21) | Open |

## Campaign and scripts

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L10 | Quest coverage: one fetch task from one giver on one level; no storyline, timed, failed, cancelled, multi-stage or simulation-NPC tasks; dialogue window not shown | G02, G03 | S08-T04–T06, S10-T04/T05 | Open |
| L11 | Installed scripts outside a participant context still decide for actor 0 (NPC condlists, meet/talk, disguise, warfare; 167 lines in the S03-T05 audit); `has_alife_info`, `alife():actor()` and the story object are not participant-aware; party vs personal info portions undecided (R04, R09) | G03 | S08, S10-T04/T05 | Open |
| L12 | Player-bound monster abilities (phantom, psy dog aura, controller, snork, pseudogigant, scanning) are explicit no-ops or guarded (B21) | G02, G03 | S10-T02 | Open |
| L13 | AI lighting: NPCs outside every participant's view keep their last lighting; dynamic lights do not count (as on the DX11 client) | G03 (S03-T06) | S10-T02 | Open |
| L14 | One level (`k00_marsh`) and one save; no level change or new game | G02–G05 | S09, S10-T01 | Open. 2026-10-21: now enforced: a level-changer touch crashed the headless server (enc-9); the campaign server and co-op clients refuse level-changer touches and `M_CHANGE_LEVEL` and log them ([S06-T05](validation/S06-T05.md), enc-12) |

## Persistence

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L15 | Checkpoint has no crash or journal guarantee; ownership, captured contexts, participant references and actor 2's script state are not persisted (R07, R08) | G02, G03 | Stage 7 (S07-T01–T06) | Closed 2026-10-23 for the parts Stage 7 owns: [S07-T02](validation/S07-T02.md) (generations published atomically, store tests st-2 incl. the 8-point termination matrix), [S07-T03](validation/S07-T03.md) (write-through journal, fail-closed), [S07-T04](validation/S07-T04.md) (recovery: rec-2, rec-3 with 50 records replayed, identities and participant references restored), [S07-T06](validation/S07-T06.md) (engine termination matrix mx1). What stays open has its own rows: actor 2's script state (L32), ownership (S08, K4), captured contexts (L33) |
| L32 | Actor 2 has no Lua binder and no `pstor`: its script state (tasks, info portions, story flags) is not persisted or run; actor 1's is, through the native `.scoc` | G07 (S07-T01 §3 D) | S10-T05 / the presentation-script split (no task yet: R12) | Open |
| L33 | Captured script contexts and presentation outboxes are not persisted; a restart drops what was queued (presentation only) | G07 | S10-T05 | Open |
| L34 | Checkpoint publication runs on the simulation thread: the native save (54–128 ms) plus the store's flush, rename, rotation and pointer steps (20–620 ms per generation) stall the tick; `checkpoint_seconds` bounds the rollback and the stall rate | G07 (S07-T02) | S11-T01 (move the store steps off the thread; keep the take inside the tick) | Open |
| L35 | Durability is tested against process termination only: power loss and devices that misreport flush completion are not tested and not claimed (contract §5, §12); retention keeps one previous generation | G07 (S07-T02/T03) | S11 (R08: record as a product limit; no engineering task) | Open |
| L36 | Inventory, quest and reward effects have no journal records or adapters: a restart rolls them back to the last generation; only the synthetic ledger, deaths and respawns are durable | G07 | S08-T01–T06 (records and adapters with their crash tests) | Open |

## Network and security

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L16 | One Windows host, loopback only; no LAN, internet, NAT or firewall test; no internet-reachability claim (R06) | G04, G05 | S11-T01 | Open |
| L17 | Active man in the middle: the handshake runs inside unauthenticated GNS encryption, so an on-path attacker relaying a live handshake keeps the session. User-accepted for development; preferred fix is an application-level key exchange signed by a pinned server key and the player keys (R06) | G04 | G11 (release) | Open |
| L18 | Liveness defaults provisional (1 s / 5 s / 15 s; slots kept until released) | G04 | S11-T01 | Open |
| L19 | No sanitizer in the pinned toolchain; races checked by repetition and invariants | G04 | S11-T04 | Open |
| L20 | protobuf 3.21.12 (GNS dependency) is old | G04 (S04-T01) | S11-T05 | Open |

## Reliability and testing

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L21 | Short runs: server runs about 2 min, co-op sessions under 5 min; no long-run stability, memory or drift evidence | G02, G05 | S11-T03 | Open |
| L22 | Behavioural comparisons are rule- and milestone-based; timing and totals differ from the client | G02 | S06 (measured convergence) | Open. Measured: own-actor convergence (S06-T03); combat health/slot/rounds agreement ≥ 95 % with equal end state ([S06-T04](validation/S06-T04.md)); NPC combat measured: health/alive agreement ≥ 99.9 % with equal end state, deaths shown within 120 ms, nominal to 10 % loss / 100 ms ([S06-T05](validation/S06-T05.md), enc-15…19). Remaining: long runs (S11-T03) and the single-player rule-based captures |
| L23 | GameState smoke's activity fixture: the walked stalker sometimes stops short (smoke-143, smoke-148, smoke-151; about 4 in 95 runs). Rerun under a new name; two failures in a row is a defect | G03, G05 | S11-T01/T02 | Open |
| L24 | Rare client access violation at exit (capture-10 `0xC0000005` after teardown; 2 of ~40 S05-T03 client runs; none in S05-T04) | G02, G05 | S11-T04 | Open |
| L25 | Client ALife scheduler stalls after load on a replica (S05-T02); not needed by a replica, cause open | G05 | S06-T02 (check while client presentation changes) | Closed 2026-10-21: [S06-T02](validation/S06-T02.md), not reproduced: seen only in S05-T02 world-2; in every later client run (S05-T03 to S06-T02, including snap-1…12) the replica's ALife shedule count rises steadily. The replica runs no offline simulation; the world-2 cause is unknown |

## Baseline waivers (G01)

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L26 | Completed-quest/reward verification on the unmodified client waived for Stage 2 entry (the quest attempt failed) | G01 | S08-T06, S11-T04 | Open |
| L27 | Original-client comparison (copied original installation) not run | G01 | S11-T04 | Open |
| L28 | Actor-relative presentation (blowout waves, ambient loops, siren position, cover-dependent psychic factor) is computed for participant 1's actor (`db.actor` outside a context) and shown to both players | S06-T08 (after G06) | S10-T05; the presentation split before S08 | Open |
| L29 | The emission's gameplay (cover check, kills, death camera) runs single-player-style on actor 0 and its death camera is broadcast | S06-T08 (after G06) | S10-T05 | Open |
