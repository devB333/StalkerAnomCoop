# Carried limits

The open limits each accepted gate carried forward, in one place, so they can
be tracked to closure. Created 2026-10-20 at the user's request when G05 was
accepted (L28–L31 added at G06, 2026-10-21; L32–L36 at G07, accepted 2026-10-23; L37–L43 at G08, accepted 2026-10-25; L44–L49 at the G08 condition review, accepted 2026-10-27); it collects the limits recorded in [G01](validation/G01.md),
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
| L01 | Movement is the only player action that reaches the server: no fire, use, jump, lean, inventory, trade or dialogue from a client | G05 | S06-T04 (combat), S06-T07 (jump, lean), S08-T01–T03 (items), S10-T04 (dialogue, trade) | Open. 2026-10-21: fire, reload and activate-slot now reach the server ([S06-T04](validation/S06-T04.md), combat-12/13); use, jump, lean, inventory moves, trade and dialogue remain. 2026-10-24: item pickup reaches the server as a validated durable action from every pickup path, the use key included ([S08-T01](validation/S08-T01.md), pick-12/13/14); 2026-10-24: drops and moves between a player and a box or a body too ([S08-T02](validation/S08-T02.md), tr-13…18); equip moves, use/eat, giving, trade and dialogue remain 2026-10-25 (G08): the party quest's accept and turn-in too (kind 8); equip and use are L37, the talk window L43. 2026-10-26: equip moves and use/eat reach the server ([S08-T07](validation/S08-T07.md)); giving, trade and dialogue remain. |
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
| L10 | Quest coverage: one fetch task from one giver on one level; no storyline, timed, failed, cancelled, multi-stage or simulation-NPC tasks; dialogue window not shown | G02, G03 | S08-T04–T06, S10-T04/T05 | Open 2026-10-25 (G08): a second quest, Sidorovich's fetch, shared by both players through its installed functions ([S08-T04](validation/S08-T04.md)); storyline, timed, failed, cancelled and multi-stage tasks remain (S10-T04/T05). |
| L11 | Installed scripts outside a participant context still decide for actor 0 (NPC condlists, meet/talk, disguise, warfare; 167 lines in the S03-T05 audit); `has_alife_info`, `alife():actor()` and the story object are not participant-aware; party vs personal info portions undecided (R04, R09) | G03 | S08, S10-T04/T05 | Open |
| L12 | Player-bound monster abilities (phantom, psy dog aura, controller, snork, pseudogigant, scanning) are explicit no-ops or guarded (B21) | G02, G03 | S10-T02 | Open |
| L13 | AI lighting: NPCs outside every participant's view keep their last lighting; dynamic lights do not count (as on the DX11 client) | G03 (S03-T06) | S10-T02 | Open |
| L14 | One level (`k00_marsh`) and one save; no level change or new game | G02–G05 | S09, S10-T01 | Open. 2026-10-21: now enforced: a level-changer touch crashed the headless server (enc-9); the campaign server and co-op clients refuse level-changer touches and `M_CHANGE_LEVEL` and log them ([S06-T05](validation/S06-T05.md), enc-12) |

## Persistence

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L15 | Checkpoint has no crash or journal guarantee; ownership, captured contexts, participant references and actor 2's script state are not persisted (R07, R08) | G02, G03 | Stage 7 (S07-T01–T06) | Closed 2026-10-23 for the parts Stage 7 owns: [S07-T02](validation/S07-T02.md) (generations published atomically, store tests st-2 incl. the 8-point termination matrix), [S07-T03](validation/S07-T03.md) (write-through journal, fail-closed), [S07-T04](validation/S07-T04.md) (recovery: rec-2, rec-3 with 50 records replayed, identities and participant references restored), [S07-T06](validation/S07-T06.md) (engine termination matrix mx1). What stays open has its own rows: actor 2's script state (L32), ownership (S08, K4), captured contexts (L33) |
| L32 | Actor 2 has no Lua binder and no `pstor`: its script state (tasks, info portions, story flags) is not persisted or run; actor 1's is, through the native `.scoc` | G07 (S07-T01 §3 D) | S10-T05 / the presentation-script split (no task yet: R12) | Open 2026-10-27: narrowed: thirst, sleep and psy health are kept per player, saved and shown per player; the booster memory is per player; the pre-hit handlers run for the actor hit ([S08-T07](validation/S08-T07.md), user's revision of T07-8); tasks, info portions of actor 2 and its `pstor` remain (L44). |
| L33 | Captured script contexts and presentation outboxes are not persisted; a restart drops what was queued (presentation only) | G07 | S10-T05 | Open |
| L34 | A checkpoint take serializes the world on the simulation thread (45–65 ms with 26k objects, up to 129 ms seen); compression, the file write and every store step run on a worker since S07-T08. A client sees a pause of about 100 ms (its interpolation buffer) at a take, 168 ms in the slow case, down from 183–367 ms | G07 (S07-T02, revised by S07-T08 2026-10-23) | A new task (incremental or copy-on-write serialization) only if play tests still show a hitch | Open |
| L35 | Durability is tested against process termination only: power loss and devices that misreport flush completion are not tested and not claimed (contract §5, §12); retention keeps one previous generation | G07 (S07-T02/T03) | S11 (R08: record as a product limit; no engineering task) | Open |
| L36 | Inventory, quest and reward effects have no journal records or adapters: a restart rolls them back to the last generation; only the synthetic ledger, deaths and respawns are durable | G07 | S08-T01–T06 (records and adapters with their crash tests) | Open. 2026-10-24: pickups are durable `pickup` records with an idempotent absolute-post-state adapter, crash-tested at every failpoint ([S08-T01](validation/S08-T01.md), pick-4…7, pick-14); drops and transfers too ([S08-T02](validation/S08-T02.md), tr-14…17); consumption (a used item comes back with its record after a restart), equip state, trade, quest and reward effects remain 2026-10-25 (G08): the party quest's accept and completion commit with a published generation (contract §8a; [S08-T06](validation/S08-T06.md)); consumption (L39), equip state (L37), trade and the quests not adapted remain. 2026-10-26: equip state and consumption are durable (`equip`, `use` records, [S08-T07](validation/S08-T07.md)); server-made changes to the players' inventories too ([S08-T08](validation/S08-T08.md)); trade and the quests not adapted remain. |

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

## Inventory and quests (G08)

Added 2026-10-25 when the user accepted G08 "as long as the limits will be
solved": each row's owner is the task that solves it.

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L37 | Equip (slot, belt, ruck moves) and use/eat do nothing on a client: those engine events stay refused on a replica; activate-slot is the only equip action | G08 ([S08-T02](validation/S08-T02.md)) | S08-T07 | Closed 2026-10-26: equip moves and use/eat are validated durable server actions (kinds 9, 10; journal `equip`, `use`) from every client path, with the clients' layouts matching the server's ([S08-T07](validation/S08-T07.md), eq-5, eq-6, eq-7…eq-13) |
| L38 | A container created after the last checkpoint (a script's stash, a body of an NPC that died since) is not durable: after a restart its committed contents lie on the ground where it was (`transfer-replay-owner-gone`) | G08 ([S08-T02](validation/S08-T02.md)) | S08-T08 | Closed 2026-10-26: a container's definition is journaled before the first committed transfer touching it after each publication and re-created at recovery (a body dead); server-made moves into containers (the backpack stash) are journaled too ([S08-T08](validation/S08-T08.md), ct-6, ct-10…ct-12) |
| L39 | Consumption after a pickup is not durable: a used item comes back with its pickup record after a restart | G08 ([S08-T01](validation/S08-T01.md)) | S08-T07 | Closed 2026-10-26: a `use` record releases the item at replay and its earlier pickup/transfer records are skipped (`consumed-later`); eq-8 killed the server right after the eat of a picked-up item became durable and it stayed eaten ([S08-T07](validation/S08-T07.md)) |
| L40 | Ammunition stacks, nested containers (a backpack in a box) and weapon addons move whole; no split or merge | G08 ([S08-T02](validation/S08-T02.md)) | S08-T08 | Closed 2026-10-26: stacks moved into a box merge on the server as the installed scripts merge them, durably (`stack` records) and replicated; no installed path splits a stack (decision T08-5); a backpack is one item and a deployed stash a durable box; addons are flags of their weapon ([S08-T08](validation/S08-T08.md)) |
| L41 | The party quest's presentation: the partner's PDA does not list the task; the bonus stash's map spot is not replicated; news sent while a player is away is not replayed | G08 ([S08-T04](validation/S08-T04.md)) | S08-T09 | Closed 2026-10-26: both PDAs list the party task with its pooled count and final state (CampaignEvent 11; clients evaluate no task logic); the scripts' map spots reach both players, also on a later join; missed news are replayed on a rejoin ([S08-T09](validation/S08-T09.md), pq-2, pq-3, pq-4) |
| L42 | A party task whose anchor (participant 1's actor) dies and respawns is undefined and untested | G08 ([S08-T05](validation/S08-T05.md)) | S08-T09 | Closed 2026-10-26: the task stays active; its turn-in waits for participant 1's respawn and then runs in the new body; its script data stays in the original body's saved store; tested live and with a restart between the respawn and the turn-in (which also fixed a restart that could not start after participant 1's respawn) ([S08-T09](validation/S08-T09.md), pq-2, pq-6) |
| L43 | The talk window does not reach the server: players cannot yet accept or turn in the party quest by talking to the giver (the fixture sends the intents) | G08 ([S08-T04](validation/S08-T04.md)) | S10-T04 (first child) | Open |

## Added at the G08 condition review (accepted 2026-10-27)

Found while closing L37–L42 (S08-T07…T09). Each has an owner task; the user
asked for them to be listed with the gate and accepted them with it on
2026-10-27 ("they all work You can close the gate").

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L44 | After participant 1 has respawned, a party task accepted **later** keeps its script data on a body without the installed Lua binder: a restart would lose it (the task in progress at the death is safe) | G08 review ([S08-T09](validation/S08-T09.md)) | S10-T05 (with L32) | Open |
| L45 | A use's next-frame script work (the deployable backpack's transfers into its stash) is journaled one host tick after the use: a termination in that tick leaves the ruck in the actor and no stash (consistent, nothing lost) | G08 review ([S08-T08](validation/S08-T08.md), ct-7/ct-11) | S11-T02 | Open |
| L46 | Rolled back with the fight by design (decisions T08-3, T08-7): ammunition boxes, grenades and bolts used up between checkpoints, and the loot of a body no player touched | G08 review ([S08-T08](validation/S08-T08.md)) | S11-T02 (review at release qualification) | Open |
| L47 | A use's first-person animation plays only on its user's client; the partner sees no use animation (the user's weapon cannot fire meanwhile, decision T07-5) | G08 review ([S08-T07](validation/S08-T07.md)) | S10-T05 | Open |
| L48 | Equipping an artefact to the belt was exercised only as the refusal (the fixture outfit has no artefact slots); the accepted path is the same engine call and adapter as slot and ruck | G08 review ([S08-T07](validation/S08-T07.md)) | S10-T01 | Open |
| L49 | Sleeping and passing out in co-op: the installed sleep (`ui_sleep_dialog`, a game-time skip) and the forced sleep at extreme thirst or sleep deprivation (with its 5 % death chance) would skip time for the whole server; untested in co-op, per-player values now drive it | G08 review (per-player survival values) | S10-T03 (world time) | Open |

## Baseline waivers (G01)

| ID | Limit | From | Owner | Status |
|---|---|---|---|---|
| L26 | Completed-quest/reward verification on the unmodified client waived for Stage 2 entry (the quest attempt failed) | G01 | S08-T06, S11-T04 | Open 2026-10-25 (G08): a quest completion with rewards verified on the co-op server and both clients ([S08-T04](validation/S08-T04.md), q-2, qn-2); the unmodified-client comparison remains (S11-T04). |
| L27 | Original-client comparison (copied original installation) not run | G01 | S11-T04 | Open |
| L28 | Actor-relative presentation (blowout waves, ambient loops, siren position, cover-dependent psychic factor) is computed for participant 1's actor (`db.actor` outside a context) and shown to both players | S06-T08 (after G06) | S10-T05; the presentation split before S08 | Open |
| L29 | The emission's gameplay (cover check, kills, death camera) runs single-player-style on actor 0 and its death camera is broadcast | S06-T08 (after G06) | S10-T05 | Open |
