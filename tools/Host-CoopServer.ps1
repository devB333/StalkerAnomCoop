# S11-T00: a dedicated co-op server on this PC (no game played here): both players join from their own PCs with
# New game > Join co-op (the address and join code printed below). The first player to join owns the session
# (New game and Load from the client's menu). Stop it with tools/Stop-CoopServer.ps1 (a clean stop: a checkpoint).
#   powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File C:\...\tools\Host-CoopServer.ps1 `
#       -Runtime C:\...\server-runtime [-Continue] [-Exe C:\...\AnomalyCampaignServer.exe] [-Code ABCDE-23456]
# -Continue: the last campaign played (else the runtime's own campaign). A new join code is made at every start
# unless -Code gives one; players already admitted need no code. The address and code are also written to the
# runtime's appdata\coop-host-info.txt.
param(
    [Parameter(Mandatory = $true)][string]$Runtime,
    [switch]$Continue,
    [string]$Exe = '',
    [string]$Code = ''
)
$ErrorActionPreference = 'Stop'
# (Windows PowerShell 5.1: $PSScriptRoot is empty in a parameter's default, so the default is set here)
if (-not $Exe) { $Exe = Join-Path (Split-Path $PSScriptRoot -Parent) 'builds\stage2\campaign\_game\bin_dbg\AnomalyCampaignServer.exe' }
$profile = Join-Path $Runtime 'coop.ltx'
$fsgame = Join-Path $Runtime 'fsgame.ltx'
$appdata = Join-Path $Runtime 'appdata'
function Value([string]$key) {
    foreach ($line in Get-Content $profile) { if ($line -match "^\s*$key\s*=\s*(.*?)\s*$") { return $Matches[1].Trim('"') } }
    return ''
}
$alphabet = '23456789ABCDEFGHJKMNPQRSTUVWXYZ'
if (-not $Code) {
    $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    $bytes = New-Object byte[] 1
    $chars = @()
    while ($chars.Count -lt 10) {
        $rng.GetBytes($bytes)
        if ($bytes[0] -lt 248) { $chars += $alphabet[$bytes[0] % 31] }
    }
    $Code = (-join $chars[0..4]) + '-' + (-join $chars[5..9])
}
Set-Content -Path (Join-Path $appdata 'coop-join-code.txt') -Value $Code -Encoding ASCII
if ($Continue) { Set-Content -Path (Join-Path $appdata 'coop-continue.txt') -Value '1' -Encoding ASCII }
$stop = Value 'stop_file'
if ($stop -and (Test-Path $stop)) { Remove-Item $stop }
$port = ((Value 'listen') -split ':')[-1]
$address = ''
foreach ($ip in @(Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | ForEach-Object { $_.IPAddress })) {
    $p = $ip.Split('.')
    if ([int]$p[0] -eq 100 -and [int]$p[1] -ge 64 -and [int]$p[1] -le 127) { $address = $ip; break }
}
if (-not $address) { $address = '(this PC''s Tailscale address)' }
Set-Content -Path (Join-Path $appdata 'coop-host-info.txt') -Value @('[host]', "address = ${address}:$port", "join_code = $Code") -Encoding ASCII
Write-Host ''
Write-Host '  Co-op server starting (about a minute).'
Write-Host "  Address:   ${address}:$port"
Write-Host "  Join code: $Code"
Write-Host '  Players: New game > Join co-op, Host = the address, Password = the join code.'
Write-Host '  Stop it with Stop-Server (tools\Stop-CoopServer.ps1); closing this window does not stop the game cleanly.'
Write-Host ''
& (Join-Path $PSScriptRoot 'Start-CoopServer.ps1') -Runtime $Runtime -Fsgame $fsgame -Profile $profile -Exe $Exe
