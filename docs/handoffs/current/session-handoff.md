# Session handoff — S02-T06 blocked on the actor-binder decision (B20)

## Next session: get the decision, then continue S02-T06

S02-T04 and S02-T05 are Completed (patches 0013, 0014). S02-T06 started with
the actor's Lua binder and is **Blocked** on an architecture decision; the
diagnostics are permanent (patch 0015). Details and evidence:
[S02-T06 report](../../validation/S02-T06.md), blocker B20 in
[headless-service-boundary.md](../../headless-service-boundary.md).

**What was found.** The actor is scheduled every tick and its Lua
`net_spawn` ran, but the C++ script binder deleted the Lua binder during the
first update: a native fault in `ActorMenu.get_maingame()`
(`ui/UIActorMenu_script.cpp:50`, null `CurrentGameUI()`) was swallowed by the
binder's `catch (...)` (xrGame has no C++ exceptions, so those catches catch
structured exceptions only). With the accessors returning nil when there is
no game UI, the next subscriber of `actor_on_first_update`,
`actor_status.script:266` `get_hud():AddDialogToRender(HUD)`, raises a Lua
error, which `lua_pcall_failed` makes fatal. The installed scripts mix
gameplay and HUD in the actor's callbacks (28 first-update and 27 update
subscribers; `get_hud()` at 61 sites in 30 scripts).

**Decision to request from the user** (options in the report): (1) an inert
server-side game UI, (2) a server script profile that skips HUD subscribers,
(3) non-fatal Lua errors on the server (diagnostic only), or (4) declare the
actor's Lua binder Stage 3 actor context (D4) and limit S02-T06 to services
outside it (combat between NPCs, ALife actor position, native save/reload of
the world). Do not build a HUD stand-in or disable the actor binder without
that decision.

**Current smoke state.** `Run-Stage2Smoke.ps1 -Scope GameState` now fails at
the first ticks (`campaign-level-loaded` is the last stage; exit 1) with the
`actor_status.script:266` fatal error, because the fault is no longer
swallowed. Core and Services scopes are unaffected. The last fully green
GameState run is smoke-60 (campaign-44 source); client-18/capture-07 is the
matching client run.

If the decision is (4), the next S02-T06 steps are: a combat fixture between
NPCs (capture-07 saw a Clear Sky stalker fight monsters within 31 s on the
client, the server's 21 s saw none), the ALife actor position check (objects
at 674.3–674.9 m switched online on the client only), and a native
save/reload of the world with the actor's binder state recorded as missing.

## Findings that shape the next tasks

- **`CScriptBinder` swallowed native faults** and deleted binders silently;
  now logged with symbols (`! CScriptBinder::shedule_Update: native
  exception …` plus `fault at`/`frame` lines with dbghelp). Look for this
  message in every server log.
- **Lua runtime errors are fatal** (`script_engine.cpp:266-288`) on both
  products; a server-side script problem ends the process, it does not
  degrade.
- **`CActor::s_schedule_updates`** and the `S02-T06 actor …` tick report line
  show the actor's scheduling and Lua binder state after the first 50 ticks;
  `objects_per_update` in the activity capture's `meta` row (65534 until the
  binder's first update completes, then 20) is the cheap indicator.
- Hearing changes AI behaviour on the server (S02-T05c); `g_bLoaded` gates
  hearing; the actor is a hearer; actor-owned items are not crows on the
  server (no view entity).
- `tools/xray-db-extract.py` reads Lua and configs from the `.db` archives
  (`list`, `extract`, `grep`; extracted text is cp1251: `LC_ALL=C grep -a`).

## Tools and runs

- **Last run names:** `campaign-52`, `smoke-67` (blocked state), `client-18`,
  `capture-07`; `smoke-20` reserved for the user. Next: `campaign-53`,
  `client-19`, `smoke-68`, `capture-08`. Check `builds/logs/S02-T02/`.
- **Smoke:** `python3 tools/prepare-stage2-smoke.py <run>`, then
  `Run-Stage2Smoke.ps1 -Run <run> -Scope Core|Services|GameState`; the server
  runs with `-snd_cpu`. GameState expects `campaign-activity-passed` and
  `campaign-hearing-passed`.
- **Client captures:** `python3 tools/prepare-stage2-smoke.py <run> <task>`
  then `Run-ClientCollisionCapture.ps1 -Run <run> -Capture collision|physics|activity|hearing`.
- **Compare:** `tools/compare-activity-capture.py`, `tools/compare-hearing-capture.py`.
- **Build:** `Build-Stage2.ps1 -Run <name> -Product Campaign|Client`; quote
  the script path from WSL. Game-module-only changes build in 1–2 min.
- **Patches:** 0001–0015 exported; next export is
  `python3 tools/export-engine-patch.py <name> 17`.
- Smart App Control is off (user, 2026-10-17).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and the saved-game fixture. The two G01 waivers are unchanged.
Previous snapshot: [before the S02-T06 decision](../done/snapshot-before-S02-T06-decision.md).
