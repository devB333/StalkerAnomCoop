# S06-T06: make a fixture save from a new single-player game, unattended, with the accepted
# unmodified client. Prepare the isolated runtime first with
#   python3 tools/prepare-stage2-smoke.py <run> S06-T06
# The new-game screen (ui_mm_faction_select.script UINewGame:OnStartGame) only writes
# [character_creation] in axr_options.ltx and runs "start server(all/single/alife/new)"
# (ui_main_menu.script main_menu:StartGame); the installed scripts apply those values on
# the actor's first update (faction, start location, name, loadout). This script writes
# the same values into the run's own copy of axr_options.ltx, sets the installed timed
# autosave (other/autosave_timer_new, minutes) and starts a new game. The installed
# game_autosave_new.script then saves "<user> - tempsave" through the native save path;
# that pair is the fixture. The accepted source-client runtime is only read.
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    [string]$Faction = 'stalker',
    [string]$Map = 'rookie_village',
    [string]$Name = 'Fixture',
    # The faction's default kit as the screen fills it (every "false" line of
    # [<faction>_loadout] and its parents in items\settings\new_game_loadouts.ltx).
    [Parameter(Mandatory = $true)][string]$Loadout,
    [int]$AutosaveMinutes = 1,
    # An extra start point "name,lvid,gvid,x,y,z" for -Map: appended to the run's own loose copy of
    # plugins\new_game_start_locations.ltx (the installed one, read from -StartLocations), so the
    # installed new-game code places the actor there as at any start point.
    [string]$ExtraStart = '',
    [string]$StartLocations = '',
    [int]$TimeoutSeconds = 900
)
$ErrorActionPreference = 'Stop'
$task = 'S06-T06'
$workspace = Split-Path $PSScriptRoot -Parent
$runtime = Join-Path $workspace "builds\stage2\test-install\$task\$Run"
$fixture = Join-Path $runtime 'fixture.json'
if (-not (Test-Path $fixture)) { throw "Prepare the isolated runtime first (prepare-stage2-smoke.py $Run $task)." }
$record = Get-Content $fixture -Raw | ConvertFrom-Json
$config = Join-Path $runtime 'fsgame.ltx'
if ((Get-FileHash $config -Algorithm SHA256).Hash.ToLower() -ne $record.config_sha256) { throw 'Runtime configuration changed.' }
$accepted = Join-Path $workspace 'builds\build_2\test-install\source-client'
$logDir = Join-Path $workspace "builds\logs\$task\run-$Run"
if (Test-Path $logDir) { throw "Preserving existing run: $logDir" }
$running = @(Get-Process -Name 'AnomalyDX11' -ErrorAction SilentlyContinue)
if ($running.Count) { throw 'A client is already running; close it first.' }

# bin: the accepted runtime's own (unmodified) client and DLLs.
$bin = Join-Path $runtime 'bin'
if (Test-Path $bin) { throw "Runtime already has a bin directory: $bin" }
Copy-Item (Join-Path $accepted 'bin') $bin -Recurse
foreach ($file in Get-ChildItem $accepted -Filter '*.mhr') { Copy-Item $file.FullName $runtime }

# Settings and shader cache, copied; windowed, no "Press any key" wait.
$appdata = Join-Path $runtime 'appdata'
$settings = Get-Content (Join-Path $accepted 'appdata\user.ltx') | ForEach-Object {
    if ($_ -match '^rs_screenmode ') { 'rs_screenmode windowed' }
    elseif ($_ -match '^vid_mode ') { 'vid_mode 800x600' }
    elseif ($_ -match '^keypress_on_start ') { 'keypress_on_start off' }
    else { $_ }
}
$settings | Set-Content -Encoding ASCII (Join-Path $appdata 'user.ltx')
Copy-Item (Join-Path $accepted 'appdata\shaders_cache') $appdata -Recurse
# The marsh fixture pair the runtime preparation copied is not used here.
Get-ChildItem (Join-Path $appdata 'savedgames') -File | Remove-Item

if ($ExtraStart) {
    $f = $ExtraStart.Split(',')
    if ($f.Count -ne 6 -or -not $StartLocations) { throw '-ExtraStart needs name,lvid,gvid,x,y,z and -StartLocations' }
    $plugins = Join-Path $runtime 'gamedata\configs\plugins'
    if (Test-Path (Join-Path $plugins 'new_game_start_locations.ltx')) { throw 'The runtime already overrides new_game_start_locations.ltx' }
    New-Item -ItemType Directory $plugins -Force | Out-Null
    $text = (Get-Content $StartLocations -Raw) + "`r`n[$($f[0])]`r`nlvid = $($f[1])`r`ngvid = $($f[2])`r`nx = $($f[3])`r`ny = $($f[4])`r`nz = $($f[5])`r`n"
    [System.IO.File]::WriteAllText((Join-Path $plugins 'new_game_start_locations.ltx'), $text, [System.Text.Encoding]::ASCII)
}

# The new-game values, as UINewGame:OnStartGame writes them with the screen's defaults
# (difficulty 1, economy 1, story mode and opened routes on, the other modes off).
$values = [ordered]@{
    new_game_difficulty = '1'; new_game_economy = '1'; new_game_economy_treasure = '1'
    new_game_story_mode = 'true'; new_game_opened_routes = 'true'
    new_game_character_name = $Name; new_game_faction = $Faction
    new_game_loadout = $Loadout; new_game_map = $Map
}
$options = Join-Path $runtime 'gamedata\configs\axr_options.ltx'
$section = ''
$seen = @{}
$lines = Get-Content $options | ForEach-Object {
    if ($_ -match '^\s*\[([^\]]+)\]') { $section = $Matches[1] }
    if ($section -eq 'character_creation' -and $_ -match '^(\s*)(\w+)(\s*)=' -and $values.Contains($Matches[2])) {
        $seen[$Matches[2]] = $true
        $Matches[1] + $Matches[2] + $Matches[3] + '= ' + $values[$Matches[2]]
    } elseif ($section -eq 'options' -and $_ -match '^(\s*)other/autosave_timer_new(\s*)=') {
        $seen['autosave'] = $true
        $Matches[1] + 'other/autosave_timer_new' + $Matches[2] + '= ' + $AutosaveMinutes
    } else { $_ }
}
foreach ($key in @($values.Keys) + @('autosave')) { if (-not $seen[$key]) { throw "axr_options.ltx has no $key line" } }
$lines | Set-Content -Encoding ASCII $options

New-Item -ItemType Directory $logDir -Force | Out-Null
Copy-Item $options (Join-Path $logDir 'axr_options-before.ltx')
$exe = Join-Path $bin 'AnomalyDX11.exe'
# -start must come last: the engine executes everything after it as one console command.
$arguments = '-fsltx ' + $config + ' -start server(all/single/alife/new) client(localhost)'
$result = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    command = '"' + $exe + '" ' + $arguments
    executable_sha256 = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLower()
    config_sha256 = $record.config_sha256
    runtime = $runtime
    new_game = $values
    autosave_minutes = $AutosaveMinutes
    extra_start = $ExtraStart
    start_locations_source = $StartLocations
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

# Wait for the timed autosave pair, then for both files to stop growing.
$saves = Join-Path $appdata 'savedgames'
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$pair = $null
while ((Get-Date) -lt $deadline -and -not $process.HasExited) {
    $scop = Get-ChildItem $saves -Filter '*tempsave.scop' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($scop) {
        $scoc = Join-Path $saves ($scop.BaseName + '.scoc')
        if (Test-Path $scoc) {
            $sizes = @($scop.Length, (Get-Item $scoc).Length)
            Start-Sleep -Seconds 5
            $scop.Refresh()
            if ($sizes[0] -eq $scop.Length -and $sizes[1] -eq (Get-Item $scoc).Length -and $sizes[0] -gt 0) { $pair = $scop; break }
            continue
        }
    }
    Start-Sleep -Seconds 2
}
$result.timed_out = -not $pair
$result.exited_early = $process.HasExited
if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
$result.finished_utc = [DateTime]::UtcNow.ToString('o')
if ($pair) {
    $result.save = @()
    foreach ($extension in @('scop', 'scoc', 'dds')) {
        $file = Join-Path $saves ($pair.BaseName + '.' + $extension)
        if (-not (Test-Path $file)) { continue }
        Copy-Item $file $logDir
        $result.save += [ordered]@{ name = (Split-Path $file -Leaf); bytes = (Get-Item $file).Length; sha256 = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower() }
    }
}
$clientLog = Get-ChildItem (Join-Path $appdata 'logs') -Filter '*.log' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($clientLog) { Copy-Item $clientLog.FullName (Join-Path $logDir 'client.log') }
$result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $logDir 'result.json')
$result | ConvertTo-Json -Depth 5
if (-not $pair) { exit 1 }
exit 0
