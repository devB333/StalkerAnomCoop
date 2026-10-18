---
name: stage2-run
description: Run a Stage 2 build (Client/Campaign) or smoke test (Core/GameState) under a fresh run name and record the result. Use whenever a build or runtime test of engine-work is needed.
argument-hint: build <Client|Campaign> | smoke <Core|GameState>
---

Run a Stage 2 build or smoke test: $ARGUMENTS

1. **Pick a fresh run name** matching `^[a-zA-Z0-9-]+$`. List `builds/logs/S02-T02/` and take the next number in the series (`client-0N`, `campaign-0N`, `smoke-0N`). Never reuse a name.
2. **Before building**, make sure no test client or campaign server is running, because the build replaces the executable:
   `powershell.exe -NoProfile -Command "Get-Process | Where-Object { $_.Path -like 'C:\Users\djb20\StalkerAnomCoop\builds\*' } | Select-Object Id,Path"`
   If anything is listed, stop and ask the user.
3. **Run the command** through Windows PowerShell with a Windows path:
   - Build: `powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\djb20\StalkerAnomCoop\tools\Build-Stage2.ps1 -Run <name> -Product <Client|Campaign>`
   - Smoke: run `python3 tools/prepare-stage2-smoke.py <name>` first, then `powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\djb20\StalkerAnomCoop\tools\Run-Stage2Smoke.ps1 -Run <name> -Scope <Core|GameState>`
   Builds are slow, so use a long timeout or run in the background.
4. **Read the outcome** from the exit code and the logs under `builds/logs/S02-T02/`. A successful compile is not behavioural acceptance. A smoke passes only if the script says so.
5. **Record the result** in the current task's `docs/validation/<task-ID>.md`: the run name, the exact command, the exit code, expected vs. observed results, output hashes if any, and links to the logs. Don't paste logs into the report.
6. Report to the user in one or two lines: the run name, pass/fail and the key error if it failed.
