# S11-T00: stops a co-op server cleanly (its stop file: the server takes a checkpoint and exits).
#   powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File C:\...\tools\Stop-CoopServer.ps1 -Runtime C:\...\server-runtime
param([Parameter(Mandatory = $true)][string]$Runtime)
$ErrorActionPreference = 'Stop'
$profile = Join-Path $Runtime 'coop.ltx'
$stop = ''
foreach ($line in Get-Content $profile) { if ($line -match '^\s*stop_file\s*=\s*(.*?)\s*$') { $stop = $Matches[1].Trim('"') } }
if (-not $stop) { throw "No stop_file in $profile" }
New-Item -ItemType File $stop -Force | Out-Null
Write-Host 'The server is stopping (it saves first).'
