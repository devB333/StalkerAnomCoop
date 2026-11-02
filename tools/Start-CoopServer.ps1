# S10-T06: the dedicated co-op campaign server's launcher. Runs
# AnomalyCampaignServer.exe -campaign-serve on a prepared runtime and, when it
# exits with 75 (the owner's co-op menu asked for a new game or another saved
# campaign), starts it again on the campaign named in the runtime's
# appdata\coop-relaunch.ltx. Any other exit code ends the launcher with it.
#   powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File C:\...\tools\Start-CoopServer.ps1 `
#       -Runtime C:\...\server -Fsgame C:\...\server\fsgame.ltx -Profile C:\...\server\coop.ltx
param(
    [Parameter(Mandatory = $true)][string]$Runtime,
    [Parameter(Mandatory = $true)][string]$Fsgame,
    [Parameter(Mandatory = $true)][string]$Profile,
    [string]$Exe = (Join-Path (Split-Path $PSScriptRoot -Parent) 'builds\stage2\campaign\_game\bin_dbg\AnomalyCampaignServer.exe')
)
$ErrorActionPreference = 'Stop'
# S11-T00: started hidden by the host's co-op menu: its lines also go to the runtime's app data logs
$launcherLog = Join-Path $Runtime 'appdata\logs\coop-launcher.log'
New-Item -ItemType Directory (Split-Path $launcherLog -Parent) -Force | Out-Null
function Say([string]$line) {
    Write-Host $line
    Add-Content -Path $launcherLog -Value ([DateTime]::UtcNow.ToString('o') + ' ' + $line)
}
while ($true) {
    $arguments = '-campaign-serve -snd_cpu -fsltx ' + $Fsgame + ' -coop-config ' + $Profile
    Say ("starting: " + $arguments)
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $Exe
    $start.Arguments = $arguments
    $start.WorkingDirectory = $Runtime
    $start.UseShellExecute = $false
    $start.EnvironmentVariables['TEMP'] = Join-Path $Runtime 'tmp'
    $start.EnvironmentVariables['TMP'] = Join-Path $Runtime 'tmp'
    $process = [System.Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    if ($process.ExitCode -ne 75) {
        Say ("server exited with " + $process.ExitCode)
        exit $process.ExitCode
    }
    $file = Join-Path $Runtime 'appdata\coop-relaunch.ltx'
    if (-not (Test-Path $file)) { Say 'relaunch asked but no coop-relaunch.ltx'; exit 75 }
    $values = @{}
    foreach ($line in Get-Content $file) {
        if ($line -match '^\s*([a-z_]+)\s*=\s*(.*?)\s*$') { $values[$Matches[1]] = $Matches[2] }
    }
    Move-Item $file ($file + '.used') -Force
    Say ("switching to campaign " + $values['campaign'] + " (" + $values['why'] + ")")
    $Fsgame = $values['fsgame']
    $Profile = $values['profile']
}
