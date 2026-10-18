# Engine investigation — first execution

Date: 2026-10-15. Scope: investigation, feasibility and documentation only. No engine/backend implementation, game execution, original-file modification, build, or gameplay validation.

## Workspace safety and tooling

Original resolved installation: `/mnt/c/Users/djb20/StalkerAnom` (exists). Development root: `/mnt/c/Users/djb20/StalkerAnomCoop` (exists). These are separate sibling directories; original is outside active writable roots. All project commands use the development root. No original game program or extraction tool was executed. One 16,138,752-byte DX11 executable was copied for analysis; no large game archive was copied/extracted.

Workspace initially had no usable repository. `git init` was blocked by a sandbox read-only `.git` mount and succeeded with approved escalation. Existing content was preserved. `.gitignore` excludes downloaded reference checkout, binaries, Ghidra projects, caches/logs and archive assets. Empty client/server/protocol/tests directories are deferred until implementation. Native Ghidra runs with explicit workspace project, user home, temporary and log locations; no Windows-native tools were launched, so WSL-to-Windows path translation was not needed. Future Windows calls must verify `wslpath -w` output and executable-side path handling.

Available: Git, Python 3, objdump, strings, Node/npm/REA CLI, Java 21.0.12; REA MCP 6.5.0 responded. Its Ghidra provider availability checks passed for `/home/djb20/tools/ghidra_12.1.4_PUBLIC`, Linux x64. Native persistent analysis status/evidence is maintained in [reverse-engineering evidence](reverse-engineering-evidence.md). CMake, Ninja, G++, Clang++ and analyzeHeadless were not on PATH (Ghidra was found by explicit path); no Windows compiler/SDK probe or engine build was performed. REA's reported provider readiness alone is not a decompilation result.

## Installed identity and source compatibility

`bin/AnomalyDX11.exe`: SHA-256 `c857b6f49d32c62245611b26106862a773185533856e8b858a7d7e849ed2fb4e`, PE32+ AMD64, preferred image base `0x140000000`, PE timestamp `2024-09-23T08:08:36Z`. Static strings identify `X-Ray Monolith v1.5.3`, `Sep 23 2024`, and PDB path `H:\Anomaly\engine\_build\_game\bin_dbg\AnomalyDX11.pdb`. The embedded Windows version is `1.0.5.6`; do not mistake it for the campaign release. Installed `changes152to153.txt` corroborates a 1.5.3 distribution. Timestamp/version strings are build metadata, not proof of unmodified provenance or an exact source commit.

All nine bin executables were hashed in [binary manifest](../reverse-engineering/binary-manifest.json). VerifiedDX11.exe has a different hash/size; its name is not proof of verification, and it was not selected for analysis. No PDB was found by Ghidra's configured search. No runtime logs existed at the checked `appdata/logs` path. Installed gamedata is empty; scripts/configs are archived under db/configs. Installed Lua behavior has not been established. Community checkout scripts are not substituted for installed campaign evidence.

| Candidate | Observed compatibility evidence | Assessment |
|---|---|---|
| [themrdemonized/xray-monolith](https://github.com/themrdemonized/xray-monolith) | Local shallow checkout pinned to `c57fe5b9fab4fe0506659db03137c52d2500eb2f`, commit date 2026-10-11; README targets Anomaly 1.5.3 and documents community/MT modifications. `src/xrEngine/xrEngine.vcxproj:60-76` uses SDK 10.0 / toolset v143 | Closest release-target candidate among those inspected; selected reference, **not** verified closest binary match. Current code substantially postdates installed build |
| [Ishmaeel/xray-monolith](https://github.com/Ishmaeel/xray-monolith) | Repository README explicitly describes a 1.5.2-derived personal fork, imported from Anomaly Bitbucket source | Older baseline comparator; no redundant clone or exact revision analysis performed |
| [OpenXRay/xray-16](https://github.com/OpenXRay/xray-16/blob/dev/README.md) | README lists CoC 1.4.22, CoP 1.6.02 and Clear Sky; describes broad refactoring | Useful history/design reference, not established Anomaly drop-in compatibility |

Reference source was downloaded read-only for analysis (sandbox DNS failure required approved Git clone escalation). No engine source edits were made. Exact installed source revision remains unresolved. Locate original 1.5.3/historical source and matching build metadata before choosing a production fork. Compare actor selection, local-client state import and dedicated task/script guards against the installed binary; string similarity alone is insufficient.

## Primary technical findings

All source references below use that pinned checkout. Full line-range ledgers: [actor investigation](actor-findings.md), [simulation investigation](simulation-findings.md), [network investigation](network-findings.md).

- **Actors:** factory has SP and MP variants, per-instance condition and inventory and retained replication. `Actor_Network.cpp:58-67,511-550` selects a global SP actor. `CALifeGraphRegistry::update/setup_current_level` (`alife_graph_registry.cpp:49-95`) selects one actor and active level. Generic enemy manager can evaluate entity targets, but monster aura/anti-aim/poltergeist logic calls Actor(). Representation is feasible in principle; campaign-level multi-player behavior is unproven.
- **Headless:** `CRenderDevice::FrameMove/on_idle`, object/scheduler/physics paths expose separable simulation, but device/HUD services persist outside rendering guards. `CLevel::OnFrame` (`Level.cpp:1034-1053,1139-1169`) disables tasks/level Lua under dedicated mode. `CommitRenderSet` schedules real bullet workload. No verified dedicated build or no-renderer execution exists.
- **Networking:** real DirectPlay hosting/sending, legacy MP game factories, event dispatch and actor state code remain. SP often bypasses transport. `xrServer::Process_update/Process_save` trusts a privileged local client; `GE_INV_ACTION` forwards to that client. This architecture is incompatible with allowing either player to supply canonical outcomes.
- **Levels:** global game level, physics world and AI graph plus singular online ALife registry indicate one online level per process. Offline world simulation does not prove independently playable online levels. Start with coordinated party transitions; multiple online levels need context refactoring or coordinated workers.

## Secondary systems: sampled, not exhaustive

Native `CGameTask` evaluates conditions and applies info/Lua effects; `GametaskManager` uses task registry key 0. `CPhraseDialog::SayPhrase` invokes script actions with native speaker objects. `CInventoryOwner` and ownership handlers provide useful owner/parent checks but not atomic durable transactions. `CALifeStorageManager` provides compressed world snapshots with Lua hooks and client-state gathering, not an established multiplayer commit journal. `CALifeTimeManager` derives world time from Device elapsed time. ALife online/offline switching is actor-anchored.

Installed main/side/repeatable quest scripts, reputation policy, emissions, stashes/traders, spawn rules and Lua callback persistence still need targeted archive inspection. These are explicit unknowns, not assumed Lua-only mechanisms. See [state ownership](state-ownership.md) for proposed conflict/recovery semantics and [protocol](network-protocol.md) for preliminary networking.

## Checklist and next evidence

Workspace verification: answered. Tool availability: answered, with build/tool limitations above. Installed version: strong static evidence; exact source match unresolved. Multiple actors/headless: partially answered with concrete blockers. Existing MP: source-level reuse established, runtime operation unresolved. Architecture comparison and preliminary protocol/state/roadmap: documented proposals. Persistent Ghidra import, save and reopening without reanalysis: verified for the hashed DX11 copy; see [evidence ledger](reverse-engineering-evidence.md) and [verification metadata](../reverse-engineering/persistence-verification.json). No function-level binary correspondence was claimed.

Next exact seams: `CLevel::net_Start/OnFrame`, `CEngineAPI::Initialize`, `CRenderDevice::FrameMove`, `CPHWorld`, `CActor::net_Spawn/shedule_Update/UpdateCL`, `xrServer::Update/Process_update/Process_save`, `CSE_ALifeCreatureActor::UPDATE_Read/UPDATE_Write`, `CALifeGraphRegistry`, `CALifeUpdateManager::change_level`, `CALifeStorageManager`, `CGameTask::SetTaskState`, `CPhraseDialog::SayPhrase`, installed Lua task/dialogue scripts. The [roadmap](implementation-roadmap.md) defines gates and tests; none are marked complete.

## Subsequent architecture review

The 2026-10-15 architecture review reused the saved Ghidra database read-only and mapped six functions without rerunning automatic analysis. GH-A01–06 corroborate the global actor accessor, singular ALife actor/current-level registry, trusted internal update/save import and model-dependent object spawn. See [updated architecture decisions](architecture.md) and [function map](../reverse-engineering/architecture-function-map.json). New source-level lifecycle tracing also shows that dedicated startup retains an internal CLevel/client runtime, that physics and ALife require separate frame/scheduler paths, and that DEDICATED_SERVER_ONLY implies NO_SINGLE, excluding ALife creation. These refine the proposed headless strategy; they do not establish a working server or exact source-build equivalence.
