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
while ($true) {
    $arguments = '-campaign-serve -snd_cpu -fsltx ' + $Fsgame + ' -coop-config ' + $Profile
    Write-Host ("starting: " + $arguments)
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
        Write-Host ("server exited with " + $process.ExitCode)
        exit $process.ExitCode
    }
    $file = Join-Path $Runtime 'appdata\coop-relaunch.ltx'
    if (-not (Test-Path $file)) { Write-Host 'relaunch asked but no coop-relaunch.ltx'; exit 75 }
    $values = @{}
    foreach ($line in Get-Content $file) {
        if ($line -match '^\s*([a-z_]+)\s*=\s*(.*?)\s*$') { $values[$Matches[1]] = $Matches[2] }
    }
    Move-Item $file ($file + '.used') -Force
    Write-Host ("switching to campaign " + $values['campaign'] + " (" + $values['why'] + ")")
    $Fsgame = $values['fsgame']
    $Profile = $values['profile']
}
