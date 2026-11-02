# S.T.A.L.K.E.R. Anomaly Co-op — Engineering Roadmap

Planning baseline: 2026-10-15. **The original planning execution authorized no implementation; the user authorized roadmap execution on 2026-10-15 by requesting that work begin.** This document is the authoritative development order; the older [implementation roadmap](docs/implementation-roadmap.md) remains historical evidence. The user's approved D1–D8 below supersede pending-decision wording in earlier reports. Preserve those reports rather than treating their older proposals as new decisions.

## Roadmap at a glance

| Stage | Outcome | Completion gate | Status |
|---|---|---|---|
| 0 | Safe workspace, preserved evidence, reproducible source ownership | G00 | Completed |
| 1 | Compatible unmodified Windows client, isolated content validation | G01 | Completed |
| 2 | Genuine headless native campaign simulation | G02 | Completed |
| 3 | Two explicit participants and server authority | G03 | Completed |
| 4 | Validated transport, protocol and sessions | G04 | Completed |
| 5 | Two clients observe the same actors and world | G05 | Completed |
| 6 | Server movement, reconciliation and combat | G06 | Completed |
| 7 | Recoverable checkpoints, durable critical outcomes and resynchronization | G07 | Completed |
| 8 | Durable inventory and one complete shared quest | G08 | Completed |
| 9 | Confirmed party travel with failure recovery | G09 | Completed |
| 10 | Broader campaign and simulation coverage | G10 | In Progress (S10-T01, S10-T02 (a…f), S10-T03 (a…f), S10-T04a…f, S10-T05a…d, S10-T06 complete, S10-T07 part 1 complete, part 2 checked hands-on 2026-11-01, part 3 checked hands-on (demo-3), part 4 checked hands-on (demo-4: B1 fixed), part 5 checked hands-on (demo-5: B4 and B23 fixed), part 6 (crash B24, patch 0092) in code, not run yet; G10 reviewed by the user 2026-11-02: nine revisions, all in code (patch 0093, campaign-429 / client-311; bugfix-17/18 pass); hands-on demo-6: a client crash on a world item's pickup (B26, fixed in code); the host and join menu added by the user 2026-11-02 and moved to Stage 11 (S11-T00); next: the user's hands-on session, then the gate on the user's word) |
| 11 | Reliability, performance, regression and release qualification | G11 | In Progress (S11-T00 local hosting and the host and join menu, started 2026-11-02 by the user's direction while G10 waits on the user's verification) |

Engineering tasks were initially unchecked; current statuses and validation links below record execution. Prior source investigation and Ghidra persistence verification are reusable evidence, **not completed implementation gates**. Stage 0 is complete. S01-T01 toolchain readiness and S01-T02 isolated content preparation have passed; two unmodified DX11 builds passed (0 errors each). Artifact comparison and scoped gameplay acceptance passed on 2026-10-17. The user explicitly waived completed-quest/reward and original-client comparison for G01; those remain untested risks. Exact commands and evidence are recorded in the current task reports. S02-T01 source mapping is complete; [headless service boundary](docs/headless-service-boundary.md) and [fixture specification](docs/headless-fixture.json) define the implementation work. S02-T02 is complete: both products rebuild; Core and Services (spatial DBs, script engine, character registries) startup/shutdown smokes pass with partial-start unwind checked; the [internal context](docs/campaign-server-context.md) is defined. By user decision on 2026-10-18, real game-state/ALife construction (H02) moved to S02-T05/T06; its renderer/presentation blockers are recorded. By 2026-10-18 the campaign server also loads real models, materials and particle definitions on the CPU, drives its own clock, constructs the persistent object and, from the fixture save, the level object, server and ALife (level `k00_marsh`, actor present) with no device, then loads the level geometry, physics world and client game state, spawns every server entity client-side and runs the first server ticks with no HUD, render level or view entity (S02-T05d, 2026-10-18); its ticks step the native physics world the same way the client does (a dropped crate falls, lands and sleeps identically, S02-T04b, 2026-10-18). Its ticks also drive the object list, the scheduler, the stalkers' planners, ALife switching and offline scheduling and the ALife clock from activity anchors instead of a camera, and a stalker walks a commanded path exactly as on the client (S02-T04c, 2026-10-18). Its CPU sound core gives every sound feedback with no audio device, and AI sound events (the walked stalker's footsteps from the material pairs, NPC voices, monster idles) reach the same hearers and `feel_sound_new` as on the client (S02-T05c, 2026-10-18). The actor's Lua binder runs on the server against an inert, CPU-only game UI the user chose for the installed scripts' HUD calls (S02-T06a, 2026-10-18); the server stays real-time. A stalker and a spawned boar fight to the death headless exactly as on the client: sight, enemy selection, rifle fire, bullet flight and impacts, hits through the host's event loop, health and death (S02-T06c, 2026-10-18). The server writes a native checkpoint through the host's own save path, restarts the level from it and gets the same ALife, clock, actor and NPC state back, and the normal client loads the same checkpoint and matches (S02-T06d, 2026-10-18). A quest is given to the Clear Sky mechanic through the installed dialogue functions, tracked by both halves of the task system, carried through a checkpoint, completed through the installed turn-in with its rewards and carried through a second checkpoint, on the server and from the server's checkpoints on the client (S02-T06e, 2026-10-18). S02-T06 is Completed. The user accepted G02 for development on 2026-10-18 with its recorded limits (one quest, one level, short runs, B21 and others) carried into Stage 3 as named risks, each with a Stage 3 handling rule and a later owner; see [G02](docs/validation/G02.md). S03-T01 is Completed (2026-10-18): the [participant identity model](docs/participant-identity.md) and registry are in patch 0021 with passing unit fixtures and a passing level identity check. S03-T02 is Completed (2026-10-18): local view access is client-only, gameplay names its participant actor explicitly, the server's message policy refuses remote state imports, world control and game events (patch 0022, [inventory](docs/actor-access-inventory.md)). S03-T03 is Completed (2026-10-19): a second genuine actor spawned beside the saved one is participant 2's by explicit assignment, both actors are hit, equipped, driven by their own participant's input, killed and released independently on the campaign server, the slot is reused under a new entity id, and the installed single-player script gates found on the way (`se_actor` state read, the pre-hit callback, `db.actor`) are recorded for S03-T05 (patch 0023, [S03-T03](docs/validation/S03-T03.md)). S03-T04 is Completed (2026-10-19): the ALife switch rules use the nearest alive participant actor, so each player keeps its own area online and a detached player's retained actor keeps its area; a monster targets either player; the player-bound abilities are explicit no-ops or participant-aware (patch 0024, [S03-T04](docs/validation/S03-T04.md)). Found on the way: AI vision on the headless server has no light (luminosity 0), an open item for a server light model. S03-T05 is Completed (2026-10-19): native and Lua code name their participant through an explicit script context entered only by native entry points; the installed quest line ran for each participant concurrently with its rewards on its own actor (patch 0025, [S03-T05](docs/validation/S03-T05.md)). S03-T06 (added by user decision) is Completed (2026-10-19): the server computes each participant's lighting as its client would, so AI vision is no longer in darkness, and matches the client at steady state (patch 0026, [S03-T06](docs/validation/S03-T06.md)). Every Stage 3 task is Completed. The user accepted G03 for development on 2026-10-19 with its recorded limits carried forward to their named owners; see [G03](docs/validation/G03.md). S04-T01 is Completed (2026-10-20): GameNetworkingSockets v1.6.1 is pinned, builds with the accepted toolchain and passed the isolated transport tests on one Windows host; it is accepted for the S04-T02 adapter with its liveness, send-buffer, lane-ordering and power-throttling findings ([S04-T01](docs/validation/S04-T01.md)). S04-T02 is Completed (2026-10-20): a project-owned transport adapter (`protocol/`, `coop::net::Transport`) keeps GameNetworkingSockets in one source file, delivers network input only through `poll()` on its owner thread, and has explicit per-channel overload rules (critical: close, bulk: backpressure, transient: drop); 51 tests pass on a fake backend and on GNS over loopback, including shutdown and threading races ([S04-T02](docs/validation/S04-T02.md)). S04-T03 is Completed (2026-10-20): protocol v1.0 ([wire format](docs/network-protocol.md#wire-format-v1)) has 15 message types on 6 channels, a bounded stateless decoder that refuses anything outside its schema, a canonical encoder, and per-connection epoch, sequence, entity-generation and resync checks; 1.9 million fuzz mutations found no over-read and no non-canonical acceptance ([S04-T03](docs/validation/S04-T03.md)). S04-T04 is Completed (2026-10-20): [sessions](docs/session-authentication.md) bind connections to roster players by ECDSA P-256 challenge-response (Windows BCrypt; DPAPI key files; public keys only on the server), two slots kept across reconnects, a reconnect replacing the stale session, build/content/version checks, rate limits and handshake timeouts; impersonation, replay, third player, mismatch and flood tests pass on the fake and GNS; an active man in the middle on the network path remains a stated limit ([S04-T04](docs/validation/S04-T04.md)). S04-T05 is Completed (2026-10-20): reliable heartbeats with suspect/timeout detection both ways, same-process resume with single-use rotated tokens, stale connections unable to submit, detached players keeping their slots, and server shutdown, measured on the fake and GNS loopback ([S04-T05](docs/validation/S04-T05.md)). Every Stage 4 task is Completed. The user accepted [G04](docs/validation/G04.md) for development on 2026-10-20 with its recorded limits (loopback only, the handshake's man-in-the-middle limit as a G11/R06 item, provisional liveness defaults) carried to their named owners. S05-T01 is Completed (2026-10-20): the protocol links into both products, a serving campaign server and two real clients connect with matched build and content, clients refuse local game starts and every injected outcome is refused ([S05-T01](docs/validation/S05-T01.md), patch 0027). S05-T02 is Completed (2026-10-20): a joining client gets the server's world as a native checkpoint plus the lifecycle stream after its watermark, holds it as a passive replica (no simulation, every local outcome refused), and both clients' replicas, including a late join and a rejoin under constant spawn/release churn, equal the server's online set ([S05-T02](docs/validation/S05-T02.md), patch 0028). S05-T03 is Completed (2026-10-20): each client views and steers its own participant's actor through the server and sees the other player's actor where the server has it, with the server's time and weather ([S05-T03](docs/validation/S05-T03.md), patch 0029). S05-T04 is Completed (2026-10-20): one bounded session showed two joins, a late join and a rejoin converging on the server's world under server-owned churn, both actors on both clients and own-only movement; a rejoin input bug was fixed ([S05-T04](docs/validation/S05-T04.md), patch 0030). Every Stage 5 task is Completed. The user accepted [G05](docs/validation/G05.md) for development on 2026-10-20; its limits and those carried from G01–G04 are tracked in [carried limits](docs/carried-limits.md) (L01–L27) with owner tasks. S06-T01 is Completed (2026-10-20): the server takes each player's movement as intent through a per-player input queue (sequenced by tick, validated, one input per server tick) and the native control path decides speed, stance and collision; forged streams, a teleport intent and a forged position move nothing; a defect that pinned two nearby actors was fixed ([S06-T01](docs/validation/S06-T01.md), patch 0031). S06-T02 is Completed (2026-10-21): protocol v1.2 sends each player's snapshot set (participants, creatures, moving physics objects) as deltas against the snapshot it acknowledged, with bounded full resyncs and a congestion-adaptive part budget; clients interpolate NPCs, objects and actors from a drift-following server clock; NPCs on clients are now truly remote (they had been running AI there since S05-T02) and animate; presentation p95 0.11–0.14 m nominal and at 5 % loss and 40 ms lag, 11–14 KB/s per client ([S06-T02](docs/validation/S06-T02.md), patch 0032). S06-T03 is Completed (2026-10-21): each client predicts its own actor through the same native control path and reconciles it against the server's last processed input, with visual-only smoothing; deliberately wrong predictions converge; the own actor responds in 83–100 ms instead of 250 ms; the other player's aim is replicated and a HUD arm-snapping bug from two actors sharing camera statics is fixed ([S06-T03](docs/validation/S06-T03.md), patch 0033). S06-T04 is Completed (2026-10-21): fire is a key state the server presses on the player's own actor, so the native weapon decides cadence, ammo and ballistics; activate-slot and reload are validated actions; health, slot, rounds and a shot counter replicate to both clients (protocol v1.4); forged hits are refused; hits between the two players follow one co-op rule (the installed pre-hit callback had let only the first player hurt the second) ([S06-T04](docs/validation/S06-T04.md), patch 0034). S06-T05 is Completed (2026-10-21): a boar fights both players and Fanat on the server; damage, NPC deaths and participant deaths (now native for both participants) converge on both clients within 120 ms, nominal and at 5 % / 40 ms and 10 % / 100 ms; the dead take no input and a forged revive is refused; the server and clients refuse level changes after a level changer crashed the headless server ([S06-T05](docs/validation/S06-T05.md), patch 0035). The user accepted [G06](docs/validation/G06.md) on 2026-10-21 and added S06-T06 (Cordon fixture without Fanat), S06-T07 (jump and lean), S06-T08 (storm and emission visuals) and S07-T07 (respawn with a lootable corpse); Stage 6 stays In Progress until those three Stage 6 additions are done. S06-T06 is Completed (2026-10-22): the co-op programs run in Cordon from a user-made save with no companion and pass there, nominal and impaired; a server crash on blood marks, a join race under loss and a stale replica object were fixed ([S06-T06](docs/validation/S06-T06.md), patch 0036). S06-T07 is Completed (2026-10-22): jump and lean reach the server as input and are predicted on the client (protocol v1.5); on the way, windowed clients release the mouse so their windows can be moved, a player's weapon switch and reload reach the server and the other player's drawn weapon shows, and the other player's items are presentation only ([S06-T07](docs/validation/S06-T07.md), patches 0037–0040). S06-T08 is Completed (2026-10-22): both clients show the server's storm (same weather descriptors and blend, the server's lightning) and emission (the weather effect and its phases, the scripts' camera and post-process effectors, sounds and particles) within a bounded delay, nominal and at 5 % loss / 40 ms; the emission's gameplay stays S10-T05 ([S06-T08](docs/validation/S06-T08.md), patch 0041). Every Stage 6 task is Completed. Stage 7 (2026-10-22/23): the persistence contract (S07-T01), the project-owned store library and checkpoint generations (S07-T02), the durable journal with the synthetic ledger (S07-T03), recovery with identity restore and replay (S07-T04), the client's epoch-fenced reconnect (S07-T05), the termination matrix and the death record (S07-T06) and the respawn with a lootable corpse (S07-T07) are Completed under patch 0042, and the user's G07 decisions (2026-10-23: no stall at a checkpoint → S07-T08 moves publication to a worker thread; respawn at the level entry point; the faction kit) are implemented and re-validated; the user accepted [G07](docs/validation/G07.md) on 2026-10-23 (the remaining checkpoint pause is play-tested; a host-triggered save is kept as the optional S07-T09). S08-T01 is Completed (2026-10-24): item pickup is a validated, durable server action (one winner in concurrent claims, survives every crash window), claimed from the real use key ([S08-T01](docs/validation/S08-T01.md), patch 0045). S08-T02 is Completed (2026-10-24): drops and box/corpse transfers are validated durable actions with an ownership-invariant checker, and EntityIds are no longer reissued after a restart ([S08-T02](docs/validation/S08-T02.md), patch 0046). S08-T03 is Completed (2026-10-24): both clients' item graphs match the server's after kills and restarts ([S08-T03](docs/validation/S08-T03.md), patch 0047). S08-T04–T06 are Completed (2026-10-25): one installed quest is shared by both players with the user's reward policy and commits once through every restart window ([S08-T04](docs/validation/S08-T04.md), [S08-T05](docs/validation/S08-T05.md), [S08-T06](docs/validation/S08-T06.md), patch 0048). The user accepted [G08](docs/validation/G08.md) on 2026-10-25 on the condition that its limits are solved: they are L37–L43, owned by the added S08-T07 (equip, use, consumption), S08-T08 (new containers, stacks), S08-T09 (quest presentation, anchor death) and S10-T04 (the talk window); Stage 8 stays In Progress until S08-T07…T09 are Completed. S08-T07 is Completed (2026-10-26): a client's equip moves and use/eat are validated durable server actions (the installed scripts' checks run on the server in the user's context, the booster memory per player), both clients' inventory layouts match the server's, and a used item stays used through every restart window, including after its pickup (L37, L39 closed; [S08-T07](docs/validation/S08-T07.md), patch 0049); the gameplay decisions taken are listed for approval in [Stage 8 closing decisions](docs/stage8-closing-decisions.md). S08-T08 is Completed (2026-10-26): a container a committed transfer touches is recorded and re-created after a restart (a stash, a body), the server's own changes to the players' inventories are journaled (the backpack stash), ammunition merges on the server durably and replicated (L38, L40 closed; [S08-T08](docs/validation/S08-T08.md), patch 0050). S08-T09 is Completed (2026-10-26): both players' PDAs list the party task with its count and state (clients evaluate no task logic), the scripts' map spots reach both players, missed news are replayed on a rejoin, and the party task survives participant 1's death and respawn, also across a restart (a restart after participant 1's respawn could not start before; fixed) (L41, L42 closed; [S08-T09](docs/validation/S08-T09.md), patch 0051). Every limit the G08 condition assigned to Stage 8 is closed; the [condition review](docs/validation/G08.md#condition-review--ready-for-the-users-confirmation-2026-10-26) and the [gameplay decisions](docs/stage8-closing-decisions.md) wait for the user's confirmation; Stage 8 stays In Progress until then. At the review (2026-10-27) the user approved T07-1…4, 6, 7, 9 and revised T07-5 (no firing while the use animation plays) and T07-8 (thirst, sleep, psy health per player): both implemented (patch 0052, campaign-291 / client-190; [S08-T07](docs/validation/S08-T07.md) "Revisions"); the new limits are L44–L49. **The user confirmed the G08 condition on 2026-10-27** ("they all work You can close the gate"): all gameplay decisions approved (T07-5, T07-8 as revised; patches 0052–0053, campaign-292 / client-191), L44–L49 added; **Stage 8 is Completed**. S09-T01 is Completed (2026-10-27): the [travel contract](docs/travel-contract.md) defines transition ids (`(epoch << 32) | serial`), the consent state machine (`coop::proto::TransitionCoordinator`, 7 unit tests), the request at a level changer (kind 11) with its checks (the changer, the actor's place, the partner present and alive, no NPC hunting the party), the partner's answer (kind 12; refused unless it names the live proposal and comes from the right party), 30-s expiry, cancellation, and the aborts (a party member's disconnect, death, combat, the changer's loss; no one-player consent); on clients a changer touch is a prompt and the keys answer; an agreed transition ends as the S09-T02 hand-over, nothing loads. Validated end to end over nine fixture rounds, a server crash with a proposal live and a hands-on trial ([S09-T01](docs/validation/S09-T01.md), patch 0054, campaign-298 / client-195); the gameplay decisions are listed for G09 in [Stage 9 decisions](docs/stage9-decisions.md). S09-T02 is Completed (2026-10-28): an agreed travel is carried out through the durable store as the native `change_level` carries out one player's — a barrier (no input, every action refused), the `transition` record prepared, both actors' ALife entities moved to the changer's destination, the destination generation published (its pointer swap is the commit point), the record committed, the level reloaded in process from that generation (the identity registry re-bound from its record) while both clients leave and rejoin on the new level; the party travelled Cordon → Darkscape with its inventories (tv-11), and a crash before the commit recovers the source level, after it the destination (tv-12, tv-13) ([S09-T02](docs/validation/S09-T02.md), patch 0055, proto-build-46, campaign-301 / client-198; decisions T02-1…T02-7). S09-T03 is Completed (2026-10-28): after the reload the destination world is held (input consumed and discarded, actions refused) until every client's validated ready names the held transition and level, or 60 s pass; wrong readies refused, a delayed partner waited for, a silent one timed out, no input applied during any hold ([S09-T03](docs/validation/S09-T03.md), patch 0056, proto-build-47, campaign-302 / client-199; decisions T03-1, T03-2). S09-T04 is Completed (2026-10-28): the travel failure matrix ([travel contract](docs/travel-contract.md) §11) is defined and every window exercised: a party member's disconnect during the barrier rolls back naming who left and the party travels on its retry (tv-17); a failed publication before the commit point rolls back with a phase 3 record, both told, the retry travels (tv-18); a server crash during the reload or the hold recovers the destination and a recovered server holds nothing (tv-19, tv-20); a client back after the hold's timeout is told the party went on (tv-21); a client that crashed at the destination load restarts, is told the hold (CampaignEvent 13 state 10) and the party resumes together (tv-26; two defects in the new notice found by tv-22/tv-25 and fixed); after a crash before the take the party asked again and travelled, to Garbage this time (tv-23) ([S09-T04](docs/validation/S09-T04.md), patch 0057, proto-build-48, campaign-305 / client-202; decisions T04-1…T04-6). Every Stage 9 task is Completed. **The user accepted [G09](docs/validation/G09.md) on 2026-10-28** ("ok that sounds good I accept the gate") after reviewing the hold's timeout and the late rejoin: decisions T01-1…T04-6 approved as listed, L14 closed, the new limits L50–L56 added with owners; **Stage 9 is Completed**. S10-T01 is Completed (2026-10-28): the [campaign coverage matrix](docs/campaign-coverage-matrix.md) inventories the installed campaign (33 levels, 122 level changers forming 98 directed passages, 413 scripts, 610 task sections in 29 files, 17 mutant classes, 36 trade files), classifies 24 content families with an owner/context policy, a fixture and the evidence each needs, labels untested and excluded content, and creates 21 fixture-sized children S10-T02a…S10-T05d with stable IDs and owned limits ([S10-T01](docs/validation/S10-T01.md)); its 16 scope decisions ([Stage 10 decisions](docs/stage10-decisions.md) T01-1…T01-16: every passage in scope, a new game, one party faction, party story info, nearest-participant NPC logic, per-player goodwill and money, consent to sleep, guides as party travel, companions per recruiter, the exclusions) plus T01-17…T01-19 (a dedicated server controlled from the clients: Create/Load/Join, the creator owns the session, the joiner inherits faction and type; child S10-T06) were **approved by the user on 2026-10-28**. Next: S10-T04a.

## A. Project objective

Build exactly two active players initially into one persistent Anomaly 1.5.3 campaign, using matched Windows clients and a dedicated authoritative Monolith process. The server owns world simulation, canonical players, quests, combat, inventory and recovery. Clients provide input and presentation, with bounded prediction and reconciliation. Both players occupy one online level, with offline ALife retained and coordinated travel. Preserve single-player behavior where practical; never use a player's process as authority.

All work, test installations, generated logs, temporary files and databases stay under `/mnt/c/Users/djb20/StalkerAnomCoop` (the current lowercase WSL workspace resolves to the same directory). `/mnt/c/Users/djb20/StalkerAnom` is strictly read-only. Never launch from, patch, or route configuration/saves/output into the original installation. Verify Windows paths explicitly when invoking Windows tools from WSL.

## B. Approved architecture and evidence baseline

### Accepted decisions

| Decision | Binding direction | Engineering validation still required |
|---|---|---|
| D1 — Authoritative Monolith runtime | Keep backend-owned `CLevel`, necessary `game_cl_Single`, `xrServer`, lifecycle, scheduler, physics, AI and ALife. Separate gameplay from presentation. Do not move all simulation into `xrServer` or create a permanent artificial global player. | Startup, execution order and actor-independent services must actually run. A private internal state bridge must never confer authority on a remote client. |
| D2 — Genuine headless operation | Remove GPU/UI/presentation dependencies while retaining CPU models, skeletons, animation transforms, collision, hit detection, physics, AI, quests and inventory. A hidden window is diagnostic only. | Prove no GPU device or draw submission is needed; blanket no-op render/model interfaces are not acceptable if they remove gameplay. |
| D3 — Validated matched builds | Pin an Anomaly 1.5.3-compatible revision and Windows MSVC/SDK configuration; build the unmodified client first and run against a separate test installation. Validate assets, installed scripts, quests, save/load and core play before major multiplayer changes. Build both products from the accepted baseline thereafter. **User-approved 2026-10-17 G01 exception:** completed-quest/reward and copied original-client comparison are waived for Stage 2 entry; record them as untested and revisit on related failures or release qualification. | [G01](docs/validation/G01.md) accepts a bounded development baseline, not full campaign compatibility. Do not claim untested quest rewards or original-client equivalence. |
| D4 — Explicit participants | Both actors are campaign participants with explicit player/party/entity contexts. Retain useful actor replication and native campaign services, not competitive MP as a campaign replacement. Each rendering client retains its local view actor. | Replace implicit native/Lua global-actor mutations and show independent health, equipment, inputs and contributions. Player 2 cannot be merely a companion visual. |
| D5 — Same-level party travel | One detailed online simulation initially. Server validates requests, notifies both players and initially requires both confirmations. Suspend or order mutations, load both clients, restore and synchronize authoritative state, then resume. | Specify consent expiry, cancellation, timeout, disconnect and failed-load recovery. No automatic unconfirmed teleport. Independent online levels remain outside scope without unnecessarily hard-coding them out of identifiers/protocol. |
| D6 — Transport adapter | Provisionally use GameNetworkingSockets, isolated behind a C++ adapter; preserve useful native serializers. Reliable critical events and discardable high-frequency snapshots have separate policies. Server validates intentions. | Isolated Windows toolchain, connectivity and reliable/unreliable tests precede acceptance. No custom reliable UDP. A demonstrated incompatibility requires evidence and approval before substituting transport. |
| D7 — Recoverable authority | Native checkpoints plus a durable journal of validated effects; stable player/entity/item IDs; checkpoint generations, revisions and duplicate-safe outcomes. Commit critical progression durably before acknowledging permanent success. Clients discard stale state and resynchronize after recovery. | Prove critical quest/inventory/reward/ownership outcomes survive documented crash boundaries. Transient physics/projectiles/precise NPC positions may roll back. Perfect deterministic replay and zero data loss are not assumed. The native simulation, not storage, remains authority. |
| D8 — One backend process | Simulation, networking, campaign and persistence use modular interfaces inside one dedicated Monolith process. | Thread/queue and persistence latency must be measured. Separate networking, persistence or campaign services require a demonstrated limitation and approval. |

### Evidence to reuse, not re-investigate by default

Read [architecture](docs/architecture.md), [engine investigation](docs/engine-investigation.md), [feasibility](docs/multiplayer-feasibility.md), [state ownership](docs/state-ownership.md), [protocol](docs/network-protocol.md), [binary evidence](docs/reverse-engineering-evidence.md) and `AGENTS.md`. Evidence references E01–E12 below abbreviate specific existing findings; source paths are relative to `engine/xray-monolith/src/` at **`c57fe5b9fab4fe0506659db03137c52d2500eb2f`**. Remap locations if the accepted source changes; do not assume line numbers persist.

| Ref | Confirmed source behavior / location | Existing binary corroboration, when available |
|---|---|---|
| E01 | `Actor_Network.cpp` under `xrGame`, 58–67 and 511–550: global `Actor()` and spawn assignment; `Actor.h:74–79,727–733`, `InventoryOwner.cpp:37,90–92`: per-instance state. | GH-A01, RVA `0xfb2e0`, global return and SP assertion; independent participant behavior untested. |
| E02 | `xrGame/alife_graph_registry.cpp:49–96`, `alife_dynamic_object.cpp:160–183`: one selected actor/level and actor-distance activation. | GH-A02 `0x386670`, GH-A05 `0x386100`: selected actor and one level registry; distance behavior source-only. |
| E03 | `xrGame/Level_start.cpp:217–230`, `Level_network_start_client.cpp:134–164`; `xrServer.cpp:213–263`; `xrEngine/IGame_Level.cpp:191–200`: simulation spans internal client/world and server. | No binary mapping claimed. `xrServer` alone is not the online simulation loop. |
| E04 | `xrGame/xrServer_process_update.cpp:7–95`, `Level_network.cpp:294–312`, `xrServer.cpp:797–812,890–917,1040–1048`: privileged local-state import/direct dispatch. | GH-A03 `0x71ef80`, GH-A04 `0x71edd0`: local assertions and state import/save paths. Not evidence of a remote exploit. |
| E05 | `xrEngine/no_single.h:1–7`, `xrGame/game_sv_single.cpp:26–33`, `Level.cpp:1034–1053,1142–1169`: dedicated flags suppress required ALife/tasks/scripts. | Source-only. Existing dedicated switches are not a working campaign-server configuration. |
| E06 | `xrEngine/device.cpp:401–580,694–754`, `EngineAPI.cpp:215–240,311–323`, `x_ray.cpp:1559–1576`; `xrGame/Level_load.cpp:50–96`, `GamePersistent.cpp:703–735`: loop/load/UI/render coupling. | Source-only. Headless startup has not been run. |
| E07 | `xrEngine/xr_object.cpp:70–85,255–280`, `xrGame/CharacterPhysicsSupport.cpp:184–205`: visual/model data, skeleton collision and CPU bone/physics requirements. | GH-A06 `0x72650`: model/cform spawn correspondence; ABI and full layouts unverified. |
| E08 | `xrPhysics/PHWorld.cpp:185–200,280–295,506–556`; `xrGame/alife_update_manager.cpp:58–78,92–140`, `GamePersistent.cpp:817–824`; stalker and base-monster scheduled updates. | Source-only: physics, scheduler and AI need deliberate tick ownership; no measured runtime yet. |
| E09 | `xrEngine/xr_object.cpp:379–406`, `xr_object_list.cpp:198–266`; `xrGame/Level_Bullet_Manager.cpp:1225–1236`: camera-dependent scheduling; `CommitRenderSet` launches ballistics work. | Source-only. Render-named functions may perform essential simulation. |
| E10 | `xrNetServer/NET_Server.cpp:245–443,614–658`; `xrServerEntities/object_factory_register.cpp:193–240`; `xrGame/Actor_Network.cpp:88–158,290–350`, `actor_mp_client_import.cpp:11–80`: legacy transport/factories/replication. | Installed MP class metadata exists; working multiplayer and campaign authority are not demonstrated. |
| E11 | `xrEngine/IGame_Level.cpp:20–26`, `xrPhysics/PHWorld.cpp:40–60`, `xrGame/ai_space.h:38–41`, `alife_update_manager.cpp:159–240`: singular online world/physics/AI level and actor-based travel. | GH-A02/A05 corroborate ALife portion only. Offline ALife does not establish multi-level online play. |
| E12 | `xrGame/alife_storage_manager.cpp:46–123,245–250`, `GameTask.cpp:40–60,137–209`, `GametaskManager.cpp:47–68`, `PhraseDialog.cpp:85–129`, `PhraseScript.cpp:156–166,234–270`; reusable u16 IDs in `xrServerEntities/xrServer_Object_Base.h:71–77`, `xrGame/xrServer.h:145–155`. | GH-A04 corroborates receiving save-state path, not complete durability. Native and Lua mutations need adapters; native IDs are insufficient persistent identities. |

The existing `AnomalyDX11.exe` SHA-256 is `c857b6f49d32c62245611b26106862a773185533856e8b858a7d7e849ed2fb4e`; installed metadata indicates Monolith 1.5.3, September 2024, whereas the candidate source is newer. Correspondence is partial, not exact-source proof. Persistent project: `reverse-engineering/ghidra/Anomaly153-c857b6f49d32.gpr` with its `.rep` directory. [Persistence verification](reverse-engineering/persistence-verification.json) records a successful no-analysis reopen with matching hash, **49,494 functions and 258,577 symbols**. [Architecture function map](reverse-engineering/architecture-function-map.json) records GH-A01–A06. Reuse these results. Do not reimport, delete, overwrite or run whole-binary autoanalysis again. Only a specific unresolved implementation question justifies a targeted saved-project read; new analysis must preserve existing databases and record exact identity/RVA/confidence.

**Unverified beyond scoped G01 acceptance:** full campaign compatibility, headless gameplay, two campaign participants, transport integration, native checkpoint completeness, recovery guarantees and installed campaign Lua behavior. Packed installed scripts have not yet been audited. G01 accepted user-observed gameplay with two explicit waivers; S02-T01 is source mapping only, with no headless or multiplayer runtime pass.

## C. Development roadmap

### Task conventions and common completion contract

Each task below includes its required fields. References to `server/`, `client/`, `protocol/`, `tests/`, test installations and validation reports are **planned artifacts**, not claims they already exist. Existing engine files are identified through the evidence catalog and named subsystems. Follow surrounding C++ conventions and keep gameplay changes modular and reversible. Keep original SP configuration available for regression.

For every task, write a concise `docs/validation/<task-ID>.md` (or link an existing report containing the same evidence), recording source/dependency revisions, configuration, exact commands, fixture identities, expected and observed results, limitations, and relevant source lines. Keep large logs/dumps/builds in ignored workspace directories, for example `builds/logs/<task-ID>/`; never commit proprietary assets or Ghidra databases. Record code commit/patch location and reversion instructions when code changes. A test command must actually run to count. Compilation alone does not satisfy behavioral acceptance. Failure or unavailable Windows/runtime access leaves the task **Blocked**, with a concrete next step.

A stage's first task requires the previous stage gate; other prerequisites below include necessary same-stage tasks. A gate depends on **all tasks in its stage**, not only its final task. Complexity estimates describe scope, not duration. Split a task into stable suffix IDs before starting if its chosen fixture or dependency is too large for a focused session; retain the parent and evidence links.

### Stage 0 — Workspace and project baseline

#### S00-T01 — Verify safety and preserve investigation context

- [x] **Status: Completed** · **Reasoning effort: Medium** · **Complexity: Low**
- **Evidence:** [Validation report](docs/validation/S00-T01.md).
- **Purpose:** Establish a trustworthy starting point without repeating RE.
- **Prerequisites:** None; roadmap execution authorization.
- **Relevant files/subsystems:** AGENTS.md; root and engine Git repositories; evidence ledger; persistence-verification.json.
- **Actions:** Resolve both paths and writable roots; check original existence, workspace separation, nested Git status and planned output routes. Check saved-project files and recorded identities; reuse prior reopen verification unless corruption or missing files is detected.
- **Outputs:** Workspace/evidence manifest and preservation notes. Plus the common task evidence record.
- **Acceptance:** Original lies outside writable roots; all output paths are in workspace; existing changes and persistent evidence are accounted for.
- **Validation:** Read-only path/Git/manifest checks; if persistence is genuinely suspect, bounded native no-analysis reopen with output paths explicit.
- **Risks/blockers:** Case aliases, independent MCP permissions, missing databases; stop on unsafe writable-root configuration.

#### S00-T02 — Establish reproducible source and artifact ownership

- [x] **Status: Completed** · **Reasoning effort: Medium** · **Complexity: Moderate**
- **Evidence:** [Validation report](docs/validation/S00-T02.md).
- **Purpose:** Prevent changes disappearing in the ignored reference checkout.
- **Prerequisites:** S00-T01.
- **Relevant files/subsystems:** Root .gitignore; engine/xray-monolith/.git; build and validation directories.
- **Actions:** Record reference SHA/remotes/dirty state; choose a separately versioned working fork or reproducible patch series in workspace. Preserve reference tree; document how root revision identifies exact engine revision and dependency lock data. Audit ignores for binaries/assets/databases.
- **Outputs:** Source manifest, working-copy/versioning procedure and artifact layout. Plus the common task evidence record.
- **Acceptance:** A future engine change has a clear tracked home and reconstructible baseline without adding game assets to Git.
- **Validation:** Reconstruct manifest references read-only; inspect ignore rules and repository boundaries.
- **Risks/blockers:** Nested ignored repositories can hide work; do not reset or overwrite existing changes.

#### S00-T03 — Define Windows test environment and compatibility fixtures

- [x] **Status: Completed** · **Reasoning effort: High** · **Complexity: Moderate**
- **Evidence:** [Validation report](docs/validation/S00-T03.md).
- **Purpose:** Make Stage 1 tests repeatable and safe.
- **Prerequisites:** S00-T02.
- **Relevant files/subsystems:** src/engine-vs2022.sln; docs/engine-investigation.md; planned builds/test-install and fixtures.
- **Actions:** Inventory available MSVC/SDK/build dependencies without assuming WSL compilers suffice. Define isolated installation layout, config/save/log/temp routing and minimal asset-copy strategy. Select representative start/save/load/quest/combat fixtures and expected outcomes; record access gaps.
- **Outputs:** Tool inventory, test-layout specification and baseline acceptance checklist. Plus the common task evidence record.
- **Acceptance:** Every planned launch writes only in workspace; representative compatibility checks and Windows access requirements are explicit.
- **Validation:** Check WSL-to-Windows path conversion, config routing and available tools; no launch from original.
- **Risks/blockers:** Windows interaction/toolchain may require user environment access; writable hardlinks into original are forbidden.

### Stage 1 — Source compatibility validation

#### S01-T01 — Select and pin the candidate source and toolchain

- [x] **Status: Completed** · **Reasoning effort: High** · **Complexity: Moderate**
- **Evidence:** [Validation report](docs/validation/S01-T01.md).
- **Purpose:** Turn the reference fork into a reproducible candidate.
- **Prerequisites:** G00.
- **Relevant files/subsystems:** Source manifest; src/engine-vs2022.sln; xrEngine project files.
- **Actions:** Compare recorded candidate requirements with Anomaly 1.5.3 content; select one revision, exact MSVC/SDK/configuration and dependency versions. Document drift and bounded fallback candidates; preserve pristine source identity.
- **Outputs:** Pinned candidate/build manifest and exact proposed build command. Plus the common task evidence record.
- **Acceptance:** Candidate and all build inputs are identified; no claim of binary equivalence or compatibility yet.
- **Validation:** Inspect project/dependency settings and resolve compiler/runtime availability.
- **Risks/blockers:** 2026 source versus 2024 installed binary; unresolved dependency downloads or SDK mismatch.

#### S01-T02 — Prepare the isolated Anomaly test installation

- [x] **Status: Completed** · **Reasoning effort: High** · **Complexity: Moderate**
- **Evidence:** [Validation report](docs/validation/S01-T02.md).
- **Purpose:** Protect original files while testing realistic content.
- **Prerequisites:** S01-T01.
- **Relevant files/subsystems:** Planned builds/test-install; content and config manifest.
- **Actions:** Copy only required content into a separate writable installation; record hashes/versions and filesystem mappings. Avoid unnecessary archive copies, writable hardlinks and original-path output. Establish separate test saves and logs; verify paths before launch.
- **Outputs:** Runnable isolated content layout and reproducible setup instructions. Plus the common task evidence record.
- **Acceptance:** Original remains unchanged; saves/config/logs and engine outputs cannot target original.
- **Validation:** Compare selected asset hashes; inspect resolved paths, permissions and launcher/config settings.
- **Risks/blockers:** Disk space, redirected app-data paths, modded content or incomplete content provenance.

#### S01-T03 — Compile the unmodified client reproducibly

- [x] **Status: Completed** · **Reasoning effort: High** · **Complexity: High**
- **Evidence:** [Validation report](docs/validation/S01-T03.md).
- **Purpose:** Prove the selected source can be built before modifying gameplay.
- **Prerequisites:** S01-T01; S01-T02.
- **Relevant files/subsystems:** Pristine working source; Windows solution and dependencies.
- **Actions:** Build unmodified client in declared configuration. Capture toolchain versions, dependency resolution, output hashes and warnings. Repeat from clean build outputs using same pinned inputs; investigate failures in a separate branch.
- **Outputs:** Build recipe, two successful build records and executable identity manifest. Plus the common task evidence record.
- **Acceptance:** Unmodified pinned source compiles using documented reproducible steps; repeat outputs have explained differences if not byte-identical.
- **Validation:** Run exact recorded MSBuild or verified equivalent commands on Windows; verify source diff is empty.
- **Risks/blockers:** Required source fixes invalidate the unmodified-candidate claim; select another compatible revision or request an explicit baseline-gate amendment.

#### S01-T04 — Validate assets, scripts and core gameplay

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High**
- **Purpose:** Prove practical Anomaly 1.5.3 compatibility.
- **Prerequisites:** S01-T03.
- **Relevant files/subsystems:** Isolated installation; candidate client; installed Lua/config archives.
- **Actions:** Run new campaign and representative level, NPC/mutant encounter, inventory/equipment, dialogue, one quest, world-time behavior, save/reload and level travel. Compare expected fixture outcomes and inspect script/asset errors. Extract only scripts needed for failures into ignored workspace artifacts.
- **Outputs:** Compatibility matrix, runtime logs and defect list tied to build/content hashes. Plus the common task evidence record.
- **Acceptance:** Representative observed fixtures pass with no unexplained critical Lua/asset regressions. The user-approved G01 exception waives completed-quest/reward and copied original-client comparison; neither is a passed test. See [G01](docs/validation/G01.md).
- **Validation:** Actual Windows gameplay observations plus save round-trip state comparisons; retain evidence, not merely launch screenshots.
- **Risks/blockers:** A successful main menu does not validate campaign compatibility; unavailable interactive execution blocks acceptance.

#### S01-T05 — Accept and freeze the tested baseline

- [x] **Status: Completed** · **Reasoning effort: High** · **Complexity: Moderate**
- **Purpose:** Prevent multiplayer work on an unvalidated fork.
- **Prerequisites:** S01-T04.
- **Relevant files/subsystems:** Compatibility matrix; source/build/content manifests.
- **Actions:** Resolve or explicitly classify nonblocking defects; freeze exact accepted source and configurations, preserve reproducible SP build, and publish commands and limitations. If mandatory failures persist, return to candidate selection without broad multiplayer changes.
- **Outputs:** G01 acceptance report and baseline revision references. Plus the common task evidence record.
- **Acceptance:** Unmodified clean builds and the user-approved scoped runtime checks pass; source/content/toolchain identities are frozen. Waived fixtures remain explicit risks in [G01](docs/validation/G01.md).
- **Validation:** Review build/runtime evidence against S00 checklist; require a rerun only for affected changed inputs.
- **Risks/blockers:** Representative tests are not full-campaign certification; record bounded coverage.

### Stage 2 — Headless simulation foundation

#### S02-T01 — Map the minimum headless service boundary

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High**
- **Evidence:** [Validation report](docs/validation/S02-T01.md), [service boundary](docs/headless-service-boundary.md), and [fixture identity/assertions](docs/headless-fixture.json).
- **Purpose:** Bound changes before touching the loop.
- **Prerequisites:** G01.
- **Relevant files/subsystems:** E03; E05–E09; Level_start; game_cl_Single; Device; GamePersistent.
- **Actions:** Trace only startup-to-first-tick dependencies in accepted source. Classify CPU gameplay versus GPU/UI/audio presentation and mixed functions. Specify service seams, initialization/shutdown order and minimal real campaign fixture with no external clients.
- **Outputs:** Headless dependency map and fixture assertions. Plus the common task evidence record.
- **Acceptance:** Plan covers scheduler, physics, AI, ALife, scripts, bones, collision and ballistics; no permanent dummy actor or blanket model no-op.
- **Validation:** Targeted source inspection against existing evidence; targeted binary read only for a named unresolved mismatch.
- **Risks/blockers:** Campaign bootstrap may require explicit actor context earlier than Stage 3; move only minimum prerequisite work with documented dependency.

#### S02-T02 — Add a campaign-preserving dedicated configuration

- [x] **Status: Completed** · **Reasoning effort: High** · **Complexity: High**
- **Evidence:** [Context and services validation](docs/validation/S02-T02-context.md) and [internal context definition](docs/campaign-server-context.md); earlier [resumed validation](docs/validation/S02-T02-resume.md). `campaign-07`/`client-04` build with 0 errors; Core, Services and injected-failure unwind smokes behave as expected; GameState stops at `persistent-context-required` (exit 3, not a pass).
- **Scope decision (user, 2026-10-18):** Completed on its acceptance criteria. Real `game_sv_Single`/ALife/`CLevel`/`game_cl_Single` construction (H02) moved to S02-T05/T06; blockers are material-pair media, the persistent render factory, ALife's main-menu reset and `CLevel` player HUD models.
- **Purpose:** Avoid existing dedicated flags disabling campaign services.
- **Prerequisites:** S02-T01.
- **Relevant files/subsystems:** E03; E05; engine project configuration; game_sv_single; Level initialization.
- **Actions:** Add a separate dedicated target/role that retains CLevel, required game_cl_Single and ALife. Define server-owned internal context, explicit configuration roots and lifecycle teardown; preserve SP target.
- **Outputs:** Dedicated build configuration and initialization smoke harness. Plus the common task evidence record.
- **Acceptance:** Dedicated target compiles with required campaign code included; no NO_SINGLE-induced loss of ALife/tasks.
- **Validation:** Build both configurations; inspect effective macros and exercise initialization/shutdown as far as currently supported.
- **Risks/blockers:** This intermediate target may still have graphics dependencies and does not satisfy G02.

#### S02-T03 — Preserve CPU model, animation and collision services

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High**
- **Suffix tasks (split 2026-10-18; all four completed 2026-10-18):**
  - **S02-T03a — Material library without presentation media: Completed.** [Validation](docs/validation/S02-T03a.md). Patch 0004.
  - **S02-T03b — CPU-only model pool and real model load: Completed.** [Validation](docs/validation/S02-T03b.md). Patch 0005. The renderer module is attached with no device, and only the model pool exists. Shader, geometry and GPU buffer creation are skipped and counted. Bone materials are registered without the persistent object. Real actor, NPC and HUD models load with their bones, motions and CPU skin data, and a missing model fails explicitly. Prerequisite: S02-T03a.
  - **S02-T03c — Bone and animation evaluation against a matched client capture: Completed.** [Validation](docs/validation/S02-T03c.md). Patch 0007. The server's bones are bit-identical to the client's over 50 steps. Evaluating bones needs an advancing device clock (`CalculateBones` returns early while `dwTimeGlobal` equals `UCalc_Time`), and camera throttling must be removed. Prerequisites: S02-T03b and S02-T04a (available).
  - **S02-T03d — Skeleton collision, hit picking and physics shell: Completed.** [Validation](docs/validation/S02-T03d.md). Patch 0011 (capture only; the native spawn already builds everything without the renderer). On the loaded level every alive entity has a skeleton collision form and a physics character; 491 skeleton forms, 1,780 elements and 247 shells exist; the actor's picks hit head, chest and ground. The client on the same save builds identical structure for every common object; the actor's trunk hit boxes agree within 2.3 cm. Limb pose differs where the client has a view entity (G03). Prerequisites: S02-T03c and S02-T05d.
- **Purpose:** Keep gameplay fidelity while removing GPU model requirements.
- **Prerequisites:** S02-T02.
- **Relevant files/subsystems:** E07; xr_object; CharacterPhysicsSupport; IKinematics/model loaders.
- **Actions:** Separate CPU asset loading and bone evaluation from GPU resource creation for one actor/NPC fixture. Retain skeleton collision, animation-driven transforms and physics-shell setup; define ownership/lifetime. Split the material library's CPU physics data from its pair sounds and wallmark arrays so the campaign server can load it ([context blocker 1](docs/campaign-server-context.md)).
- **Outputs:** CPU model service boundary and headless asset fixture tests. Plus the common task evidence record.
- **Acceptance:** Fixture produces valid bones/collision/hit transforms without GPU resources; missing data fails explicitly.
- **Validation:** Compare representative transforms, collision queries and shell creation with baseline within documented tolerances.
- **Risks/blockers:** Renderer interfaces may own substantial CPU loaders; asset lifetime or animation callbacks can remain coupled.

#### S02-T04 — Drive time, scheduling and physics explicitly

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High**
- **Suffix tasks (split 2026-10-18; the parent completes when all three do):**
  - **S02-T04a — Server clock and serial frame driver: Completed.** [Validation](docs/validation/S02-T04a.md). Patch 0006.
    - Tick-based Device clocks; the integer, float and frame clocks always agree.
    - Native callback priority order; the scheduler runs in the persistent slot.
    - Parallel jobs and `seqFrameMT` run serially, exactly once per tick.
    - Explicit pause; loading counts as paused time.
    - Catch-up is bounded, and any dropped time is reported.
  - **S02-T04b — Native physics stepping and falling-body collision: Completed.** [Validation](docs/validation/S02-T04b.md). Patch 0012 (fixture only; the stepping is native). With no device the server ticks drive `CPHWorld::FrameStep` at the native 0.01 s step: a 100 kg crate lifted 1 m falls under the engine's 19.62 m/s² gravity with its per-step 1/1.01 velocity scale, contacts the level floor at step 34 and is put to sleep by the world, resting within 1 mm of its saved height. The normal client on the same save matches: identical free fall over all 34 steps, same contact step and materials, final positions 4.5 mm apart. Prerequisites: S02-T04a and S02-T05d. Reordered on 2026-10-18 by user decision.
  - **S02-T04c — Objects, AI, ALife and server activity policy: Completed.** [Validation](docs/validation/S02-T04c.md). Patch 0013. The server's activity anchors (online actors) replace the camera for crow selection and scheduling; on the loaded level the ticks drive the object list, the scheduler, the stalkers' planners, ALife switching, offline scheduled work and the ALife clock, and a stalker commanded through the native script path walks a 10.5 m reachable path to arrival (smoke-57, `campaign-activity-passed`). The normal client from the same source (client-17, capture-06) passes the same fixture: same stalker update rate, planner actions, ALife clock and destination. One asserted difference, `objects_per_update` 65534 against 20, traces to the actor's Lua binder first update not running on the server; owned by S02-T06 with the actor ALife position and actor-item crow findings. Prerequisites: S02-T04a/b and S02-T05's level and ALife context. Reordered on 2026-10-18 by user decision.
- **Purpose:** Make server progression independent of rendering.
- **Prerequisites:** S02-T02.
- **Relevant files/subsystems:** E06; E08; Device time; GamePersistent; PHWorld; scheduler.
- **Actions:** Introduce server clock/frame driver with documented order and native physics substeps. Drive objects, scheduler, AI and ALife; replace camera-derived simulation prioritization with server activity policy for the fixture. Define pause and bounded catch-up behavior.
- **Outputs:** Dedicated tick driver and counters/trace fixture. Plus the common task evidence record.
- **Acceptance:** Simulation time, physics and scheduled entities advance without render callbacks or local input.
- **Validation:** Assert frame/order counters, falling-body collision, AI scheduled work and bounded long-frame handling.
- **Risks/blockers:** Do not replace native physics step with an arbitrary network tick or let unfocused-window pause freeze authority.

#### S02-T05 — Remove remaining presentation startup dependencies

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High**
- **Suffix tasks (split 2026-10-18; the parent completes when all four do):**
  - **S02-T05a — Game persistent object with no device: Completed.** [Validation](docs/validation/S02-T05a.md). Patch 0008. The real `CGamePersistent` constructs and app-starts with the environment data, no UI/menu/fonts, and all GPU resource creation skipped and counted. Its presentation frame is never registered; the frame driver keeps the scheduler slot.
  - **S02-T05b — CLevel, xrServer, game_sv_Single and ALife from the fixture save: Completed.** [Validation](docs/validation/S02-T05b.md). Patch 0009. The real `CApplication`, `CLevel`, `xrServer`, `game_sv_Single` and `CALifeSimulator` load the fixture save on the server with no device: level `k00_marsh`, the saved actor on it, 26,243 ALife objects, 1,074 server entities and the level AI graph, then a clean native teardown. The saved actor's view-entity decision is deferred to S02-T05d, where the actor is spawned client-side.
  - **S02-T05c — CPU sound-event layer for AI hearing and CPU particle definitions: Completed.** [Validation](docs/validation/S02-T05c.md). Patch 0014 (particles: patch 0009, 1,602 effects, 623 groups). The engine's own sound core runs on the CPU with no audio device and no listener (`-snd_cpu`): sources load, emitters run the native state machine, AI sound events reach the level's hearers and `feel_sound_new`, 2D events are dropped rather than placed at a fabricated listener; material pairs create their step/collide/breaking sounds on the server, and the footstep and impact range gates use the activity anchors instead of the camera. The `-nosound` stand-in's 1,002 `sound_theme` aborts are gone. smoke-60 passed `campaign-hearing-passed`; the client (client-18, capture-07) passes the same fixture and the comparison matches: the walked stalker's footsteps reach the same hearers with the same AI range and volume on both sides (B18 resolved).
  - **S02-T05d — Renderer-free level load, spawn and first tick: Completed.** [Validation](docs/validation/S02-T05d.md). Patch 0010. The client stages load `k00_marsh` with no device: CFORM (3,077,895 triangles), physics world, `game_cl_Single`, level script process; HUD, render-level load, texture upload and precache skipped (`level.som` is absent from this level's content). The first 50 server ticks spawn all 1,090 server entities client-side (the saved actor, 20 stalkers, 14 monsters, 374 zones, 120 items), step physics 370 times and tear down cleanly. Decision: the server adopts no control or view entity; the saved actor stays a real local object and `g_actor`.
- **Purpose:** Reach real renderer-free level initialization.
- **Prerequisites:** S02-T03a–c and S02-T04a (dependency loop resolved by user decision, 2026-10-18). S02-T03d, S02-T04b and S02-T04c follow this task's level load. G02 still requires every task.
- **Relevant files/subsystems:** E06; E09; EngineAPI; Level_load; IGame_Level; GamePersistent; Level_Bullet_Manager.
- **Actions:** Separate loading progress, HUD/menu, particles and presentation audio from gameplay hooks. Keep required sound stimuli as simulation data. Retain bullet workload outside rendering; load selected campaign level with no graphics device or view actor requirement. Construct persistent campaign state, `CLevel`, `xrServer`/`game_sv_Single`/ALife and `game_cl_Single` in native order (H02, moved from S02-T02; [context blockers 2–5](docs/campaign-server-context.md)) and extend the GameState smoke until it passes.
- **Outputs:** Renderer-free boot/load path and headless execution logs. Plus the common task evidence record.
- **Acceptance:** No GPU initialization/draw calls or interactive camera required; objects, collision and simple ballistics still update.
- **Validation:** Launch dedicated executable in a noninteractive/no-GPU test environment where available; instrument graphics creation paths and gameplay counters.
- **Risks/blockers:** Hidden-window-only success is insufficient; disabling sound/particles must not remove AI stimuli or gameplay callbacks.

#### S02-T06 — Validate native campaign simulation and checkpoint round-trip

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High**
- **S02-T06a — Actor Lua binder on the server, inert game UI: Completed (2026-10-18).** [Validation](docs/validation/S02-T06.md). Patches 0015 (diagnostics) and 0016. The actor's Lua binder never completed its first update on the server: a native fault in `ActorMenu.get_maingame()` (no game UI) was swallowed by the binder's `catch (...)` (xrGame has no C++ exceptions) and the binder deleted; with nil accessors the next HUD subscriber (`actor_status.script:266`) raised a fatal Lua error. The installed scripts' actor callbacks mix gameplay and HUD (28/27 subscribers; `get_hud()` at 61 sites in 30 scripts): blocker B20. **User decision (2026-10-18): inert server-side game UI.** The server now creates the UI core, fonts, HUD manager and the real `CUIGameSP` as CPU objects (GPU resources skipped and counted, UI base resolution 1024×768), never updated (`CHUDManager::OnFrame` returns on the server) nor drawn. Result: the binder completes its first update, `objects_per_update` drops to the client's 20, offline ALife work falls from ~91 to ~1.4 advances per tick, the server runs real-time (the HUD window's update alone had cost ~100 ms per tick), smoke-74 passes and the client (client-20, capture-08) matches on both fixtures.
- **S02-T06b — ALife actor follows the live actor: Completed (2026-10-18).** Patch 0017. The smoke teleports the actor 3 m as the script API does and after 50 ticks the ALife actor's `o_Position` matches the live position exactly (smoke-76, `campaign-actor-position-passed`): the host's `M_UPDATE` export path runs on the server, so the S02-T04c client-only switches at the 675 m boundary were positional, not a stale server position. Remaining S02-T06 items: combat between NPCs, native save/reload, the G02 evidence record.
- **S02-T06c — Combat between NPCs headless: Completed (2026-10-18).** Patch 0018. The shared combat fixture spawns a `boar_normal` through the native ALife spawn path 12 m from the fixture stalker (actor god mode for the fixture's duration) and observes the fight through observation hooks in the bullet manager and `CEntityAlive` plus the stalker's visual memory and enemy selection. On the server (smoke-78, `campaign-combat-passed`) the boar came online in 0.2 s, the stalker saw and selected it, fired 19 rounds, 6 impacted the boar as fire wounds and it died at 9.5 s with the server real-time; the client (client-21, capture-09) passes the same rules (2 rounds, 3 fire wounds, death at 10.3 s) and `compare-combat-capture.py` matches. A dog in smoke-77 was out-run by every round and died of physics contact damage against the stalker, which is the installed engine's behaviour. Remaining S02-T06 item at the time: native save/reload.
- **S02-T06d — Native checkpoint round-trip headless: Completed (2026-10-18).** [Validation](docs/validation/S02-T06.md) part 5. Patch 0019. The server sends the `M_SAVE_GAME` packet the console `save` command and the autosave manager send, handled synchronously by the in-memory server (`game_sv_Single::save_game` → `CALifeStorageManager::save`: `ClientSend`, `ClientSave`, the Lua storage manager's `.scoc`, then the `.scop`), checks the written header against the live state, stops the level and starts it again from the checkpoint the way `MakeReconnect` does. On the server (smoke-83, `campaign-checkpoint-passed`) the save took 53 ms for 26,235 objects; after the restart ALife holds exactly the 26,235 objects and 1,042 level objects the checkpoint wrote, the clock resumes from the saved time, the ALife actor, the fixture stalker and the dead boar come back with their saved positions and health, and after 50 ticks the live actor stands 0.000 m from the checkpoint with its binder's first update done and the Lua marshal state decoded from the `.scoc`. The normal client (client-24, capture-11) loads the same checkpoint and `compare-checkpoint-capture.py` matches every rule (actor 5 cm away, one ALife object fewer after 75 s of game time). The G02 evidence record is written; acceptance is the user's.
- **S02-T06e — A quest across two checkpoints: Completed (2026-10-18).** [Validation](docs/validation/S02-T06.md) part 6. Patch 0020. The server moves the actor beside Novikov (the Clear Sky mechanic), uses him and starts the talk state the way `RunTalkDialog` does, and runs the dialogue script functions that offer and accept `mar_base_stalker_tech_task_1`; the native task manager and the Lua task manager both track it; the given task crosses the checkpoint; the fetch item is spawned the way `alife():create` does, the status functor sets the complete stage, the turn-in functions complete it through `UpdateTasks` and `task_callback` (money 1,000 → 10,700, Clear Sky goodwill 0 → 75, info portion set, toolkit removed, native task Completed, Lua entry retired) and the completed state crosses a second checkpoint (smoke-95, exit 0, 47 quest rules). The normal client restores the given task from the server's first checkpoint and completes it the same way (capture-12) and loads the completed state from the second with the server's exact money, goodwill and info portion (capture-13); `compare-quest-capture.py` matches all seven comparisons. Found and recorded: B21, monster AI bound to `Level().CurrentEntity()` (the psy dog aura crashed the server and is guarded; phantom and legacy trader remain for Stage 3); a squad member online at a save is held under its group after the load (installed behaviour; the checkpoint rule names and accepts it).
- **Purpose:** Prove headless viability before implementing multiplayer.
- **Prerequisites:** S02-T05.
- **Relevant files/subsystems:** E02; E05; E08; E12; ALife; native task/Lua services; storage manager.
- **Actions:** Run bounded campaign fixture with no external clients; supply real entity/context data explicitly where necessary, never a permanent fake global player. Exercise AI, physics, time, Lua/task execution and native save/reload. Record missing actor-context work for Stage 3.
- **Outputs:** Headless integration fixture, native save comparison and G02 evidence. Plus the common task evidence record.
- **Acceptance:** Relevant CPU services advance and restore without renderer, UI or human client; campaign services do not depend on a permanent global placeholder actor.
- **Validation:** Observe counters and fixture outcomes across load/run/save/restart; SP smoke regression on matched source.
- **Risks/blockers:** Native checkpoint here has no journal/crash guarantee; unresolved fundamental actor requirement blocks G02 rather than being hidden by a dummy actor.

### Stage 3 — Actor and authority model

**Carried from G02** ([table](docs/validation/G02.md#user-decision-and-carried-limits)): B21 and the player-bound monster abilities (S03-T02, S03-T04), the one-quest coverage (S03-T05), one level and save, short runs and per-run variation. Every Stage 3 task that changes engine-work reruns the GameState smoke; a regression reopens G02.

#### S03-T01 — Define stable participants and authoritative contexts

- [x] **Status: Completed (2026-10-18)** · **Reasoning effort: High** · **Complexity: High** · [Validation](docs/validation/S03-T01.md) · [Identity and ownership table](docs/participant-identity.md) · Patch 0021. 128-bit campaign/player/party ids, 64-bit entity ids carried on `CSE_Abstract` across the ALife switches, `{u16, generation}` engine references fed from `xrServer::PerformIDgen`/`FreeID`, two participant slots as policy with slot generations and disconnect retention, a server-only authority token and action-context authorization. Four unit fixtures pass in the `Identity` smoke scope; in the loaded campaign every online and ALife object's binding matches its stored entity id and the actor is held by a participant. The GameState smoke passes with the hooks live (G02 regression rule).
- **Purpose:** Separate persistent identity from engine slots and views.
- **Prerequisites:** G02.
- **Relevant files/subsystems:** E01; E04; E12; state-ownership.md; planned server participant registry.
- **Actions:** Define campaign/player/party/entity/item IDs and generation-aware engine-ID mapping. Implement runtime participant registry with two active slots as policy, explicit action context and server/client role boundaries. Persistence comes in Stage 7.
- **Outputs:** Identity/context interfaces and ownership table. Plus the common task evidence record.
- **Acceptance:** Two identities cannot alias through recycled u16 IDs; no client-supplied identity becomes authority by assertion.
- **Validation:** Unit fixtures for slot reuse, disconnect identity retention and role/context misuse.
- **Risks/blockers:** Existing script and save formats may expose numeric engine IDs; map rather than silently replace wire/native formats.

#### S03-T02 — Separate local view access from gameplay authority

- [x] **Status: Completed (2026-10-18)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S03-T02.md) · [Inventory](docs/actor-access-inventory.md) · Patch 0022. Explicit actor access (`CampaignActorAccess.h`): the local view actor is client-only, gameplay names its participant actor through the registry, `Actor()` is a counted compatibility accessor that refuses with two actors, every server-side caller is recorded; actors adopt participants at spawn; a client gets a client-role registry. The server message policy refuses state imports, world control and game events from any sender but the bridge. All 126 + 372 sites classified; the B21 phantom, trader and psy dog aura sites resolved. Identity fixtures 53 + 259 checks and the level authority check 63 checks pass; the GameState smoke passes (smoke-106).
- **Purpose:** Keep cameras local without global actor mutations.
- **Prerequisites:** S03-T01.
- **Relevant files/subsystems:** E01; E04; Actor.cpp; Actor_Network; game_cl_single; Level_network.
- **Actions:** Introduce explicit actor/context lookup for core gameplay; retain local-view access on clients only. Restrict privileged internal state-import bridge to server runtime; enumerate and reject remote outcome paths.
- **Outputs:** Core actor access adapters and authority-boundary tests. Plus the common task evidence record.
- **Acceptance:** Server gameplay can operate without client view Actor(); remote state imports cannot set health/inventory/world state.
- **Validation:** Negative role tests; exercise two actor contexts and server with no rendering client.
- **Risks/blockers:** A global-access compatibility wrapper must not silently choose Player 1 for authoritative mutations. **G02 carried input (B21):** inventory and classify every `Level().CurrentEntity()` / `Actor()` use in `src/xrGame` (126 / 372 raw call sites on 2026-10-18); resolve the phantom and trader sites and the psy dog aura guard or keep each behind a recorded guard.

#### S03-T03 — Instantiate two independent native actors

- [x] **Status: Completed** (2026-10-19, [S03-T03](docs/validation/S03-T03.md), patch 0023) · **Reasoning effort: Extra High** · **Complexity: High**
- **Purpose:** Prove representation before networking integration.
- **Prerequisites:** S03-T02.
- **Relevant files/subsystems:** E01; E10; actor factories; Actor_Network; InventoryOwner.
- **Actions:** Use controlled server harness to create two genuine campaign actors; bind identities and independent input/control contexts. Verify condition, equipment, inventory containers, death and lifecycle independently.
- **Outputs:** Two-actor native fixture and lifecycle evidence. Plus the common task evidence record.
- **Acceptance:** Both are participants, independently addressable with distinct state; destroying one does not invalidate the other or global services.
- **Validation:** Scripted native commands exercise each actor, lifecycle/slot reuse and save-ready identity mapping.
- **Risks/blockers:** Legacy MP actor representation may assume competitive rules; reuse representation without replacing campaign mode.

#### S03-T04 — Adapt ALife activation and basic AI to both participants

- [x] **Status: Completed (2026-10-19)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S03-T04.md) · Patch 0024. The ALife switch rules measure from the nearest alive participant actor; with the actors 1,098 m apart every object on k00_marsh obeys the rule, each actor's area stays online and the far area goes offline on return; a boar takes each actor as its enemy; a detached participant's retained actor keeps its area and the level goes on; the player-bound abilities are explicit no-ops or participant-aware (table in the report). Open item: the headless server has no light, so AI vision runs as in darkness (S03-T04 Limits).
- **Purpose:** Prevent one actor from defining the whole active world.
- **Prerequisites:** S03-T03.
- **Relevant files/subsystems:** E02; E08; enemy_manager; ALife graph and online/offline switch.
- **Actions:** Keep one active level but use participant set for activation/deactivation policy. Validate generic enemy selection and awareness for either player; record special global-Actor abilities for Stage 10.
- **Outputs:** Participant-aware activation and basic NPC targeting fixture. Plus the common task evidence record.
- **Acceptance:** Entities needed near either player remain online; NPC can detect/target either; one disconnect does not destroy level context.
- **Validation:** Move two fixture actors to separated areas of same level and inspect activation and target changes.
- **Risks/blockers:** Activation union increases simulation load; special mutant abilities remain explicitly uncovered until later. **G02 carried input:** record each player-bound ability (phantom, psy dog aura, controller, snork, pseudogigant, scanning) as participant-aware or explicitly uncovered for S10-T02; measure tick cost with the union and record run length.

#### S03-T05 — Establish native and Lua participant context contract

- [x] **Status: Completed (2026-10-19)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S03-T05.md) · Patch 0025. Participant script contexts are entered only by native entry points and authorized against the registry; Lua lookups are strict; the server's compatibility layer makes the installed scripts' `db.actor` and `AC_ID` follow the context and carries it into task checks and timers; what a player would see goes to its presentation outbox. Both participants run the installed mechanic quest concurrently, each rewarded on its own actor; forged, stale and missing contexts enter nothing. Remaining single-actor script sites are audited (167 lines) for S08/S10.
- **Purpose:** Make later quest work possible without implicit Player 1.
- **Prerequisites:** S03-T03; S03-T04.
- **Relevant files/subsystems:** E05; E12; script bindings; native task/dialogue services; installed relevant scripts.
- **Actions:** Inventory only entry points required by initial fixtures; define server-authoritative mutation bindings with explicit participant/party context and client presentation callbacks. Test a harmless contextual script action for both actors; defer full quest implementation.
- **Outputs:** Context API, bounded script audit and G03 fixture. Plus the common task evidence record.
- **Acceptance:** Native and Lua fixture identifies the correct actor/party; no arbitrary global fallback; both contexts coexist.
- **Validation:** Run same script fixture for both participants and invalid/missing-context cases; SP compatibility smoke.
- **Risks/blockers:** Installed campaign scripts are not equivalent to repository addons; defer broad audit but document unresolved calls. **G02 carried input:** rerun the S02-T06e quest fixture with an explicit participant context for each actor; the dialogue window goes to client presentation callbacks.

#### S03-T06 — Server light model for AI vision

- [x] **Status: Completed (2026-10-19)** · **Reasoning effort: High** · **Complexity: Moderate** · Added 2026-10-19 by user decision (S03-T04 open item), before G03 · [Validation](docs/validation/S03-T06.md) · Patch 0026. Each tick, per participant, the server runs the client's own light tracking on the actor and one object of its view, with the weather/time-of-day blend and modifiers at its eye and the level's sun and static lights as CPU objects; at steady state the server and the normal client read the same actor identically (0.0213 / 0.0213); AI observers read the participants 14–42× brighter than the old darkness floor; 38.9 µs per tick for two participants.
- **Purpose:** AI on the campaign server sees every target as in darkness: the visibility value scales with the target's luminosity, which only the renderer's lighting pass fills (`Layers/xrRender/LightTrack.cpp:180-293`, `xrGame/visual_memory_manager.cpp:322-328`), and the server never renders.
- **Prerequisites:** S03-T04.
- **Relevant files/subsystems:** `CROS_impl::update` (sun ray, sky rays, static hemi lights from `build.lights`); `CLight_DB::Load/LoadHemi/Update`; `CEnvironment::lerp` and the level's environment modifiers; the client's per-frame tracking (`r4_R_render.cpp:45-63`).
- **Actions:** Run the client's CPU lighting on the server per participant, as a client does for its own view: the participant actor every tick and one object in its view frustum in turn, with the time-of-day/weather blend and modifiers at that actor's eye; load the level's static lights on the CPU. No constant luminosity; NPCs are not otherwise refreshed (user decision 2026-10-19).
- **Outputs:** Server lighting module, fixture and client comparison. Plus the common task evidence record.
- **Acceptance:** Participant actors' luminosity on the server follows sun, sky, weather and static lights and matches the client's for the same checkpoint within a stated tolerance; AI visibility of each participant uses it.
- **Validation:** GameState stage for both participants; client capture at the same checkpoint compared; the G02 combat and S03-T04 awareness stages rerun.
- **Risks/blockers:** Dynamic lights (flashlights, campfires) do not count on the DX11 client either; NPC targets keep the client's off-screen behaviour.

### Stage 4 — Networking foundation

#### S04-T01 — Validate GameNetworkingSockets in isolation

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: High** · **Complexity: Moderate** · [Validation](docs/validation/S04-T01.md) · No engine patch. GNS v1.6.1 (commit `d534c19a`, BCrypt crypto, static protobuf 3.21.12, ICE off) builds with the pinned v143 toolchain without warnings; its DLL adds only `bcrypt.dll` to what the campaign server already loads. On one Windows host over loopback (net-8): two peers connect and a third is refused, 12,000 reliable messages survive 10% loss, reordering and duplication intact and in order, unreliable delivery tracks loss with stale ones discarded, and a send-side discard rule on a lower-priority lane keeps critical latency within 9 ms of baseline on a 3× oversubscribed link. Crash and reconnect traces recorded. Accepted for S04-T02 with five adapter/liveness findings; no separate-machine test was available.
- **Purpose:** Accept transport only after Windows compatibility evidence.
- **Prerequisites:** G03.
- **Relevant files/subsystems:** Planned tools/network-probe; protocol adapter contract; pinned Windows toolchain.
- **Actions:** Pin library/dependencies/license; build isolated dedicated listener and two peers. Test connect, disconnect, reliable ordering and unreliable delivery under induced loss; record threading and packaging needs.
- **Outputs:** Transport probe, exact build/run commands and acceptance decision. Plus the common task evidence record.
- **Acceptance:** Works with accepted compiler/runtime; reliable traffic survives loss and stale transient data can be discarded without blocking critical traffic.
- **Validation:** Local and separate-machine connections where available; controlled loss/reordering and reconnect traces.
- **Risks/blockers:** NAT/deployment expectations must be stated; failure requires evidence before an approved fallback, not custom UDP.

#### S04-T02 — Implement a bounded transport adapter

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: High** · **Complexity: High** · [Validation](docs/validation/S04-T02.md) · No engine patch. `protocol/` builds `coop_net` (interface, shared policy, in-memory fake; no transport library) and `coop_net_gns` (the only GNS includer) with the Stage 1 compiler, `/MD`, C++17, 0 warnings (proto-build-4). Calls off the owner thread are refused; library callbacks only route notices to the owner's inbox. Critical overflow closes the connection (never a silent drop), bulk overflow refuses the send, transient sends are dropped when stale or over budget; inbound unknown-channel, wrong-class and oversize messages close the peer. tr-4: 51/51 (fake and GNS loopback scenarios, 7 GNS race tests), repeated 3 full and 8 race runs. Loopback on one host only; no sanitizer.
- **Purpose:** Contain library-specific code and thread interactions.
- **Prerequisites:** S04-T01.
- **Relevant files/subsystems:** Planned protocol transport interface; server/client adapters; native networking integration points E10.
- **Actions:** Wrap listen/connect/send/poll/close; define separate delivery classes, bounded queues and ownership. Marshal received intents to simulation thread; keep native serializers behind explicit boundaries.
- **Outputs:** Transport adapter and fake-transport tests. Plus the common task evidence record.
- **Acceptance:** Gameplay code does not depend on library API; network callbacks cannot mutate world directly; overload has explicit policy.
- **Validation:** Loopback adapter tests, shutdown races and queue-limit stress.
- **Risks/blockers:** Transport ordering is not global gameplay ordering; unreliable stream sequence still needs application policy.

#### S04-T03 — Define and validate the initial protocol codec

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: High** · **Complexity: High** · [Validation](docs/validation/S04-T03.md) · No engine patch. `coop_protocol` (`protocol/include/coop/proto/`): v1.0, 24-byte header (the session's ids bind once in Hello/Welcome), each type bound to one channel and sender, every length/count/enum/float/identity bounded before use, opaque blobs bounded and never handed to a native serializer, encoder refuses what the decoder refuses. Guard: reliable sequences contiguous, transient forward-only, older epochs dropped and newer ones violations, recycled generations refused, resync chunks ordered with CRC. pt-3: 20/20 (19 round-trip fixtures, 6,238 truncations, per-field limits, versions, 2.0 M fuzz mutations behind a guard page with 0 non-canonical acceptances, protocol over fake and GNS transport); tr-7 regression 51/51.
- **Purpose:** Reject malformed and incompatible inputs early.
- **Prerequisites:** S04-T02.
- **Relevant files/subsystems:** docs/network-protocol.md; planned protocol codecs; E12 ID mapping.
- **Actions:** Specify version/content compatibility, campaign/session/epoch IDs, stream sequences, ticks, lengths and stable IDs. Implement hello, input/action envelopes, result, entity lifecycle, snapshot/baseline ack and resync framing with bounded decoding.
- **Outputs:** Versioned protocol schema, codecs and malformed-packet fixtures. Plus the common task evidence record.
- **Acceptance:** Length/count/version/identity bounds enforced; obsolete epochs and recycled entity generations rejected.
- **Validation:** Round-trip and truncation/oversize/fuzz fixtures; unknown version/type handling.
- **Risks/blockers:** Native serializers may assume trusted packets and unstable layouts; never expose unchecked deserialization.

#### S04-T04 — Bind authenticated sessions to canonical players

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: High** · **Complexity: High** · [Validation](docs/validation/S04-T04.md) · No engine patch. `coop_session`: per-player ECDSA P-256 keys (BCrypt), DPAPI key files made by `coop-keytool`, a public-key roster on the server; Hello → AuthChallenge → AuthProof (signed transcript of both nonces, campaign, player, build, content) → Welcome; slots owned by players across reconnects, a reconnect replaces the stale session; per-address and per-channel rate limits, handshake timeout; game code sees messages attributed only through the binding. pt-6 43/43 (session tests on fake and GNS: two joins, impersonation, replay, third player, mismatches, floods, order, no secrets in diagnostics); tr-8 51/51. Stated limit: an active man in the middle relaying a live handshake (GNS encryption is unauthenticated; certificates need a compiled-in CA).
- **Purpose:** Make two connections safe participants rather than trusted state writers.
- **Prerequisites:** S04-T03.
- **Relevant files/subsystems:** Server session manager; participant registry; adapter; protocol handshake.
- **Actions:** Define initial deployment/authentication and credential storage; bind reconnect credentials to stable player IDs. Enforce two active slots, build/content match and rate limits; external peers receive no privileged local-client role.
- **Outputs:** Join/session state machine and rejection tests. Plus the common task evidence record.
- **Acceptance:** Spoofed player IDs, third active client and incompatible builds cannot enter authority; two legitimate peers receive distinct identities.
- **Validation:** Positive two-peer joins plus impersonation, replay, excessive requests and mismatch tests.
- **Risks/blockers:** Do not confuse transport encryption with application identity; do not log tokens or commit credentials.

#### S04-T05 — Handle liveness and in-memory reconnect

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: High** · **Complexity: Moderate** · [Validation](docs/validation/S04-T05.md) · No engine patch. Client heartbeats (reliable, echoed: RTT) and per-side last-heard timers decide suspect/timeout (defaults 1/5/15 s, provisional; transport timeout set above). Resume needs the key proof plus the latest single-use token (stored hashed, constant-time compare) and keeps session id, slot and generation; a reconnect closes the old connection before the Welcome and every message carries its `join_serial`; detached players keep their slots (optional timed release); `Server::shutdown`. pt-9 61/61 (fake and GNS: detection within one heartbeat of the setting, 15 resume cycles, in-flight old messages dropped, shutdown, release), pt-10/11 liveness repeats, tr-9 51/51. No cross-restart recovery (Stage 7).
- **Purpose:** Establish session lifecycle before world replication.
- **Prerequisites:** S04-T04.
- **Relevant files/subsystems:** Session manager; disconnect policy; docs/network-protocol.md.
- **Actions:** Implement measured/configurable heartbeats, bounded timeout policy, token rotation and same-process resume. Define actor behavior while disconnected; stage later durable recovery separately.
- **Outputs:** Lifecycle tests, operational settings and G04 report. Plus the common task evidence record.
- **Acceptance:** Disconnect detected; stale session cannot keep submitting; reconnect restores same participant without duplicate slots.
- **Validation:** Dropped connection, delayed old messages, repeated reconnect and server shutdown tests.
- **Risks/blockers:** No cross-restart recovery claim yet; timeout defaults must be documented as provisional.

### Stage 5 — First playable two-player session

#### S05-T01 — Integrate matched client and dedicated roles

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S05-T01.md) · [Design](docs/coop-session.md) · Patch 0027. The protocol libraries (v1.1) link statically into both products (`Coop.props`, delay-loaded GNS); build and content identity computed and matched; `-campaign-serve` runs the campaign in real time with the session server on its tick, binds the roster to participants, attaches/detaches sessions, gives a joining participant an actor (without re-running the new-game squad population) and keeps detached actors; clients connect from the menu and refuse local game starts. coop-9: server alone, two clients joined, 3/3 injected outcomes refused, rejoin; build and content mismatches refused on real processes; smoke-146 (66 stages) and the single-player checkpoint capture (capture-25) pass.
- **Purpose:** Connect real Monolith clients to server-owned campaign simulation.
- **Prerequisites:** G04.
- **Relevant files/subsystems:** E03; E04; E10; client/server startup; game_cl_single.
- **Actions:** Route external clients through adapter and retain backend-owned native simulation. Disable client authority over shared AI/ALife/campaign mutations while preserving rendering, input and local presentation. Add safe isolated launch profiles.
- **Outputs:** One-server/two-client launch recipe and role guards. Plus the common task evidence record.
- **Acceptance:** All three matched executables start and connect; only server advances canonical shared gameplay.
- **Validation:** Process logs and deliberate client outcome injection; verify server runs without either client.
- **Risks/blockers:** Client native world may accidentally execute scripts/AI with side effects; role guards must precede replication.

#### S05-T02 — Replicate entity lifecycle and initial world snapshot

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S05-T02.md) · Patch 0028. A joining client receives a native checkpoint of the server's world with the online set at the lifecycle watermark (1.8 MB, ~7 s; loaded in ~9.5 s), then follows the server's Spawn/Despawn stream; the client's replica runs no ALife simulation, presents NPCs as remote, makes its own participant's actor the player and refuses every local spawn, release and game event. world-7 (two clients, late join, rejoin, a server spawn/release every second): all three replicas equal the server's online set at their commit (1,088–1,090 entities: engine id, entity id, generation, section), all present.
- **Purpose:** Give clients one consistent world identity graph.
- **Prerequisites:** S05-T01.
- **Relevant files/subsystems:** Object factories; native serializers E10; identity map; snapshot protocol.
- **Actions:** Implement server spawn/despawn/tombstones and initial snapshot at a revision watermark. Buffer dependent events during join; map stable IDs to local engine objects with generations. Bound snapshot chunks and restart when baseline invalidates.
- **Outputs:** Lifecycle replication and join snapshot integration tests. Plus the common task evidence record.
- **Acceptance:** Both clients have matching authoritative entity identities; late join cannot resurrect destroyed entities or miss in-flight changes.
- **Validation:** Join during spawn/despawn; reorder lifecycle versus snapshots; compare canonical ID/state summaries.
- **Risks/blockers:** Local creation and server creation can duplicate objects; copied saves must not overwrite authority.

#### S05-T03 — Display both participants with separate local views

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: High** · **Complexity: High** · [Validation](docs/validation/S05-T03.md) · Patch 0029. Each client views and controls its own participant's actor and sees the other drawn whole; input goes to the server, which applies it only to the actor the session's context resolves; 20 Hz actor snapshots interpolated by server time; the server's game time and weather follow to the clients. view-11: each walk moved only its own actor (2.23 / 2.16 m vs 0.00 m), both clients show both actors within 0.15 m of the server (0.7 mm still), same sky. No prediction (S06-T03).
- **Purpose:** Complete visible two-player representation.
- **Prerequisites:** S05-T02.
- **Relevant files/subsystems:** CActor/CActorMP representation; Actor_Network; actor_mp_client_import; cameras/HUD.
- **Actions:** Bind each client view to its own authoritative participant. Render remote actor equipment/pose using reusable replication interfaces; preserve independent input collection and local UI.
- **Outputs:** Two-client actor/view demonstration and regression fixtures. Plus the common task evidence record.
- **Acceptance:** Each player sees both distinct actors and controls only its own input context; client camera choice never changes server participant.
- **Validation:** Simultaneous two-client observation and independent camera/input checks with recorded build IDs.
- **Risks/blockers:** Basic display is not validated movement prediction or full combat; those remain Stage 6.

#### S05-T04 — Verify a bounded shared-world session

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: High** · **Complexity: Moderate** · [Validation](docs/validation/S05-T04.md) · Patch 0030 · [G05 record](docs/validation/G05.md). session-2: server and two matched clients, c2 joined late, quit and rejoined under 10 s medkit churn; all three replicas equal the server's online set; each walk (c1, c2 before and after the rejoin) moved only its own actor; both viewpoints captured. Fixed: a rejoined player's input was refused as old (input watermark kept across connections).
- **Purpose:** Make the first playable gate observable.
- **Prerequisites:** S05-T03.
- **Relevant files/subsystems:** Two-client test harness; lifecycle snapshots; dedicated logs.
- **Actions:** Run small same-level fixture with two joins, both actors visible, server-controlled entity change, late join and disconnect/rejoin. Publish exact supported interactions and state comparison results.
- **Outputs:** G05 report and repeatable minimal session scenario. Plus the common task evidence record.
- **Acceptance:** Two real modified clients observe both players and the same server-owned entity changes without duplicate simulation authority.
- **Validation:** Run all three processes; compare server/client canonical subsets and observe both viewpoints.
- **Risks/blockers:** Do not label the whole campaign playable; movement/combat and durability are incomplete.

### Stage 6 — Authoritative movement and combat

#### S06-T01 — Process validated movement inputs on the server

- [x] **Status: Completed (2026-10-20)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S06-T01.md) · Patch 0031, protocol `input_queue.h` (proto-build-18). Per-player input queue: sequenced by tick, validated, at most 5 waiting, one consumed per server tick; the native control path decides speed, stance and collision. move-8: duplicates ×4, a shifted counter with a +100000 jump and a replay moved no faster or further than a normal walk; Prone, a teleport intent and a forged snapshot moved nothing; both actors walked, crouched and sprinted, and their bodies never overlapped. Fixed: two actors within about 1.5 m pinned each other (`UpdateObjectBox` zeroed the other actor's control acceleration).
- **Purpose:** Replace client-reported positions with native authoritative movement.
- **Prerequisites:** G05.
- **Relevant files/subsystems:** Actor movement/controller; physics; input protocol; authority guards.
- **Actions:** Sequence input batches per player; bound time windows and queue sizes; validate identity, movement modes, speed/stamina and collision. Drive native controller on server; reject teleport/outcome packets.
- **Outputs:** Authoritative input path and movement validation fixtures. Plus the common task evidence record.
- **Acceptance:** Server movement obeys collision and constraints; duplicates/stale inputs cannot double-advance; spoofed outcomes cannot move actors.
- **Validation:** Two actors moving against obstacles; malformed/time-shifted input and impossible-speed tests.
- **Risks/blockers:** Native movement may depend on camera orientation or client timestep; translate intention explicitly.

#### S06-T02 — Implement snapshots, interpolation and baselines

- [x] **Status: Completed (2026-10-21)** · **Reasoning effort: High** · **Complexity: High** · [Validation](docs/validation/S06-T02.md) · Patch 0032, protocol v1.2 `snapshot.h`/`interpolation.h` (proto-build-26). Deltas against acknowledged baselines, bounded full resyncs, adaptive part budget; NPCs and moving objects on clients (L04 closed). snap-11/snap-12 (nominal; 5 % loss, 40 ms lag): all 12 presentation checks pass, moving NPCs p95 0.11–0.14 m, rested objects within 1.4 mm, 11–14 KB/s per client. Fixed: NPCs ran AI on replica clients (spawns forced local) and, once remote, stood in the bind pose.
- **Purpose:** Provide timely state without reliable-stream backlog.
- **Prerequisites:** S06-T01.
- **Relevant files/subsystems:** Protocol snapshots; server replication; client interpolation.
- **Actions:** Add ticked full/delta snapshots, acknowledged baselines, entity generations and state revisions. Interpolate remote motion; discard obsolete state; cap bandwidth/interest to active relevant entities. Preserve critical lifecycle causality.
- **Outputs:** Snapshot pipeline and loss/baseline tests. Plus the common task evidence record.
- **Acceptance:** Missing baseline triggers bounded full resync; old snapshots never overwrite newer state; reliable events are not blocked by obsolete motion.
- **Validation:** Loss/reordering/baseline eviction tests plus bandwidth measurements.
- **Risks/blockers:** Use provisional 30 Hz simulation and 15–20 Hz replication only as measured candidates; native physics step remains independently preserved.

#### S06-T03 — Predict and reconcile the local actor

- [x] **Status: Completed (2026-10-21)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S06-T03.md) · Patch 0033, protocol v1.3 `prediction.h` (proto-build-29). Native own-actor prediction, reconciliation against the server's last processed input, visual-only correction smoothing; pred-8/pred-9 (nominal; 5 % loss, 40 ms lag): deliberately wrong predictions (G, P) stay under 0.28 m and converge within 4 cm, own actor moves in 83–100 ms against 250 ms without prediction. Fixed: refused inputs not reconciled; other actor's arms not following its aim; own HUD arms snapping (two actors shared `cam_Update` statics). L02 closed.
- **Purpose:** Maintain responsiveness while accepting server correction.
- **Prerequisites:** S06-T02.
- **Relevant files/subsystems:** Local actor prediction; input acknowledgement; native movement controller.
- **Actions:** Buffer bounded unacknowledged inputs; reconcile authoritative state and replay only supported movement prediction. Separate visual smoothing from collision state; reset buffers on epoch/teleport/transition.
- **Outputs:** Prediction/reconciliation implementation and correction metrics. Plus the common task evidence record.
- **Acceptance:** Deliberately wrong local predictions converge to server state; old input cannot affect a new epoch.
- **Validation:** Latency/loss tests and forced correction scenarios; measure error and smoothing limits.
- **Risks/blockers:** Do not replay inventory, quests or authoritative damage as movement prediction side effects.

#### S06-T04 — Implement one authoritative weapon/combat slice

- [x] **Status: Completed** ([report](docs/validation/S06-T04.md)) · **Reasoning effort: Extra High** · **Complexity: High**
- **Purpose:** Prove the server resolves hits, ammo and damage.
- **Prerequisites:** S06-T01; S06-T02.
- **Relevant files/subsystems:** Level_Bullet_Manager E09; weapon/fire logic; actor health; event dispatch E04.
- **Actions:** Choose one native weapon and simple target; validate equip/ammo/fire cadence and input timing. Resolve native trajectory/raycast, hit and health on server; replicate outcomes and presentation separately. Start with server-present-time hit tests.
- **Outputs:** Weapon encounter fixture and attack-validation tests. Plus the common task evidence record.
- **Acceptance:** Clients cannot report damage/hits/rewards as truth; server ammo/health outcomes agree on both clients.
- **Validation:** Simultaneous shots, empty ammo, forged damage, repeated fire and movement-while-firing tests.
- **Risks/blockers:** Render-coupled ballistics and animation hitboxes; historical rewind only if measured playability justifies a separately scoped task.

#### S06-T05 — Validate basic NPC combat and stage convergence

- [x] **Status: Completed (2026-10-21)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S06-T05.md) · Patch 0035. Native participant deaths on the server, native death presentation on clients, the dead take no input, no level change; four final encounter runs pass all checks nominal and at 5 % / 40 ms and 10 % / 100 ms.
- **Purpose:** Include an actual authoritative opponent and close the movement gate.
- **Prerequisites:** S06-T03; S06-T04.
- **Relevant files/subsystems:** AI targeting E08; NPC health/death; two-client harness.
- **Actions:** Extend fixture to one NPC encounter targeting either participant. Replicate health/death and tested interactions; verify client prediction cannot overturn outcomes. Measure provisional simulation/replication rates and failure limits.
- **Outputs:** G06 report, encounter test and rate/bandwidth observations. Plus the common task evidence record.
- **Acceptance:** Two clients converge after corrections and NPC/player damage/death under declared network conditions.
- **Validation:** Repeat encounter with controlled latency/loss and forged outcomes; SP combat regression.
- **Risks/blockers:** Critical deaths are only in-memory here; do not claim persistent campaign success until Stage 7 adapters cover them.

#### S06-T06 — Cordon fixture without Fanat

- [x] **Status: Completed (2026-10-22)** · [Validation](docs/validation/S06-T06.md) · Patch 0036, `proto-build-31`. A user-made Cordon save; the co-op programs pass there with no companion; fixed on the way: a server crash on blood marks, a join race at 10 % loss, a replica missing an object whose id was reused · **Reasoning effort: High** · **Complexity: Moderate** · Added 2026-10-21 by user decision at G06 (decision 4), after G06
- **Purpose:** Run every later co-op test in Cordon (`l01_escape`) with no companion interfering.
- **Prerequisites:** G06.
- **Relevant files/subsystems:** fixture save and `docs/headless-fixture.json`; `prepare-stage2-smoke.py`, `prepare-coop-session.py`; the host fixtures (`CoopServerHost.cpp`: arm, encounter, combat_heal) and client programs (`CoopClientLink.cpp`) that name Fanat (19409) or marsh positions; the GameState smoke's fixture ids.
- **Actions:** Make a Cordon fixture save with no companion: a new single-player game started in Cordon with the accepted client (or a user-made save), recorded with hashes like the marsh fixture. Remove Fanat from the co-op fixtures (no companion; the encounter's spawn rule no longer names him). Port the co-op programs (movement, snapshots, combat, encounter, session) and decide which single-player smoke and capture fixtures move with it (their NPC/monster ids are save-specific).
- **Outputs:** Cordon fixture record, updated tools and fixtures, rerun of the Stage 6 programs in Cordon. Plus the common task evidence record.
- **Acceptance:** The Stage 6 co-op programs pass in Cordon with no companion; the marsh fixture is kept for the existing single-player smoke unless it is moved too.
- **Validation:** Encounter (nominal and impaired), combat, session and snapshot runs in Cordon; the existing smoke and captures as regression.
- **Risks/blockers:** A new game started without the faction-selection screen may not be a representative save; a user-made save may be needed. Cordon has its own level changers and story scripts near the start.

#### S06-T07 — Jump and lean as server input

- [x] **Status: Completed** (2026-10-22) · [Validation](docs/validation/S06-T07.md) (patches 0037–0040, protocol v1.5) · **Reasoning effort: Medium** · **Complexity: Low** · Added 2026-10-21 (user question at G06; L01 had no owner for jump and lean)
- **Purpose:** Let players jump and lean through the server like movement and fire.
- **Prerequisites:** S06-T06.
- **Relevant files/subsystems:** `protocol/` input buttons; `CoopServerHost.cpp` (`apply_inputs`); `CoopClientLink.cpp` (input capture, prediction).
- **Actions:** Add jump and lean (left, right) bits to the input; press the native keys on the player's own actor on the server, as fire is; predict jump on the client through the native control path; validate rates (no jump while airborne or dead).
- **Outputs:** Protocol change, engine change, tests. Plus the common task evidence record.
- **Acceptance:** Both players jump and lean on the server and both clients show it; forged or repeated jumps do no more than the native control allows.
- **Validation:** A movement leg with jumps and leans, nominal and impaired; protocol tests; prediction check.
- **Risks/blockers:** Jump prediction against server physics may correct visibly on landing.

#### S06-T08 — Storm and emission visuals on both clients

- [x] **Status: Completed** (2026-10-22) · [Validation](docs/validation/S06-T08.md) (patch 0041; three new CampaignEvent kinds, protocol v1.5 unchanged) · **Reasoning effort: High** · **Complexity: High** · Added 2026-10-21 by user decision at G06 (decision 5): weather visuals as soon as it makes sense; L09 moved here from S10-T05
- **Purpose:** Both players see the same storm and emission when the server has one.
- **Prerequisites:** S06-T06.
- **Relevant files/subsystems:** weather manager (`CEnvironment`, `WeatherManager` script), thunderbolts and rain, `surge_manager`/`psi_storm_manager` scripts, camera effectors and post-processes, the S05-T03 weather replication.
- **Actions:** Find what decides storm and emission visuals (native weather blend, script-driven effects, random thunderbolts). Replicate the server's state (weather blend and its transition, wind, thunderbolt events, emission phase and timers) and present it on clients with the native effects. Keep the emission's gameplay (cover checks, damage, kills) on the server; that remains S10-T05.
- **Outputs:** Protocol/state change, engine and script-presentation changes, screenshots from both clients. Plus the common task evidence record.
- **Acceptance:** During a server storm and a forced emission both clients show the same weather section and blend, rain, lightning flashes within a bounded delay, and the emission's sky, sound and camera phases in step with the server.
- **Validation:** A forced storm and a forced emission (fixture), measured per phase on both clients; screenshots.
- **Risks/blockers:** Some effects are Lua-driven on the actor's binder (L05: no gameplay scripts on clients); presentation may need a client-side script path.

### Stage 7 — Persistence infrastructure

#### S07-T01 — Specify recoverable state and critical transaction boundaries

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High**
- **Purpose:** Make durable guarantees precise before implementing storage.
- **Prerequisites:** G06.
- **Relevant files/subsystems:** E12; state-ownership.md; native saves; proposed journal interfaces.
- **Actions:** Inventory checkpoint contents and gaps for two actors, stable IDs, scripts and world. Classify critical effects versus transient rollback; define ordered transaction/revision/operation IDs and schema compatibility. Specify prepare/validate/durable commit/apply/ack phases and recovery behavior.
- **Outputs:** Persistence schema/guarantee matrix and synthetic critical-effect fixture. Plus the common task evidence record.
- **Acceptance:** Every acknowledged critical effect has a reconstructible representation; transaction bundles and failure windows are explicit.
- **Validation:** Review against inventory/quest/reward/death/ownership examples; simulate each crash boundary on paper with expected recovery states.
- **Risks/blockers:** Native saves may omit second actor/script state; opaque side effects cannot be called durably recoverable.

#### S07-T02 — Create consistent authoritative checkpoint generations

- [x] **Status: Completed (2026-10-23)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S07-T02.md) · Patch 0042
- **Purpose:** Replace client-dependent saves with recoverable server checkpoints.
- **Prerequisites:** S07-T01.
- **Relevant files/subsystems:** alife_storage_manager; Process_save E04; actor/identity registries.
- **Actions:** Capture native world plus supplemental participant/ID/schema state at a consistent revision. Publish generations with checksums/manifests and crash-safe temporary-write/flush/rename semantics verified on Windows; retain previous valid checkpoint and referenced journal.
- **Outputs:** Checkpoint adapter and corrupt/partial checkpoint tests. Plus the common task evidence record.
- **Acceptance:** Restore a consistent checkpoint without trusting client saves; incomplete new generation does not destroy last recoverable one.
- **Validation:** Round-trip two actors/world; interrupt each publication boundary; inspect actual filesystem guarantees.
- **Risks/blockers:** Native save hooks may mutate world or gather client state; snapshot barrier must cover these effects.

#### S07-T03 — Implement durable critical outcome journal

- [x] **Status: Completed (2026-10-23)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S07-T03.md) · Patch 0042
- **Purpose:** Ensure permanent success is acknowledged only after durability.
- **Prerequisites:** S07-T01.
- **Relevant files/subsystems:** Planned server journal/transaction module; simulation command queue.
- **Actions:** Select local journal format/storage with justified durability semantics. Serialize validated resolved effects, identities, expected revisions and deduplication outcomes. Commit critical bundle before success; apply exactly once in logical outcome, fail closed on storage failure, and retain unresolved committed effects for recovery.
- **Outputs:** Journal module, synthetic mutation adapter and operation-result index. Plus the common task evidence record.
- **Acceptance:** Duplicate IDs return same outcome; conflicting revision fails; acknowledged synthetic critical effects survive process termination.
- **Validation:** Fault injection before/after append, flush, apply and acknowledgement; disk-full/write-failure tests.
- **Risks/blockers:** An fsync-equivalent claim must match Windows implementation; simulation must not continue inconsistently after commit/apply failure.

#### S07-T04 — Recover checkpoints and committed effects safely

- [x] **Status: Completed (2026-10-23)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S07-T04.md) · Patch 0042
- **Purpose:** Restore critical outcomes without deterministic simulation replay.
- **Prerequisites:** S07-T02; S07-T03.
- **Relevant files/subsystems:** Recovery coordinator; native load; journal adapters; stable-ID map.
- **Actions:** Load highest valid checkpoint generation and replay only committed effects after watermark using schema-aware idempotent adapters. Include definitions/tombstones for durable entities created or destroyed after checkpoint. Reject incompatible content; define journal retention and safe truncation.
- **Outputs:** Recovery pipeline and checkpoint-plus-tail fixtures. Plus the common task evidence record.
- **Acceptance:** Committed critical effects survive while transients may roll back; no duplicate entities or effects; corrupt/incompatible data is detected without silent reset.
- **Validation:** Kill/restart across checkpoint/journal interleavings, torn trailing records, duplicate outcomes and ID reuse.
- **Risks/blockers:** Raw event re-execution may repeat Lua side effects or random rewards; restore resolved effects rather than rerunning gameplay scripts.

#### S07-T05 — Resynchronize clients after restored authority

- [x] **Status: Completed (2026-10-23)** · **Reasoning effort: High** · **Complexity: High** · [Validation](docs/validation/S07-T05.md) · Patch 0042
- **Purpose:** Fence stale sessions and incompatible prediction state.
- **Prerequisites:** S07-T04.
- **Relevant files/subsystems:** Session manager; authority epoch; snapshots; action-result queries.
- **Actions:** Issue new authority epoch after restart; bind campaign generation/revision to snapshots. Reject old input/events, clear predictions and stale local world, and supply full or validated partial resync plus durable operation outcomes.
- **Outputs:** Crash reconnect/resync path and convergence tests. Plus the common task evidence record.
- **Acceptance:** Both clients converge to restored server state; lost acknowledgements do not cause duplicate mutations; stale world cannot continue playing.
- **Validation:** Restart while clients remain running; deliver delayed packets and retry operation IDs before/after reconnect.
- **Risks/blockers:** Resuming token alone cannot establish snapshot compatibility; explicit revision handshake is required.

#### S07-T06 — Qualify the persistence contract before inventory and quests

- [x] **Status: Completed (2026-10-23)** · [Validation](docs/validation/S07-T06.md) · [G07 report](docs/validation/G07.md) · Patch 0042 · · **Reasoning effort: Extra High** · **Complexity: High**
- **Purpose:** Make D7 an observed guarantee within stated limits.
- **Prerequisites:** S07-T05.
- **Relevant files/subsystems:** Synthetic effects; checkpoint/journal fault harness; docs guarantee matrix.
- **Actions:** Run deliberate termination matrix covering every commit/apply/ack/checkpoint boundary and simultaneous client retries. Add representative durable NPC-death/state adapter from Stage 6; record permitted transient rollback and storage limitations.
- **Outputs:** G07 report with recovery matrix and durable-effect coverage. Plus the common task evidence record.
- **Acceptance:** No acknowledged tested critical outcome disappears or applies twice; world/clients converge; publication failure retains recovery path.
- **Validation:** Automated failpoints plus external forced process termination; distinguish process-crash tests from untested power-loss guarantees.
- **Risks/blockers:** Synthetic tests alone do not certify future inventory/quest adapters; those need Stage 8 crash tests.

#### S07-T07 — Respawn a dead player with a lootable corpse

- [x] **Status: Completed (2026-10-23)** · [Validation](docs/validation/S07-T07.md) · Patch 0042 · · **Reasoning effort: High** · **Complexity: High** · Added 2026-10-21 by user decision at G06 (decision 2)
- **Purpose:** A dead player can continue: respawn after a prompt while the old body stays in the world with its inventory, as the installed Azazel mode does.
- **Prerequisites:** S07-T02 (the respawn point comes from the latest checkpoint); S07-T03 (the death and respawn are critical outcomes).
- **Relevant files/subsystems:** `CActor::Die`, participant slots and reuse (S03-T03), the Azazel script for reference, `CoopClientLink.cpp` (prompt), protocol action, checkpoints.
- **Actions:** On death the client shows a respawn prompt; the respawn is an intent the server validates. The server spawns a new actor for the same participant slot at the participant's position in the latest checkpoint (save or autosave), or, with none since the player joined, at the last position the server tracked for the living actor. The old actor stays as a corpse with its inventory. Record death and respawn durably.
- **Outputs:** Respawn flow, protocol action, tests. Plus the common task evidence record.
- **Acceptance:** A dead player respawns only after confirming, at the declared point, with the corpse and its items still in the world on the server and both clients; a forged or early respawn is refused; death and respawn survive a server restart.
- **Validation:** Death, prompt, respawn, corpse check on both clients, nominal and impaired; restart between death and respawn.
- **Risks/blockers:** Looting the corpse needs inventory transfers (S08-T02); until then the corpse's items are visible but cannot be taken. The new actor carries the faction's new-game starting kit (user decision 2026-10-21).

#### S07-T08 — Publish checkpoints without stalling the simulation

- [x] **Status: Completed (2026-10-23)** · **Reasoning effort: High** · **Complexity: High** · Added 2026-10-23 by user decision at G07 ("there can't be any hitching") · [Validation](docs/validation/S07-T08.md) · Patch 0042
- **Purpose:** A checkpoint generation must not stall the server's tick: no visible hitch for the players.
- **Prerequisites:** S07-T02, S07-T06.
- **Relevant files/subsystems:** `CoopPersistence` (worker thread), `CALifeStorageManager::save` (a sink for the serialized world), `CheckpointStore::publish/recover` (an in-flight generation while the journal keeps the old file), `start_resync` (a join waits for the generation it requested).
- **Actions:** Measure where the save's time goes (Lua `.scoc`, serialization, compression, write, store steps). Keep only the serialization (and what the scripts write) on the simulation thread; compress, write and publish on a worker; the journal writer switches to the new generation's file when the publication completes, and records committed meanwhile stay in the previous journal, which recovery reads too. Joins stream the generation they requested once it is published.
- **Outputs:** Worker-thread publication, store and test changes, a tick-stall measurement. Plus the common task evidence record.
- **Acceptance:** The simulation thread's time per publication is reported and bounded by the serialization alone; recovery and the termination matrix's checkpoint points still pass with the asynchronous publication.
- **Validation:** rec-style restart runs and the `checkpoint:*` matrix points on the new build; the tick timing around publications.
- **Risks/blockers:** The native serialization itself is synchronous engine code; if it is tens of milliseconds, an incremental or copy-on-write serialization is a further task, reported here with numbers.

#### S07-T09 — Host-triggered checkpoint (optional, by user decision at G07)

- [ ] **Status: Not Started** · **Reasoning effort: Medium** · **Complexity: Moderate** · Added 2026-10-23 at the G07 acceptance ("I'll test first and leave that as an option")
- **Purpose:** Let the host choose when the remaining checkpoint pause happens (the world serialization, L34) instead of a timer: a quicksave-style save when ready.
- **Prerequisites:** G07 (accepted). Activated only if the play test shows a hitch or the user asks.
- **Relevant files/subsystems:** `CoopServerHost.cpp` (`request_publication`), a `checkpoint` action kind accepted from the host player only, `CoopClientLink.cpp` (the quicksave key, "Saving…/Saved" HUD message), `prepare-coop-session.py` (`--host-save`).
- **Actions:** `checkpoint_seconds` stays an optional safety timer (default 0). The host's quicksave key sends the action; the server refuses it from anyone else; the publication runs as today (worker thread); joins and the clean stop still publish automatically; the HUD confirms when the generation is published.
- **Outputs:** The action, the key binding, a driver option, one restart run from a manual generation. Plus the common task evidence record.
- **Acceptance:** A non-host request is refused; the host's save publishes a generation the restart recovers from; no periodic take when the timer is 0.
- **Validation:** A session with `checkpoint_seconds = 0`, two manual saves, a kill and restart.
- **Risks/blockers:** The rollback window becomes unbounded between manual saves (the journal still carries critical effects); record it as the user's choice.

### Stage 8 — Inventory and campaign synchronization

#### S08-T01 — Implement durable authoritative item pickup

- [x] **Status: Completed (2026-10-24)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S08-T01.md) · Patch 0045
- **Purpose:** Resolve the first real ownership conflict atomically.
- **Prerequisites:** G07.
- **Relevant files/subsystems:** InventoryOwner; ownership events; stable item IDs; journal adapters.
- **Actions:** Validate actor/range/level/alive/item availability/capacity. Commit one ownership transfer with preconditions and revision, then replicate one outcome. Persist item definition when necessary for post-checkpoint reconstruction.
- **Outputs:** Pickup transaction and concurrent-claim fixture. Plus the common task evidence record.
- **Acceptance:** Exactly one player wins competing pickup; one stable item has one owner before and after restart.
- **Validation:** Both clients request same item concurrently; retry same/different op IDs; kill before/after commit/ack.
- **Risks/blockers:** Native callbacks may independently grant/remove items; all effects must be contained in transaction adapter.

#### S08-T02 — Add transfers and ownership invariants

- [x] **Status: Completed (2026-10-24)** · **Reasoning effort: High** · **Complexity: High** · [Validation](docs/validation/S08-T02.md) · Patch 0046
- **Purpose:** Generalize one transaction without duplicating containers.
- **Prerequisites:** S08-T01.
- **Relevant files/subsystems:** Inventory containers; transfer/equip/ammo paths; journal outcomes.
- **Actions:** Implement bounded player/container transfer slice with source/destination versions, capacity and ownership checks. Define disconnect and conflicting operations; atomically persist all affected containers and unique-item constraints.
- **Outputs:** Transfer transaction and invariant checker. Plus the common task evidence record.
- **Acceptance:** No item exists in two owners or vanishes on a committed transfer; client mutations cannot bypass server checks.
- **Validation:** Concurrent pickup/transfer/equip, disconnect mid-operation and capacity/revision failures.
- **Risks/blockers:** Nested containers/ammo stacks require explicitly tested semantics; unsupported variants must fail clearly.

#### S08-T03 — Verify inventory recovery and reconnect

- [x] **Status: Completed (2026-10-24)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S08-T03.md) · Patch 0047
- **Purpose:** Close persistence gaps before adding quest rewards.
- **Prerequisites:** S08-T02.
- **Relevant files/subsystems:** Inventory adapters; S07 fault harness; client UI reconciliation.
- **Actions:** Test full transaction windows with duplicate/delayed messages, server termination and lost acknowledgements. Restore ownership graph and reconcile both inventories/UI from server state.
- **Outputs:** Inventory crash matrix and accepted durable schema. Plus the common task evidence record.
- **Acceptance:** All acknowledged ownership changes survive; retries return original outcomes; both clients agree after recovery.
- **Validation:** External kills and deterministic failpoints on pickup/transfers; compare canonical item graph hashes.
- **Risks/blockers:** A passing journal test is insufficient if native inventory callbacks leave unrecorded effects.

#### S08-T04 — Select and adapt one shared quest end to end

- [x] **Status: Completed (2026-10-25)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S08-T04.md) · Patch 0048
- **Purpose:** Bound native/Lua integration to a real campaign slice.
- **Prerequisites:** S08-T03.
- **Relevant files/subsystems:** E12; GameTask/GametaskManager; PhraseDialog/PhraseScript; installed quest scripts.
- **Actions:** Select one installed quest with observable prerequisites, objective contributions and rewards. Map native/Lua mutations and define global/party/player ownership and repeat-instance IDs. Route mutations through explicit server contexts and journal-ready resolved effects; clients present dialogue/task state.
- **Outputs:** Quest-specific authority adapters and coverage map. Plus the common task evidence record.
- **Acceptance:** Both players are eligible genuine contributors; no client callback can complete or reward the quest independently.
- **Validation:** Exercise prerequisites and actions from each participant; invalid client completion/script calls rejected.
- **Risks/blockers:** Quest may assume only one actor/info registry; split adapters by actual dependency rather than rewriting every script.
- **User decisions (2026-10-24, R09):** the quest is Sidorovich's repeatable fetch "Mobbing the Mob" (`esc_m_trader_task_3`, bring 6–12 `bandit_patch`) unless S08-T04 finds it unusable; patches in **either** player's inventory count and the turn-in takes them from both; the **money** is paid to each player for the patches taken from their own inventory (the total equals single-player); **both** players get the Loner goodwill (and the same side-faction changes); the possible bonus stash is **one** shared stash marked for both.

#### S08-T05 — Commit shared objectives and rewards once

- [x] **Status: Completed (2026-10-25)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S08-T05.md) · Patch 0048
- **Purpose:** Make quest completion atomic and conflict-safe.
- **Prerequisites:** S08-T04.
- **Relevant files/subsystems:** Quest adapters; inventory transactions; dialogue consequences; operation registry.
- **Actions:** Define canonical ordering for two contributions, simultaneous completion and NPC death during dialogue. Commit objective/completion/reward/unique-item effects as consistent bundles, with repeat-instance identity and explicit reward ownership policy.
- **Outputs:** Quest transaction implementation and race fixtures. Plus the common task evidence record.
- **Acceptance:** Both can advance same quest; completion/rewards occur once per intended instance; death invalidates pending dialogue when required.
- **Validation:** Simultaneous actions, repeated delivery, conflicting choices, disconnected contributor and quest-NPC death tests.
- **Risks/blockers:** Do not acknowledge quest completion before durable rewards/flags; content-specific rewards need documented policy.

#### S08-T06 — Qualify the durable shared quest slice

- [x] **Status: Completed (2026-10-25)** · **Reasoning effort: Extra High** · **Complexity: High** · [Validation](docs/validation/S08-T06.md) · Patch 0048
- **Purpose:** Demonstrate real campaign progress survives failures.
- **Prerequisites:** S08-T05.
- **Relevant files/subsystems:** Two-client quest fixture; native save/journal recovery; UI presentation.
- **Actions:** Run full quest from prerequisites through rewards with each player contributing. Restart at objective/completion/reward windows, reconnect clients and compare quest/inventory state. Record tested content and remaining unsupported systems.
- **Outputs:** G08 report and replayable quest test recipe. Plus the common task evidence record.
- **Acceptance:** Complete tested quest and inventory slice survive retries/reconnect/crash without duplicate completion or lost critical rewards.
- **Validation:** Two real clients plus failure harness; compare server canonical quest/item state and both views.
- **Risks/blockers:** One quest does not prove broad story compatibility; expansion remains Stage 10.

#### S08-T07 — Equip, use and consumption as server actions (added at G08)

- [x] **Status: Completed (2026-10-26)** · **Reasoning effort: High** · **Complexity: High** · Added 2026-10-25 at the G08 acceptance ("I accept as long as the limits will be solved"): owns L37, L39 (both closed) · [Validation](docs/validation/S08-T07.md) · Patch 0049
- **Purpose:** A client's slot, belt and ruck moves and its eating/using items (medkits, food, drugs) happen on the server, and consumption is durable.
- **Prerequisites:** G08 (accepted).
- **Relevant files/subsystems:** the client's `GEG_PLAYER_ITEM2SLOT/2BELT/2RUCK` and `GEG_PLAYER_ITEM_EAT` events (refused on a replica today, `CoopReplica.cpp`), the S08-T02 interception point (`xrServer::Process_event`), `CInventory`, `CEatableItem::UseBy`, the installed `actor_on_item_use` callbacks, the journal.
- **Actions:** Equip moves become a validated action (the item in the player's own inventory, the slot rules of `CInventory`); use/eat becomes an action applied by the engine on the server's actor (effects: health, satiety, radiation, the item consumed or a use spent) and committed as a record (the item's removal or remaining uses as absolute post-state), so a restart neither returns a used item nor loses an acknowledged one.
- **Outputs:** The actions, the adapters, a fixture (equip, eat, use a medkit; concurrent use and transfer of one item), crash runs. Plus the common task evidence record.
- **Acceptance:** A client's equip and use work and match the server; a used item stays used across every restart window; a transfer and a use of one item resolve to one outcome.
- **Validation:** Failpoints around the use record, an external kill, an impaired run; item graph comparison.
- **Risks/blockers:** Item use runs installed Lua (`actor_on_item_use`, Anomaly's item scripts) whose side effects need the same audit as the quest's.

#### S08-T08 — Durable new containers and stack semantics (added at G08)

- [x] **Status: Completed (2026-10-26)** · **Reasoning effort: High** · **Complexity: Moderate** · Added 2026-10-25 at the G08 acceptance: owns L38, L40 (both closed) · [Validation](docs/validation/S08-T08.md) · Patch 0050
- **Purpose:** Containers created after a checkpoint (a stash a script spawns, a body of an NPC that died since) keep their committed contents through a restart, and stacks and nested containers move as the engine's inventory allows.
- **Prerequisites:** G08 (accepted).
- **Relevant files/subsystems:** journal kinds `entity`/`tombstone` (contract §6), `recreate_pickup_item`, `transfer_container`, ammo stacks (`CWeaponAmmo`), backpacks in boxes, weapon addons.
- **Actions:** Give a container created after the checkpoint an `entity` record (its definition) when an item is first committed into it, so recovery re-creates the container before replaying its transfers; define and implement splitting and merging of ammo stacks and moving a container with contents; refuse unsupported variants with a named reason.
- **Outputs:** The records and adapters, a fixture (a post-checkpoint box, a corpse, partial stacks, a backpack in a box), crash runs. Plus the common task evidence record.
- **Acceptance:** After every restart window the committed contents are in their container (not on the ground); no stack is duplicated or lost.
- **Validation:** The S08-T02/T03 matrix with the new cases; item graph comparison.
- **Risks/blockers:** Bodies of NPCs that died after the checkpoint come back alive with the rollback unless their death is durable too (S07 records participant deaths; NPC deaths only partly).

#### S08-T09 — Party quest presentation and robustness (added at G08)

- [x] **Status: Completed (2026-10-26)** · **Reasoning effort: Medium** · **Complexity: Moderate** · Added 2026-10-25 at the G08 acceptance: owns L41, L42 (both closed) · [Validation](docs/validation/S08-T09.md) · Patch 0051
- **Purpose:** Both players see the party's task as single player shows it, and the party task survives its anchor's death.
- **Prerequisites:** G08 (accepted).
- **Relevant files/subsystems:** the presentation outbox (CampaignEvent 4), the client's PDA task list (`CGameTaskManager` presentation), map spots (`level.map_add_object_spot`, `treasure_manager`), `party_news`, `rebuild_party_tasks`, the respawn path (S07-T07).
- **Actions:** Present the party task (title, description, objective count, state) in both players' PDA task lists; replicate the map spots the server adds for a player (the bonus stash's); queue news for a player who is away and deliver it on the rejoin; define what happens to an active party task when participant 1's actor dies and respawns (the task moves with the participant to the new actor, its portable store copied) and test it.
- **Outputs:** Presentation records and client handling, the anchor-death rule and fixture. Plus the common task evidence record.
- **Acceptance:** Both clients list the party task with its count; both see the stash's map spot; a rejoined player gets the news he missed; the task completes after its anchor's respawn.
- **Validation:** Two-client runs with a disconnect and a scripted death of participant 1 mid-quest; a restart.
- **Risks/blockers:** The native PDA expects tasks it owns; the presentation may need a client-side task object that never runs Lua.

### Stage 9 — Coordinated same-level transitions

#### S09-T01 — Implement request and two-player consent

- [x] **Status: Completed** · **Reasoning effort: High** · **Complexity: High** · [S09-T01](docs/validation/S09-T01.md) (2026-10-27)
- **Purpose:** Make party travel explicit and server-controlled.
- **Prerequisites:** G08.
- **Relevant files/subsystems:** E11; level changer interaction; session/party state; transition protocol.
- **Actions:** Define transition ID, destination preconditions and state machine. Server validates request, notifies both and requires both confirmations with expiry. Specify refusal, cancellation, combat restrictions and disconnect policy without automatic teleport.
- **Outputs:** Transition contract and consent handling. Plus the common task evidence record.
- **Acceptance:** No load begins from one player intent alone; duplicate/stale confirmations cannot approve a different transition.
- **Validation:** Both accept, one refuses, timeout, disconnect and replayed confirmation scenarios.
- **Risks/blockers:** Policy cannot silently substitute one-player consent after disconnect; changed policy needs user approval.

#### S09-T02 — Prepare authoritative travel state and load destination

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High** · [S09-T02](docs/validation/S09-T02.md) (2026-10-28)
- **Purpose:** Preserve one canonical world through native level migration.
- **Prerequisites:** S09-T01.
- **Relevant files/subsystems:** alife_update_manager::change_level; native save; checkpoint/journal coordinator.
- **Actions:** Quiesce or order gameplay at a transition barrier; durably record source recovery generation and participant/entity travel state. Adapt native change_level for both identities and one online world; prepare destination state with explicit commit point.
- **Outputs:** Server travel coordinator and source/destination recovery records. Plus the common task evidence record.
- **Acceptance:** No mutation is lost across barrier; both actors and offline ALife map consistently to prepared destination.
- **Validation:** Native transition fixture with two actors, inventory/quest state and queued conflicting actions.
- **Risks/blockers:** Native one-actor migration and save hooks may assume global Actor(); define rollback before commit and roll-forward after it.

#### S09-T03 — Synchronize both clients before resuming

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High** · [S09-T03](docs/validation/S09-T03.md) (2026-10-28)
- **Purpose:** Prevent gameplay against mixed level states.
- **Prerequisites:** S09-T02.
- **Relevant files/subsystems:** Client loader; transition prepare/ready/commit messages; snapshot/resync.
- **Actions:** Send destination/content generation; load both clients and deliver authoritative snapshots. Require validated readiness for the same transition/revision before resuming; reject pre-transition inputs and reset prediction/baselines.
- **Outputs:** Coordinated load/resume path. Plus the common task evidence record.
- **Acceptance:** Both clients resume in same level with canonical actors/items/quests; neither can act against old level state.
- **Validation:** Different client load durations, delayed ready/commit, packet duplication and wrong destination revision.
- **Risks/blockers:** UI readiness alone is insufficient; snapshot application and identity binding must be acknowledged.

#### S09-T04 — Handle cancellation, failed load and interrupted travel

- [x] **Status: Completed** · **Reasoning effort: Extra High** · **Complexity: High** · [S09-T04](docs/validation/S09-T04.md) (2026-10-28)
- **Purpose:** Make travel recoverable rather than a permanent split.
- **Prerequisites:** S09-T03.
- **Relevant files/subsystems:** Transition state machine; disconnect manager; recovery coordinator.
- **Actions:** Implement documented precommit cancellation/rollback and postcommit destination recovery. Define timeout behavior, disconnected participant pending state and reconnect rejoin; never let clients resume different levels.
- **Outputs:** Failure policy implementation and G09 report. Plus the common task evidence record.
- **Acceptance:** Failure leaves a recoverable consistent source or destination; no unilateral unconfirmed teleport or lost durable progress.
- **Validation:** Kill server at each transition state; fail one client load; disconnect before/after consent/commit; retry and recover.
- **Risks/blockers:** Long load can hold world paused; timeout policy must be explicit and tested, not silently degrade consent rules.

### Stage 10 — Broader simulation compatibility

#### S10-T01 — Build a bounded campaign coverage matrix

- [x] **Status: Completed (2026-10-28)** · **Reasoning effort: High** · **Complexity: Moderate** · [Validation](docs/validation/S10-T01.md) · [Coverage matrix](docs/campaign-coverage-matrix.md) · [Stage 10 decisions](docs/stage10-decisions.md) (T01-1…T01-19, **approved by the user 2026-10-28**)
- **Purpose:** Expand by evidence rather than claiming universal script support.
- **Prerequisites:** G09.
- **Relevant files/subsystems:** Installed scripts/configs; E01–E12; prior special-actor audit.
- **Actions:** Inventory systems and representative content families: stalkers, mutant abilities, offline ALife, dialogue, factions, traders/stashes, time/emissions and story chains. Rank remaining global-Actor mutations; create fixture-specific child tasks before broad implementation.
- **Outputs:** Coverage matrix with stable child IDs, required release scope and exclusions. Plus the common task evidence record.
- **Acceptance:** Every targeted family has owner/context policy, test fixture and acceptance evidence requirement; untested content is labeled.
- **Validation:** Targeted source/script search using existing references; no whole-binary analysis or indiscriminate extraction.
- **Risks/blockers:** Large campaign scope may need many sessions; this task plans batches rather than claiming to fix all content.

#### S10-T02 — Expand NPC and mutant authoritative behavior

- [x] **Status: Completed** (S10-T02a Completed 2026-10-29: [NPC logic per participant](docs/validation/S10-T02a.md), patch 0070; S10-T02b Completed 2026-10-29: [NPC loot and looting](docs/validation/S10-T02b.md), patch 0071; S10-T02c Completed 2026-10-30: [controller and psy auras](docs/validation/S10-T02c.md), patch 0072; S10-T02d Completed 2026-10-30: [bloodsucker, burer, poltergeist](docs/validation/S10-T02d.md), patch 0073; S10-T02e Completed 2026-10-30: [generic mutants, phantoms, AI lighting](docs/validation/S10-T02e.md), patch 0074; S10-T02f Completed 2026-10-30: [companions per recruiter](docs/validation/S10-T02f.md), patch 0075; all six children done) · **Reasoning effort: Extra High** · **Complexity: High; execute as fixture-sized children**
- **Purpose:** Remove remaining participant-specific combat assumptions.
- **Prerequisites:** S10-T01.
- **Relevant files/subsystems:** AI stalker/base monster; poltergeist_telekinesis; anti_aim_ability; monster_aura; E08.
- **Actions:** For each selected family, execute separately scoped child tasks adapting targeting, special attacks, animation hit logic and death/loot outcomes for both actors. Preserve server decisions and relevant critical persistence.
- **Outputs:** Per-family changes/tests and coverage entries. Plus the common task evidence record.
- **Acceptance:** Each required family targets either participant and converges under reconnect; untested abilities remain explicitly blocked from release claims.
- **Validation:** Two-position actor fixtures, special attacks, AI authority rejection and restart around death/loot.
- **Risks/blockers:** Global Actor() specials can bypass generic enemy manager; never infer all mutants pass from one NPC test.
- **Children (S10-T01, 2026-10-28):** S10-T02a NPC logic per participant (condlists, meet, danger, combat-ignore, disguise; L11 NPC part; **Completed 2026-10-29**, patch 0070); S10-T02b NPC loot and looting as server transfers (**Completed 2026-10-29**, patch 0071); S10-T02c controller and psy auras (**Completed 2026-10-30**, patch 0072); S10-T02d bloodsucker, burer, poltergeist abilities (**Completed 2026-10-30**, patch 0073); S10-T02e generic mutants, phantoms and AI lighting (L12, L13); S10-T02f companions per recruiter (**Completed 2026-10-30**, patch 0075). See the [coverage matrix](docs/campaign-coverage-matrix.md#4-child-tasks).

#### S10-T03 — Expand offline ALife, spawning and world time

- [x] **Status: Completed** (S10-T03a Completed 2026-10-30: [ALife switching and respawn](docs/validation/S10-T03a.md), patch 0076; S10-T03b Completed 2026-10-30: [offline combat and squad targets](docs/validation/S10-T03b.md), patch 0077; S10-T03c Completed 2026-10-30: [world time and sleep](docs/validation/S10-T03c.md), patch 0078; S10-T03d Completed 2026-10-30: [guides, fast travel, backpack](docs/validation/S10-T03d.md), patch 0079; S10-T03e Completed 2026-10-31: [passage sweep](docs/validation/S10-T03e.md), patch 0080; S10-T03f Completed 2026-10-31: [new game](docs/validation/S10-T03f.md), patch 0081; all six children done) · **Reasoning effort: Extra High** · **Complexity: High**
- **Purpose:** Keep one online world consistent with persistent offline simulation.
- **Prerequisites:** S10-T01.
- **Relevant files/subsystems:** ALife scheduler/registries; online/offline switching; spawn/despawn; time services.
- **Actions:** Adapt participant-union activation across travel/disconnect; synchronize canonical spawn/death identities and offline updates. Define persistent time/random-state requirements and emission-relevant scheduling; test identity reuse and checkpoint boundaries.
- **Outputs:** ALife/time adapters and offline-online fixtures. Plus the common task evidence record.
- **Acceptance:** No duplicate spawn or resurrection on offline/online transitions; both players observe one canonical time/world state.
- **Validation:** Leave/reenter activation zones, travel and restart with offline entities; compare canonical identities and outcomes.
- **Risks/blockers:** Offline simulation uses engine assumptions beyond actor distance; performance scales with union of active areas.
- **Children (S10-T01, 2026-10-28):** S10-T03a ALife switching and respawn across travel, disconnect and restart (L53 watch; **Completed 2026-10-30**, patch 0076); S10-T03b offline combat and squad targets (**Completed 2026-10-30**, patch 0077); S10-T03c world time and sleep (L49; T01-8; **Completed 2026-10-30**, patch 0078); S10-T03d guides, fast travel and backpack travel as party travel (T01-9; **Completed 2026-10-30**, patch 0079); S10-T03e passage sweep over every level changer (L50; T01-1; **Completed 2026-10-31**, patch 0080, L50 closed); S10-T03f new game on the campaign server (L51; if T01-2 is approved; **Completed 2026-10-31**, patch 0081, L51 closed).

#### S10-T04 — Expand dialogue, factions and economy transactions

- [x] **Status: Completed** (all six children done 2026-10-29; S10-T04a Completed 2026-10-29: [the talk window](docs/validation/S10-T04a.md), patch 0058, L43 closed; S10-T04b Completed 2026-10-29: [party info portions and actor-2 script state](docs/validation/S10-T04b.md), patch 0059, L44 closed, L11/L32 narrowed; S10-T05b, reordered before S10-T04c by the user on 2026-10-29, Completed 2026-10-29: [every quest is a party quest](docs/validation/S10-T05b.md), patch 0060, L10 narrowed; S10-T04e Completed 2026-10-29: [stashes and treasure](docs/validation/S10-T04e.md), patch 0069; S10-T04d Completed 2026-10-29: [factions and reputation per player](docs/validation/S10-T04d.md), patch 0068; S10-T04f Completed 2026-10-29: [upgrades at mechanics](docs/validation/S10-T04f.md), patch 0067, protocol v1.8; S10-T04c Completed 2026-10-29: [trade, repair and the use of world objects](docs/validation/S10-T04c.md), patches 0063–0066, protocol v1.7, L48 closed: the door (use-object + joint drive), the purse and the native trade window with server prices run (trade-2 pass); the repair condition to the client and the hands-on trade check remain; upgrades split to S10-T04f (T04c-3)) · **Reasoning effort: Extra High** · **Complexity: High; execute as subsystem-sized children**
- **Purpose:** Cover shared consequences beyond the first quest.
- **Prerequisites:** S10-T01.
- **Relevant files/subsystems:** PhraseDialog/PhraseScript; reputation; traders; stashes; inventory journal.
- **Actions:** Use separate content-sized child tasks for dialogue locks/cancellation, faction/reputation scope, trader stock/money and stash/unique ownership. Resolve effects on server and commit multi-entity economy/progression bundles. First child (owns L43, added at G08): route a client's talk window to the server's dialogue, so a player reaches the S08 party quest's accept and turn-in (kind 8) by talking to the giver.
- **Outputs:** Context/persistence adapters and conflict tests per selected subsystem. Plus the common task evidence record.
- **Acceptance:** Required consequences are shared or player-specific as explicitly defined; no duplicate money/items or conflicting dialogue commits.
- **Validation:** Concurrent purchase/dialogue, NPC death, reconnect and crash before/after durable success.
- **Risks/blockers:** Reputation and rewards may be per-player in some content; choose and document policies before migration.
- **Children (S10-T01, 2026-10-28):** S10-T04a the talk window (L43, first child as decided at G08); S10-T04b party info portions and actor-2 script state (L11 info part, L32, L44; T01-4); S10-T04c trade, repair and the use of world objects (L01 trade, L36 trade, L48; T01-7; **Completed 2026-10-29**, patches 0063–0066, L48 closed); S10-T04f upgrades at mechanics (split from S10-T04c by the user, T04c-3; **Completed 2026-10-29**, patch 0067, protocol v1.8, L01 narrowed, L58 added); S10-T04d factions and reputation per player (T01-6; **Completed 2026-10-29**, patch 0068, L59 added); S10-T04e stashes and treasure (T01-14; **Completed 2026-10-29**, patch 0069).

#### S10-T05 — Expand emissions and shared story progression

- [x] **Status: Completed** (2026-10-31; all four children: S10-T05d Completed 2026-10-31: [presentation split](docs/validation/S10-T05d.md), patches 0084–0085; S10-T05a Completed 2026-10-31: [emission and psi-storm per player](docs/validation/S10-T05a.md), patch 0082; S10-T05c Completed 2026-10-31: [storyline chains](docs/validation/S10-T05c.md), patch 0083; S10-T05b Completed 2026-10-29: [every quest is a party quest](docs/validation/S10-T05b.md), patch 0060) · **Reasoning effort: Extra High** · **Complexity: High; execute as story/event-sized children**
- **Purpose:** Prove campaign systems work beyond a single side quest.
- **Prerequisites:** S10-T02; S10-T03; S10-T04.
- **Relevant files/subsystems:** World event/emission scripts; story flags; native tasks; campaign matrix.
- **Actions:** Implement selected world-event and story-chain child slices with explicit participant safety/progression context. Test prerequisites, repeatable tasks, time-driven events and rewards across travel and recovery.
- **Outputs:** Story/event coverage evidence and G10 report. Plus the common task evidence record.
- **Acceptance:** Every declared release-required family/chain passes its matrix; both participants can contribute; unsupported content is not represented as compatible.
- **Validation:** Two-client chained playthrough fixtures, emission with separated same-level actors, repeated tasks and crash/reconnect checkpoints.
- **Risks/blockers:** Content coverage may reveal new authority paths; expand bounded tasks without bypassing G07/G08 guarantees.
- **Children (S10-T01, 2026-10-28):** S10-T05a emission and psi-storm gameplay per participant (L28, L29; T01-11; **Completed 2026-10-31**, patch 0082); S10-T05b dynamic task types under the party policy (L10; **Completed 2026-10-29**: [every quest is a party quest](docs/validation/S10-T05b.md), patch 0060, decisions T05b-1…T05b-5); S10-T05c storyline chains (DoctorX, Living Legend; T01-16; **Completed 2026-10-31**, patch 0083); S10-T05d presentation split: per-player news, PDA, HUD scripts, statistics (L05, L06, L33, L47; split into d1 presentation traffic and the live loops at a join (patch 0084), d2 HUD statics and statistics (patch 0085), d3 body presentation (B4, B7, L47 routed to S10-T07); **Completed 2026-10-31**).
- **Child added 2026-10-28 by user direction (S10-T06, owner: the Stage 10 parent tasks' gate G10):** co-op menu and session control — a client's Co-op page with Create game (the standard creation screen), Load game and Join session as owner-only session-control commands to the dedicated server, the joiner inheriting faction and game type; decisions T01-17…T01-19 in [Stage 10 decisions](docs/stage10-decisions.md). Prerequisite S10-T03f.

#### S10-T06 — Co-op menu and session control
- [x] **Status: Completed** (2026-10-31: [S10-T06](docs/validation/S10-T06.md), patch 0086 `session-control`, proto-build-56, campaign-410 / client-296, session-2 pass 8/8) · **Reasoning effort: Extra High** · **Complexity: High** · Added 2026-10-28 by user direction (decisions T01-17…T01-19).
- **Purpose:** Let the players start a new campaign or load another one from a client, with the standard creation screen, without touching the server machine.
- **Prerequisites:** S10-T03f (the server's new game).
- **Relevant files/subsystems:** `protocol/include/coop/proto/messages.h` (action kind, reasons); `CoopServerHost.cpp` (owner, campaign library, create/load, relaunch); `x_ray.cpp` (the relaunch exit code); `CoopClientLink.cpp` and the client's Lua layer over the installed `ui_main_menu.script`, `ui_mm_faction_select.script`, `ui_load_dialog.script`; `tools/Run-CoopSession.ps1`, a server launcher.
- **Children:** **a** protocol: action kind 27 (session: list, create in parts, load, loadout), reasons 43–46, CampaignEvent 20 (list, switch); **b** the server: the owner (the first key a fresh server accepts, T01-18), the campaign library under its app data, a create (the creator's choices checked by the creation screen's rules, a new campaign profile with `new_game = 1`) or a load carried out as a clean stop and a relaunch on the chosen campaign (exit code 75; the launcher restarts it), both clients told the new campaign id; **c** the client: New game opens the standard creation screen and its Start becomes the create (the owner) or the player's own loadout for the next new game (the partner, T01-19); Load lists the server's campaigns in the standard load window; the client follows a switch and reconnects; **d** a fixture: the owner creates a new game, then loads the first campaign back; the partner's create is kept as its loadout and its load refused.
- **Acceptance:** a two-client run switches campaigns twice from the owner's client (new game, then load back) with both clients in each world; the partner's commands are refused or kept as its loadout; nothing a client sends gives items, money or a faction the creation screen would not.
- **Validation:** `tools/compare-coop-session.py`; protocol tests (proto-build-56).

#### S10-T07 — Bug-squashing pass before Stage 11
- [ ] **Status: In Progress** (2026-10-31: part 1, the rows that need no input, complete: [S10-T07](docs/validation/S10-T07.md), patch 0087, B6/B8/B10/B12/B13/B16/B17/B18/B19 fixed; 2026-11-01: part 2, the user's rows, in code: B2, B4, B7, B21, B22, B23 fixed, patch 0088; 2026-11-01 hands-on demo-1: PDA, task updates and trade pass, B4, B22, B23 fail; 2026-11-02: part 3, their real causes fixed (a replica's path speed, the client's jump after a correction's hop, the server's campfire turn gated on renderer flags), patch 0089, builds campaign-422 / client-305; hands-on demo-3 2026-11-02: jump fine, frame rate fine, but the user wants all fires lit (T07-10 to record), NPCs now walk backwards, the trader portrait still missing; part 4 in code and built (patch 0090, campaign-424 / client-307): a replica's body yaw set and no longer turned by its own look code (B4), every campfire burns with near-only shadows on a client (B23, decision T07-10), a hidden trade partner panel shown with a `panel=` probe (B1); hands-on demo-4: B1 fixed, NPCs twisted at the hip, still a stall near a fire; part 5 (patch 0091, campaign-426 / client-309): a replica runs no own sight action (B4), no campfire shadow by default with console settings and a frame trace (B23); hands-on demo-5: B4 and B23 fixed, one client crash (B24: a part's blend list overflowed) fixed in code (patch 0092, campaign-427 / client-310), not run yet; 2026-11-02: the G10 review's nine revisions in code (part 7, patch 0093, campaign-429 / client-311; bugfix-17 sleep and bugfix-18 party task pass; B25 donate-as-sale fixed in code); next: the user's hands-on session, then G10) · **Reasoning effort: Extra High** · **Complexity: High** · Added 2026-10-30 by user direction: a major bug-fixing stage right before the jump to Stage 11.
- **Purpose:** Fix the defects collected in the [bug-squashing list](docs/known-issues.md) (B1…) during Stage 10 before release qualification starts.
- **Prerequisites:** S10-T02 to S10-T06 complete; the user's G10 decision review (revisions from it are implemented here too).
- **Relevant files/subsystems:** Every subsystem named in the list's rows; `docs/known-issues.md` K-entries.
- **Actions:** Confirm each open row with the user where details are missing (B2), reproduce it in a session run or a hands-on demo, fix it as its own patch, and mark the row fixed with the patch and run. A row not fixed is either waived by the user or carried as a limit in `docs/carried-limits.md`.
- **Outputs:** Fixes, reruns and an updated list. Plus the common task evidence record.
- **Acceptance:** No row left open without a user waiver or a carried-limit row; the Stage 10 session fixtures still pass after the fixes.
- **Validation:** Each fix's reproduction rerun before and after; a closing two-client hands-on demo by the user.
- **Risks/blockers:** Some rows (B4 NPC movement presentation, B7 zoom) overlap S10-T05d; do them once, wherever they land first.

### Stage 11 — Reliability, performance and release validation

#### S11-T00 — Local hosting and the host and join menu
- [ ] **Status: In Progress** (2026-11-02: built and run on one PC: [S11-T00](docs/validation/S11-T00.md), patches 0095 `local-hosting` and 0096 `dedicated-server`, proto-build-63 st-38, campaign-442 / client-324; a player who hosts (menu-7, kit-9) and a dedicated server both players join (menu-10, kit-8) pass; test kits in `builds/kits/`; waiting on the user's two-PC test over Tailscale; moved from S10-T08 to Stage 11 by the user on Discord: "Exactly after you reach a point where the only thing left is for me to verify the G10 changes, I want you to start building the network infrastructure to host the server locally in g11"; G10's revisions are in code and wait only on the user's verification; the user on Discord, 2026-11-02: "move onto stage 11 while I'm gone, we're going to take tailscale approach of hosting the server locally. Both players will connect to the server and then join the session using a code": the host runs the server on its own PC, players reach it over Tailscale and join with the host's code; Stage 11 work that needs no hands-on may run alongside G10's pending hands-on by this direction, G10 still closes only on the user's word) · **Reasoning effort: Extra High** · **Complexity: High** · Added 2026-11-02 by user direction ("we will need a join ui menu that covers hosting and joining and integrates with the stalker new game menu"; the user's choices: a Stage 10 task before G10, and invites by swapping codes in the menu).
- **Purpose:** Let players host and join a co-op campaign from Anomaly's own main menu, with no launch profile, key tool or roster file edited by hand.
- **Prerequisites:** S10-T06 (the co-op menu: New game and Load routed to the server, the owner), S10-T07 part 7; runs alongside G10's pending hands-on by the user's direction (G10 still closes only on the user's word).
- **Relevant files/subsystems:** the client's Lua layer over `ui_main_menu.script` and `ui_mm_faction_select.script` (`CoopClientLink.cpp` `session_menu_install`); the client's start and connect (`coop_client_app_start`, its profile); `protocol/tools/coop_keytool.cpp` and the session layer's roster (`docs/session-authentication.md`); the campaign server launcher (`tools/Run-CoopSession.ps1`'s relaunch loop, exit code 75); `x_ray.cpp` start-up.
- **Actions:** A Co-op entry in the main menu with **Host** and **Join**. Host: pick New game (the standard creation screen, story mode only) or a saved campaign, start the campaign server on this PC in the background, become its owner, show the address and port to give the friend, and an **Add player** box that takes a friend's player code (the roster line) and lets that player in. Join: create the player's key on first use (DPAPI, as `coop-keytool new-player`), show **your player code** to send to the host, take the host's address, connect and enter the world; reconnect and the in-game pause menu's leave as today. Record the menu's gameplay choices as decision rows for G10.
- **Outputs:** The menu, the host-side server launch and roster editing, the client's key creation and connect from the menu, a hands-on guide. Plus the common task evidence record.
- **Acceptance:** On one PC, a host started from the menu and a second client that joins from the menu by code and address play a session (new game, then a load) with no file edited by hand; a client with an unknown code is refused with a message; single player is unchanged.
- **Validation:** A two-client run driven through the menu's functions (a fixture), a hands-on session by the user, and the protocol tests if the session layer changes.
- **Risks/blockers:** A server started by a client must outlive the menu and stop cleanly; the friend's machine needs the same build and game files (the build and content identity); reaching a host over the internet still needs a forwarded UDP port or a VPN (the network test, independent).

#### S11-T01 — Run integrated adverse-network qualification

- [ ] **Status: Not Started** · **Reasoning effort: High** · **Complexity: High**
- **Purpose:** Verify interactions under realistic network failures.
- **Prerequisites:** G10.
- **Relevant files/subsystems:** Two-client automation; transport fault injection; protocol/replication.
- **Actions:** Define supported latency/loss/reordering/bandwidth profiles and acceptance limits. Run movement, combat, quest, inventory and travel scenarios together; include bounded rate/queue abuse and malformed input.
- **Outputs:** Network qualification matrix and defects with reproduction commands. Plus the common task evidence record.
- **Acceptance:** All supported profiles meet declared convergence/playability limits; no authority violation or unbounded backlog.
- **Validation:** Automated network faults plus real client observations; compare final authoritative state and latency/error metrics.
- **Risks/blockers:** Profiles are release claims only after measurement; local-loopback-only evidence is insufficient.

#### S11-T02 — Run integrated crash and recovery qualification

- [ ] **Status: Not Started** · **Reasoning effort: Extra High** · **Complexity: High**
- **Purpose:** Validate durability across subsystem boundaries.
- **Prerequisites:** S11-T01.
- **Relevant files/subsystems:** Checkpoint/journal harness; quest/economy/travel operations; reconnect.
- **Actions:** Expand forced server/client termination and lost-ack/duplicate cases across all required durable adapters. Exercise corrupt latest checkpoint fallback, failed writes, incompatible version and repeated restarts.
- **Outputs:** Recovery qualification matrix and documented rollback guarantees. Plus the common task evidence record.
- **Acceptance:** No acknowledged covered critical outcome is lost/duplicated; clients converge or stop explicitly on unrecoverable incompatibility.
- **Validation:** Failpoints plus external process kills with independent state invariant checks.
- **Risks/blockers:** Do not claim power-loss durability or zero data loss beyond observed storage guarantees.

#### S11-T03 — Measure long-session stability and resource budgets

- [ ] **Status: Not Started** · **Reasoning effort: High** · **Complexity: High**
- **Purpose:** Make performance claims on declared hardware.
- **Prerequisites:** S11-T02.
- **Relevant files/subsystems:** Dedicated profiler/counters; Windows runtime; coverage fixtures.
- **Actions:** Set hardware/content/network profiles and acceptance budgets before soak. Measure CPU, memory, tick overruns, journal/checkpoint latency and bandwidth through extended combat/travel/reconnect sessions; fix measured bottlenecks only.
- **Outputs:** Soak logs, resource report and reproducible performance scenarios. Plus the common task evidence record.
- **Acceptance:** No unbounded memory/queue/journal growth; declared tick and interaction budgets hold for required workload and soak duration.
- **Validation:** Recorded long gameplay runs and before/after profiles for fixes; repeat only affected scenarios.
- **Risks/blockers:** No defensible budget exists yet; define duration and thresholds from measured target environment, not invented universal limits.

#### S11-T04 — Qualify single-player and matched-build regression

- [ ] **Status: Not Started** · **Reasoning effort: High** · **Complexity: High**
- **Purpose:** Preserve original mode and prevent configuration drift.
- **Prerequisites:** S11-T03.
- **Relevant files/subsystems:** Accepted SP fixtures; client/server build manifest; installed content.
- **Actions:** Run Stage 1 compatibility suite against final SP configuration and matched coop products. Recheck assets/Lua/quests/save/load and native movement/combat after shared engine changes; classify deliberate coop-only differences.
- **Outputs:** SP regression matrix and matched-build identity report. Plus the common task evidence record.
- **Acceptance:** Mandatory SP fixtures pass; matched products use identical accepted engine/protocol/content revisions.
- **Validation:** Clean builds and representative SP plus coop runtime regression; inspect configuration guards.
- **Risks/blockers:** Headless/authority changes can leak into SP through shared globals and macros.

#### S11-T05 — Package a reproducible distribution and verify clean setup

- [ ] **Status: Not Started** · **Reasoning effort: High** · **Complexity: High**
- **Purpose:** Make tested binaries installable without proprietary content or original edits.
- **Prerequisites:** S11-T04.
- **Relevant files/subsystems:** Build automation; manifests; license/dependency inventory; planned release staging.
- **Actions:** Document clean Windows build and separate-install deployment; package matched products/configs with hashes and licenses, exclude game archives/secrets/Ghidra databases. Test installation and rollback in a fresh workspace profile, then review all gates and limitations.
- **Outputs:** Release candidate artifacts, setup/rollback guide and G11 acceptance report. Plus the common task evidence record.
- **Acceptance:** Fresh setup reproduces two-client session and recovery smoke; distribution contains only permitted project/dependency artifacts; all release claims have evidence.
- **Validation:** Clean-environment build/install/run verification and archive manifest inspection; no install into original.
- **Risks/blockers:** Publication, original-directory installation or architecture/scope expansion require separate authorization; packaging is not permission to publish.

## D. Stage completion gates

Every gate requires **all tasks and required child tasks in its stage** to pass, evidence linked in the progress ledger, and no unresolved defect that invalidates the stage's guarantee. A blocked gate blocks downstream implementation even if some code compiles. Historical evidence can satisfy investigation prerequisites; it cannot substitute for the new runtime checks below. A later regression reopens the affected gate and invalidates dependent release claims until repaired.

| Gate | Required observed evidence before the next stage |
|---|---|
| G00 | Resolved safe paths and output routes; existing changes/repositories preserved; source/artifact ownership documented; saved Ghidra verification and artifacts accounted for; Windows access and baseline fixture requirements recorded. Missing build tools may be a Stage 1 blocker, but must be explicit. |
| G01 | Pinned **unmodified** source compiles with recorded Windows toolchain/dependencies and repeatable commands. Separate test installation runs representative asset, Lua, gameplay and save/load fixtures with no unexplained critical compatibility defect. **2026-10-17 user exception:** completed-quest/reward and copied original-client comparison are waived for Stage 2 entry, recorded as untested risks in [G01](docs/validation/G01.md). Mere launch or matching strings is insufficient. |
| G02 | Dedicated process loads and advances selected native campaign fixture with no external client and no GPU initialization/draw dependency. Physics/collision, CPU bones, object updates, scheduler, AI, ALife, world time, relevant script/task execution and native save/reload are observed. No permanent artificial global player; hidden rendering window does not pass. **2026-10-18 user acceptance:** accepted for development with the recorded limits and B21 carried into Stage 3 as named risks; Stage 3 engine changes rerun the GameState smoke and a regression reopens G02. See [G02](docs/validation/G02.md). |
| G03 | Two independent genuine actor identities, conditions/equipment/input contexts; explicit native/Lua participant access; server gameplay independent of client view actor; basic AI and activation work near either actor; remote paths cannot access private state-writer privileges. |
| G04 | Isolated GNS test passes on accepted toolchain; adapter, bounded/versioned codec, authenticated two-player sessions, rejection tests, liveness and in-memory reconnect work. No implication of crash persistence yet. |
| G05 | One dedicated process and two real matched clients run simultaneously. Each sees both actors and the same server-controlled entity changes. Initial snapshot, lifecycle, late join and rejoin converge; no client owns shared simulation. |
| G06 | Native server movement, validated input, snapshots, interpolation/reconciliation and one NPC combat encounter converge on both clients under declared network conditions. Forged movement/damage outcomes are rejected. Rates and limitations measured. |
| G07 | Consistent native-plus-supplemental checkpoints, stable identities, journal outcomes and epoch fencing restore tested critical effects. Forced termination around commit/apply/ack/publication shows no acknowledged tested critical effect lost or duplicated. Clients discard stale state and converge; transient rollback and untested storage failure guarantees are explicit. |
| G08 | Concurrent item pickup/transfers preserve one ownership graph. Both players contribute to one complete shared quest; completion and reward effects commit once. Actual inventory and quest adapters pass duplicate, lost-ack, disconnect and crash/recovery tests. |
| G09 | Both explicitly confirm travel, authoritative mutation barrier and migration preserve state, both load/synchronize before resume. Refusal, cancellation, timeout, failed load, client disconnect and server crash leave a recoverable consistent source or destination. |
| G10 | Declared required NPC/mutant, offline ALife, dialogue/economy/reputation, world-event and story slices pass their coverage matrix and durable-effect tests. Required child tasks complete; untested content is clearly excluded, not implicitly compatible. The S10-T07 bug-squashing pass is complete (added 2026-10-30 by the user). |
| G11 | Integrated network/crash/duplicate/conflict/transition matrices pass; measured long-session resource budgets hold; SP regression passes; clean matched builds and fresh separate-install distribution smoke succeed. Release scope and known limitations match evidence. |

Stage 2 may need a minimal real campaign actor/context to exercise native systems; **no connected human client** does not mean **no actor entity**. Explicit context plumbing needed for the headless fixture may move forward from Stage 3 with recorded evidence. This does not authorize a permanent global placeholder, skipping G02, or implementing full multiplayer early. If correct native initialization cannot meet D1/D2 without a prohibited workaround, stop at that blocker and request an architecture decision with evidence.

## E. Dependencies and critical path

The main critical path is:

`S00-T01 → G00 → G01 → G02 → G03 → G04 → G05 → G06 → G07 → G08 → G09 → G10 → G11`

Within a stage, the prerequisites on each task define the graph; all tasks collectively satisfy its gate. Useful sibling work after shared prerequisites includes S02-T03/T04 (CPU model services and frame driver), S06-T03/T04 (reconciliation and the combat slice), S07-T02/T03 (checkpoint and journal modules), and S10-T02/T03/T04 (bounded content families). These are dependency opportunities, not instructions to spawn agents or start simultaneous risky modifications. Keep integration/tests sequential where shared world or files require it.

Critical uncertainties and their earliest resolving tasks:

1. **Source/content compatibility — S01-T01–T05.** Current partial binary/source correspondence cannot prove compatibility. Failed mandatory baseline tests prevent multiplayer changes; avoid building a large fork before resolving them.
2. **CPU gameplay extraction and actor-independent boot — S02-T01–T06.** GPU model ownership, render-driven ballistics and macro-disabled campaign services are known source blockers. If native simulation cannot run correctly without prohibited global-player scaffolding, G02 stays blocked.
3. **Explicit two-participant semantics — S03-T01–T05.** Reused remote actors must retain campaign capabilities. ALife activation and scripts must not silently choose the first player.
4. **Transport/toolchain fit — S04-T01.** Provisional selection is not proof of an integrable Windows dependency. Complete the isolated test before embedding it throughout networking code.
5. **Recoverable critical effects — S07-T01–T06.** Native save completeness, supplemental state and journal apply semantics determine whether progression can be durable. Inventory/quest adapters must not get ahead of G07.
6. **Campaign side-effect coverage — S08 and S10.** An undocumented native/Lua mutation can bypass both authority and durability. Expand by tested slices, not by assuming one successful quest covers all campaign scripts.
7. **Consistent travel — S09.** Native one-actor change-level and world globals need two-participant migration and failure fencing. Multi-level online simulation is not a fallback within approved scope.

Do not schedule downstream implementation as if these gates have already passed. A source-location mismatch alone calls for bounded investigation, not a new architecture. For an actual impossible or materially different dependency, record the failure, proposed options, impact on gates and D1–D8, then seek approval before changing an accepted decision. Move only evidenced prerequisites; retain original task IDs and add `superseded by` links where necessary.

## F. Flexible session execution strategy

At the start of a development session:

1. Read `AGENTS.md`, this plan's summary/current gate, and [session handoff](docs/handoffs/current/session-handoff.md). Then read only evidence and source needed for the selected task. Confirm workspace/repository state before editing; preserve pre-existing changes.
2. Pick a clear primary objective from the next unblocked stage. State its acceptance test and required inputs. Verify that implementation has been authorized for the requested milestone; this planning run itself changes no engine code.
3. Use existing source references and saved Ghidra evidence. Inspect targeted functions if line mappings changed or a specific uncertainty matters. Do not repeat whole-binary analysis or reopen persistence work merely because a new session started.
4. Establish the smallest executable fixture that can resolve the task. Run isolated transport/storage/service checks before major integration. Keep native gameplay behavior rather than replacing difficult dependencies with unverified no-ops.

During a session, complete multiple related tasks when their prerequisites pass, or split a milestone across sessions. Investigate newly discovered dependencies, refactor code directly needed by the objective and run additional correctness tests as warranted. Task IDs are continuity tools, not rigid session boundaries. Large content families in Stage 10 must be split into explicitly tracked fixture-sized child tasks before implementation; do not let those parent tasks become unbounded work.

Do not skip gates, start unrelated stages without a documented dependency, reopen approved architecture without new evidence, or perform speculative cleanup. Avoid unnecessary repeated builds after unchanged passing tests; rerun affected checks after relevant code/content/configuration changes. New tool/library documentation should be consulted when implementation needs it, with actual versions pinned; this plan does not assert current library API details.

End a session after a coherent milestone, a necessary focus change, or when context grows unwieldy. Before stopping, update task statuses, gate evidence and the handoff. A blocked task needs the smallest next diagnostic/action, not another broad investigation. Preserve an incomplete but reproducible working state; never mark an untested change completed to tidy the ledger. Do not install into the original directory or publish artifacts without separate explicit authorization.

## G. Persistent session handoff

`docs/handoffs/current/session-handoff.md` is the short entry point for the next session; this plan remains the authoritative roadmap. It is the only file in `docs/handoffs/current/`. Before it is rewritten, the old version is copied to `docs/handoffs/done/snapshot-before-<what-comes-next>.md` with `## Complete` as its first line; snapshots are history only. Update both at every future implementation-session end, including blocked sessions. The handoff must contain:

- Current milestone/gate, completed task IDs and any in-progress or blocked task IDs.
- Files changed, root/engine revisions or reproducible patch locations, and pre-existing changes to preserve.
- Confirmed results and exact build/test command or concise validation-report link, including failures and unrun checks.
- Current blockers and their evidence; source functions/revision or binary hash/RVA when relevant.
- Remaining work and the next recommended task with a concrete first action.
- Any proposed ordering/scope changes; approved D1–D8 remain binding unless user approval is recorded.

Use links to `docs/validation/<ID>.md` rather than pasting long logs. Record where ignored binaries/logs/fixtures live, and retain sanitized evidence sufficient to distinguish observed results from claims. Keep completed history in task reports/progress ledger so the handoff stays short. During future implementation, update legacy `docs/implementation-roadmap.md` only as needed to link current evidence per `AGENTS.md`; do not maintain contradictory milestone orders. This planning execution intentionally preserves it unchanged.

## H. Decision and risk register

Accepted choices are not proof of working capabilities. Limits carried by accepted gates are tracked separately in [carried limits](docs/carried-limits.md). The following register identifies implementation choices and approval boundaries; normal choices within D1–D8 can be made and documented without repeatedly asking permission.

| ID / priority | Unresolved question or risk | Recommended next action / decision owner | Gate and approval boundary |
|---|---|---|---|
| R01 / Critical | Candidate source may not run installed 1.5.3 content; toolchain unavailable. | Engineer: pin candidate/toolchain, use representative real fixtures and record incompatibilities. Prefer a compatible source revision over speculative patches to a mismatched fork. | G01. A proposal to waive unmodified-client/runtime validation or change target platform/content baseline requires user approval. |
| R02 / Critical | Renderer supplies CPU assets; flags remove campaign; local actor assumptions obstruct boot. | Engineer: preserve native CPU services and introduce explicit context and frame ownership. Use hidden window only to diagnose. | G02/G03. Permanent artificial actor, GPU-required final server, independent simulation rewrite or service split changes D1/D2/D8 and requires approval. |
| R03 / High | Ignored source repository or dirty workspace could lose implementation history. | Engineer: maintain explicit working-fork/patch ownership and source manifest before editing. Never reset existing changes. | G00 and every session. Destructive cleanup or database overwrite is not authorized. |
| R04 / High | Installed Lua and native tasks use global player state. | Engineer: audit fixture-specific mutations; explicit actor/party contexts and resolved authoritative effects; expand coverage in bounded batches. | G03/G08/G10. Reducing Player 2 to a companion or replacing campaign mode changes D4 and requires approval. |
| R05 / High | GNS compiler/dependency/deployment constraints. | Engineer: isolated pinned build, two-peer test and deployment requirements. Reuse serializers selectively; adapter protects engine from library choice. | G04. Evidence-backed transport replacement needs approval of D6 amendment; do not silently implement custom UDP. |
| R06 / High | Initial authentication and connectivity expectations unspecified. | Engineer: document supported deployment and authenticated identity binding before accepting real sessions; use maintained transport security facilities where appropriate, keep credentials private. | G04/G11. Paid/hosted identity/relay services or expanded public service deployment need explicit scope approval. No claim of internet reachability based only on LAN tests. |
| R07 / Critical | Native checkpoint omits critical participant/script state; replay repeats effects. | Engineer: contents matrix, supplemental records, resolved effect journal, idempotent adapters and fail-closed durable commit; retain prior generation. | G07/G08. Weakening acknowledged critical progression durability requires approval; no silent loss or deterministic replay promise. |
| R08 / High | Storage choice, flush/atomic rename guarantees and compaction need validation on Windows. | Engineer: select smallest in-process durable journal implementation with tested crash semantics; record process-crash versus power-loss limits, checksums and schema compatibility. | G07/G11. Separate persistence service changes D8; unsupported zero-loss claims are prohibited. |
| R09 / High | Shared rewards, faction/reputation and conflicting dialogue policies vary by content. | Engineer: classify globally shared, party-shared, authoritative per-player and client-local state. Recommend once-per-party quest completion with explicit content-specific reward allocation; keep identity/health/inventory per player. | G08/G10. Present materially gameplay-changing reward/story policies for user approval before implementing them; do not conflate approval of shared campaign with approval of every reward rule. |
| R10 / High | Travel failure can strand clients or discard progress. | Engineer: both-player confirmation, durable transition identity/barrier, rollback before commit and destination recovery after commit; reconnect must sync before playing. | G09. One-player consent, automatic teleport or independent online levels changes D5 and requires approval. |
| R11 / High | View-driven optimization and participant-union activation increase CPU load. | Engineer: measure representative worlds; preserve gameplay work and tune scheduling/interest rather than silently disabling simulation. Select network rates from evidence. | G02/G06/G11. Simulation-fidelity reduction or multi-process expansion that changes D1/D8 requires approval. |
| R12 / High | A successful slice may hide unsupported campaign branches. | Engineer: publish required content coverage matrix and explicit exclusions; add child tasks before new systems. | G10/G11. Materially reducing promised release campaign scope requires user agreement; one quest is a milestone, not the final objective. |
| R13 / Medium | Existing binary and accepted source may drift further. | Engineer: record accepted SHA and remap evidence selectively. Keep reference binary and Ghidra project identities distinct from source-built binaries. | All gates. New binary versions get separate hash-keyed databases only when necessary; never overwrite saved analysis. |
| R14 / High | Core physics/hit logic diverges under prediction or lag. | Engineer: authoritative native movement/combat and measured correction errors; start without rewind, add bounded historical collision only if evidence warrants a separate task. | G06/G11. Do not accept client damage as a shortcut; major changes to gameplay fidelity require review. |

Architecture fallback comparison remains the existing [architecture report](docs/architecture.md): an independent backend reconstructs native simulation at substantial fidelity/maintenance cost; a separate-service hybrid adds IPC and recovery complexity without removing headless blockers. **Neither is a parallel implementation track.** Approved D1/D8 selects the single dedicated native runtime. Revisit only if a new concrete blocker establishes why the chosen approach cannot meet requirements.

## I. Progress tracking

Statuses are exactly **Not Started**, **In Progress**, **Blocked**, and **Completed**. Keep task checkboxes unchecked for the first three; check only after acceptance and validation evidence exist. Mark gates separately; checking all tasks without their behavioral evidence does not pass a gate. When findings invalidate a completed task, reopen it and explain why. Do not renumber existing IDs; use suffix IDs for scoped children and list their dependencies.

| Record | Current state | Evidence / next action |
|---|---|---|
| Planning deliverables | Completed | This roadmap and the session handoff (`docs/handoffs/`); documentation-only execution. |
| Prior architectural investigation | Existing evidence available | E01–E12 and linked reports; not a runtime implementation gate. |
| Prior persistent Ghidra save/reopen | Existing verification available | `reverse-engineering/persistence-verification.json`; preserve and reuse. |
| Stage 0 tasks | Completed | S00-T01–T03; see [G00](docs/validation/G00.md). |
| S01-T01 | Completed | [Pinned installed toolchain and 33 evaluated projects](docs/validation/S01-T01.md). |
| S01-T02 | Completed | [79 independent content files verified; output aliases isolated](docs/validation/S01-T02.md). |
| S01-T03 | Completed | [Two clean builds and artifact comparison](docs/validation/S01-T03.md); user-tested third build identity matched source output. |
| S01-T04 | Completed | [Observed gameplay and explicit untested quest/reward and original-client comparison](docs/validation/S01-T04.md). |
| S01-T05 | Completed | [Frozen development baseline](docs/validation/S01-T05.md) and [G01 acceptance](docs/validation/G01.md). |
| G00 / G01 | Completed | [Readiness](docs/validation/G00.md); [scoped development baseline acceptance](docs/validation/G01.md). G02–G06 Completed (own rows below; [G06](docs/validation/G06.md) accepted 2026-10-21); G07 ready for acceptance (row below); G08–G11 Not Started. |
| S02-T01 | Completed | [Source map and pinned fixture specification](docs/validation/S02-T01.md); static verification passed, no engine changes or runtime test. |
| S02-T02 | Completed | [Context and services validation](docs/validation/S02-T02-context.md): Core/Services pass, unwind checked; H02 construction moved to S02-T05/T06 by user decision. |
| S02-T03 | Completed | Split into T03a–d, all Completed: [T03a](docs/validation/S02-T03a.md), [T03b](docs/validation/S02-T03b.md), [T03c bones](docs/validation/S02-T03c.md), [T03d collision](docs/validation/S02-T03d.md) (structure identical to the client on the same save). |
| S02-T04 | Completed | Split into T04a–c, all Completed: [T04a clock/frame driver](docs/validation/S02-T04a.md), [T04b native physics stepping, falling body](docs/validation/S02-T04b.md) (server and client step the same drop), [T04c objects, AI, ALife, activity policy](docs/validation/S02-T04c.md) (server and client pass the same observe-and-walk fixture; the actor's Lua binder finding goes to S02-T06). |
| S02-T05 | Completed | Split into T05a–d, all Completed: [T05a persistent](docs/validation/S02-T05a.md), [T05b level/server/ALife](docs/validation/S02-T05b.md), [T05c CPU sound events and AI hearing](docs/validation/S02-T05c.md) (server and client hear the same footsteps), [T05d level load, spawn, first ticks](docs/validation/S02-T05d.md). |
| S02-T06 | Completed | Split into T06a–d, all Completed ([report](docs/validation/S02-T06.md)): T06a actor Lua binder and inert game UI (B20 decided and resolved; patches 0015, 0016); T06b ALife actor position (patch 0017); T06c combat between NPCs (patch 0018; server and client fight the same spawned boar and match); T06d native checkpoint round-trip (patch 0019; the server saves, restarts from the checkpoint and gets its state back, and the client loads the same checkpoint and matches); T06e a quest across two checkpoints (patch 0020; given, carried, completed with rewards, carried again, on both products). |
| G02 | Completed | [Accepted for development 2026-10-18](docs/validation/G02.md) with the recorded limits carried into Stage 3; each limit has a Stage 3 handling rule and a later owner. |
| S03-T01 | Completed | [Identity, engine-id mapping and participant registry](docs/validation/S03-T01.md) (patch 0021): unit fixtures 111 checks pass; level identity check and GameState smoke pass (smoke-103). The hearing-stage variation's cause is recorded as an S03-T04 input. |
| S03-T02 | Completed | [Local view vs gameplay authority](docs/validation/S03-T02.md) (patch 0022): explicit actor access adapters, the [call-site inventory](docs/actor-access-inventory.md), the server message policy; unit fixtures 53 + 259 checks and the level authority check 63 checks pass; GameState smoke passes (smoke-106). |
| S03-T03 | Completed | [Two independent native actors](docs/validation/S03-T03.md) (patch 0023): per-participant control (`controls_actor`, the participant input context, explicit actor assignment), the single-player binder rule, the spawn state-frame resync, 23 view and 14 script-binding guards; the two-actor level fixture passes 116 checks (smoke-117, all 62 GameState stages); the installed scripts' single-actor gates (`se_actor` state read, hit callback, `db.actor`) are recorded for S03-T05. Next: S03-T04. |
| S03-T04 | Completed | [Participant-aware ALife activation](docs/validation/S03-T04.md) (patch 0024): the activation set behind the five switch rules, the awareness conversions, the ability no-ops and guards (poltergeist, vampire and alien added by the `Actor()->` sweep); the activation fixture passes 35 checks (smoke-128, 129, 132; all 63 GameState stages); the union costs +44 online objects and +0.17 ms object update per tick at 1,098 m. Open: server AI vision has no light. Next: S03-T05. |
| S03-T05 | Completed | [Native and Lua participant context contract](docs/validation/S03-T05.md) (patch 0025): participant script contexts entered only by native entry points (dialogue, phrases, use, owned tasks, captured deferred work), strict `campaign.*` Lua lookups, the server's compatibility layer (`db.actor`/`AC_ID` per context, out-of-context reads audited: 320,729 from 167 lines), presentation outboxes; the script-context fixtures pass 62 unit and 101 level checks (smoke-133, smoke-136, all 64 GameState stages): both participants run the installed mechanic quest concurrently with rewards on their own actors; client-29 quest capture matches. G03 accepted 2026-10-19. |
| S03-T06 | Completed | [Server light model for AI vision](docs/validation/S03-T06.md) (patch 0026): per-participant CPU light tracking (actor + one view object per tick), CPU environment blend, level lights as CPU objects; lighting fixture 14 checks (smoke-141, 144; all 66 GameState stages); server = client 0.0213 at steady state (capture-22); tutorials became a presentation and S03-T04's awareness phase moved actor 1 to a quiet spot after lit vision exposed both. G03 accepted 2026-10-19. |
| G03 | Completed | [Accepted for development 2026-10-19](docs/validation/G03.md): S03-T01..T06 Completed; carried limits named with their owners. |
| S04-T01 | Completed | [GameNetworkingSockets in isolation](docs/validation/S04-T01.md) (no engine patch): pinned v1.6.1 + protobuf 3.21.12 + BCrypt, built with the Stage 1 compiler (net-build-7); 7 probe scenarios and GNS's `test_crypto`/`test_connection` pass (net-8), loopback on one host only. Findings for S04-T02/T05: dead-peer detection needs reliable traffic, a dead session holds its slot until timeout, the per-connection send buffer is shared by critical and transient data, reliable order is per lane, Windows power throttling breaks `Sleep(1)` poll loops. |
| S04-T02 | Completed | [Bounded transport adapter](docs/validation/S04-T02.md) (no engine patch): `protocol/` `coop::net::Transport` with GNS and fake backends, owner-thread-only delivery, per-class overload policy; tr-4 51/51 (25 scenarios × fake/GNS loopback, 7 race tests, 6 policy tests), repeated. Findings: GNS impairments survive Kill/Init; closing a listen socket drops its connections without a close packet (handled); critical producers must pace below the send rate. |
| S04-T03 | Completed | [Protocol codec v1](docs/validation/S04-T03.md) (no engine patch): [wire format](docs/network-protocol.md#wire-format-v1), bounded decoder, canonical encoder, per-connection guard; pt-3 20/20 incl. 2.0 M guarded fuzz mutations; tr-7 51/51. Content-digest coverage, auth proof and rate limits passed to S04-T04; entity generation mapping to S05-T02. |
| S04-T04 | Completed | [Authenticated sessions](docs/validation/S04-T04.md) (no engine patch), [design](docs/session-authentication.md): ECDSA P-256 challenge-response, DPAPI key files, public-key roster, player-owned slots, replacement on reconnect, rate limits; pt-6 43/43, tr-8 51/51. Limit: active network MITM (R06, G11). Build/content digest computation to S05-T01. |
| S04-T05 | Completed | [Liveness and in-memory reconnect](docs/validation/S04-T05.md) (no engine patch): heartbeats, suspect/timeout, rotated-token resume, stale-connection fencing, shutdown; pt-9 61/61, pt-10/11, tr-9 51/51. |
| G04 | Completed | [Accepted for development 2026-10-20](docs/validation/G04.md): S04-T01..T05 Completed; carried: loopback only, active-MITM limit (G11/R06), no sanitizer, engine integration (S05-T01). |
| S05-T01 | Completed | [Matched client and dedicated roles](docs/validation/S05-T01.md) (patch 0027, protocol v1.1): serve mode, client link, role guard, identity checks; coop-9 three processes, injection 3/3 refused; smoke-146 and capture-25 regressions pass. |
| S05-T02 | Completed | [Lifecycle and initial world snapshot](docs/validation/S05-T02.md) (patch 0028): checkpoint join stream, Spawn/Despawn with commit ids, client replica guards; world-7 three replicas match the server; smoke-149 and capture-27 regressions pass (smoke-148: the known intermittent walk stop). |
| S05-T03 | Completed | [Participant views](docs/validation/S05-T03.md) (patch 0029): own view per client, input to the server, actor snapshots, world clock and weather; view-11 ownership and presentation match; smoke-150 and capture-28 regressions pass. |
| S05-T04 | Completed | [Bounded shared-world session](docs/validation/S05-T04.md) (patch 0030): session-2 two joins, late join, rejoin, 26 server-owned medkit spawns/releases; three replicas match the server; own-only walks ×3; rejoin input fix; smoke-152 and capture-30 regressions pass. G05 recorded. |
| G05 | Completed | [Accepted for development 2026-10-20](docs/validation/G05.md): S05-T01..T04 Completed; carried limits tracked in [carried limits](docs/carried-limits.md) (L01–L27). Next: S06-T01. |
| S06-T01 | Completed | [Validated movement input](docs/validation/S06-T01.md) (patch 0031, proto-build-18): per-player input queue (tick sequencing, validation, one input per tick); move-8 all 24 checks pass (duplicates, shifted counter, replay, invalid stance, teleport intent, forged snapshot, two-actor collision); fixed two actors pinning each other; session-3, smoke-153, capture-31 pass. L03 closed. Next: S06-T02. |
| S06-T02 | Completed | [Snapshots, interpolation and baselines](docs/validation/S06-T02.md) (patch 0032, proto-build-26): protocol v1.2 deltas against acknowledged baselines, adaptive budget, 87/87 protocol and 51/51 transport tests; snap-11/snap-12 presentation checks pass (nominal; 5 % loss, 40 ms lag); NPCs kept remote and animated on clients; session-6, smoke-155, capture-34 pass; move-12 24/24, move-13/14 22–23/24 (Fanat pushing the standing actor). L04, L25 closed. Next: S06-T03. |
| S06-T03 | Completed | [Own-actor prediction](docs/validation/S06-T03.md) (patch 0033, proto-build-29): native prediction and reconciliation, 95/95 protocol tests; pred-8/pred-9 all checks pass (wrong predictions converge; 83–100 ms vs 250 ms response); aim replicated; HUD arm snap fixed (measured 6.6 M → 0 rad/s, user confirmed); session-8, snap-14, smoke-156, capture-35 pass. L02 closed. |
| S06-T04 | Completed | [Weapon/combat slice](docs/validation/S06-T04.md) (patch 0034, proto-build-30): fire as input, activate-slot and reload actions, outcomes replicated (v1.4), participant hit rule (`friendly_fire`, default on); 96/96 protocol tests; combat-13 and combat-12 (5 % loss, 40 ms) pass all 21 checks, agreement 99.4–99.7 %; session-10, snap-16, pred-11, smoke-160/161, capture-37 pass. Next: S06-T05. |
| S06-T05 | Completed | [NPC encounter and deaths](docs/validation/S06-T05.md) (patch 0035): native participant deaths on the server, native death presentation on clients, the dead take no input, no level change (enc-9 crash fixed); enc-15/19 (nominal), enc-17 (5 % / 40 ms), enc-18 (10 % / 100 ms) pass all 13 checks, agreement ≥ 99.9 %, deaths shown within 120 ms, 50 Hz, 10–19 KB/s per client; combat-16, session-12, snap-18, smoke-163, capture-42/43 pass; capture harness "Press any key" timeout fixed. G06 accepted 2026-10-21. |
| G06 | Completed | [Accepted 2026-10-21](docs/validation/G06.md) with five user decisions: friendly fire stays on; respawn after a prompt with a lootable corpse (S07-T07); Azazel/Ironman unavailable in co-op; tests move to Cordon without Fanat (S06-T06); storm and emission visuals soon (S06-T08). Jump and lean added (S06-T07). |
| G07 | Completed | [Accepted 2026-10-23](docs/validation/G07.md): every Stage 7 task Completed under patch 0042 (campaign-238 / client-138, proto-build-37); the user's three decisions (no stall → S07-T08; respawn at the level entry point; the faction kit) implemented and re-validated; L15 closed for Stage 7's parts, L32–L36 added (L34 revised). The user play-tests the remaining checkpoint pause and keeps a host-triggered save as an option (S07-T09, optional). Next: S08-T01. |
| G08 | Completed | [Accepted 2026-10-25](docs/validation/G08.md) ("I accept as long as the limits will be solved"): every Stage 8 task S08-T01…T06 Completed under patches 0045–0048 (campaign-267 / client-167, proto-build-42); the new limits are L37–L43, each owned by a task that solves it: S08-T07 (equip/use, consumption), S08-T08 (new containers, stacks), S08-T09 (quest presentation, anchor death), S10-T04 (talk window). Stage 8 stays In Progress until S08-T07…T09 are Completed. 2026-10-26: S08-T07…T09 Completed (patches 0049–0051), L37–L42 closed; the [condition review](docs/validation/G08.md#condition-review--ready-for-the-users-confirmation-2026-10-26) and the [gameplay decisions](docs/stage8-closing-decisions.md) wait for the user's confirmation. **Condition confirmed 2026-10-27** ("they all work You can close the gate"): decisions approved, L44–L49 added, Stage 8 Completed. |
| G09 | Completed | [Accepted 2026-10-28](docs/validation/G09.md) ("ok that sounds good I accept the gate"): every Stage 9 task S09-T01…T04 Completed under patches 0054–0057 (campaign-305 / client-202, proto-build-48); both players confirm, the barrier and the migration preserve state (item layout hash unchanged across every travel), both load before the resume, and refusal, cancellation, timeout, failed load, client disconnect and server crash each leave a recoverable source or destination (tv-3…tv-26). Decisions T01-1…T04-6 approved as listed ([Stage 9 decisions](docs/stage9-decisions.md)); L14 closed; new limits L50–L56. Two passages travelled; a new game is L51. |
| S06-T06 | Completed | [Cordon fixture](docs/validation/S06-T06.md) (patch 0036, proto-build-31): user-made Cordon save ([record](docs/cordon-fixture.json)); no fixture names Fanat; Cordon's weak boar; fixed: server crash on blood marks (no wallmark engine), join race at 10 % loss (other channels wait for the client's first in-session message), a reused id with the same section kept a stale replica object; cenc-19/20 (nominal), cenc-21 (5 % / 40 ms), cenc-22 (10 % / 100 ms) pass all checks; ccombat-5, cpred-5, csession-5, csnap-5 pass in Cordon; smoke-168, capture-52/53 pass; 98/98 protocol tests. Next: S06-T07. |
| S06-T07 | Completed | Jump and lean as server input (added at G06). Part 1 done ([report](docs/validation/S06-T07.md), patch 0037): windowed co-op clients take the mouse only on a click inside, so a window can be dragged by its title bar; checked with `tools/Test-WindowCapture.ps1` (window-capture-3). Part 2 done (patch 0038, after a user demo): a player's own weapon switch and reload reach the server, the other player's drawn weapon shows, both players start holstered. Part 3 (0039): remote items presentation only; the knife glitch is logged as [K1](docs/known-issues.md). Part 4 (0040, protocol v1.5): jump and lean reach the server (act-5). Final co-op regression on campaign-217 passes (tenc-1/2, tcombat-1, tpred-1, tsession-1, tsnap-1); smoke-169, capture-56 (checkpoint) and capture-59 (combat) pass. Next: S06-T08. |
| S06-T08 | Completed | [Storm and emission visuals](docs/validation/S06-T08.md) (patch 0041, campaign-222/client-124): the server sends its environment (cycle, the two blended descriptors, the weather effect and its time left), its thunderbolts and the scripts' camera/post-process effectors, sounds and particles (participant and broadcast outboxes); clients blend the same descriptors from the shared clock, play the server's bolts and the emission's phases. cweather-2 (nominal), cweather-3 (5 % loss, 40 ms) and cweather-4 (final build) pass every check on both clients: descriptors equal, weight within 0.001, effect start within 92 ms, bolts within 291 ms, records within 443 ms, effectors in step. L09 closed; L28/L29 added (actor-relative presentation and the emission's gameplay use participant 1's actor: S10-T05). Fixed on the way: a backward clock step ended the effect on clients; events were queued before a client was in the world. Next: S07-T01. |
| S07-T01 | Completed | [Persistence contract](docs/persistence-contract.md) and [report](docs/validation/S07-T01.md) (2026-10-22): guarantee matrix, ids, transaction phases, publication, journal, recovery, resync, respawn, synthetic ledger fixture, 17 crash boundaries on paper. Specification only. |
| S07-T02 | Completed | [Checkpoint generations](docs/validation/S07-T02.md) (2026-10-23, patch 0042, proto-build-35): the store library (`coop_store`: durable file primitives with failpoints, `CheckpointStore::publish/recover`, supplemental and manifest codecs) passes 9 store tests incl. the 8-point `TerminateProcess` matrix (st-2, 107/107); the server publishes a generation (native save under the store's temporary name + supplemental record, verify/flush/rename/manifest/rotation/pointer/retention) at every join, every `checkpoint_seconds` and at a clean stop; a recovered two-actor checkpoint continues every object's EntityId and each saved actor is assigned to its participant by its saved id (rec-2, rec-3). Limit: publication stalls the tick 20–620 ms. |
| S07-T03 | Completed | [Durable journal](docs/validation/S07-T03.md) (2026-10-23, patch 0042): write-through append-only journal per generation with torn-tail repair and corruption refusal; the epoch record at every start; the synthetic `ledger` action (kind 3) through receive/validate/resolve/durable commit/apply/acknowledge with the operation index and fail-closed reason 5; client program 6; `coop-ledger.csv`; st-2 journal tests, rec-2 (160 operations) and rec-3 (183 + 172). |
| S07-T04 | Completed | [Recovery](docs/validation/S07-T04.md) (2026-10-23, patch 0042): recovery at prepare (pointer → verified generation, else G−1, journals read, epoch allocated durably, refusals with named reasons, never the base save when generations exist), identity restore before the load, actor assignment after it (stage `coop-server-recovered`), replay of committed records at start; rec-2 (restart from generation 9, replay 0) and rec-3 (restart from generation 5 with 50 journal records replayed: money 1509/495 restored exactly, both actors reassigned, 26,172 entities continued). |
| S07-T05 | Completed | [Resync after restart](docs/validation/S07-T05.md) (2026-10-23, patch 0042): new epoch per incarnation; on a connection end the client leaves its stale level, keeps pending operations and reconnects with backoff; rec-3: both clients rejoined under epoch 2 in 2 attempts, got a fresh world, every pending operation accepted, replica sets equal to the server's (one K4 item aside). |
| S07-T06 | Completed | [Qualification](docs/validation/S07-T06.md) (2026-10-23, patch 0042): the termination matrix at `commit:before-append/-after-flush/-after-apply/-after-ack`, the five `checkpoint:*` steps and an injected append failure (mx1 10/10 on campaign-230, the fail-closed point re-run as mx2-10 on campaign-233 after a result-dispatch defect was fixed): no acknowledged ledger effect lost or duplicated, retries answered from the index where durable and afresh otherwise, publication failures keep the previous generation; the `death` record (every `CEntityAlive::Die`, replayed by killing the entity natively) is the representative durable Stage 6 outcome (death-2). Permitted rollback and storage limits recorded (L34–L36). [G07 report](docs/validation/G07.md) ready for acceptance. |
| S07-T07 | Completed | [Respawn](docs/validation/S07-T07.md) (2026-10-23, patch 0042; revised the same day under G07 decisions 2 and 3, campaign-238 / client-138): a dead player is prompted (HUD message, use key or the `auto_respawn` driver), the respawn is action kind 4 validated by the server (alive → refused), resolved to the participant's **level entry point** (recorded in supplemental schema 2) with a reserved EntityId, committed as a `respawn` record before the spawn, applied (a genuine actor bound to the slot with the dead actor's community, name, rank, reputation and money and the **faction kit**; the corpse stays with its inventory) and followed by a rejoin event; death-2/3/4: the server terminated right after the apply, the restart replayed the death and the respawn, the client rejoined as the new actor with the corpse on both clients. Limits: corpse not lootable until S08-T02; the fixture's entry point is next to the death spots. |
| S07-T08 | Completed | [No-stall publication](docs/validation/S07-T08.md) (2026-10-23, patch 0042, proto-build-37, campaign-238 / client-138): the save's compression, file write and every store step run on a worker thread; the simulation thread keeps the world serialization (45–65 ms, up to 129 ms under load); on the client the update gap at a checkpoint fell from 183–367 ms to about 100 ms (rec-4); recovery (rec-4, death-3/4) and the five `checkpoint:*` termination points (mx3 5/5) pass with the asynchronous publication. L34 revised. |
| S08-T01 | Completed | [Durable pickup](docs/validation/S08-T01.md) (2026-10-24, patch 0045, protocol v1.6 / proto-build-39, campaign-250 / client-152): action kind 5 validated on the server's state (alive, online unowned item, `CanTakeItem`, the engine's reach), committed as a `pickup` record (absolute post-state + the item's spawn state) before the engine's ownership event applies it and before the acknowledgement; `Reparent` replicates every owner change; recovery re-applies or re-creates the item under its EntityId. Race fixture with the real use key on one side: one winner in every round (most in the same server tick), same-id repeats get the original outcome, new ids are refused; the crash matrix (before-append fail-closed, after-flush, after-apply, after-ack) and 5 %/40 ms pass. Limits: drops/consumption after a pickup roll back (S08-T02); item script callbacks unaudited. |
| S08-T02 | Completed | [Transfers and invariants](docs/validation/S08-T02.md) (2026-10-24, patch 0046, proto-build-41, campaign-256 / client-156): drops (kind 6) and moves between a player and an inventory box or a dead body (kind 7) are validated, durable `transfer` records, made from the client's own inventory events (every UI and script path); the item must be where the client saw it and unchanged since its lifecycle cursor (A-B-A refused `Stale`); an ownership-invariant checker over every ALife object runs at recovery, checkpoints and the end. Fixed on the way: **EntityIds were issued again after a restart** (now reserved durably in blocks). Crash matrix, impaired, disconnect-mid-operation and concurrent take/equip pass. Limits: equip/use stay refused on clients; a container created after the checkpoint is not durable (its contents land on the ground). |
| S08-T03 | Completed | [Inventory recovery](docs/validation/S08-T03.md) (2026-10-24, patch 0047, campaign-259 / client-159): a canonical item graph (every published item's owner) hashed by the server every 10 s and compared by each client at the same point of the lifecycle stream, plus each client's own inventory against its replica; external kills at 95/100/125 s, failpoints, 10 %/100 ms. Fixed: a rejoining client's entity table kept old tombstones and dropped despawns (the K4 rec-3 root cause); a negative wait at recovery. After the fixes every compared point matched. |
| S08-T04 | Completed | [Shared quest](docs/validation/S08-T04.md) (2026-10-25, patch 0048, proto-build-42): Sidorovich's "Mobbing the Mob" as one party task through its installed dialogue functions on the server (action kind 8: accept / turn in); a server-only Lua adapter pools both players' patches and resolves the rewards (each paid for his own patches, goodwill to both, one stash: the user's policy); the task lives on participant 1's actor (durable script state); a client's local completion attempt changes nothing. |
| S08-T05 | Completed | [Commit once](docs/validation/S08-T05.md) (2026-10-25): accept and completion answered only after a published generation holds them (contract §8a); simultaneous turn-ins join one completion; a disconnected contributor is counted and paid; a dead giver is refused. |
| S08-T06 | Completed | [Qualification](docs/validation/S08-T06.md) (2026-10-25, campaign-267 / client-167): the quest end to end and restarts after the accept, after the completion's effects, after the publication, mid-collection and finished after recovery; fixed: tasks loaded from a checkpoint could not complete with two actors. |
| S08-T07 | Completed | [Equip, use, consumption](docs/validation/S08-T07.md) (2026-10-26, patch 0049, proto-build-43, campaign-276 / client-175): equip (kind 9, `equip` record before the apply) and use (kind 10: the installed before-use check and `CInventory::Eat` on the server in the user's context, the resolved `use` record before the answer) from every client path; layouts replicated (CampaignEvent 9) and checked at every item-graph point; the booster memory per player; a used item stays used through every restart window (eq-7…eq-12), also after its pickup (L39). L37, L39 closed. |
| S08-T08 | Completed | [Containers and stacks](docs/validation/S08-T08.md) (2026-10-26, patch 0050, proto-build-44, campaign-282 / client-181): `entity` records re-create post-checkpoint boxes and bodies; server-made inventory changes observed per tick and journaled (`pickup`/`transfer`/`tombstone`); ammunition merges on the server (`stack`, CampaignEvent 10); client retries for late Reparents and amounts; two S08-T07 use-record defects fixed. L38, L40 closed. |
| S08-T09 | Completed | [Party presentation, anchor death](docs/validation/S08-T09.md) (2026-10-26, patch 0051, campaign-286 / client-185): CampaignEvent 11 party tasks and logic-free client task copies; script map spots to both players and to joiners; news replayed on a rejoin; the turn-in runs in participant 1's live body after a respawn; restart after participant 1's respawn fixed. L41, L42 closed. |
| S09-T01 | Completed | [Travel request and two-player consent](docs/validation/S09-T01.md) (2026-10-27, patch 0054, proto-build-45, campaign-298 / client-195): [travel contract](docs/travel-contract.md); transition ids never reused or continued across a restart; both accept / one refuses / timeout / disconnect / stale and replayed confirmations / cancel / not at the changer / busy and wrong party / combat / a server crash mid-proposal all validated (tv-3, tv-4, tv-6); no load (S09-T02). Decisions T01-1…T01-9 in [Stage 9 decisions](docs/stage9-decisions.md). |
| S09-T02 | Completed | [Travel state and destination load](docs/validation/S09-T02.md) (2026-10-28, patch 0055, proto-build-46, campaign-301 / client-198): barrier, `transition` record, both actors' entities moved, the destination generation as the commit point, the in-process reload, the clients' rejoin; Cordon → Darkscape travelled (tv-11); crash before the commit → source level (tv-12), after it → destination (tv-13). Decisions T02-1…T02-7. |
| S09-T03 | Completed | [Coordinated resume](docs/validation/S09-T03.md) (2026-10-28, patch 0056, proto-build-47, campaign-302 / client-199): the hold after the reload, the validated ready (kind 13; 31/32), the 60-s deadline, state 9; tv-14 both ready, tv-15 delayed + wrong readies, tv-16 timeout; no input applied during a hold. Decisions T03-1, T03-2. |
| S09-T04 | Completed | [Cancellation, failed load and interrupted travel](docs/validation/S09-T04.md) (2026-10-28, patch 0057, proto-build-48, campaign-305 / client-202): the [failure matrix](docs/travel-contract.md#11-the-failure-matrix-s09-t04); a barrier disconnect rolls back naming who left and the retry travels (tv-17); a failed publication rolls back (phase 3) and the retry travels (tv-18); crashes during the reload (tv-19) and the hold (tv-20) recover the destination; a client back after the hold's timeout is told the party went on (tv-21); a client that crashed at the load restarts, is told the hold and the party resumes together (tv-26, after two defects found by tv-22/tv-25); after a crash before the take the party asked again and travelled (tv-23, to Garbage). Decisions T04-1…T04-6. [G09](docs/validation/G09.md) is written and waits for the user's item-by-item review. |
| S10-T03d | Completed | [Guides, fast travel and backpack travel](docs/validation/S10-T03d.md) (2026-10-30, patch 0079 `relocate`; protocol kind 26, proto-build-55 st-30; campaign-388 / client-275): a guide's trip (server dialogue) and a fast travel or backpack jump (client PDA) become a party travel proposal to a point, carried out by the S09 executor (same level too); the trip's time skips once when agreed; a guide's fee is refunded on a refusal; **reloc-4 pass 5/5** (guide: refused then agreed to Darkscape, 2.5 h once, fee once), **reloc-5 pass 5/5** (client fast travel on the same level, 1 h once). |
| S10-T03e | Completed | [Passage sweep](docs/validation/S10-T03e.md) (2026-10-31, patch 0080 `passage-sweep`; proto-build-55; campaign-394 / client-281): a server fixture reads every level changer of the spawn data and travels the party through each passage by the S09 consent path and executor; **sweep-7 pass: all 99 directed passages ok in 114 trips**, both clients rejoining each time, exits 0; **L50 closed**; fixes found on the way: B11's phantom cleanup crash (both clients), B15's identity-check stop after a reload (destroy-pending objects skipped, retried 3×10 ticks) and the stop's `log_status` crash. |
| S10-T03f | Completed | [New game on the campaign server](docs/validation/S10-T03f.md) (2026-10-31, patch 0081 `new-game`; proto-build-55; campaign-397 / client-283): with `new_game = 1` and no generation the server starts `all/single/alife/new`; the installed `on_game_load` applies the creator's choices from the server's `axr_options.ltx`; the new-game script's `ChangeLevel` to the start is an agreed party travel (T03f-1); player 2 joins as the creator's faction with its own name, money and loadout (T03f-3); **newgame-2 (loner, Zaton) and newgame-4 (Duty, Bar) pass 4/4**, newgame-3 recovers after a server kill; **L51 closed**. |
| S10-T05a | Completed | [Emission and psi-storm per player](docs/validation/S10-T05a.md) (2026-10-31, patch 0082 `emission-per-player`; proto-build-55; campaign-400 / client-286): the installed surge runs once for the world (muted, hurting nobody) and once per player in that player's context with its own cover, hits, waves, kill and effects; a psi-storm's vortices come down around each player in turn and hit by each player's distance and cover; **emission-1 and emission-4 (late join) pass 6/6, emission-5 (psi-storm) 4/4**; S06-T08's L29 closed, L28 narrowed to the ambient loops and HUD indicators. |
| S10-T05c | Completed | [Storyline chains](docs/validation/S10-T05c.md) (2026-10-31, patch 0083 `story-chains`; proto-build-55; campaign-402 / client-288): the DoctorX storyline's `drx_sl_*` variables are kept on the host actor whoever's context writes them (they split per player before), each change published as a checkpoint; Living Legend runs on the party's info portions and tasks; **story-2 pass 5/5**: a step by player 2 and one by player 1 advance the one chain once each, and both players read the same chain after a travel to Garbage and after a server restart. |
| S10-T07 | In Progress | [Bug-squashing pass](docs/validation/S10-T07.md) — **part 1 complete** (2026-10-31, patch 0087 `bug-pass-1`; protocol kind 28, proto-build-57 st-32; campaign-415 / client-301): B6 (a long move of a client's own actor), B8 (the upgrade tooltip), B10 (the bloodsucker's hold), B12 (companion hotkeys and wheel), B13 (the client clock), B16 (the server gone), B17 (ambient loops per player), B18 (combat music per player), B19 (fights with player 2 counted) fixed and checked by runs (bugfix-1…11); L58 closed, L28 narrowed; decisions T07-1…4. B1, B2, B4, B5, B7, B9, B11 and the new B20 wait on the user (see [G10](docs/validation/G10.md)). **Part 2** (2026-11-01, patch 0088 `bug-pass-2`; proto-build-59 st-34; campaign-419 / client-304): B2, B4, B7, B21, B22, B23 in code; the user's hands-on demo-1 passed the PDA, task updates and trade and failed B4, B22, B23. **Part 3** (2026-11-02, patch 0089 `bug-pass-3`; campaign-422 / client-305): B4 (the pickers' `standing()` read a path speed a replica never computes), B22 (the client follows the server's jump after a correction's hop refused its own), B23 (the campaign server's campfire turn was gated on renderer flags, every campfire stayed lit) fixed at their causes with proof lines; hands-on demo-3: jump and frame rate fine, NPCs walked backwards, the user wants all fires lit, the trader portrait still missing. **Part 4** (2026-11-02, patch 0090 `bug-pass-4`; campaign-424 / client-307): B4 (the client's own `Exec_Look` / direction control rebuilt a replica's rotation from a yaw never set), B23 (decision T07-10: every campfire burns, a client's campfire shadow only within 20 m), B1 (a hidden trade partner panel shown and filled, `panel=` probe). Hands-on demo-4: **B1 fixed**; NPCs face their motion but twist at the hip; still a stall near a fire. **Part 5** (2026-11-02, patch 0091 `bug-pass-5`; campaign-426 / client-309): B4 (a replica's own sight action overwrote the server's head aim), B23 (no campfire shadow by default; console `coop_campfire_shadow` / `coop_campfire_light`; `frame-trace`). Hands-on demo-5: **B4 and B23 fixed**. **Part 6** (2026-11-02, patch 0092 `blend-list-guard`; campaign-427 / client-310): B24, a client crash animating the other player (a model part's 16-blend list overflowed in release), fixed by retiring the oldest blend; not run yet. **G10 review (2026-11-02)**: every decision and limit answered (new limits L60-L72; B2, B22 fixed; B5, B20, B21, B11 carried; B9 waived); nine revisions to implement (T05b-4, T03b-2, T04f-2, T03a-2, T03c-2, item rewards to both, a failed task's penalty to both, story mode only, donate and workshop repair through the server), then a hands-on session (B7, B10, B24) and the gate. |
| S11-T00 | In Progress | [Local hosting and the host and join menu](docs/validation/S11-T00.md) (added 2026-11-02 by the user, moved to Stage 11): Host and Join in Anomaly's main menu, players join by code over Tailscale, a player's PC or a dedicated server PC hosts; patches 0095-0096, menu-10 / kit-8 / kit-9 pass on one PC; test kits in `builds/kits/`; waiting on the user's two-PC test |
| S10-T06 | Completed | [Co-op menu and session control](docs/validation/S10-T06.md) (2026-10-31, patch 0086 `session-control`; protocol kind 27, reasons 43–46, event 20, proto-build-56 st-31 115/115; campaign-410 / client-296): the owner (the first key the server accepted) creates a new game from the standard creation screen or loads a campaign of the server's library in the standard load window; the server checks the choices by the screen's rules, checkpoints, tells both clients and is relaunched on that campaign (exit 75, `tools/Start-CoopServer.ps1`); the partner's creation screen keeps its loadout for the next new game, its load is refused. session-2 pass 8/8 (a new game at Zaton with both players' loadouts, then the first campaign loaded back). Decisions T06-1…6. |
| S10-T05d | Completed | [Presentation split](docs/validation/S10-T05d.md) (2026-10-31; proto-build-55). **d1** presentation traffic (patch 0084 `presentation-traffic`, campaign-406 / client-292, presentation-4 pass 4/4): unchanged volumes, stopped sounds' volumes and removes/factors for effectors not on are not sent; the live looped sounds are played to a joining client once its presentation starts; a muted play is not routed, a play in a player's context is that player's; each surge pass has its own loops (0 absent records, was 672; 1,872 records vs 243,142). **d2** HUD statics (patch 0085 `hud-statics`, campaign-408 / client-294, presentation-5 pass 5/5, emission 6/6): custom statics shown in a player's context are that player's own set on the server and are shown on its HUD with their text (the exposed player saw the psi indicator change colour, the covered one none); statistics stay the party's (T05d-5). **d3**: B4/L57, B7, L47 recorded as not shown, routed to S10-T07. B3, B14 fixed; B17, B18 found; L05, L28 narrowed; decisions T05d-1…5. Join replay and text update of statics not exercised by a run. |
| S10-T03c | Completed | [World time and sleep](docs/validation/S10-T03c.md) (2026-10-30, patch 0078 `sleep`; protocol kinds 24/25, reason 42, event 19, proto-build-54 st-29 115/115; campaign-383 / client-272): sleeping is a party consent (the travel consent's rules) that skips the world's time once and rests both players; passing out is that player's 8 s blackout with no time skip; **sleep-2**: every sleep check passes (3.005 h skipped once, both rested, both clients' clocks followed; a refusal skipped nothing; player 2's blackout held its input only), 4/5 for client 2's known teardown crash (B5); L49 narrowed. |
| S10-T03b | Completed | [Offline combat and squad targets](docs/validation/S10-T03b.md) (2026-10-30, patch 0077 `squad-targets`; campaign-380 / client-270): a squad sent at "the actor" (bounty hunters, task squads) goes for the nearest player instead of actor 0; offline combat has no actor reference; **squad-1 pass 6/6** (a squad near player 2 targeted and reached it, 147 → 30 m in 42 s, then turned to player 1 when player 1 was nearer). |
| S10-T03a | Completed | [ALife switching and respawn](docs/validation/S10-T03a.md) (2026-10-30, patch 0076 `alife-respawn`; campaign-379 / client-269): the installed respawn guard and a squad's distance to "the actor" measure the nearest player (a squad could respawn beside player 2); an ALife identity fixture; **alife-4/6 pass 9/9** (guard held 23 m from player 2 with player 1 315 m away; a forced respawn far from both seen by both clients; the dead stay dead and the respawned squad exists once after a server restart; a disconnected player's area stays online), **alife-5** the same across a Cordon → Darkscape travel (8/9: client 1's known teardown crash, B5). |
| S10-T02f | Completed | [Companions per recruiter](docs/validation/S10-T02f.md) (2026-10-30, patch 0075 `companions`; campaign-372 / client-264): a companion belongs to its recruiter (a registry saved in the NPC's script vars, the squad target, `db.actor` and the story actor answer for the recruiter, the command dialogue is refused to the other player, a recruit asks for a checkpoint); the NPC evaluation scope now covers the stalker's planner, where `axr_beh`'s walk runs; **companion-4 pass 6/6**: the companion walked 103 m to player 2 in 27 s past player 1 and passed every check after a server restart. |
| S10-T02e | Completed | [Generic mutants, phantoms, AI lighting](docs/validation/S10-T02e.md) (2026-10-30, patch 0074 `fauna`; campaign-367 / client-259): thirteen mutant classes take either player as their enemy (fixture: each spawned beside player 2, never player 1's); the pseudogigant's stomp kick on the kicked player's client (`MonsterEffect` phase 4); the psy dog's aura driven on clients; a client crash on a failed psy dog phantom spawn fixed; lighting counts both players' views; fauna-4 pass 7/7. Decisions T02e-1…4; L12 closed, L13 narrowed to the rule. Hands-on pending for G10. |
| S10-T02d | Completed | [Bloodsucker, burer, poltergeist](docs/validation/S10-T02d.md) (2026-10-30, patch 0073 `mutant-abilities`; campaign-362 / client-254): the vampire grab, the anti-aim with the burer's stamina hit and shield, and the poltergeist's detection and telekinesis target the monster's participant enemy (or the nearest participant) on the server; the viewer's effects go to that player's client (presentation `MonsterEffect`, the camera-effector kind); mutant-2 pass 6/6. Decisions T02d-1…3; L12 narrowed. Hands-on pending for G10. |
| S10-T02c | Completed | [Controller and psy auras](docs/validation/S10-T02c.md) (2026-10-30, patch 0072 `controller-psy`; campaign-357 / client-249): the controller's tube attack and control hit target its participant enemy on the server, their camera, HUD and sound effects go to that player's client (presentation `ControllerPsyHit`); auras are each client's own presentation, updated by the client link for every presented creature; psy-3 pass 6/6 (player 2 hit, player 1 untouched, effects and aura on client 2 only). Decisions T02c-1…3; L12 narrowed. Hands-on pending for G10. |
| S10-T02b | Completed | [NPC loot and looting](docs/validation/S10-T02b.md) (2026-10-29, patch 0071 `npc-loot`; campaign-354 / client-246): an NPC's take from a body stays the installed scheme's own move on the server (reparents to both clients, item graphs match); a dead body within 75 m of either player is kept from the release manager's release (T02b-2); loot-1 pass 5/5 (3 items generated in player 2's context, 2 looted by an NPC, the body kept). Decisions T02b-1…2. Hands-on pending for G10. |
| S10-T02a | Completed | [NPC logic per participant](docs/validation/S10-T02a.md) (2026-10-29, patch 0070 `npc-logic`; campaign-350 / client-242): while an NPC's script binder update runs on the server, `db.actor` and `AC_ID` resolve to the nearest alive participant (T01-5; `NpcEvaluationScope`), a participant context keeps its actor; npc-1 pass 5/5 (the stalker nearest player 2 saw player 2 inside its scope, the host's actor outside). Decisions T02a-1…2; L11 narrowed (NPC part); L57 routed to S10-T05d. Hands-on pending for G10. |
| S10-T04e | Completed | [Stashes and treasure](docs/validation/S10-T04e.md) (2026-10-29, patch 0069 `stashes`; campaign-349 / client-241): a client's USE on a box reaches the server (the treasure manager spawns a stash's contents there) and opens the client's own loot window (since 0063 no loot window opened on a client); a participant's take from a box fires the box's take callback in the taker's context (the stash's searched state and spot removal on both maps); a checkpoint after a stash's contents appear; stash-2 pass 7/7 (spot on both, 4 items spawned, one taken, recovered after a kill). Decisions T04e-1…3. Hands-on pending for G10. |
| S10-T04d | Completed | [Factions and reputation per player](docs/validation/S10-T04d.md) (2026-10-29, patch 0068 `relations`; campaign-347 / client-239): goodwill and reputation stay the engine's per-character registry (T01-6); the Lua goodwill exports map character 0 to the context's player / the client's actor; the server feeds each client's registry (CampaignEvent 18: own goodwill table, faction relations, reputation; on change and on entering) and keeps the faction table in its checkpoint state (the engine does not save it); relations-3 pass 8/8 (p2's goodwill alone, a world relation on both clients, p1's reputation, all recovered after a kill). Decisions T04d-1…4; L59 added. Hands-on pending for G10. |
| S10-T04f | Completed | [Upgrades at mechanics](docs/validation/S10-T04f.md) (2026-10-29, patch 0067 `upgrades`; protocol v1.8 proto-build-53, st-28 51/51; campaign-341 / client-233): the mechanic's dialogue phrase (`dialogs.npc_is_tech`, run on the server) opens the player's upgrade session; the upgrade table (kind-16 event) drives the window; installs and repairs are kind-22 intents, journaled (`upgrade` record), applied through the engine's path in the buyer's context and mirrored on every client (kind-17 event); Anomaly's trade/repair windows are the installed Lua UI, whose script exports become the intents on a client (T04f-5, amends T04c-1 by T04f-6); upgrade-7 pass 7/7 (17680 RU install, 1042 RU repair, both replayed after a kill). Decisions T04f-1…6; L01 narrowed; L58 added. Hands-on pending for G10. |
| S10-T04c | Completed | [Trade, repair and the use of world objects](docs/validation/S10-T04c.md) (2026-10-29, patches 0063 `use-object-money`, 0064 `trade`, 0065 `remote-weapon-switch`, 0066 `remote-weapon-anim-repair-belt`; protocol v1.7 proto-build-51; campaign-333 / client-225): a client's USE on a door reaches the server and the installed ph_door logic's joint drive swings it on every client (T04c-2); the player's purse is pushed to its client (T04c-4); the native trade window opens with the server's prices, buy and sell are re-priced, journaled (`trade` record) and applied by `CTrade::TransferItem`, replayed after a kill (T04c-1, T04c-5); trade-3 pass (door, open, busy/not-talking refusals, buy 144, sell 593, two replays). Also: the party-task map-spot crash (demo-1) fixed, the partner's weapon switch completes on a replica (K1 follow-up); the partner's swing, reload, draw and holster animations from the server's weapon state (T04c-6; anim-1 23/23); a repaired item's condition in the layout event (T04c-7) and an artefact on the belt (L48 closed): trade-4 8/8. Hands-on checks (trade window, partner's switch and animations) pending for G10. Upgrades split to S10-T04f (T04c-3). |
| S10-T05b | Completed | [Every quest is a party quest](docs/validation/S10-T05b.md) (2026-10-29, patch 0060 `party-tasks-general`, campaign-316 / client-213): the Stage 8 allow list is gone, every task the installed task ini knows is accepted, rebuilt after a restart, turned in and rewarded as the party's by either player (T05b-1); rewards to both in full, the same amount (T05b-2: `relocate_money`, `relocate_item_section`, `reward_item` and `CGeneralTask.give_reward` resolved by the adapter, applied per actor by the server); a task scope sees the party's inventory (T05b-5), the death and hit callbacks run in the participant attacker's context (T05b-3); party-3: Sidorovich's bounty accepted by player 2, recovered across a server kill, target killed by player 2, both paid 10,500 and +75 goodwill, item probe and death-attribution probe pass. Limits: one general task type run (bounty), `give_reward` stored items and in-task money costs are the anchor's (T05b-4), fail/cancel not per actor; L10 narrowed. |
| S10-T04b | Completed | [Party info portions and actor-2 script state](docs/validation/S10-T04b.md) (2026-10-29, patch 0059 `party-info-portions`, campaign-313 / client-210): story info portions are mirrored between the participant actors with a personal list per player (T04b-1), `has_alife_info` reads the context's player (T04b-2), and an actor without the installed binder (participant 2's, a respawned body) gets an `alife_storage_manager` entry so its `pstor` is saved and restored (T04b-3). info-1: 30 reads as expected across a kill and restart, both mirrors, the personal portion kept apart, both actor states restored (probes 4242 / 7). L44 closed; L11, L32 narrowed. Limits: the binder-driven per-actor systems stay the script actor's (S10-T05). |
| S10-T04a | Completed | [The talk window](docs/validation/S10-T04a.md) (2026-10-29, patch 0058 `talk-window`, proto-build-49, campaign-312 / client-209): a co-op client's talk window drives the server's dialogue (ActionIntent 14 start / 15 say / 16 end, reasons 33–36; CampaignEvent 14 open / answer / list / closed); one speaker per NPC (T01-15, reason 33); the party quest's accept and turn-in phrase actions run through the S08 party path in the anchor's context (hooks in `CDialogScriptHelper::Action` and `CAI_PhraseDialogManager::AnswerPhrase`); a talking actor's inputs are dropped; the client window's remote mode (trade/upgrade off). talk-7: accept by talking (durable after generation 3), busy refusals, turn-in by talking with both players paid (5,247 / 4,797) and goodwill +75 each, 34/35 refusals, 1,491 inputs held; talk-8 the crash window at `quest:after-accept`; talk-9 the turn-in by the player who did not accept. Decisions T04a-1…4 for G10. L43 closed, L01 narrowed. Limits: non-party tasks through the window are not given (S10-T05b); dialogue preconditions other than the task read the talking player's actor (S10-T04b). |
| S10-T01 | Completed | [Campaign coverage matrix](docs/validation/S10-T01.md) (2026-10-28; no engine change): the [matrix](docs/campaign-coverage-matrix.md) inventories the installed campaign from the extracted `scripts.db0`, `configs.db0` and `spawns.db0` (33 levels, 122 level changers / 98 directed passages via `tools/list-passages.py`, 413 scripts, 610 task sections, 17 mutant classes), ranks the remaining global-player sites (NPC condlists and meet logic first, then emissions, presentation, tasks, story dialogue), gives each of 24 families a policy, fixture, evidence requirement and status (19 Required, 3 Excluded, variants and the other story chains Untested) and defines 21 children S10-T02a…S10-T05d. L48, L50, L51, L52 still hold with routed owners. The 19 scope decisions in [Stage 10 decisions](docs/stage10-decisions.md) were approved by the user on 2026-10-28 (S10-T06 added for the client-side session control); next: S10-T04a (the talk window). |

As work progresses, replace the aggregate engineering/gate rows with compact current-stage status plus completed-gate links; detailed results remain in task reports. Keep the stage summary at the top synchronized. S02-T03–T06 are complete, S02-T06 including the native checkpoint round-trip (T06d), the quest round-trip (T06e) and the G02 evidence record. G02 was accepted for development on 2026-10-18 with named limits carried into Stage 3. The Services smoke pass does not satisfy any gameplay requirement. G02 requires S02-T02–T06 and their behavioral evidence.

## Note: distribution kits (the user, 2026-11-02)

Build the three distribution kits into `Kits/` at the project root (`Kits/clientKit`, `Kits/client1HostKit`, `Kits/ServerHostKit`; `tools/make-coop-kit.py`) **only after a final test**. The user first tests the game over the network (Tailscale, two PCs) and authorizes G10; until then the kits used for testing stay in `builds/kits/`.
