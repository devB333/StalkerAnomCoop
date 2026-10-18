# Campaign server internal context — S02-T02

This document defines the server-owned internal context for `AnomalyCampaignServer.exe`.
It is a definition plus the part that is implemented now. It is not a gameplay result.
Source is `builds/build_1/engine-work` at `c57fe5b9fab4fe0506659db03137c52d2500eb2f`,
with `patches/engine/series` applied. Paths are relative to `src/`. Runtime evidence
is in [S02-T02 context validation](validation/S02-T02-context.md).

## Role

- **Build role.** Compile-time `XR_CAMPAIGN_SERVER`, defined only in the campaign
  xrEngine configuration (`CampaignServer.targets`, `xrEngine/xrEngine.vcxproj`).
  It errors out if `NO_SINGLE`, `DEDICATED_SERVER_ONLY` or `DEDICATED_SERVER` is
  also defined (`xrEngine/x_ray.cpp`, `RunCampaignServerSmoke` preamble).
- **Runtime role.** `g_campaign_server = true`, with `g_dedicated_server` left `false`.
  Every campaign service the old runtime flag skips stays enabled:
  - task, map, spawn and restriction managers (`xrGame/Level.cpp:230-279`)
  - `CAI_Space::init` (`xrGame/ai_space.cpp:44-69`)
  - level scripts (B07)
- **Owner.** The process owns every service listed below. There is no external
  client. The future in-process `CLevel` ↔ `xrServer` direct connection is a
  private state bridge (S02-T01 B05/B19), never a player session.

## Configuration roots

The process takes one required absolute `-fsltx` path. Each run gets a fresh root
from `tools/prepare-stage2-smoke.py`:
- writable `$app_data_root$`, logs, saves and temp inside the run directory
- archives read from the accepted content copy
- `$game_data$` set to the archive mount, `$fs_root$\gamedata`

The generator refuses aliases that leave the workspace. It also refuses writable
aliases that leave the run.

## Lifecycle: native order, owned teardown

Stages start in the native client order. Each started service records its stop
function. Stop runs in reverse, so a failed start unwinds only services that exist.
A service that fails to start logs `<service>-failed` and the process returns 4.

| # | Stage | Native counterpart | Status |
|---|---|---|---|
| 1 | Core, filesystem, log, settings | `WinMain_impl`, `InitSettings` | Implemented (Core) |
| 2 | Engine events/scheduler, command-only console, game factory | `Engine.Initialize`, `CConsole`, `CEngineAPI::Initialize` | Implemented (Core) |
| 2a | `cpu-models`: R4 module attached with no device; model pool only; GPU resource creation skipped and counted ([S02-T03b](validation/S02-T03b.md)) | `CRender::create` → `Models` (`Layers/xrRenderPC_R4/r4.cpp:535`) | Implemented (Services) |
| 3 | `spatial-db`: `g_SpatialSpace`, `g_SpatialSpacePhysic` | `xrEngine/x_ray.cpp:643-644`, `Startup` | Implemented (Services) |
| 4 | `materials`: `GMLib.Load` with pair media names but no pair sounds/wallmarks ([S02-T03a](validation/S02-T03a.md)) | `xrGame/GamePersistent.cpp:156-160`, `OnAppStart` | Implemented (Services) |
| 5 | `script-engine`: `CAI_Space::init` with installed `_G` and common scripts; ALife `start_game_callback` must resolve but is not called | `xrGame/ai_space_inline.h:86-94`, `xrServerEntities/script_engine.cpp:371-434` | Implemented (Services) |
| 6 | `campaign-registries`: dialogs, character info, specific characters, communities, ranks, reputations, monster communities | CPU subset of `init_game_globals` (`xrGame/xrgame_dll_detach.cpp:48-65`) | Implemented (Services) |
| 7 | Persistent campaign state (game parameters, environment data) | `IGame_Persistent` / `CGamePersistent` constructor | **Blocked** |
| 8 | `CLevel`, `xrServer`, `game_sv_Single`, ALife from the fixture save | `xrGame/Level_start.cpp:107-125`, `game_sv_single.cpp:26-35` | **Blocked** |
| 9 | `game_cl_Single` and level-dependent initialization | `Level_start.cpp:340-363`, `game_base.cpp:205` | **Blocked** |

Stage 5 runs before stage 6 because of a real dependency. Every `CXml::Load` calls
`XMLLuaCallback` (`xrXMLParser/xrXMLParser.cpp:143`). That calls Lua
`_G.COnXmlRead` through `ai().script_engine()` (`xrGame/ScriptXMLInit.cpp:55-66`).
Native `init_game_globals` therefore creates the script engine lazily, during
dialog/character XML loading. The campaign server starts it explicitly first, so the
owner list matches what exists (observed in `smoke-13`; see the validation report).

The CPU subset in stages 5–6 is in new functions in `xrGame/xrgame_dll_detach.cpp`:
- `xrCampaignScriptEngine_Initialize/Destroy`
- `xrCampaignGameGlobals_Initialize/Destroy`

The client's `init_game_globals` and `clean_game_globals` are unchanged. The subset
leaves out UI geometry, inventory shaders, HUD sound settings, wallmark and fire
particle shaders, and UI window lists. These are presentation-only, or are filled
in only by UI and level code.

Teardown runs these steps in reverse order:
1. registries, string table and relation registry
2. script engine, AI space and object factory
3. spatial DBs
4. engine and xrGame detach
5. console, settings and core

`clean_game_globals` deletes the AI space before the registries. Either order is
safe, because registry teardown only frees data.

## Blockers on stages 7–9 (and the resolved stage 4)

Each is a renderer or presentation dependency on the native path. Removing one
belongs to a later task. None may be bypassed with a placeholder.

1. **Material pairs own sound and render media** (resolved by S02-T03a). `CGameMtlLibrary::Load` loads
   each `SGameMtlPair` (`xrEngine/GameMtlLib.cpp:222-231`). `SGameMtlPair::Load`
   creates `ref_sound` objects for breaking, step and collide sounds through the
   sound manager (`xrEngine/GameMtlLib_Engine.cpp:27-60`, `107-139`). Each pair
   also holds `FactoryPtr<IWallMarkArray> m_pCollideMarks`, made through the render
   factory (`xrEngine/GameMtlLib.h:202`). With no sound manager, `smoke-12`
   crashed in `CreateSounds`. Materials are CPU physics data, so the split belongs
   with S02-T03/T04. Keeping sound names as stimulus data and leaving out playback
   and wallmarks is S02-T05 (B18). The pair media strings (`*SoundsStr`,
   `CollideParticlesStr`, `CollideMarksStr`) are already stored, so no data has to
   be invented. `GMLib.Load` also only logs and returns when the material file is
   missing (`GameMtlLib.cpp:73-77`). The split must turn that into a failure.
2. **Persistent state creates GPU resources.** The `IGame_Persistent` constructor
   creates `CEnvironment` (`xrEngine/IGame_Persistent.cpp:51`). Its constructor
   calls `OnDeviceCreate` (`xrEngine/Environment.cpp:59-72`, `Environment_render.cpp:216-219`).
   That reaches `dxEnvironmentRender::OnDeviceCreate`, which creates the sky and
   cloud shaders and geometry through the resource manager
   (`Layers/xrRender/dxEnvironmentRender.cpp:400-406`). The render factory itself is
   always available: `FactoryPtr` falls back to the statically linked
   `RenderFactoryImpl` (`Include/xrRender/FactoryPtr.h:9-13`). The GPU work is the
   resource creation, not the factory object. Native `net_start1` and the ALife
   constructor both read `g_pGamePersistent->m_game_params`
   (`Level_start.cpp:115`, `alife_simulator.cpp:54-55`). The environment data and
   render split is S02-T04/T05 (S02-T01 B03, B09, B10).
3. **ALife start uses the persistent main menu.** `restart_all` calls
   `MainMenu()->DestroyInternal(true)` (`xrGame/alife_simulator.cpp:28-36`) before
   the script engine is re-initialized. It needs a presentation adapter or a
   separated reset path (S02-T05).
4. **`CLevel` loads player HUD models through the renderer.** The constructor calls
   `g_player_hud->load_default()` (`xrGame/Level.cpp:268`). That reaches
   `::Render->model_Create` (`xrGame/player_hud.cpp:820-842`). Since S02-T03b,
   that HUD arms model (`actor_hud_05` visual) loads in the CPU-only model pool.
   The rest of the `CLevel` constructor hasn't been tried: `CDebugRenderer`, the
   spawn-prefetch thread and the HUD/UI members. Separating the HUD is still
   S02-T05 (B04).
5. **Game state needs `CLevel`.** `game_GameState` reads
   `Level().timeServer_Async()` in its constructor (`xrGame/game_base.cpp:205`).
   So `game_cl_Single` comes after stage 8, as in native order.

This construction is owned by S02-T05/T06 under the 2026-10-18 scope decision
(see the validation report).

The `GameState` smoke scope runs stages 1–3, 5 and 6. It then reports
`persistent-context-required` and exits 3. That is the first unmet prerequisite,
not a pass.

## Limits

- Stage 5 runs real installed scripts at startup only. No level script process,
  ALife callback, task evaluation or Lua world access has run.
- Graphics absence is checked through engine state: `Device.m_hWnd`, `m_pRender`,
  `b_is_Ready`, `Render` and `pInput`. There is no trace of native graphics API
  calls.
- Start failures that raise engine assertions still terminate the process.
  Partial-start unwind covers the service-level failures the smoke checks,
  including the injected ones.
