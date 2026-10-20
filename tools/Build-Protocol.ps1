param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    # Build-NetworkProbe.ps1 run whose GNS and protobuf install is linked.
    [ValidatePattern('^[a-zA-Z0-9-]+$')][string]$GnsFrom = 'net-build-7',
    # Task whose log folder receives the run.
    [ValidatePattern('^S[0-9]{2}-T[0-9]{2}$')][string]$Task = 'S04-T02'
)
# Builds the project-owned protocol libraries (protocol/) and their tests
# (tests/) with the Stage 1 pinned compiler and the S04-T01 pinned CMake/Ninja.
# Outside engine-work by design. Outputs: builds/protocol/build/<run>,
# logs: builds/logs/<task>/<run>.
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$pins = Get-Content (Join-Path $workspace 'tools\network-probe\pins.json') -Raw | ConvertFrom-Json
$toolchain = Get-Content (Join-Path $workspace 'docs\toolchain-manifest.json') -Raw | ConvertFrom-Json
$probeRoot = Join-Path $workspace ($pins.root -replace '/', '\')

$installation = Join-Path $workspace 'builds\build_1\vs2022'
$compiler = Join-Path $installation ('VC\Tools\MSVC\' + $toolchain.msvc_tools_version + '\bin\Hostx64\x64\cl.exe')
$dumpbin = Join-Path $installation ('VC\Tools\MSVC\' + $toolchain.msvc_tools_version + '\bin\Hostx64\x64\dumpbin.exe')
if (-not (Test-Path $compiler)) { throw 'Relocated compiler is missing.' }
if ((Get-FileHash $compiler -Algorithm SHA256).Hash.ToLower() -ne $toolchain.files.compiler.sha256) { throw 'Compiler hash differs from Stage 1 pin.' }

function Resolve-Pinned([string]$relative) { Join-Path $probeRoot ($relative -replace '/', '\') }
foreach ($tool in @($pins.build_tools.cmake, $pins.build_tools.ninja)) {
    $file = Resolve-Pinned $tool.file
    if (-not (Test-Path $file)) { throw "Pinned download missing: $file (run Build-NetworkProbe.ps1 first)" }
    if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLower() -ne $tool.sha256) { throw "Hash mismatch for $file" }
}
$cmake = Resolve-Pinned $pins.build_tools.cmake.executable
$ninja = Resolve-Pinned $pins.build_tools.ninja.executable
$gnsInstall = Join-Path $probeRoot "install\$GnsFrom\gns"
$pbInstall = Join-Path $probeRoot "install\$GnsFrom\protobuf"
$gnsDll = Join-Path $gnsInstall 'bin\GameNetworkingSockets.dll'
if (-not (Test-Path $gnsDll)) { throw "No GNS install for $GnsFrom." }

$logDir = Join-Path $workspace "builds\logs\$Task\$Run"
$buildDir = Join-Path $workspace "builds\protocol\build\$Run"
if ((Test-Path $logDir) -or (Test-Path $buildDir)) { throw "Run $Run already exists." }
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$tempDir = Join-Path $logDir 'tmp'
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

$source = Join-Path $workspace 'tests'
$steps = @(
    ('"' + $cmake + '" -S "' + $source + '" -B "' + $buildDir + '" -G Ninja "-DCMAKE_MAKE_PROGRAM=' + $ninja +
        '" "-DCMAKE_CXX_COMPILER=' + $compiler + '" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL' +
        ' "-DCMAKE_PREFIX_PATH=' + $gnsInstall + ';' + $pbInstall + '"'),
    ('"' + $cmake + '" --build "' + $buildDir + '"')
)
$lines = @('@echo off', 'setlocal',
    ('set "TEMP=' + $tempDir + '"'), ('set "TMP=' + $tempDir + '"'),
    ('call "' + (Join-Path $installation 'Common7\Tools\VsDevCmd.bat') + '" -no_logo -arch=x64 -host_arch=x64 -vcvars_ver=' + $toolchain.msvc_tools_version + ' -winsdk=' + $toolchain.windows_sdk_version),
    'if errorlevel 1 exit /b %errorlevel%')
$n = 0
foreach ($s in $steps) {
    $n++
    $lines += ('echo === step ' + $n + ' ===')
    $lines += $s
    $lines += 'if errorlevel 1 exit /b %errorlevel%'
}
$lines += 'exit /b 0'
$batchPath = Join-Path $logDir 'build.cmd'
($lines -join "`r`n") | Set-Content -Encoding ASCII $batchPath

$result = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    run = $Run
    task = $Task
    gns_from = $GnsFrom
    gns_dll_sha256 = (Get-FileHash $gnsDll -Algorithm SHA256).Hash.ToLower()
    compiler = $compiler
    compiler_sha256 = $toolchain.files.compiler.sha256
    build_dir = $buildDir
    steps = $steps
}
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = 'cmd.exe'
$start.Arguments = '/d /c ""' + $batchPath + '" > "' + (Join-Path $logDir 'stdout.log') + '" 2> "' + (Join-Path $logDir 'stderr.log') + '""'
$start.WorkingDirectory = $workspace
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $start
$timer = [System.Diagnostics.Stopwatch]::StartNew()
if (-not $process.Start()) { throw 'Build process did not start.' }
if (-not $process.WaitForExit(1800000)) { throw "Build wrapper timed out: $batchPath" }
$process.Refresh()
$result.exit_code = $process.ExitCode
$result.elapsed_s = [math]::Round($timer.Elapsed.TotalSeconds, 1)
$result.finished_utc = [DateTime]::UtcNow.ToString('o')
if ($process.ExitCode -eq 0) {
    $outputs = @(Get-ChildItem -Path $buildDir -Recurse -Include *.exe, *.lib, *.dll -File)
    $result.outputs = @($outputs | ForEach-Object {
        [ordered]@{ path = $_.FullName; bytes = $_.Length; sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower() }
    })
    $exe = Join-Path $buildDir 'transport-tests.exe'
    if (Test-Path $exe) {
        & $dumpbin /nologo /dependents $exe | Out-File -Encoding ASCII (Join-Path $logDir 'dependents.txt')
    }
}
$result | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $logDir 'result.json')
Get-Content (Join-Path $logDir 'stdout.log') -Tail 25
Get-Content (Join-Path $logDir 'stderr.log') -Tail 15
exit $process.ExitCode
