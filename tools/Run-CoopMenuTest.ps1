# S11-T00: run a session prepared by tools/prepare-coop-menu.py the way players start one from the menu: both
# clients start WITHOUT -coop-config (single player as installed, plus the co-op menu from their app data
# coop-menu.ltx). Player 1's menu hosts (it writes the server's join code and choice, starts
# tools/Start-CoopServer.ps1 and joins at 127.0.0.1); player 2's menu joins with the host's address and code.
# With prepare-coop-menu.py --test both menus act by themselves (their test driver).
#   powershell.exe ... -File C:\...\tools\Run-CoopMenuTest.ps1 -Run <run> [-Task S11-T00] [-ClientStarts '0,20']
#       [-Seconds 420] [-AfterSeconds 30] [-Phases 2]
# -Phases 2: then both clients start again with the host's test set to host_continue (the last campaign) and
# the friend rejoining (a roster player by then); its logs go to {c1,c2,server}-2.
# A --dedicated session (prepare-coop-menu.py): each phase starts tools/Host-CoopServer.ps1 first (phase 2 with
# -Continue) and both clients join with its code.
# After -Seconds (from the first client's start) the clients' stop files are written (they quit cleanly; the
# host's quit stops its server through the server's stop file); after -AfterSeconds more anything left is
# stopped. Logs go to builds/logs/<task>/run-<run>/{c1,c2,server}.
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    [ValidatePattern('^S[0-9]{2}-T[0-9]{2}[a-z]?$')][string]$Task = 'S11-T00',
    [string]$ClientStarts = '0,20',
    [int]$Seconds = 420,
    [int]$AfterSeconds = 40,
    [int]$Phases = 1
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$sessionDir = Join-Path $workspace "builds\stage5\$Task\$Run"
$session = Get-Content (Join-Path $sessionDir 'session.json') -Raw | ConvertFrom-Json
if (-not $session.menu) { throw 'Not a menu session: prepare it with tools/prepare-coop-menu.py.' }
$logDir = Join-Path $workspace "builds\logs\$Task\run-$Run"
if (Test-Path $logDir) { throw "Preserving existing run: $logDir" }
if (@(Get-Process -Name 'AnomalyDX11', 'AnomalyCampaignServer' -ErrorAction SilentlyContinue).Count) {
    throw 'A client or campaign server is already running; close it first.'
}
New-Item -ItemType Directory $logDir -Force | Out-Null
# the server's exe needs the SDK DLLs beside it (as Run-CoopSession.ps1 does)
$serverExeDir = Split-Path $session.menu.server_exe -Parent
foreach ($dll in Get-ChildItem (Join-Path $workspace 'builds\build_1\engine-work\sdk\binaries') -Filter '*.dll') {
    $dest = Join-Path $serverExeDir $dll.Name
    if (-not (Test-Path $dest)) { Copy-Item $dll.FullName $dest }
}
$timer = [System.Diagnostics.Stopwatch]::StartNew()
$events = New-Object System.Collections.Generic.List[object]
function Note([string]$what) { $events.Add([ordered]@{ t = [math]::Round($timer.Elapsed.TotalSeconds, 1); event = $what }) }
$starts = @($ClientStarts.Split(',') | ForEach-Object { [int]$_ })
$names = @('c1', 'c2')
$result = [ordered]@{ started_utc = [DateTime]::UtcNow.ToString('o'); run = $Run; task = $Task; clients = [ordered]@{} }
function Run-Phase([int]$phase) {
    $clients = @{}
    if ($session.menu.dedicated) {
        # the dedicated server's console (tools/Host-CoopServer.ps1), hidden; -Continue in phase 2
        Remove-Item (Join-Path $session.menu.server_runtime 'appdata\coop-host-info.txt') -ErrorAction SilentlyContinue
        $arguments = '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File "' +
            (Join-Path $workspace 'tools\Host-CoopServer.ps1') + '" -Runtime "' + $session.menu.server_runtime + '"'
        if ($phase -gt 1) { $arguments += ' -Continue' }
        $hostConsole = Start-Process powershell.exe -ArgumentList $arguments -PassThru -WindowStyle Hidden
        Note "server-console-started#$phase"
    }
    $phaseStart = $timer.Elapsed.TotalSeconds
    while ($timer.Elapsed.TotalSeconds - $phaseStart -lt $Seconds) {
        for ($i = 0; $i -lt 2; $i++) {
            $name = $names[$i]
            if (-not $clients.ContainsKey($name) -and $timer.Elapsed.TotalSeconds - $phaseStart -ge $starts[$i]) {
                $proc = $session.processes.$name
                $runtime = Join-Path $workspace ($proc.runtime -replace '/', '\')
                if (Test-Path $proc.stop_file) { Remove-Item $proc.stop_file }
                $exe = Join-Path $runtime 'bin\AnomalyDX11.exe'
                $start = New-Object System.Diagnostics.ProcessStartInfo
                $start.FileName = $exe
                $start.Arguments = '-fsltx ' + $proc.fsgame + ' -nointro'
                $start.WorkingDirectory = $runtime
                $start.UseShellExecute = $false
                $start.EnvironmentVariables['TEMP'] = Join-Path $runtime 'tmp'
                $start.EnvironmentVariables['TMP'] = Join-Path $runtime 'tmp'
                $clients[$name] = [System.Diagnostics.Process]::Start($start)
                Note "$name-started#$phase"
                $result.clients["$name#$phase"] = [ordered]@{ command = '"' + $exe + '" ' + $start.Arguments }
            }
        }
        Start-Sleep -Milliseconds 500
    }
    foreach ($name in $names) {
        New-Item -ItemType File $session.processes.$name.stop_file -Force | Out-Null
        Note "$name-stop-file#$phase"
    }
    $deadline = $timer.Elapsed.TotalSeconds + $AfterSeconds
    while ($timer.Elapsed.TotalSeconds -lt $deadline -and (@($clients.Values | Where-Object { -not $_.HasExited }).Count -or
            @(Get-Process -Name 'AnomalyCampaignServer' -ErrorAction SilentlyContinue).Count)) {
        Start-Sleep -Milliseconds 500
    }
    New-Item -ItemType File $session.processes.server.stop_file -Force | Out-Null
    Start-Sleep -Seconds 5
    foreach ($name in $names) {
        $p = $clients[$name]
        if ($p -and -not $p.HasExited) { $p.Kill(); Note "$name-killed#$phase" }
        if ($p) { $p.WaitForExit(); $result.clients["$name#$phase"].exit_code = $p.ExitCode }
    }
    foreach ($p in @(Get-Process -Name 'AnomalyCampaignServer' -ErrorAction SilentlyContinue)) { $p.Kill(); Note "server-killed#$phase" }
    foreach ($name in @('server', 'c1', 'c2')) {
        $runtime = Join-Path $workspace ($session.processes.$name.runtime -replace '/', '\')
        $dest = Join-Path $logDir ("$name" + $(if ($phase -gt 1) { "-$phase" } else { '' }))
        New-Item -ItemType Directory $dest -Force | Out-Null
        Get-ChildItem (Join-Path $runtime 'appdata\logs') -File -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item $_.FullName $dest }
        # the menu's and the server's files, not the join code (a secret the host gives its friend)
        Get-ChildItem (Join-Path $runtime 'appdata') -File -Filter 'coop-*' -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -notmatch 'join-code|host-info' } | ForEach-Object { Copy-Item $_.FullName $dest }
    }
}
Run-Phase 1
if ($Phases -ge 2) {
    # the second evening: the host continues the last campaign; the friend (a roster player now) rejoins
    foreach ($name in $names) {
        $runtime = Join-Path $workspace ($session.processes.$name.runtime -replace '/', '\')
        $menu = Join-Path $runtime 'appdata\coop-menu.ltx'
        (Get-Content $menu) -replace '^test = host_new$', 'test = host_continue' | Set-Content $menu  # (a hosting client)
        Remove-Item (Join-Path $runtime 'appdata\coop-host-info.txt') -ErrorAction SilentlyContinue
        Get-ChildItem (Join-Path $runtime 'appdata\logs') -File -ErrorAction SilentlyContinue | Remove-Item
    }
    Note 'phase-2'
    Run-Phase 2
}
$result.events = $events
$result.finished_utc = [DateTime]::UtcNow.ToString('o')
$result | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $logDir 'result.json')
Write-Host "logs in $logDir"
