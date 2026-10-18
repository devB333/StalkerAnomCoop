param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    [Parameter(Mandatory = $true)][ValidateSet('Core', 'Services', 'GameState')][string]$Scope,
    # Test-only: report this service as failed before it starts, to check the partial-start unwind.
    [ValidateSet('', 'spatial-db', 'campaign-registries', 'script-engine')][string]$InjectFailure = ''
)
if ($InjectFailure -and $Scope -ne 'Services') { throw '-InjectFailure requires -Scope Services.' }
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$runtime = Join-Path $workspace "builds\stage2\test-install\S02-T02\$Run"
$fixture = Join-Path $runtime 'fixture.json'
if (-not (Test-Path $fixture)) { throw 'Prepare the isolated smoke fixture first.' }
$exeDir = Join-Path $workspace 'builds\stage2\campaign\_game\bin_dbg'
$exe = Join-Path $exeDir 'AnomalyCampaignServer.exe'
if (-not (Test-Path $exe)) { throw "Dedicated executable missing: $exe" }
$config = Join-Path $runtime 'fsgame.ltx'
$record = Get-Content $fixture -Raw | ConvertFrom-Json
if ((Get-FileHash $config -Algorithm SHA256).Hash.ToLower() -ne $record.config_sha256) { throw 'Smoke configuration changed.' }
$sdkBinaries = Join-Path $workspace 'builds\build_1\engine-work\sdk\binaries'
foreach ($dll in Get-ChildItem $sdkBinaries -Filter '*.dll') {
    $dest = Join-Path $exeDir $dll.Name
    if (Test-Path $dest) {
        if ((Get-FileHash $dest -Algorithm SHA256).Hash -ne (Get-FileHash $dll.FullName -Algorithm SHA256).Hash) { throw "Different existing DLL: $dest" }
    } else { Copy-Item $dll.FullName $dest }
}
$logDir = Join-Path $workspace "builds\logs\S02-T02\run-$Run"
if (Test-Path $logDir) { throw "Preserving existing smoke run: $logDir" }
New-Item -ItemType Directory $logDir -Force | Out-Null
$env:TEMP = Join-Path $runtime 'tmp'
$env:TMP = $env:TEMP
$mode = switch ($Scope) { 'Core' { '-campaign-core-smoke' } 'Services' { '-campaign-services-smoke' } default { '-campaign-smoke' } }
$arguments = $mode + ' -fsltx ' + $config
if ($InjectFailure) { $arguments += ' -campaign-fail-at ' + $InjectFailure }
$result = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    command = '"' + $exe + '" ' + $arguments
    executable_sha256 = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLower()
    config_sha256 = $record.config_sha256
    runtime = $runtime
    scope = $Scope
    injected_failure = $InjectFailure
    level_loaded = $false
}
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = $exe
$start.Arguments = $arguments
$start.WorkingDirectory = $runtime
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $start
if (-not $process.Start()) { throw 'Smoke process did not start.' }
if (-not $process.WaitForExit(120000)) {
    $process.Kill()
    $process.WaitForExit()
    $result.timed_out = $true
} else { $result.timed_out = $false }
$process.Refresh()
$result.process_id = $process.Id
$result.process_type = $process.GetType().FullName
$result.has_exited = $process.HasExited
$result.exit_code = $process.ExitCode
$result.finished_utc = [DateTime]::UtcNow.ToString('o')
$stagePath = Join-Path $runtime 'appdata\logs\campaign-smoke-stages.log'
$expectedStages = @('core-ready', 'settings-ready', 'scheduler-ready', 'console-ready',
    'game-factory-ready', 'graphics-window-input-absent')
$expectedExit = 0
if ($Scope -ne 'Core') {
    # Services start in native order and stop in reverse; a failed start unwinds only started services.
    $started = @()
    foreach ($service in @('spatial-db', 'script-engine', 'campaign-registries')) {
        if ($service -eq $InjectFailure) { $expectedStages += "$service-failed"; $expectedExit = 4; break }
        $expectedStages += "$service-ready"
        $started += $service
    }
    if (-not $InjectFailure) {
        $expectedStages += 'services-graphics-window-input-absent'
        if ($Scope -eq 'GameState') { $expectedStages += 'persistent-context-required'; $expectedExit = 3 }
    }
    [array]::Reverse($started)
    foreach ($service in $started) { $expectedStages += "$service-stopped" }
}
$expectedStages += @('engine-stopped', 'settings-stopped', 'shutdown-complete')
$observedStages = @()
if (Test-Path $stagePath) {
    $observedStages = @(Get-Content $stagePath | ForEach-Object { $_ -replace '^S02-T02 stage=', '' })
    Copy-Item $stagePath (Join-Path $logDir 'stages.log')
}
$result.stages = $observedStages
$result.expected_stages = $expectedStages
$result.expected_exit_code = $expectedExit
$result.stages_passed = ($observedStages -join '|') -eq ($expectedStages -join '|')
$result.outcome_as_expected = $result.exit_code -eq $expectedExit -and -not $result.timed_out -and $result.stages_passed
# GameState still reports an unmet prerequisite, so it never counts as a pass.
$result.passed = $result.outcome_as_expected -and $Scope -ne 'GameState'
$result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $logDir 'result.json')
if (Test-Path $stagePath) { Get-Content $stagePath }
if ($result.timed_out) { exit 124 }
if ($null -eq $result.exit_code) { throw 'Smoke process returned no exit code; see result.json.' }
if (-not $result.outcome_as_expected) { throw 'Smoke stages or exit code differ from the expected outcome; see result.json.' }
if ($result.passed) { exit 0 }
exit $process.ExitCode
