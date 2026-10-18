param(
    [ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run = 'build-1',
    [ValidatePattern('^[a-zA-Z0-9-]+$')][string]$SourceDirectory = 'engine-work',
    [switch]$Rebuild
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$toolchain = Get-Content (Join-Path $workspace 'docs\toolchain-manifest.json') -Raw | ConvertFrom-Json
$source = Join-Path $workspace "builds\$SourceDirectory"
$logDir = Join-Path $workspace "builds\logs\S01-T03\$Run"
if (Test-Path $logDir) { throw "Run directory already exists: $logDir. Choose a new run name." }
New-Item -ItemType Directory -Path $logDir | Out-Null
$tempDir = Join-Path $workspace "builds\tmp\$Run"
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null
$msbuild = $toolchain.files.msbuild.path
if ((Get-FileHash $msbuild -Algorithm SHA256).Hash.ToLower() -ne $toolchain.files.msbuild.sha256) {
    throw 'MSBuild identity differs from the pinned manifest.'
}
$target = if ($Rebuild) { 'Rebuild' } else { 'Build' }
$command = @(
    ('"' + $msbuild + '"'),
    ('"' + $source + '\src\engine-vs2022.sln"'),
    '/nologo', '/m:2', '/nr:false', "/t:$target",
    '/p:Configuration=DX11', '/p:Platform=x64', '/p:PreferredToolArchitecture=x64',
    ('/p:WindowsTargetPlatformVersion=' + $toolchain.windows_sdk_version),
    ('/p:VCToolsVersion=' + $toolchain.msvc_tools_version),
    '/p:CL_MPCount=4', '/clp:ErrorsOnly;Summary',
    ('"/flp:LogFile=' + $logDir + '\msbuild.log;Verbosity=normal"'),
    ('"/bl:' + $logDir + '\msbuild.binlog"')
) -join ' '
$batch = @(
    '@echo off', 'setlocal',
    ('set "TEMP=' + $tempDir + '"'),
    ('set "TMP=' + $tempDir + '"'),
    ('call "' + $toolchain.installation_path + '\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 -vcvars_ver=' + $toolchain.msvc_tools_version + ' -winsdk=' + $toolchain.windows_sdk_version),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cd /d "' + $source + '\src"'),
    $command,
    'exit /b %errorlevel%'
) -join "`r`n"
$batchPath = Join-Path $logDir 'build.cmd'
$batch | Set-Content -Encoding ASCII $batchPath
$result = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    command = $command
    batch_file = $batchPath
    source_revision = 'c57fe5b9fab4fe0506659db03137c52d2500eb2f'
    source_directory = $source
    configuration = 'DX11|x64'
    target = $target
    toolchain_manifest_sha256 = (Get-FileHash (Join-Path $workspace 'docs\toolchain-manifest.json') -Algorithm SHA256).Hash.ToLower()
}
$result | ConvertTo-Json | Set-Content (Join-Path $logDir 'request.json')
$process = Start-Process cmd.exe -ArgumentList @('/d', '/c', ('"' + $batchPath + '"')) -WorkingDirectory $workspace -Wait -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $logDir 'stdout.log') -RedirectStandardError (Join-Path $logDir 'stderr.log')
$result.exit_code = $process.ExitCode
$result.finished_utc = [DateTime]::UtcNow.ToString('o')
$result | ConvertTo-Json | Set-Content (Join-Path $logDir 'result.json')
Get-Content (Join-Path $logDir 'stdout.log') -Tail 40
Get-Content (Join-Path $logDir 'stderr.log') -Tail 20
exit $process.ExitCode
