# Stage 1 runtime packaging diagnostics — 2026-10-16

The user ran the source-built executable directly from `_build/_game/bin_dbg` and
reported missing DLLs, then missing `gamedata\configs\system.ltx`, then a shader
compilation failure. These were packaging/setup diagnostics requested by the user;
they do not complete S01-T03's second clean build or S01-T04's gameplay fixtures.

## Fixes and evidence

1. The build folder initially contained only `AnomalyDX11.exe` and its PDB.
   PE imports confirmed dependencies on Discord SDK, ICU and TBB. Added all five
   pinned `sdk/binaries` DLLs: `discord_game_sdk.dll`, `icuuc65.dll`, `icudt65.dll`,
   `tbb.dll`, `soft_oal.dll`. They hash-identically to the baseline copy's DLLs.
   Required DirectX and MSVC runtime DLLs already exist in Windows System32;
   no additional machine installation was needed.
2. Created `_build/_game/fsgame.ltx` and `Launch-Anomaly.cmd`, referencing only the
   workspace's copied archives in `builds/test-install/baseline/db`, with separate
   `_build/_game/appdata` saves/logs and `tmp` temporary output. No archive duplication,
   hardlinks or links to the original installation. All 36 aliases were checked.
3. The first configuration used absolute appdata/archive roots and therefore had
   not initialized `$fs_root$` when the archive scan occurred. Defined `$fs_root$`
   first, before any archive aliases. Source evidence: `xrCore/LocatorAPI.cpp`
   `LoadArchive` lines 339–387 and alias parsing around 790–838; `xrEngine/x_ray.cpp`
   `InitSettings` lines 198–230 explains the missing-system.ltx error. Retest found
   packed configuration, initialized the renderer and entered the main loop.
4. Added the pinned source's shipped `gamedata` scripts, configuration, materials,
   shaders and textures as a loose overlay under the runtime root. Upstream bundles
   this same tree as an archive: `.github/workflows/msbuild.yml:34–50,308–320`.
   The installed 1.5.3 archive copy is preserved. This is the candidate engine's
   own distribution overlay; it must be recorded as part of the compatibility baseline.
5. A user launch at 00:37:39 occurred during copying, which completed at 00:37:41.
   That process cached a partial file listing and reported shader error **X1507**:
   `fakescope.ps` could not include `night_vision.h`. The complete copied source
   includes that header. Preserved the failure log and renamed the test-only shader
   cache into `builds/logs/S01-T03/shader-cache-before-complete-assets`; no saves removed.
   The next launch used the complete files and regenerated shader cache successfully.

Packaging command (run with the test client closed and wait for completion):

```sh
python3 tools/package-stage1-runtime.py
```

It hash-checks **169 runtime files**, refuses different existing asset/config files,
records [runtime manifest](../runtime-manifest.json), establishes filesystem root
before scanning archives, and writes the launcher. Original files and immutable
reference checkout remain unchanged. Engine source remains unmodified.

## Observed startup result

Final test started 2026-10-16 04:39:07 UTC (00:39 local), PID 18992. Windows reported
`S.T.A.L.K.E.R.: Anomaly`, responsive. Log reached `Starting Main Loop` and
`Starting engine...`, with no shader compilation errors or fatal errors in the
snapshot. Evidence: `builds/logs/S01-T03/startup-5.json` and
`startup-5-complete-assets.log`; the manifest records its hash. The client was left
open for the user. Check for an existing instance before launching another.

Optional renderer integrations still emit fallback/missing-shader messages; this
startup result is not full visual/gameplay validation. Earlier user-driven play
appears in the pre-overlay log and included `Failed to load script pda_dynamic_tabs`;
that has not been retested with the complete overlay and remains a fixture item.
No quest/combat/save-load/travel acceptance is claimed.

The user subsequently confirmed that the game runs, reported a missing inventory
player model, and explicitly deferred that issue to continue the roadmap.
[Gameplay evidence and deferred defect](S01-T04.md) preserve this observation.

Use:

```text
C:\Users\djb20\StalkerAnomCoop\builds\engine-work\_build\_game\Launch-Anomaly.cmd
```

The launcher sets working directory and TEMP/TMP and supplies `-fsltx fsgame.ltx`.


## User-created build_2 repair — 2026-10-17

The user moved earlier artifacts under `builds/build_1` and supplied a new
executable under `builds/build_2/test-install/baseline/bin`. That runtime rendered
UI but reportedly a black world. Its loose gamedata lacked all 161 pinned source
distribution assets, including 32 shader files. Logs showed missing fakescope and
HDR shaders replaced with stubs; causation of the black world was not yet proven.

With user authorization and no game process running, executed
`python3 tools/repair-build2-runtime.py`. Added and hash-verified 161 assets from
the clean c57fe5b9fab4fe0506659db03137c52d2500eb2f checkout. Backed up config,
settings and log, moved the old shader cache into
`builds/build_2/repair-backups/20261017T055847Z`, defined explicit filesystem root,
and added `Launch-Anomaly.cmd` using the existing build_2 appdata profile.
Verified all 36 filesystem aliases stay within the workspace; executable and
all three save files remained hash-identical. Existing differing assets would
have caused the repair script to stop; none were overwritten.

Startup retest reached the main loop with no shader compilation or fatal errors
in the snapshot. Earlier missing fakescope/HDR diagnostics disappeared. Optional
SSFX fallbacks remain. Startup JSON, log snapshot and full repair manifest reside
in the backup directory. Client left open for user save-load/visual confirmation;
black-world fix is **not yet confirmed**. Build_2 executable source provenance
remains unverified; the overlay is from the previously pinned candidate.


## Runtime rename and New Game retry — 2026-10-17

At user request, renamed the repaired build_2 runtime from `baseline` to
`source-client` after VS Code released a Windows folder handle. Updated both
filesystem configurations and verified 36 workspace-contained aliases each,
unchanged executable and three unchanged saves. Current launch path:
`builds/build_2/test-install/source-client/Launch-Anomaly.cmd`. Previous configs,
manifest, and timed startup log are in
`builds/build_2/repair-backups/rename-20261017T061158Z`.

A 15-second scripted startup check reached the main loop and then requested a
normal window close. The user clicked New Game during that check and reported a
crash. Its log reached Cordon loading and ended with normal Direct3D teardown;
Windows Application Error events did not identify a crash at that time. The
scripted close likely interrupted the user's test. Relaunched PID 26688 with no
close timer and left it open for a proper New Game attempt. Outcome pending.


The user later clarified the player preview model was remembered from Shadow of
Chernobyl; it is not a confirmed defect in this Anomaly build. UI-01 is closed as
an incorrect expectation; see [S01-T04](S01-T04.md).
