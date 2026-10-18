# S02-T03d: run the rebuilt normal client once in an isolated runtime, let it load
# the fixture save through the console start command, capture the level's
# collision forms, shells and hit picks one second after the actor spawned
# (-s02-collision-capture), then collect the capture. Prepare the runtime first with
#   python3 tools/prepare-stage2-smoke.py <run> S02-T03d
# With -Capture physics (S02-T04b) the client runs the falling-body fixture instead
# (-s02-physics-capture); prepare the runtime with task S02-T04b. With -Capture activity
# (S02-T04c) it observes the level and walks a stalker (-s02-activity-capture); task S02-T04c.
# The accepted source-client runtime is only read: its bin DLLs, user settings and
# shader cache are copied, and the archives are read in place through fsgame.ltx.
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    [ValidateSet('collision', 'physics', 'activity')][string]$Capture = 'collision',
    [int]$TimeoutSeconds = 600
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$task = switch ($Capture) { 'physics' { 'S02-T04b' } 'activity' { 'S02-T04c' } default { 'S02-T03d' } }
$flag = switch ($Capture) { 'physics' { '-s02-physics-capture' } 'activity' { '-s02-activity-capture' } default { '-s02-collision-capture' } }
$captureFile = switch ($Capture) { 'physics' { 's02-t04b-physics.csv' } 'activity' { 's02-t04c-activity.csv' } default { 's02-t03d-collision.csv' } }
$runtime = Join-Path $workspace "builds\stage2\test-install\$task\$Run"
$fixture = Join-Path $runtime 'fixture.json'
if (-not (Test-Path $fixture)) { throw "Prepare the isolated runtime first (prepare-stage2-smoke.py <run> $task)." }
$record = Get-Content $fixture -Raw | ConvertFrom-Json
$config = Join-Path $runtime 'fsgame.ltx'
if ((Get-FileHash $config -Algorithm SHA256).Hash.ToLower() -ne $record.config_sha256) { throw 'Runtime configuration changed.' }
$accepted = Join-Path $workspace 'builds\build_2\test-install\source-client'
$built = Join-Path $workspace 'builds\build_1\engine-work\_build\_game\bin_dbg'
$logDir = Join-Path $workspace "builds\logs\$task\run-$Run"
if (Test-Path $logDir) { throw "Preserving existing run: $logDir" }

# Never start a second client or replace a running one.
$running = @(Get-Process -Name 'AnomalyDX11' -ErrorAction SilentlyContinue)
if ($running.Count) { throw 'A client is already running; close it first.' }

# bin: the rebuilt client plus the accepted runtime DLLs.
$bin = Join-Path $runtime 'bin'
if (Test-Path $bin) { throw "Runtime already has a bin directory: $bin" }
New-Item -ItemType Directory $bin | Out-Null
foreach ($file in Get-ChildItem (Join-Path $accepted 'bin') -File) {
    if ($file.Name -notin @('AnomalyDX11.exe', 'AnomalyDX11.pdb')) { Copy-Item $file.FullName $bin }
}
foreach ($name in @('AnomalyDX11.exe', 'AnomalyDX11.pdb')) { Copy-Item (Join-Path $built $name) $bin }
foreach ($file in Get-ChildItem $accepted -Filter '*.mhr') { Copy-Item $file.FullName $runtime }

# Settings and shader cache, copied; the copy runs in a small window.
$appdata = Join-Path $runtime 'appdata'
$settings = Get-Content (Join-Path $accepted 'appdata\user.ltx')
$settings = $settings | ForEach-Object {
    if ($_ -match '^rs_screenmode ') { 'rs_screenmode windowed' }
    elseif ($_ -match '^vid_mode ') { 'vid_mode 800x600' }
    else { $_ }
}
$settings | Set-Content -Encoding ASCII (Join-Path $appdata 'user.ltx')
Copy-Item (Join-Path $accepted 'appdata\shaders_cache') $appdata -Recurse

New-Item -ItemType Directory $logDir -Force | Out-Null
$exe = Join-Path $bin 'AnomalyDX11.exe'
# -start must come last: the engine executes everything after it as one console command.
$arguments = '-fsltx ' + $config + ' ' + $flag + ' -start server(player - autosave/single/alife/load) client(localhost)'
$result = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    command = '"' + $exe + '" ' + $arguments
    executable_sha256 = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLower()
    config_sha256 = $record.config_sha256
    runtime = $runtime
}
$env:TEMP = Join-Path $runtime 'tmp'
$env:TMP = $env:TEMP
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = $exe
$start.Arguments = $arguments
$start.WorkingDirectory = $runtime
$start.UseShellExecute = $false
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $start
if (-not $process.Start()) { throw 'Client did not start.' }
if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    $process.Kill()
    $process.WaitForExit()
    $result.timed_out = $true
} else { $result.timed_out = $false }
$process.Refresh()
$result.exit_code = $process.ExitCode
$result.finished_utc = [DateTime]::UtcNow.ToString('o')
$captureCsv = Join-Path $appdata "logs\$captureFile"
$result.capture_present = Test-Path $captureCsv
if ($result.capture_present) {
    Copy-Item $captureCsv (Join-Path $logDir "client-$Capture.csv")
    $result.capture_sha256 = (Get-FileHash $captureCsv -Algorithm SHA256).Hash.ToLower()
}
$clientLog = Get-ChildItem (Join-Path $appdata 'logs') -Filter '*.log' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($clientLog) {
    Copy-Item $clientLog.FullName (Join-Path $logDir 'client.log')
    $result.summary_lines = @(Select-String -Path $clientLog.FullName -Pattern $task | ForEach-Object { $_.Line })
}
$result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $logDir 'result.json')
$result | ConvertTo-Json -Depth 5
if ($result.timed_out -or -not $result.capture_present) { exit 1 }
exit 0
