# Session handoff — settings fixed; Core smoke passes

The resumed request was to fix settings loading and rerun the CPU/graphics
separation smoke. That work is finished for the supported Core startup scope.
S02-T02 remains **In Progress** because the wider GameState requirement needs a
real native level context. G02 has no headless gameplay acceptance.

Read [resumed validation](../../validation/S02-T02-resume.md) first. It records the
failure causes, scope correction, exact commands, hashes and test limitations.

## Confirmed results

- `campaign-04` and `client-02`: successful builds with recorded exit 0.
- `smoke-07 -Scope Core`: exit 0 and all required startup/shutdown stages pass.
  Window, renderer object, ready device, Render interface and input are absent
  at the tested boundary. No level/game instance/ALife tick was initialized.
- `smoke-08 -Scope GameState`: exit 3, `passed=false`, explicit
  `level-context-required`, followed by completed teardown. This scope has not
  passed and must not be represented as passing through the Core result.

## Repairs and current source

The fixture had `$game_data$` pointing away from the archive mount, which is
hard-coded as `$fs_root$\\gamedata` for these archives. The generator now aligns
them in the isolated runtime and copies/hash-checks the small loose overlay.
Archives remain in the accepted content copy. Settings now load successfully.

The normal console requires keyboard input during construction. Its new
command-only mode preserves the command registry without the editor or
presentation callbacks. The original smoke also created `game_cl_Single` before
`CLevel`; native `game_GameState` requires `Level().timeServer_Async()` in its
constructor. The test now has explicit scopes and reports that prerequisite.
No dummy actor, fake clock, blanket model no-op or omitted ALife compile flag
was introduced. Core scope installs game factory entry points, not a game world.

Worktree: `builds/build_1/engine-work` on pinned source
`c57fe5b9fab4fe0506659db03137c52d2500eb2f`. Both patches in
`patches/engine/series` reconstruct all eight modified files byte for byte in
`builds/stage2/patch-check-02`. Patch hashes are in `docs/source-manifest.json`.
Build and runtime outputs remain under `builds/`; root files remain uncommitted.

## Next work

Finish the S02-T02 internal campaign context definition and preserve native
initialization order. The later CPU model/animation/collision, frame driver and
level-loader work must provide a real level before constructing game state.
Keep the broader probe unpassed until it exercises that context; full headless
simulation requires the S02-T03–T06 evidence. Do not bypass the dependency with
a placeholder or claim AI/ALife/physics/Lua-world validation from Core startup.

Use a new run name for every test. The launcher now requires `-Scope Core` or
`-Scope GameState`; prepare its fixture with `tools/prepare-stage2-smoke.py`.
The build wrapper now captures its own .NET process exit code reliably.

Preserve the original installation, reference checkout, accepted source-client
runtime, and saved-game fixture. G01's two explicit waivers remain unchanged.
Previous handoff: [before settings fix](session-handoff-before-settings-fix.md).
