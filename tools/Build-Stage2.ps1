param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    [Parameter(Mandatory = $true)][ValidateSet('Client', 'Campaign')][string]$Product,
    [switch]$EvaluateOnly
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$source = Join-Path $workspace 'builds\build_1\engine-work'
$toolchain = Get-Content (Join-Path $workspace 'docs\toolchain-manifest.json') -Raw | ConvertFrom-Json
$installation = Join-Path $workspace 'builds\build_1\vs2022'
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
$compiler = Join-Path $installation ('VC\Tools\MSVC\' + $toolchain.msvc_tools_version + '\bin\Hostx64\x64\cl.exe')
if (-not (Test-Path $msbuild) -or -not (Test-Path $compiler)) { throw 'Relocated MSBuild or compiler is missing.' }
if ((Get-FileHash $msbuild -Algorithm SHA256).Hash.ToLower() -ne $toolchain.files.msbuild.sha256) { throw 'MSBuild hash differs from Stage 1 pin.' }
if ((Get-FileHash $compiler -Algorithm SHA256).Hash.ToLower() -ne $toolchain.files.compiler.sha256) { throw 'Compiler hash differs from Stage 1 pin.' }
$revision = (& git -C $source rev-parse HEAD).Trim()
if ($revision -ne 'c57fe5b9fab4fe0506659db03137c52d2500eb2f') { throw 'Working source revision changed.' }
$logDir = Join-Path $workspace "builds\logs\S02-T02\$Run"
if (Test-Path $logDir) { throw "Run directory exists: $logDir" }
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$tempDir = Join-Path $logDir 'tmp'
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
$serverRoot = Join-Path $workspace 'builds\stage2\campaign'
$solution = Join-Path $source 'src\engine-vs2022.sln'
$target = if ($EvaluateOnly) { Join-Path $source 'src\xrEngine\xrEngine.vcxproj' } else { $solution }
$configuration = if ($EvaluateOnly) { 'ReleaseR4' } else { 'DX11' }
$arguments = @(
    ('"' + $msbuild + '"'), ('"' + $target + '"'), '/nologo', '/nr:false', '/m:2',
    ('/p:Configuration=' + $configuration), '/p:Platform=x64', '/p:PreferredToolArchitecture=x64',
    ('/p:WindowsTargetPlatformVersion=' + $toolchain.windows_sdk_version),
    ('/p:VCToolsVersion=' + $toolchain.msvc_tools_version),
    ('"/p:SolutionDir=' + (Join-Path $source 'src') + '/"')
)
if ($Product -eq 'Campaign') {
    $targets = Join-Path $source 'src\CampaignServer.targets'
    if (-not (Test-Path $targets)) { throw 'CampaignServer.targets missing.' }
    $arguments += @(
        '/p:XR_CampaignServer=true',
        ('"/p:ForceImportAfterCppTargets=' + $targets + '"'),
        ('"/p:xrBinRootDir=' + (Join-Path $serverRoot 'bin') + '/"'),
        ('"/p:xrBinPlugRootDir=' + (Join-Path $serverRoot 'bin_plugs') + '/"'),
        ('"/p:xrLibRootDir=' + (Join-Path $serverRoot 'lib') + '/"'),
        ('"/p:xrLibDir=' + (Join-Path $serverRoot 'lib\DX11') + '/"'),
        ('"/p:xrIntDir=' + (Join-Path $serverRoot 'intermediate\DX11') + '/"'),
        ('"/p:xrGameDir=' + (Join-Path $serverRoot '_game') + '/"')
    )
}
if ($EvaluateOnly) {
    $arguments += '-getProperty:TargetName,TargetPath,OutDir,IntDir,xrGameDir,xrLibDir,CampaignServerRole'
} else {
    $arguments += @('/t:Build', '/p:CL_MPCount=4', '/clp:ErrorsOnly;Summary',
        ('"/flp:LogFile=' + (Join-Path $logDir 'msbuild.log') + ';Verbosity=normal"'),
        ('"/bl:' + (Join-Path $logDir 'msbuild.binlog') + '"'))
}
$command = $arguments -join ' '
$batch = @(
    '@echo off', 'setlocal',
    ('set "TEMP=' + $tempDir + '"'), ('set "TMP=' + $tempDir + '"'),
    ('call "' + (Join-Path $installation 'Common7\Tools\VsDevCmd.bat') + '" -no_logo -arch=x64 -host_arch=x64 -vcvars_ver=' + $toolchain.msvc_tools_version + ' -winsdk=' + $toolchain.windows_sdk_version),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cd /d "' + (Join-Path $source 'src') + '"'), $command, 'exit /b %errorlevel%'
) -join "`r`n"
$batchPath = Join-Path $logDir 'build.cmd'
$batch | Set-Content -Encoding ASCII $batchPath
$result = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    product = $Product
    evaluate_only = [bool]$EvaluateOnly
    source_revision = $revision
    command = $command
    batch_file = $batchPath
    toolchain_manifest = 'docs/toolchain-manifest.json'
}
$result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $logDir 'request.json')
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = 'cmd.exe'
$start.Arguments = '/d /c ""' + $batchPath + '" > "' + (Join-Path $logDir 'stdout.log') + '" 2> "' + (Join-Path $logDir 'stderr.log') + '""'
$start.WorkingDirectory = $workspace
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $start
if (-not $process.Start()) { throw 'Build process did not start.' }
if (-not $process.WaitForExit(1800000)) { throw "Build wrapper timed out: $batchPath" }
$process.Refresh()
if ($null -eq $process.ExitCode) { throw "Build wrapper returned no exit code: $batchPath" }
$result.exit_code = $process.ExitCode
$result.finished_utc = [DateTime]::UtcNow.ToString('o')
$result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $logDir 'result.json')
Get-Content (Join-Path $logDir 'stdout.log') -Tail 20
Get-Content (Join-Path $logDir 'stderr.log') -Tail 20
exit $process.ExitCode
