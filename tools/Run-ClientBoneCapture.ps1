# S02-T03c: run the rebuilt normal client once with -s02-bone-capture in an isolated
# runtime, then collect its bone capture. Prepare the runtime first with
#   python3 tools/prepare-stage2-smoke.py <run> S02-T03c
# The accepted source-client runtime is only read: its bin DLLs, user settings and
# shader cache are copied, and the archives are read in place through fsgame.ltx.
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    [int]$TimeoutSeconds = 300
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$runtime = Join-Path $workspace "builds\stage2\test-install\S02-T03c\$Run"
$fixture = Join-Path $runtime 'fixture.json'
if (-not (Test-Path $fixture)) { throw 'Prepare the isolated runtime first (prepare-stage2-smoke.py <run> S02-T03c).' }
$record = Get-Content $fixture -Raw | ConvertFrom-Json
$config = Join-Path $runtime 'fsgame.ltx'
if ((Get-FileHash $config -Algorithm SHA256).Hash.ToLower() -ne $record.config_sha256) { throw 'Runtime configuration changed.' }
$accepted = Join-Path $workspace 'builds\build_2\test-install\source-client'
$built = Join-Path $workspace 'builds\build_1\engine-work\_build\_game\bin_dbg'
$logDir = Join-Path $workspace "builds\logs\S02-T03c\run-$Run"
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
$arguments = '-fsltx ' + $config + ' -s02-bone-capture'
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
$capture = Join-Path $appdata 'logs\s02-t03c-bones.csv'
$result.capture_present = Test-Path $capture
if ($result.capture_present) {
    Copy-Item $capture (Join-Path $logDir 'client-bones.csv')
    $result.capture_sha256 = (Get-FileHash $capture -Algorithm SHA256).Hash.ToLower()
}
$result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $logDir 'result.json')
$result | ConvertTo-Json -Depth 5
if ($result.timed_out -or -not $result.capture_present) { exit 1 }
exit 0
