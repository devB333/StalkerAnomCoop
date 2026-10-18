# Multiplayer feasibility assessment

Status: source-backed conditional feasibility; no playable prototype, build or runtime validation. Source revision `c57fe5b9fab4fe0506659db03137c52d2500eb2f` is not an established exact match for installed Monolith 1.5.3.

## Three primary questions

1. **Two player actors: partially answered.** Retained `CActorMP`/`CSE_ActorMP`, network registration and per-instance condition/inventory support multiple representations. `Actor()`, ALife anchor, task key 0 and special monster attacks assume one campaign actor. Two independently controlled campaign actors have not been demonstrated. [Evidence](actor-findings.md).
2. **Renderer-free authority: partially answered.** Dedicated conditionals and simulation update paths survive, but device/HUD dependencies remain and dedicated mode suppresses important campaign script/task work. A full authoritative headless simulation is plausible, not proven. [Evidence S1–S12](simulation-findings.md).
3. **Multiplayer reuse: source-level answered, runtime unresolved.** DirectPlay transport, game modes, event dispatch, actor replication and serialization remain. SP shortcuts privilege a local simulation client, and received outcome/state events do not meet the requested validation model. Reuse components, replace trust semantics and external transport. [Evidence](network-findings.md).

## Ranked blockers

| Rank / subsystem | Current behavior and evidence | Required change / strategy | Difficulty, dependency and uncertainty |
|---|---|---|---|
| 1 — Online simulation authority | Client-state import in `xrServer::Process_update/Process_save`; local forwarding in `CLevel::Send` | Dedicated process owns live actor/NPC/physics objects; clients submit inputs; reject outcome mutations | Very high; requires tracing CLevel/Server update split. Runtime ownership migration untested |
| 2 — Headless campaign execution | Device/HUD dependencies; `CLevel::OnFrame` skips tasks and Lua under dedicated mode | Split gameplay from presentation, supply clock/headless services, retain authoritative task/script updates | Very high; native build and assets required. No renderer-free boot evidence |
| 3 — Participant model | `g_actor`, `CALifeGraphRegistry::m_actor`, activation distance, task key 0 | Participant registry, explicit context, union-of-player activation; party task namespace | High; depends on 1/2 and installed Lua audit. Remote actor representation alone insufficient |
| 4 — Campaign/Lua semantics | `CGameTask::SetTaskState`, `Actor()` flags and dialog script actions | Validate and commit shared effects once, separate presentation, classify per-player/party/global state | Very high content breadth; actual installed scripts remain archived and uninspected |
| 5 — Level contexts | One game level, physics world, AI graph and online ALife registry | Coordinated party transitions initially; later contexts or coordinated per-level workers | Very high for independent levels; no concurrent online-level proof |
| 6 — Identity / inventory | Reusable u16 IDs, parent checks but no durable operation IDs | Stable campaign IDs and generations, atomic validated ownership/reward transactions | High; requires lifecycle/save adapters and concurrency/crash tests |
| 7 — Persistence | Native compressed snapshot, client save preparation, callback state | Server-generated consistent checkpoints, durable semantic journal, replay/dedupe schema | High; RNG/callback/online-state coverage unverified; no deterministic replay assumption |
| 8 — Combat and AI details | Retained state/hit forwarding; generic target manager plus Actor()-bound abilities | Server-resolved ballistics, health and ammunition; explicit targets for special abilities | High; depends on simulation ownership, physics parity and lag tests |
| 9 — Build/source correspondence | 2026 community source vs installed 2024 executable; no matching PDB | Locate historical baseline, compare critical native functions, reproduce isolated build | High gating uncertainty; compiler/build availability incomplete |

Detailed files/functions/line ranges and bounded observations are in the linked source ledgers. The ranks prioritize architectural dependencies, not estimated calendar durations.

## Decision and scope limits

Prefer native authoritative Architecture A, with transport/storage modules, over independent gameplay reconstruction. Do not add a separate service solely to make the architecture look more scalable. C is a reasonable later deployment option but cannot remove native headless, singleton or campaign blockers. No estimate of CPU, memory, development duration or campaign compatibility is justified by static inspection alone.

The strongest next evidence is an isolated reproducible build and a no-client simulation/save experiment, followed by two input-controlled actors. Independent cross-level play is a separate major milestone; offline ALife is not equivalent to live physics and AI in two levels. Core story, repeatable quests, reputation, emissions, traders, stashes and installed Lua mutations require focused content investigation after the primary gates.
