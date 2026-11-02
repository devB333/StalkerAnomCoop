# S11-T00: test a host kit and a client kit together on this PC (tools/make-coop-kit.py), the way two players
# use them: the host kit's menu hosts a new campaign, the client kit's menu joins with the address and code the
# host's client wrote. For the test only, both kits run windowed (800x600, always active, no key press after
# loading) with the menu's test driver; their settings are restored afterwards. Logs: builds/logs/S11-T00/run-<run>.
#   powershell.exe ... -File C:\...\tools\Test-CoopKits.ps1 -Run kit-7 -HostKit C:\...\builds\kits\host-3
#   (a dedicated server: -ServerRuntime <a ServerHostKit's runtime> -ClientKit <kit 1> -ClientKit2 <kit 2>; in the
#   logs "host" is the second client kit then)
#       -ClientKit 'C:\...\a folder with spaces\friend-3' [-Seconds 280]
# The client kit must have been set up (Setup-Coop.ps1) first.
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    [string]$HostKit = '',
    [Parameter(Mandatory = $true)][string]$ClientKit,
    # a dedicated server instead of a host kit: its runtime (a ServerHostKit's), started with
    # tools\Host-CoopServer.ps1; then -ClientKit2 is the second player (both join with the server's code)
    [string]$ServerRuntime = '',
    [string]$ClientKit2 = '',
    [int]$Seconds = 280
)
$ErrorActionPreference = 'Stop'
$w = Split-Path $PSScriptRoot -Parent
$dedicated = [bool]$ServerRuntime
$host1 = if ($dedicated) { $ClientKit2 } else { Join-Path $HostKit 'client' }
$friend = $ClientKit
$info = if ($dedicated) { Join-Path $ServerRuntime 'appdata\coop-host-info.txt' } else { "$host1\appdata\coop-host-info.txt" }
$logDir = "$w\builds\logs\S11-T00\run-$Run"
if (Test-Path $logDir) { throw 'exists' }
New-Item -ItemType Directory $logDir | Out-Null
$saved = @{}
foreach ($k in @($host1, $friend)) {
    $saved["$k\appdata\user.ltx"] = Get-Content "$k\appdata\user.ltx" -Raw
    $saved["$k\appdata\coop-menu.ltx"] = Get-Content "$k\appdata\coop-menu.ltx" -Raw
    (Get-Content "$k\appdata\user.ltx") | ForEach-Object {
        if ($_ -like 'rs_screenmode *') { 'rs_screenmode windowed' } elseif ($_ -like 'vid_mode *') { 'vid_mode 800x600' }
        elseif ($_ -like 'keypress_on_start *') { 'keypress_on_start off' } elseif ($_ -like 'g_always_active *') { 'g_always_active on' } else { $_ }
    } | Set-Content "$k\appdata\user.ltx"
}
if ($dedicated) { Add-Content "$host1\appdata\coop-menu.ltx" "test = join`r`ntest_delay = 10`r`ntest_info = $info" }
else { Add-Content "$host1\appdata\coop-menu.ltx" "test = host_new`r`ntest_delay = 3" }
Add-Content "$friend\appdata\coop-menu.ltx" "test = join`r`ntest_delay = 10`r`ntest_info = $info"
Remove-Item $info -ErrorAction SilentlyContinue
if ($dedicated) {
    Start-Process powershell.exe -WindowStyle Hidden -ArgumentList ('-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
        (Join-Path $PSScriptRoot 'Host-CoopServer.ps1') + '" -Runtime "' + $ServerRuntime + '"') | Out-Null
}
function Start-Kit($k) {
    $s = New-Object System.Diagnostics.ProcessStartInfo
    $s.FileName = "$k\bin\AnomalyDX11.exe"; $s.Arguments = "-nointro"
    $s.WorkingDirectory = $k; $s.UseShellExecute = $false
    $s.EnvironmentVariables['TEMP'] = "$k\tmp"; $s.EnvironmentVariables['TMP'] = "$k\tmp"
    return [System.Diagnostics.Process]::Start($s)
}
$a = Start-Kit $host1
Start-Sleep -Seconds 20
$b = Start-Kit $friend
Start-Sleep -Seconds $Seconds
New-Item -ItemType File "$host1\appdata\coop-stop.txt" -Force | Out-Null
New-Item -ItemType File "$friend\appdata\coop-stop.txt" -Force | Out-Null
if ($dedicated) { & (Join-Path $PSScriptRoot 'Stop-CoopServer.ps1') -Runtime $ServerRuntime | Out-Null }
$t = 0
while ($t -lt 90 -and (-not $a.HasExited -or -not $b.HasExited -or @(Get-Process AnomalyCampaignServer -ErrorAction SilentlyContinue).Count)) { Start-Sleep 1; $t++ }
Add-Type -AssemblyName UIAutomationClient
foreach ($p in @($a, $b)) {
    if (-not $p.HasExited) {
        $p.Refresh()
        "pid $($p.Id) title [$($p.MainWindowTitle)]" | Add-Content "$logDir\dialogs.txt"
        try {
            $root = [System.Windows.Automation.AutomationElement]::FromHandle($p.MainWindowHandle)
            foreach ($e in $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
                if ($e.Current.Name) { "  $($e.Current.Name)" | Add-Content "$logDir\dialogs.txt" }
            }
        } catch { }
        $p.Kill()
    }
}
foreach ($p in @(Get-Process AnomalyCampaignServer -ErrorAction SilentlyContinue)) { $p.Kill() }
"host exit $($a.ExitCode) friend exit $($b.ExitCode)" | Set-Content "$logDir\result.txt"
$serverDir = if ($dedicated) { $ServerRuntime } else {
    ((Get-Content (Join-Path $host1 'appdata\coop-menu.ltx') | Where-Object { $_ -like 'server_runtime = *' }) -replace '^server_runtime = ', '') }
foreach ($pair in @(@('host', $host1), @('friend', $friend), @('server', $serverDir))) {
    New-Item -ItemType Directory "$logDir\$($pair[0])" | Out-Null
    Get-ChildItem "$($pair[1])\appdata\logs" -File -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item $_.FullName "$logDir\$($pair[0])" }
}
foreach ($f in $saved.Keys) { Set-Content -Path $f -Value $saved[$f] -NoNewline }
Remove-Item "$host1\appdata\coop-stop.txt", "$friend\appdata\coop-stop.txt" -ErrorAction SilentlyContinue
Write-Host "done"
