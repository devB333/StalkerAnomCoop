param([switch]$Elevated)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$installer = Join-Path $workspace 'builds\toolchain\installers\vs_buildtools.exe'
$logDir = Join-Path $workspace 'builds\logs\S01-T01'
$tempDir = Join-Path $workspace 'builds\tmp\vs-install'
New-Item -ItemType Directory -Force -Path $logDir, $tempDir | Out-Null

$signature = Get-AuthenticodeSignature $installer
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
    throw "Installer does not have a valid Microsoft signature: $($signature.Status)"
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if ($Elevated) { throw 'Administrator elevation did not succeed.' }
    $process = Start-Process powershell.exe -Verb RunAs -Wait -PassThru -ArgumentList @(
        '-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
        ('"' + $PSCommandPath + '"'), '-Elevated'
    )
    exit $process.ExitCode
}

$env:TEMP = $tempDir
$env:TMP = $tempDir
Set-Location $workspace
$arguments = @(
    '--passive', '--wait', '--norestart',
    '--installPath', ('"' + (Join-Path $workspace 'builds\vs2022') + '"'),
    '--path', ('"cache=' + (Join-Path $workspace 'builds\vscache') + '"'),
    '--path', ('"shared=' + (Join-Path $workspace 'builds\vsshared') + '"'),
    '--add', 'Microsoft.VisualStudio.Workload.VCTools',
    '--add', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    '--add', 'Microsoft.VisualStudio.Component.VC.ATL',
    '--add', 'Microsoft.VisualStudio.Component.VC.ATLMFC',
    '--add', 'Microsoft.VisualStudio.Component.Windows11SDK.26100',
    '--addProductLang', 'en-US'
)
$record = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    installer = $installer
    installer_sha256 = (Get-FileHash $installer -Algorithm SHA256).Hash.ToLower()
    signature_status = [string]$signature.Status
    signer = $signature.SignerCertificate.Subject
    arguments = $arguments
    temp_directory = $tempDir
}
$record | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $logDir 'install-request.json')
$process = Start-Process $installer -ArgumentList $arguments -Wait -PassThru
$record.exit_code = $process.ExitCode
$record.finished_utc = [DateTime]::UtcNow.ToString('o')
$record | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $logDir 'install-result.json')
if ($process.ExitCode -eq 3010) {
    Write-Output 'Installation completed; Windows reports a reboot is required. No reboot was initiated.'
}
exit $process.ExitCode
