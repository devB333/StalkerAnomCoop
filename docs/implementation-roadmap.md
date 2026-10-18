Current execution follows [plan.md](../plan.md). Stage 0/G00 and Stage 1/G01 are complete for development with the [two explicit G01 waivers](validation/G01.md). [S02-T01](validation/S02-T01.md) completed the headless service boundary and fixture specification. [S02-T02](validation/S02-T02.md) is in progress: client and campaign builds passed, settings fixed, Core startup/shutdown passed; GameState still requires real native level context. G02 has no gameplay acceptance. The roadmap below is historical.

# Implementation roadmap — authorization required

No milestone below is complete. This execution wrote investigation artifacts only. Each later milestone must record its exact revision/diff, compiler/dependency versions, reproducible build commands, test commands, observed logs/results, limitations and remaining dependencies. Proposed commands or acceptance criteria are not observed success.

## Gate 0 — establish simulation viability before networking scope expands

Select a historical 1.5.3-compatible source baseline and compare installed critical functions. Obtain/verify Windows MSVC and SDK tooling (current candidate uses v143/Windows SDK 10.0). Source solution candidates: `engine/xray-monolith/src/engine-vs2022.sln` and `engine.sln`; configuration names/dependency availability must be verified before giving a final build command. CMake/Ninja/G++/Clang were absent from this WSL PATH; Windows toolchain was not probed. No build was attempted.

After implementation authorization: isolate all runtime writable paths in workspace, audit fsgame mappings, reproduce SP baseline, then create the minimum dedicated configuration and separate presentation services. Focus on `CEngineAPI::Initialize`, `CRenderDevice::on_idle/FrameMove`, `CLevel::net_Start/OnFrame`, `IGame_Level::OnFrame`, `CPHWorld`, ALife update/storage managers. Preserve bullet UpdateWorkload even though its scheduling is named CommitRenderSet.

Acceptance: backend loads one level without either player client or a renderer, advances physics/NPC AI/world time/level Lua/task logic, and saves/reloads it. Capture CPU/memory/tick cost. Failures here block a claim of viable authoritative campaign simulation. Rollback: remove the isolated dedicated target/service adapters; keep SP defaults and original installation untouched.

## Milestones

| Milestone | Scope and likely source seams | Required tests and acceptance | Dependencies / limitations |
|---|---|---|---|
| 1 Networking foundation | Transport adapter around `xrNetServer`, `CLevel::Send`, `xrServer::SendTo_LL`; protocol envelope, dedicated sessions and two identities | Two clients connect to a standalone backend, exchange versioned messages; reject third active player, wrong versions/identity; loss/disconnect/reconnect tests | Gate 0 required before calling backend an authoritative simulation; transport-only harness is not campaign completion |
| 2 Second player actor | Factory registrations, `Actor_Network.cpp`, CActorMP, participant registry, ALife graph actor anchor | Two distinct actor IDs/conditions/inventories; correct local camera on each client; spawn/despawn/reconnect with no global pointer overwrite | Shared online level only; union activation around both players |
| 3 Authoritative movement | `CActor::shedule_Update/UpdateCL`, movement physics, clock, input dispatch; actor interpolation | Two independent inputs, server collision/speed validation, correction and prediction; loss/jitter/reorder/stale-input tests with measured error | Native physics parity unproven; preserve SP regression fixture |
| 4 Authoritative combat | `Level_Bullet_Manager`, hit dispatch, actor conditions, enemy manager | One encounter: server resolves ammo, hit, damage/death; reject forged damage/rate; NPC can target either player; latency test | Special monster abilities remain explicit limitations until audited |
| 5 Inventory | `xrServer_process_event_ownership`, InventoryOwner, ALife attach/detach, canonical IDs | Concurrent pickup yields exactly one owner; duplicate/reordered/retried transfer and crash boundaries cannot duplicate item/reward | Transaction and durable outcome design required; trade expansion later |
| 6 Shared quest | `GametaskManager`, `GameTask`, `PhraseDialog`, `PhraseScript`, installed quest Lua | One full quest including prerequisites/dialogue/reward persisted once for party; conflicting completion and NPC death during choice | Installed script extraction/audit required; one quest does not prove full campaign |
| 7 Persistence/reconnect | `CALifeStorageManager`, entity save adapters, identity map, checkpoint/journal | Server restart plus two-player reconnect reconstructs health/inventory/quest/NPC state; crash injection at write/apply/ack boundaries; reject stale epoch/client saves | Complete native serialization and callback coverage required; state versioning/migration explicit |
| 8 Campaign expansion | Monster abilities, ALife online/offline scheduling, info/reputation, tasks, dialogue, traders/stashes, emissions and transitions | Campaign matrix and long-running soak; coordinated travel with disconnect; no duplicate unique entities, rewards or divergent clocks | Independent simultaneous levels require separately approved context/worker architecture and handoff tests |

Milestone 1 is the first implementation milestone, preceded by Gate 0; do not spend a large effort on a backend whose simulation cannot yet run. It may begin with a small transport smoke test while the native gate is being established, but must report that limitation.

## Change method, compatibility and reversibility

Use existing Lua callbacks for small explicit-context experiments only where native authority is already assured. Add native bindings for player/party context and intent submission; modify compatible C++ source for ownership and simulation boundaries. Do not use binary hooks unless source integration is unavailable and a specific verified RVA/signature justifies them.

Each engine change needs a small commit with a subsystem switch or adapter, SP regression check and targeted multiplayer acceptance above. Preserve original serializers behind versioned adapters until round-trip tests pass. New multiplayer state must use separate save schemas/paths. Revert by reverting the corresponding source commit and rebuilding workspace outputs, never by patching installed binaries. No deployment to the original game directory is part of this roadmap authorization.
