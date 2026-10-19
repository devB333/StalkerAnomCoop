param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    [Parameter(Mandatory = $true)][ValidateSet('Core', 'Identity', 'Services', 'GameState')][string]$Scope,
    # Test-only: report this service as failed before it starts, to check the partial-start unwind.
    [ValidateSet('', 'identity', 'sound', 'cpu-models', 'spatial-db', 'materials', 'script-engine', 'campaign-registries', 'application', 'persistent', 'frame-driver')][string]$InjectFailure = '',
    # Optional comma-separated model list for the CPU model fixture (no spaces).
    [ValidatePattern('^[A-Za-z0-9_\\,.]*$')][string]$Models = '',
    # Test-only: the model list contains a missing model, so the fixture must fail (exit 5).
    [switch]$ExpectModelFailure
)
if ($ExpectModelFailure -and (-not $Models -or $Scope -ne 'Services')) { throw '-ExpectModelFailure requires -Models and -Scope Services.' }
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
$mode = switch ($Scope) { 'Core' { '-campaign-core-smoke' } 'Identity' { '-campaign-identity-smoke' } 'Services' { '-campaign-services-smoke' } default { '-campaign-smoke' } }
# No sound device: the engine's CPU sound core (-snd_cpu) runs emitters and AI sound events (S02-T05c).
$arguments = $mode + ' -snd_cpu -fsltx ' + $config
if ($InjectFailure) { $arguments += ' -campaign-fail-at ' + $InjectFailure }
if ($Models) { $arguments += ' -campaign-models ' + $Models }
$result = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    command = '"' + $exe + '" ' + $arguments
    executable_sha256 = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLower()
    config_sha256 = $record.config_sha256
    runtime = $runtime
    scope = $Scope
    injected_failure = $InjectFailure
    models = $Models
    expect_model_failure = [bool]$ExpectModelFailure
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
# GameState loads a saved game (300 s deadlines inside the process for the server and client stages),
# then saves a checkpoint and loads the level a second time from it (S02-T06d), completes a quest and
# does it once more (S02-T06e).
$timeoutMs = if ($Scope -eq 'GameState') { 900000 } else { 120000 }
if (-not $process.WaitForExit($timeoutMs)) {
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
if ($Scope -eq 'Identity') {
    # S03-T01: the identity registry and its unit fixtures on the core only (no level).
    $expectedStages += @('identity-ready', 'identity-engine-id-fixture-passed', 'identity-slot-reuse-fixture-passed', 'identity-disconnect-retention-fixture-passed', 'identity-role-context-fixture-passed', 'identity-actor-access-fixture-passed', 'identity-message-policy-fixture-passed', 'identity-stopped')
}
if ($Scope -eq 'Services' -or $Scope -eq 'GameState') {
    # Services start in native order and stop in reverse; a failed start unwinds only started services.
    $started = @()
    foreach ($service in @('identity', 'sound', 'cpu-models', 'spatial-db', 'materials', 'script-engine', 'campaign-registries', 'application', 'persistent', 'frame-driver')) {
        if ($service -eq $InjectFailure) { $expectedStages += "$service-failed"; $expectedExit = 4; break }
        $expectedStages += "$service-ready"
        $started += $service
    }
    if ($ExpectModelFailure) { $expectedStages += 'cpu-model-fixture-failed'; $expectedExit = 5 }
    elseif (-not $InjectFailure) {
        $expectedStages += @('cpu-model-fixture-passed', 'persistent-fixture-passed', 'frame-driver-fixture-passed', 'bone-capture-passed', 'services-graphics-window-input-absent')
        if ($Scope -eq 'GameState') { $expectedStages += @('campaign-level-created', 'campaign-server-ready', 'campaign-level-loaded', 'campaign-ticked', 'campaign-identity-passed', 'campaign-authority-passed', 'campaign-collision-passed', 'campaign-physics-passed', 'campaign-activity-passed', 'campaign-hearing-passed', 'campaign-actor-position-passed', 'campaign-combat-passed', 'campaign-quest-given', 'campaign-checkpoint-saved', 'campaign-stopped', 'campaign-restart-server-ready', 'campaign-restart-level-loaded', 'campaign-checkpoint-passed', 'campaign-quest-restored', 'campaign-quest-completed', 'campaign-quest-checkpoint-saved', 'campaign-stopped', 'campaign-quest-restart-server-ready', 'campaign-quest-restart-level-loaded', 'campaign-quest-checkpoint-passed', 'campaign-quest-persisted', 'campaign-two-actors-passed', 'campaign-activation-passed', 'campaign-stopped') }
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
# S02-T03d: the server's collision capture, kept beside the stage log.
$capturePath = Join-Path $runtime 'appdata\logs\s02-t03d-collision.csv'
$result.collision_capture_present = Test-Path $capturePath
if ($result.collision_capture_present) {
    Copy-Item $capturePath (Join-Path $logDir 'server-collision.csv')
    $result.collision_capture_sha256 = (Get-FileHash $capturePath -Algorithm SHA256).Hash.ToLower()
}
# S02-T04b: the server's falling-body physics capture.
$physicsPath = Join-Path $runtime 'appdata\logs\s02-t04b-physics.csv'
$result.physics_capture_present = Test-Path $physicsPath
if ($result.physics_capture_present) {
    Copy-Item $physicsPath (Join-Path $logDir 'server-physics.csv')
    $result.physics_capture_sha256 = (Get-FileHash $physicsPath -Algorithm SHA256).Hash.ToLower()
}
# S02-T04c: the server's activity capture (objects, scheduler, AI, ALife, stalker walk).
$activityPath = Join-Path $runtime 'appdata\logs\s02-t04c-activity.csv'
$result.activity_capture_present = Test-Path $activityPath
if ($result.activity_capture_present) {
    Copy-Item $activityPath (Join-Path $logDir 'server-activity.csv')
    $result.activity_capture_sha256 = (Get-FileHash $activityPath -Algorithm SHA256).Hash.ToLower()
}
# S02-T05c: the server's hearing capture (sound emitters, AI sound events, NPC hearing).
$hearingPath = Join-Path $runtime 'appdata\logs\s02-t05c-hearing.csv'
$result.hearing_capture_present = Test-Path $hearingPath
if ($result.hearing_capture_present) {
    Copy-Item $hearingPath (Join-Path $logDir 'server-hearing.csv')
    $result.hearing_capture_sha256 = (Get-FileHash $hearingPath -Algorithm SHA256).Hash.ToLower()
}
# S02-T06c: the server's combat capture (spawned monster, vision, bullets, impacts, hits, deaths).
$combatPath = Join-Path $runtime 'appdata\logs\s02-t06c-combat.csv'
$result.combat_capture_present = Test-Path $combatPath
if ($result.combat_capture_present) {
    Copy-Item $combatPath (Join-Path $logDir 'server-combat.csv')
    $result.combat_capture_sha256 = (Get-FileHash $combatPath -Algorithm SHA256).Hash.ToLower()
}
# S02-T06d: the server's checkpoint capture (snapshots before the save, after it, after the reload and
# after the reloaded level's first ticks) and the checkpoint it wrote and restarted from.
$checkpointPath = Join-Path $runtime 'appdata\logs\s02-t06d-checkpoint.csv'
$result.checkpoint_capture_present = Test-Path $checkpointPath
if ($result.checkpoint_capture_present) {
    Copy-Item $checkpointPath (Join-Path $logDir 'server-checkpoint.csv')
    $result.checkpoint_capture_sha256 = (Get-FileHash $checkpointPath -Algorithm SHA256).Hash.ToLower()
}
# S02-T06e: the quest capture (rows before-give, given, ticked, restored, completed, persisted), the second
# checkpoint's capture and the second checkpoint itself.
$questPath = Join-Path $runtime 'appdata\logs\s02-t06e-quest.csv'
$result.quest_capture_present = Test-Path $questPath
if ($result.quest_capture_present) {
    Copy-Item $questPath (Join-Path $logDir 'server-quest.csv')
    $result.quest_capture_sha256 = (Get-FileHash $questPath -Algorithm SHA256).Hash.ToLower()
}
$questCheckpointPath = Join-Path $runtime 'appdata\logs\s02-t06e-checkpoint.csv'
if (Test-Path $questCheckpointPath) { Copy-Item $questCheckpointPath (Join-Path $logDir 'server-quest-checkpoint.csv') }
# S03-T03: the two-actor fixture's capture (state per phase, the save-ready participant map, every check).
$twoActorsPath = Join-Path $runtime 'appdata\logs\s03-t03-two-actors.csv'
$result.two_actors_capture_present = Test-Path $twoActorsPath
if ($result.two_actors_capture_present) {
    Copy-Item $twoActorsPath (Join-Path $logDir 'server-two-actors.csv')
    $result.two_actors_capture_sha256 = (Get-FileHash $twoActorsPath -Algorithm SHA256).Hash.ToLower()
}
# S03-T04: the activation fixture's capture (ticks, switches, the rule checks, the cost windows, every check).
$activationPath = Join-Path $runtime 'appdata\logs\s03-t04-activation.csv'
$result.activation_capture_present = Test-Path $activationPath
if ($result.activation_capture_present) {
    Copy-Item $activationPath (Join-Path $logDir 'server-activation.csv')
    $result.activation_capture_sha256 = (Get-FileHash $activationPath -Algorithm SHA256).Hash.ToLower()
}
$result.checkpoint_saves = @()
foreach ($name in @('s02-t06d-checkpoint', 's02-t06e-checkpoint')) {
    foreach ($extension in @('scop', 'scoc')) {
        $savePath = Join-Path $runtime "appdata\savedgames\$name.$extension"
        if (Test-Path $savePath) {
            Copy-Item $savePath (Join-Path $logDir "$name.$extension")
            $result.checkpoint_saves += [ordered]@{ name = "$name.$extension"; bytes = (Get-Item $savePath).Length; sha256 = (Get-FileHash $savePath -Algorithm SHA256).Hash.ToLower() }
        }
    }
}
$result.stages = $observedStages
$result.expected_stages = $expectedStages
$result.expected_exit_code = $expectedExit
$result.stages_passed = ($observedStages -join '|') -eq ($expectedStages -join '|')
$result.outcome_as_expected = $result.exit_code -eq $expectedExit -and -not $result.timed_out -and $result.stages_passed
# A GameState pass means the renderer-free level load, spawn and first ticks completed (S02-T05d); it is not G02.
$result.passed = $result.outcome_as_expected
$result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $logDir 'result.json')
if (Test-Path $stagePath) { Get-Content $stagePath }
if ($result.timed_out) { exit 124 }
if ($null -eq $result.exit_code) { throw 'Smoke process returned no exit code; see result.json.' }
if (-not $result.outcome_as_expected) { throw 'Smoke stages or exit code differ from the expected outcome; see result.json.' }
if ($result.passed) { exit 0 }
exit $process.ExitCode
