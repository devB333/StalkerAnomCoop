# Dedicated cooperative architecture — evidence and decisions

Reviewed 2026-10-15. **Recommendation: pursue a dedicated Monolith-derived simulation, conditional on a headless campaign feasibility gate.** Keep the native online world inside a backend-owned `CLevel` and the necessary parts of `game_cl_Single`, alongside `xrServer`/ALife. Neither human client owns that internal runtime. `xrServer` alone is not the complete simulation. Separate services are a later option; replacing all gameplay with an independent backend has the weakest evidence-to-effort case.

This document requests architecture decisions, not implementation authorization by implication. No engine source was changed, game launched, or build performed during this review. Initial proposed direction is retained and refined below. No architecture is yet proven to run a cooperative campaign.

## Evidence scope and confidence

Source: `engine/xray-monolith`, revision `c57fe5b9fab4fe0506659db03137c52d2500eb2f` (2026 community fork targeting Anomaly 1.5.3). Paths and line ranges below are relative to that checkout's `src/` directory. Installed reference: `AnomalyDX11.exe`, SHA-256 `c857b6f49d32c62245611b26106862a773185533856e8b858a7d7e849ed2fb4e`, with 2024 build metadata. Exact source equivalence remains unknown.

The existing persistent Ghidra project `Anomaly153-c857b6f49d32` was reused with `-noanalysis -readOnly`. Six selected functions were decompiled, without importing or repeating automatic analysis. [Function map](../reverse-engineering/architecture-function-map.json) and [binary evidence ledger](reverse-engineering-evidence.md#architecture-review-six-targeted-functions-from-the-saved-project) preserve RVAs, diagnostic-string references, source correspondence, raw-output hashes and limitations. `GH-Axx` identifiers are local evidence records, not REA MCP IDs. All three targeted invocations succeeded and reported the expected hash and 49,494 retained functions.

- **Source confirmed:** behavior present in the pinned source; not automatically a fact about the installed build.
- **Binary corroborated:** matching control flow and embedded diagnostics in a selected installed-binary function. Recovered argument types and full class layouts remain unverified.
- **Proposed/inferred:** engineering recommendation based on evidence, requiring build/runtime validation.
- **Runtime observed:** none. Database persistence/reopening was verified; gameplay was not.

The binary model-spawn diagnostic names source line 241, versus line 268 in this checkout. This is direct evidence of source drift; six matches do not validate the entire fork.

## Confirmed architectural findings

| ID | Finding and source evidence | Ghidra evidence / architectural implication |
|---|---|---|
| F1 | `Actor()` in `xrGame/Actor_Network.cpp:58-67` returns `g_actor` and asserts SP mode. `CActor::net_Spawn`, 511-550, can assign that pointer from LOCAL + ASPLAYER. `xrGame/Actor.h:74-79,727-733` and `InventoryOwner.cpp:37,90-92` provide per-instance condition/inventory. | **GH-A01**, RVA `0xfb2e0`, confirms the game-mode assertion and global return. Per-instance storage/spawn assignment are source-only. Multiple actor objects are structurally possible, but one global actor cannot represent two independent player contexts. |
| F2 | `CALifeGraphRegistry::update/setup_current_level`, `xrGame/alife_graph_registry.cpp:49-96`, selects one actor and one level registry from its graph location. `alife_dynamic_object.cpp:160-183` uses that actor's distance for online/offline switching. | **GH-A02**, RVA `0x386670`, writes the actor slot and calls **GH-A05**, RVA `0x386100`, which creates one actor-selected level registry. Distance switching remains source-only. Need explicit participants and activation around either player; one current-level context remains a major constraint. |
| F3 | Server startup includes internal client startup: `CLevel::net_start4`, `xrGame/Level_start.cpp:217-230`. `Level_network_start_client.cpp:134-164` creates physics/network callbacks. `xrServer::Update`, `xrServer.cpp:213-263`, handles packets/events/game state/respawn/replication; online objects update through `xrEngine/IGame_Level.cpp:191-200`. `game_sv_single.cpp:172-182` has no replacement full online simulation loop. | Source confirmed; no binary mapping claimed. Preserve the native online runtime inside the backend instead of expecting `xrServer` to acquire physics, AI and objects simply by running alone. |
| F4 | `xrServer::Process_update/Process_save`, `xrGame/xrServer_process_update.cpp:7-95`, requires `CL->flags.bLocal` and reads entity state/load records. `CLevel::Send`, `Level_network.cpp:294-312`, and `xrServer::SendTo_LL`, `xrServer.cpp:797-812`, support local direct dispatch. | **GH-A03**, RVA `0x71ef80`, and **GH-A04**, RVA `0x71edd0`, confirm local-client assertions and indirect state import/load. Direct-dispatch mapping is source-only. Retain this as a private backend bridge if useful; never grant either network player the privileged state-writer role. This does not demonstrate a remote exploit. |
| F5 | Dedicated switches have different effects. `xrEngine/no_single.h:1-7` maps `DEDICATED_SERVER_ONLY` to `NO_SINGLE`; `game_sv_Single::Create`, `xrGame/game_sv_single.cpp:26-33`, excludes ALife creation under `NO_SINGLE`. Separately, runtime `g_dedicated_server` suppresses tasks and level Lua in `CLevel::OnFrame`, `Level.cpp:1034-1053,1142-1169`. | Source confirmed only. A new campaign-server configuration must retain ALife, tasks and authoritative Lua. Neither existing dedicated macro is proof of a usable headless campaign build. |
| F6 | `CRenderDevice::on_idle/FrameMove`, `xrEngine/device.cpp:401-580,694-754`, mixes simulation clock/frame callbacks with render services, matrices and camera work. `IGame_Level::OnFrame`, `IGame_Level.cpp:191-208`, runs objects then HUD and camera-positioned ambience. `game_cl_Single::createGameUI/OnDifficultyChanged`, `xrGame/game_cl_single.cpp:23-44`, requires UI/global actor. | Source confirmed only; current 2026 device code is not claimed identical to the 2024 binary. Need a backend lifecycle and gameplay clock independent of UI, camera, window activity and draw submission. |
| F7 | `CObject::cNameVisual_set`, `xrEngine/xr_object.cpp:70-85`, obtains models through `Render->model_Create`. `CObject::net_Spawn`, 255-280, loads visual data and constructs `CCF_Skeleton`. `CCharacterPhysicsSupport::in_NetSpawn`, `xrGame/CharacterPhysicsSupport.cpp:184-205`, uses `IKinematics`, bone calculation and physics shells. | **GH-A06**, RVA `0x72650`, corroborates visual configuration/model-setter call, cform checks and collision allocation in spawn. Renderer API and bone details are source-only. A renderer that returns null for all models would remove gameplay data; preserve CPU skeleton, animation and collision capabilities. |
| F8 | Physics registers a frame callback in `CPHWorld::Create`, `xrPhysics/PHWorld.cpp:185-200`; `OnFrame/FrameStep`, 280-295,506-556, consumes Device time and fixed substeps. ALife registers scheduled work in `xrGame/alife_update_manager.cpp:58-78,92-140`; `CGamePersistent::OnFrame`, `GamePersistent.cpp:817-824`, advances the scheduler. Stalker/monster scheduled updates: `ai/stalker/ai_stalker.cpp:1129-1155`, `ai/monsters/basemonster/base_monster.cpp:363-393`. | Source confirmed only. Server must drive all these execution paths with explicit ordering. Network tick rate is not automatically the native physics rate. No deterministic replay or measured server cost is established. |
| F9 | `CObject::UpdateCL`, `xrEngine/xr_object.cpp:379-406`, selects frequent work using view/camera/visibility; `CObjectList::Update`, `xr_object_list.cpp:198-266`, chooses that workload or all active objects. `CBulletManager::CommitRenderSet`, `xrGame/Level_Bullet_Manager.cpp:1225-1236`, schedules real ballistics. | Source confirmed only. Replace camera-based simulation prioritization, and retain bullet workload even though its caller has a rendering name. Do not delete all `UpdateCL` or render-named methods. |
| F10 | Legacy transport/game/entity infrastructure survives: DirectPlay connect/send in `xrNetServer/NET_Server.cpp:245-443,614-658`; SP/MP factories in `xrServerEntities/object_factory_register.cpp:193-240`; actor replication in `xrGame/Actor_Network.cpp:88-158,290-350` and `actor_mp_client_import.cpp:11-80`. | Source confirmed; installed RTTI-name presence for MP actors/server corroborates class metadata only. Reuse serializers, factories and remote presentation selectively. Working network sessions, validated combat and campaign replication are not established. |
| F11 | Globals include `g_pGameLevel` (`xrEngine/IGame_Level.cpp:20-26`), `ph_world` (`xrPhysics/PHWorld.cpp:40-60`) and one AI level graph (`xrGame/ai_space.h:38-41`). `CALifeUpdateManager::change_level`, `alife_update_manager.cpp:159-240`, moves/saves one actor. | GH-A02/A05 corroborate the singular ALife level component; other globals/transition are source-only. One process currently has one online level context. Offline ALife across the graph does not provide simultaneous online physics in two levels. |
| F12 | `CALifeStorageManager::save/prepare_objects_for_save`, `xrGame/alife_storage_manager.cpp:46-123,245-250`, writes a compressed native checkpoint with script hooks after client state gathering. Native task/info/dialogue changes occur in `GameTask.cpp:40-60,137-209`, `GametaskManager.cpp:47-68`, `PhraseDialog.cpp:85-129`, `PhraseScript.cpp:156-166,234-270`. | GH-A04 corroborates the receiving save-state path, not checkpoint completeness. Campaign authority includes native and Lua effects. Durable transactions, stable IDs and consistent server-generated saves must be added; a database alone cannot supply simulation correctness. |

Detailed earlier source observations remain in [actors](actor-findings.md), [simulation](simulation-findings.md), and [networking/persistence](network-findings.md). Installed campaign Lua is still archived and uninspected; repository addon scripts are not evidence of the complete installed campaign.

## Headless blockers and modification boundaries

Difficulty is a qualitative engineering estimate, not a duration or proven implementation cost. **High** spans several subsystems; **very high** may change the architecture or content compatibility.

| Priority / difficulty | Required boundary and affected subsystems | Gate that would resolve the uncertainty |
|---|---|---|
| 1 — Very high | Backend boot retaining CLevel, campaign game state, physics, scheduler and ALife; new configuration avoiding NO_SINGLE (F3/F5/F8) | Load a level, advance AI/physics/world time/Lua and save/reload with neither player connected |
| 2 — Very high | Headless services: renderer/device, CPU models/bones/collision, particles, sound, UI/loading (F6/F7/F9; `Level_load.cpp:50-96`, `Level_network_start_client.cpp:249-274`) | No graphics device/draw submission or interactive camera needed; skeleton collision and an encounter remain correct |
| 3 — Very high | Explicit participant registry, input routing, campaign/player contexts, ALife activation and AI targeting (F1/F2; `Actor.cpp:1128-1181,1773-1825`, `enemy_manager.cpp:76-89,125-141`) | Two independent inputs/conditions/inventories; activation near either player; NPC and special attacks target both correctly |
| 4 — High | Private internal simulation authority and external validated intents; replace local-process identity as authority boundary (F4/F10) | Forged client health, movement outcomes, rewards or ownership packets cannot mutate canonical state |
| 5 — Very high | Task/dialogue/Lua effects separated from presentation and committed once; versioned persistence (F5/F12) | One quest and inventory transaction survive duplicate delivery, reconnect and crash injection |
| 6 — Very high if expanded | Level globals, migration and ALife partitioning (F2/F11) | Initially coordinated two-player travel; independent levels require a separate ownership/handoff prototype |

The first two gates are the architectural risk reducers. Connecting two clients or drawing a second actor is not evidence that those gates passed.

## Backend options

| Criterion | A — dedicated Monolith runtime | B — independent gameplay backend | C — Monolith runtime plus separate service |
|---|---|---|---|
| Feasibility / reuse | Best supported by retained lifecycle, native simulation and serializers | Must extract or reconstruct physics, AI, ALife and campaign behavior | Same native feasibility as A, plus service integration |
| Correctness / fidelity | Strong reuse potential; current authority split must change | Largest divergence risk, especially Lua/native interactions | Same potential as A; IPC/state ownership adds failure modes |
| Headless | Requires the F5–F9 separation work | Natural process structure, but faithful simulation is missing | Still requires a headless native worker |
| NPC / quest authority | Native runtime decides outcomes | Backend must reproduce those outcomes | Native worker decides; service cannot replace it |
| Persistence | Local adapter for native checkpoint + journal | New canonical schema and restoration adapters | Service can store commits/checkpoints under explicit fencing |
| Difficulty / maintenance | High engine-fork cost, least new simulation code | Very high reconstruction and parallel-maintenance cost | A's engine cost plus deployment, IPC and versioned contracts |
| Performance / CPU / memory | Unknown; benchmark loaded world and retained CPU assets | Unknown; no defensible claim of savings | A's simulation footprint plus service/IPC overhead |
| Long-term scale | One campaign process initially | Architectural freedom at very large fidelity cost | Natural later service/worker split, but multi-level ownership remains hard |

**Recommend A now.** It may contain an internal historically named “client” runtime without being player-hosted: the backend process exclusively owns it. Keeping an external player as that privileged runtime would violate the project requirements. C is a later deployment option if independently scaling storage/sessions or multi-level workers justifies it; it does not fix native headless blockers. B is a fallback only if a bounded native feasibility attempt demonstrates an unacceptable obstacle.

## Decisions before implementation

### D1 — Authoritative execution model

Options: retain backend-owned CLevel/game_cl_Single services alongside xrServer; move all online simulation into xrServer; or reconstruct it externally. **Recommend the first** (F3/F4/F8). It reuses the existing object/physics/scheduler lifecycle. Risk: classes named `client` still mix presentation and authority and must be separated deliberately. The private internal state bridge must not be addressable as a remote player role. Separate session/player identity from engine simulation identity.

### D2 — What “headless” must mean

Options: disable display/draws while retaining a graphics device; replace every renderer interface with no-ops; or preserve required CPU model/animation/collision services while removing GPU/UI dependencies. **Recommend the third** (F6/F7/F9). A hidden graphics window can be a temporary diagnostic baseline only; it does not pass the headless gate. Blanket no-ops risk broken hit geometry, animation callbacks and physics. Audit gameplay sound/particle callbacks before suppressing their presentation. Do not introduce a synthetic canonical “player zero” as the permanent solution to Actor() dependencies.

### D3 — Source baseline, platform and compatibility promise

Options: patch the installed executable; reproduce its exact historical source; or build a matched client/server pair from a validated 1.5.3-compatible source baseline. **Recommend source-built matched peers**, initially on Windows with the candidate's MSVC/SDK path, while treating the installed build as a behavioral reference. This avoids requiring every native ABI to match the original executable. It still requires content/save compatibility and reproducible-build checks. The current 2026 checkout is the pinned investigation reference, not an automatically approved production baseline. Historical 1.5.3 source may offer less scope; current community code offers fixes but includes substantial MT and rendering changes. Compare the build/content gates before selecting the fork. Linux server portability and binary-hook deployment should be separate decisions, not hidden prerequisites.

### D4 — Campaign mode and actor model

Options: reuse stock competitive MP mode; preserve one campaign actor and attach a visual companion; or retain campaign services with explicit participants and adapt selected MP representation/replication. **Recommend the third** (F1/F2/F5/F10/F12). Separate campaign capability, dedicated execution, network role and local presentation instead of treating `eGameIDSingle` versus legacy MP as the entire policy. Retain per-instance CActor state and evaluate CActorMP behavior selectively; its existence does not prove it is a drop-in campaign actor. Replace implicit actor context at gameplay mutation sites with player/party/entity context, while preserving a local-view actor on each rendering client. Generic AI targets are reusable, but `poltergeist_telekinesis.cpp:218-249,333`, `anti_aim_ability.cpp:238-272` and `monster_aura.cpp:56` explicitly need target/presentation separation. Risk: installed Lua and content may contain many implicit actor assumptions.

### D5 — Same-level scope versus independent levels

Options: coordinated party travel on one online level; multiple contexts in one process; or coordinated native workers for separate online levels. **Recommend same-level play and party travel for the first implementation scope** (F2/F11). This is a scope choice requiring approval, not a silent promise of seamless independent cross-level play. Multiple contexts require removing global level/physics/AI assumptions. Workers avoid some globals but require one campaign authority, one owner per entity, ALife partitioning, clock agreement, fenced transfer and crash recovery. Neither alternative is validated. Decide early if independent levels are mandatory for the first release; that changes architecture and roadmap substantially.

### D6 — External transport and mutation boundary

Options: adapt retained DirectPlay; add GameNetworkingSockets; or use ENet. **Recommend a transport adapter and provisional GameNetworkingSockets choice**, subject to an isolated toolchain test; preserve useful legacy serializers internally (F4/F10). Existing [transport assessment](network-protocol.md) records primary library references and ENet tradeoffs. No library eliminates authority validation. Clients submit inputs/intentions; backend validates, orders, resolves and commits effects before replication. Start with one canonical mutation order, with worker jobs joined before relevant commits; do not assume current parallel callbacks supply transactional ticks. Reliable action/event streams and obsolete-discarding snapshots remain separate. Final wire schema/transport revision can wait until the simulation gate; starting with custom reliable UDP is unjustified.

### D7 — Persistence and campaign ownership

Options: native SP saves alone; deterministic replay of all client inputs; or native checkpoints plus a durable journal of validated canonical effects and stable identity mappings. **Recommend the third** (F4/F12), subject to complete snapshot/restore coverage. Native u16 IDs are reusable (`xrServer_Object_Base.h:71-77`, `xrGame/xrServer.h:145-155`); map persistent IDs/generations to them. Critical ownership/reward/quest changes need atomic effect bundles and stored operation outcomes. Neither deterministic physics/Lua replay nor zero-loss crash recovery is proven. Use quiescent checkpoint boundaries and test durability before acknowledging critical success; accept an explicit continuous-simulation rollback policy until more complete recovery is demonstrated. The database stores decisions; native simulation remains authority. [State model](state-ownership.md) defines global, party, player and presentation ownership.

### D8 — Separate backend services now or later

Options: transport/storage modules in the simulation process, or a service supervising native workers from the outset. **Recommend modules in one dedicated process initially**. This preserves the A recommendation and avoids IPC consistency work before headless viability is known. Keep versioned interfaces so C remains possible. If D5 selects independent online levels immediately, revisit C before implementation; a coordinator would need explicit command ordering, ownership fencing and global campaign responsibility.

## Unresolved questions that could change the strategy

1. Can the necessary model/animation/collision services load without creating a graphics device, including skeletal callbacks and gameplay visibility? F7 confirms the dependency but not the size of the extraction.
2. Can the native campaign run with no locally controlled actor and with zero connected players, while preserving ALife, tasks, Lua and authoritative saves? A noninteractive player entity may exist in the world; it must not be a hidden human-host authority.
3. Which source baseline builds reproducibly and preserves the installed 1.5.3 content/serialization behavior? Selected binary matches do not settle that decision.
4. How much installed quest/dialogue/AI Lua depends on `db.actor`, UI or client callbacks? Installed archives have not been extracted or audited in this pass.
5. Which actor movement, weapon and animation events currently require local control, and can they accept two explicit input streams on the server? No prediction/reconciliation parity or two-actor runtime test exists.
6. Does authoritative checkpoint assembly capture online physics/health/inventory, Lua state, delayed events, RNG state and both identities consistently? Native save completion alone cannot answer that.
7. Are independent levels required in the first usable campaign? If yes, the single-process plan needs a separate context/worker investigation before commitment.
8. What are CPU/memory/tick costs after headless separation and union-of-player ALife activation? No performance or bandwidth measurements justify sizing yet.

The absence of a binary mapping for a source-only finding is explicit, not evidence that the installed function is absent. No more binary work is needed simply to restate the current decision; target a specific remaining uncertainty when it affects the next gate.

## Prioritized next engineering steps

1. **Approve/revise D1–D5 and the single-process scope.** Decide headless definition, paired source builds, campaign actor model and level scope before broad implementation. D6/D7 establish authority/persistence constraints; library and schema details remain provisional.
2. **Validate the source/toolchain baseline and isolated runtime layout.** After implementation authorization, reproduce an unmodified client build and content load in the workspace. Pin dependencies/configuration and compare native saves; preserve original installation and Ghidra project.
3. **Perform a bounded headless campaign spike.** Keep CLevel, physics, ALife and scheduler, retain CPU gameplay assets, separate presentation and keep NO_SINGLE disabled. Acceptance: no player client or graphics device required; world time, NPC/physics, ballistics, task/Lua and save/reload all function. Record failures before choosing B/C as a fallback.
4. **Prove two participant contexts on one level.** Separate input, camera/view, health/inventory, AI targets and ALife activation. Test zero/one/two connected players and disconnection. Then replace external outcome packets with validated intentions and add the two-client transport foundation.
5. **Prove authoritative transactions and persistence with one encounter, item and quest.** Test duplicate/concurrent actions, stale IDs, NPC death during dialogue, reconnect and crash boundaries before broader campaign adaptation.
6. **Expand content and transitions.** Audit installed Lua and special AI systematically. Implement party travel first if approved; investigate multi-context/workers only if independent levels are authorized.

Every gate needs recorded build/test/runtime evidence. No milestone is complete from this review. Existing [implementation roadmap](implementation-roadmap.md) remains the milestone ledger; its gates should be refined after these architecture decisions are approved. Source changes should be modular and reversible, with SP regression checks and separate multiplayer save versions. Targeted binary patches remain a last resort, not the proposed implementation method.
