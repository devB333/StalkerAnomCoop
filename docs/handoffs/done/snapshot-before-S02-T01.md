## Complete

## New-session handoff — 2026-10-17

The user explicitly requested stopping exploration and starting a new session.
Do not continue Stage 2 work in this session. `plan.md` is authoritative. Stage 1
S01-T01–T05 and G01 are **Completed for development** under the user-approved
2026-10-17 waivers. The user declined completed-quest/reward and copied original-
client comparison for G01; both remain untested, not passed. See
`docs/validation/G01.md`, `docs/validation/S01-T04.md`,
`docs/validation/S01-T05.md` and `docs/accepted-baseline.json`.

Two clean builds at source revision c57fe5b9fab4fe0506659db03137c52d2500eb2f
passed and were compared (`docs/validation/S01-T03.md`). The user-tested EXE/PDB
match their source rebuild in `engine/xray-monolith/_build`; the reference checkout
now contains user changes made after that build. **Preserve them.** Use the clean
`builds/build_1/engine-work` checkout for Stage 2. The tested runtime is
`builds/build_2/test-install/source-client`; its EXE SHA-256 is
5b9b0719f196b7e86ee140a37ef8f0caa2561558bfd0d5b1e247a085e1363603.
The 67 tested archives and 161 source overlay files were hash-verified. User
reports New Game, inventory, save/reload, dialogue, combat, world time and level
travel working; their first quest attempt failed and remained equipped after
reload. The two waived checks are future risks.

Next task: S02-T01 headless service boundary map. Existing evidence E03/E05–E09
in `plan.md` and targeted source locations are sufficient entry points. No Stage 2
source changes or validation report were made in this session. `S02-T01` remains
Not Started. The user's latest request is to stop and resume in a fresh session.

## Latest gameplay results — 2026-10-17

User reports inventory, save/reload with persistent health/ammo/time, basic combat
(including NPC kill and death after shooting a mutant), and level travel with
state intact. Dialogue works; the first quest was attempted and failed, and it
remained equipped after reload. S01-T04 remains In Progress because a **completed
quest and reward persistence** and copied installed-binary comparison F05 are
still pending. Current log/settings/saves were hash-snapshotted under
`builds/build_2/validation-evidence/20261017-user-gameplay`. Inventory player
preview was a Shadow of Chernobyl memory; user clarified it is not an Anomaly
defect. See `docs/validation/S01-T04.md`.

## Latest user gameplay observation — 2026-10-17

In renamed `builds/build_2/test-install/source-client`, the user retried New
Game after the timed-close interruption. They report that the world appears,
movement works, and HUD works. F01 is recorded as a user-observed pass in
`docs/validation/S01-T04.md`; a live log snapshot was preserved. Do not infer
save/reload, quest, combat or travel success. S01-T04 is In Progress; the earlier
black-screen symptom did not occur in this New Game attempt. The user is now
checking inventory and save/reload.

## Latest update — runtime rename and interrupted test (2026-10-17)

The user requested renaming the repaired build_2 runtime to avoid confusing it
with the pristine baseline. After VS Code was closed, Windows allowed the rename
to `builds/build_2/test-install/source-client`. Both `fsgame.ltx` and
`fsgame-stage1.ltx` were updated; each has 36 workspace-contained aliases. EXE
and all three saves remained hash-identical. The previous configurations and a
rename manifest are in `builds/build_2/repair-backups/rename-20261017T061158Z`.
`tools/repair-build2-runtime.py` and `tools/prepare-stage1-fixtures.py` now refer
to current directories. Comparison baseline remains
`builds/build_1/test-install/baseline`.

**Important correction:** The scripted startup check launched the renamed client,
waited 15 seconds, and called `CloseMainWindow`. The user clicked New Game during
that window and reported a crash. The log shows level loading and orderly Direct3D
shutdown; no contemporaneous Windows Application Error event was found. The script's
intentional close is the likely cause. Do not classify this as a confirmed engine
crash. A new process, PID 26688, was launched without any timed close and left
open for user to retry New Game. Preserve saves and process until user reports
outcome. Current launcher is `source-client/Launch-Anomaly.cmd`.

## Latest update — 2026-10-17

User reorganized builds under `builds/build_1` and created a separate runtime in
`builds/build_2/test-install/baseline`. Earlier paths below are historical and must
be revalidated before running packaging/build tools; the toolchain was also moved.
User asks whether to continue despite black world rendering with visible UI.
Read-only inspection found 161 loose files from the pinned distribution overlay
absent from the active build_2 runtime, including all 32 shader files. Its
`appdata/logs/xray_djb20.log` reports missing shaders replaced with stubs. This is
a concrete packaging mismatch and a plausible cause, not a confirmed diagnosis.
The user authorized repair on 2026-10-17. Added all 161 pinned distribution
assets (32 shaders), defined explicit build_2 filesystem root, added
`builds/build_2/test-install/baseline/Launch-Anomaly.cmd`, and moved the old
shader cache into `builds/build_2/repair-backups/20261017T055847Z`. Config,
settings and prior log are backed up there; executable and all 3 save files
were hash-verified unchanged. All 36 aliases stay inside the workspace.
Retest reached the main loop without shader compilation/fatal errors; previous
missing fakescope/HDR messages are gone. Optional SSFX fallbacks remain.
Client is open for the user to load a save; world rendering remains unconfirmed.
Repair script: `tools/repair-build2-runtime.py`; full hashes and startup record
are in the backup directory. Do not rerun packaging while the client is open.
Build_2 executable SHA-256:
`5b9b0719f196b7e86ee140a37ef8f0caa2561558bfd0d5b1e247a085e1363603`.
This differs from our clean repeat output now at
`builds/build_1/engine-repeat/_build/_game/bin_dbg/AnomalyDX11.exe`, SHA-256
`6ea7df7f84f986f4faef69fa01b382f5da439e3a196a13a60e34cb22bbb87a32`.
Build_2 source/build provenance has not yet been established. Its overlay now
matches the pinned candidate; confirm world rendering before passing G01. Inventory
model UI-01 was later closed as an incorrect expectation. No gate waiver has been approved.

# Session handoff

Updated 2026-10-16. User authorized Build Tools installation/build retry, then
reported DLL, system.ltx and shader startup errors. Those packaging issues are
repaired. [plan.md](../../../plan.md) remains authoritative; D1–D8 are unchanged.

S00-T01–T03/G00 and S01-T01–T02 are Completed. S01-T03 is **In Progress**: first
unmodified DX11|x64 build **passed**, exit 0, **0 errors / 1,379 warnings**, MSBuild
elapsed 00:09:23.64. Second clean build subsequently passed (0 errors, 1,379 warnings,
00:10:02.46 MSBuild elapsed); artifact comparison remains unfinished.
The user moved its logs to `builds/build_1/logs/S01-T03/build-2`. S01-T04–T05 mandatory fixtures
remain Not Started; G01 In Progress; G02–G11 Not Started.

Installed Build Tools 17.14.42, MSVC tools 14.44.35207 (compiler 19.44.35229.0),
MSBuild 17.14.60.43110, SDK 10.0.26100.0, ATL/MFC. Install paths `builds/vs2022`,
`builds/vscache`, `builds/vsshared`; Windows SDK/installer also register machine
components. Installer exited 0, no reboot. First retry failed solely on cache-path
length; shortened it. [Toolchain manifest](../../toolchain-manifest.json) pins versions,
hashes and installer catalog/state. All 33 projects passed compiler/output-path evaluation.

Working source `builds/engine-work` and immutable reference are clean at
c57fe5b9fab4fe0506659db03137c52d2500eb2f. Optick in working clone is
8abd28dee1a4034c973a3d32cd1777118e72df7e. No engine patches or committed root changes.
[Build manifest](../../build-manifest.json) records EXE/PDB identities. EXE SHA-256:
4bf2c693ee06c5e1f620a161e000956ceaaa63b6fcce311edf4838ad938c3141.

Independent content `builds/test-install/baseline` contains 79 copied/hash-verified
files, 17,200,963,716 bytes. Original game and Ghidra project remain untouched.
The source-built runtime lives at `builds/engine-work/_build/_game`: five SDK DLLs
beside EXE, 169 total verified runtime files including source gamedata overlay,
explicit filesystem root before archive scanning, copied baseline DB paths, separate
appdata and tmp. Use its `Launch-Anomaly.cmd` (one directory above bin_dbg).
Do not run packaging while the client is running; wait until file copying completes.

Latest test launched at 04:39:07 UTC / 00:39 local, PID 18992, window title
S.T.A.L.K.E.R.: Anomaly, responsive. Log reached main loop with no shader compilation
or fatal errors in the snapshot. Client was left open for the user; check current
processes before restarting or rebuilding its executable. Full gameplay remains
unverified. Earlier pre-overlay play logged missing pda_dynamic_tabs; retest during
fixtures. Optional shader integrations still emit fallback warnings.

Root-owned scripts: Install-Stage1Toolchain.ps1, Get-Stage1Toolchain.ps1,
Get-Stage1BuildInputs.ps1, Build-Stage1Client.ps1, prepare-stage1-content.py,
package-stage1-runtime.py under tools/. No engine source was edited.
Build command: Windows PowerShell `tools/Build-Stage1Client.ps1 -Run build-1`.
For another run choose a new name; reconstruct clean outputs/source for the repeat
requirement, accounting for LuaJIT's generated files. Preserve existing outputs,
profiles/saves, untracked root work and immutable reference. Do not erase user play.

Evidence: [S01-T01](../../validation/S01-T01.md), [S01-T02](../../validation/S01-T02.md),
[S01-T03](../../validation/S01-T03.md), [runtime diagnostics](../../validation/S01-runtime-packaging.md).
Ignored logs: builds/logs/S01-T03/build-1/{result.json,msbuild.log,msbuild.binlog};
startup-1 through startup-5 records/logs under builds/logs/S01-T03. Prior shader
cache preserved at shader-cache-before-complete-assets. Manifests are in docs/.

Next: complete the second clean build, compare identities/explain expected differences,
then run required gameplay fixtures and freeze the accepted baseline. Runtime
packaging smoke checks were user-requested diagnostics ahead of that second build;
no stage gate or gameplay acceptance was waived.


Latest steering: user confirms the client runs, explicitly defers the missing
inventory player model and requests continuing plan.md. Deferred defect UI-01 and
preserved user-play evidence are in `docs/validation/S01-T04.md`. No player model
fix is in progress. Build wrapper accepts `-SourceDirectory engine-repeat` to
preserve the existing runtime during the clean repeat.
