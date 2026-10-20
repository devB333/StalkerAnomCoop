# S05: run one co-op session prepared by tools/prepare-coop-session.py: the
# campaign server (-campaign-serve) and its clients, each in its own isolated
# runtime. Timeline (seconds):
#   server start -> "server-listening" in its co-op log (level loaded)
#   + ServerAloneSeconds            the server runs with no client
#   + ClientStarts[i]               client i starts (offsets from that point)
#   + ClientSeconds[i] later        client i's stop file: it quits cleanly
#   last client stopped + AfterSeconds: the server's stop file
# Logs of every process go to builds/logs/<task>/run-<run>/<process>/.
# Optional -Restart 'c2:40:30' restarts client c2 40 s after its stop file,
# for 30 s more (rejoin), repeated per comma-separated entry.
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    [ValidatePattern('^S[0-9]{2}-T[0-9]{2}[a-z]?$')][string]$Task = 'S05-T01',
    [int]$ServerAloneSeconds = 20,
    [string]$ClientStarts = '0,15',
    [string]$ClientSeconds = '90,75',
    [int]$AfterSeconds = 15,
    [string]$Restart = '',
    [int]$LoadTimeoutSeconds = 420
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$sessionDir = Join-Path $workspace "builds\stage5\$Task\$Run"
$session = Get-Content (Join-Path $sessionDir 'session.json') -Raw | ConvertFrom-Json
$logDir = Join-Path $workspace "builds\logs\$Task\run-$Run"
if (Test-Path $logDir) { throw "Preserving existing run: $logDir" }
if (@(Get-Process -Name 'AnomalyDX11', 'AnomalyCampaignServer' -ErrorAction SilentlyContinue).Count) {
    throw 'A client or campaign server is already running; close it first.'
}
New-Item -ItemType Directory $logDir -Force | Out-Null

$clientNames = @($session.processes.PSObject.Properties.Name | Where-Object { $_ -ne 'server' } | Sort-Object)
$starts = @($ClientStarts.Split(',') | ForEach-Object { [int]$_ })
$durations = @($ClientSeconds.Split(',') | ForEach-Object { [int]$_ })
if ($starts.Count -lt $clientNames.Count -or $durations.Count -lt $clientNames.Count) { throw 'One start and one duration per client.' }

# The server runs from its build output, like the GameState smoke.
$serverExeDir = Join-Path $workspace 'builds\stage2\campaign\_game\bin_dbg'
$serverExe = Join-Path $serverExeDir 'AnomalyCampaignServer.exe'
$sdkBinaries = Join-Path $workspace 'builds\build_1\engine-work\sdk\binaries'
foreach ($dll in Get-ChildItem $sdkBinaries -Filter '*.dll') {
    $dest = Join-Path $serverExeDir $dll.Name
    if (-not (Test-Path $dest)) { Copy-Item $dll.FullName $dest }
}

function Start-CoopProcess([string]$name, [string]$exe, [string]$arguments, [string]$runtime) {
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $exe
    $start.Arguments = $arguments
    $start.WorkingDirectory = $runtime
    $start.UseShellExecute = $false
    $start.CreateNoWindow = ($name -eq 'server')
    $start.EnvironmentVariables['TEMP'] = Join-Path $runtime 'tmp'
    $start.EnvironmentVariables['TMP'] = Join-Path $runtime 'tmp'
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $start
    if (-not $process.Start()) { throw "$name did not start." }
    return $process
}

$timer = [System.Diagnostics.Stopwatch]::StartNew()
$events = New-Object System.Collections.Generic.List[object]
function Note([string]$what) { $events.Add([ordered]@{ t = [math]::Round($timer.Elapsed.TotalSeconds, 1); utc = [DateTime]::UtcNow.ToString('o'); event = $what }) }

$server = $session.processes.server
$serverRuntime = Join-Path $workspace ($server.runtime -replace '/', '\')
$serverArgs = '-campaign-serve -snd_cpu -fsltx ' + $server.fsgame + ' -coop-config ' + $server.profile
$result = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    run = $Run; task = $Task
    server_command = '"' + $serverExe + '" ' + $serverArgs
    server_exe_sha256 = (Get-FileHash $serverExe -Algorithm SHA256).Hash.ToLower()
    clients = [ordered]@{}
}
$serverProcess = Start-CoopProcess 'server' $serverExe $serverArgs $serverRuntime
Note 'server-started'
$serverLog = Join-Path $serverRuntime 'appdata\logs\coop-server.log'
$listening = $false
while ($timer.Elapsed.TotalSeconds -lt $LoadTimeoutSeconds -and -not $serverProcess.HasExited) {
    if ((Test-Path $serverLog) -and (Select-String -Path $serverLog -Pattern 'server-listening' -Quiet)) { $listening = $true; break }
    Start-Sleep -Milliseconds 500
}
$result.server_listening = $listening
if ($listening) { Note 'server-listening' }

$clients = @{}
$plan = @()
if ($listening) {
    $base = $timer.Elapsed.TotalSeconds + $ServerAloneSeconds
    for ($i = 0; $i -lt $clientNames.Count; $i++) {
        $plan += [pscustomobject]@{ name = $clientNames[$i]; start = $base + $starts[$i]; stop = $base + $starts[$i] + $durations[$i]; started = $false; stopped = $false; restarts = 0 }
    }
    foreach ($entry in @($Restart.Split(',') | Where-Object { $_ })) {
        $parts = $entry.Split(':')
        $first = $plan | Where-Object { $_.name -eq $parts[0] -and $_.restarts -eq 0 } | Select-Object -First 1
        $at = $first.stop + [int]$parts[1]
        $plan += [pscustomobject]@{ name = $parts[0]; start = $at; stop = $at + [int]$parts[2]; started = $false; stopped = $false; restarts = 1 }
    }
    $serverStopAt = ($plan | Measure-Object -Property stop -Maximum).Maximum + $AfterSeconds
    while (-not $serverProcess.HasExited) {
        $now = $timer.Elapsed.TotalSeconds
        foreach ($p in $plan) {
            $proc = $session.processes.($p.name)
            $runtime = Join-Path $workspace ($proc.runtime -replace '/', '\')
            if (-not $p.started -and $now -ge $p.start) {
                if ($p.restarts -gt 0) {
                    # a rejoin: wait for the first run to exit, keep its logs
                    $previous = $clients["$($p.name)#0"]
                    if ($previous -and -not $previous.HasExited) { continue }
                    $keep = Join-Path $logDir "$($p.name)-first-run"
                    New-Item -ItemType Directory $keep -Force | Out-Null
                    Get-ChildItem (Join-Path $runtime 'appdata\logs') -File -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item $_.FullName $keep }
                }
                if (Test-Path $proc.stop_file) { Remove-Item $proc.stop_file }
                $exe = Join-Path $runtime 'bin\AnomalyDX11.exe'
                $arguments = '-fsltx ' + $proc.fsgame + ' -coop-config ' + $proc.profile + ' -nointro'
                $clients["$($p.name)#$($p.restarts)"] = Start-CoopProcess $p.name $exe $arguments $runtime
                $p.started = $true
                Note "$($p.name)-started#$($p.restarts)"
                $result.clients["$($p.name)#$($p.restarts)"] = [ordered]@{ command = '"' + $exe + '" ' + $arguments; exe_sha256 = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLower() }
            }
            if ($p.started -and -not $p.stopped -and $now -ge $p.stop) {
                New-Item -ItemType File $proc.stop_file -Force | Out-Null
                $p.stopped = $true
                Note "$($p.name)-stop-file#$($p.restarts)"
            }
        }
        if ($now -ge $serverStopAt -and -not (Test-Path $server.stop_file)) {
            # every client must have exited first
            $running = @($clients.Values | Where-Object { -not $_.HasExited })
            if ($running.Count -eq 0 -or $now -ge $serverStopAt + 60) {
                New-Item -ItemType File $server.stop_file -Force | Out-Null
                Note 'server-stop-file'
            }
        }
        if ($now -ge $serverStopAt + 240) { Note 'server-kill'; $serverProcess.Kill(); break }
        Start-Sleep -Milliseconds 250
    }
}
if (-not $serverProcess.HasExited) {
    New-Item -ItemType File $server.stop_file -Force | Out-Null
    if (-not $serverProcess.WaitForExit(120000)) { $serverProcess.Kill(); Note 'server-kill' }
}
$serverProcess.WaitForExit()
Note 'server-exited'
foreach ($key in $clients.Keys) {
    $c = $clients[$key]
    if (-not $c.WaitForExit(60000)) { $c.Kill(); Note "$key-kill" }
    $result.clients[$key].exit_code = $c.ExitCode
}
$result.server_exit_code = $serverProcess.ExitCode
$result.events = $events
$result.finished_utc = [DateTime]::UtcNow.ToString('o')

# Logs, per process.
foreach ($name in $session.processes.PSObject.Properties.Name) {
    $runtime = Join-Path $workspace ($session.processes.$name.runtime -replace '/', '\')
    $dest = Join-Path $logDir $name
    New-Item -ItemType Directory $dest -Force | Out-Null
    Get-ChildItem (Join-Path $runtime 'appdata\logs') -File -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item $_.FullName $dest }
    Get-ChildItem (Join-Path $runtime 'appdata\screenshots') -File -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item $_.FullName $dest }
}
$result | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $logDir 'result.json')
$result | ConvertTo-Json -Depth 6
