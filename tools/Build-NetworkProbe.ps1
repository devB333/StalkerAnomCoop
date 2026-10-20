param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-zA-Z0-9-]+$')][string]$Run,
    # Probe-only rebuild against the libraries another run installed.
    [ValidatePattern('^[a-zA-Z0-9-]+$')][string]$ReuseLibrariesFrom
)
# S04-T01: builds pinned protobuf and GameNetworkingSockets, then the transport
# probe, with the Stage 1 pinned compiler. Outside engine-work by design.
$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$pins = Get-Content (Join-Path $workspace 'tools\network-probe\pins.json') -Raw | ConvertFrom-Json
$toolchain = Get-Content (Join-Path $workspace 'docs\toolchain-manifest.json') -Raw | ConvertFrom-Json
$root = Join-Path $workspace ($pins.root -replace '/', '\')

$installation = Join-Path $workspace 'builds\build_1\vs2022'
$compiler = Join-Path $installation ('VC\Tools\MSVC\' + $toolchain.msvc_tools_version + '\bin\Hostx64\x64\cl.exe')
$dumpbin = Join-Path $installation ('VC\Tools\MSVC\' + $toolchain.msvc_tools_version + '\bin\Hostx64\x64\dumpbin.exe')
if (-not (Test-Path $compiler)) { throw 'Relocated compiler is missing.' }
if ((Get-FileHash $compiler -Algorithm SHA256).Hash.ToLower() -ne $toolchain.files.compiler.sha256) { throw 'Compiler hash differs from Stage 1 pin.' }

function Resolve-Pinned([string]$relative) { Join-Path $root ($relative -replace '/', '\') }

# Downloads: verify each archive against its pin, then extract once.
$archives = @(
    @{ pin = $pins.protobuf; dest = 'src'; marker = $pins.protobuf.directory },
    @{ pin = $pins.build_tools.cmake; dest = 'tools'; marker = 'tools/cmake-4.3.5-windows-x86_64' },
    @{ pin = $pins.build_tools.ninja; dest = 'tools/ninja-1.13.1'; marker = $pins.build_tools.ninja.executable }
)
foreach ($a in $archives) {
    $file = Resolve-Pinned $a.pin.file
    if (-not (Test-Path $file)) {
        New-Item -ItemType Directory -Force -Path (Split-Path $file) | Out-Null
        Invoke-WebRequest -UseBasicParsing -Uri $a.pin.url -OutFile $file
    }
    $hash = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower()
    if ($hash -ne $a.pin.sha256) { throw "Hash mismatch for $file ($hash)" }
    if (-not (Test-Path (Resolve-Pinned $a.marker))) {
        Expand-Archive -Path $file -DestinationPath (Resolve-Pinned $a.dest)
    }
}
$cmake = Resolve-Pinned $pins.build_tools.cmake.executable
$ninja = Resolve-Pinned $pins.build_tools.ninja.executable
$protobufSrc = Resolve-Pinned $pins.protobuf.directory

# GameNetworkingSockets: a clean checkout of the pinned commit.
$gns = Resolve-Pinned $pins.gamenetworkingsockets.directory
if (-not (Test-Path $gns)) {
    & git clone --quiet --depth 1 --branch $pins.gamenetworkingsockets.tag $pins.gamenetworkingsockets.repository $gns
    if ($LASTEXITCODE -ne 0) { throw 'git clone failed.' }
}
$gnsCommit = (& git -C $gns rev-parse HEAD).Trim()
if ($gnsCommit -ne $pins.gamenetworkingsockets.commit) { throw "GameNetworkingSockets commit $gnsCommit differs from pin." }
$gnsDirty = (& git -C $gns status --porcelain)
if ($gnsDirty) { throw 'GameNetworkingSockets checkout has local changes.' }

$logDir = Join-Path $workspace "builds\logs\S04-T01\$Run"
if (Test-Path $logDir) { throw "Run directory exists: $logDir" }
$buildDir = Join-Path $root "build\$Run"
$installDir = Join-Path $root "install\$Run"
if ($ReuseLibrariesFrom) {
    $installDir = Join-Path $root "install\$ReuseLibrariesFrom"
    if (-not (Test-Path $installDir)) { throw "No install for $ReuseLibrariesFrom." }
}
if ((Test-Path $buildDir) -or (-not $ReuseLibrariesFrom -and (Test-Path $installDir))) { throw "Build or install directory exists for $Run." }
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$tempDir = Join-Path $logDir 'tmp'
New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

$probeSrc = Join-Path $workspace 'tools\network-probe'
$pbBuild = Join-Path $buildDir 'protobuf'; $pbInstall = Join-Path $installDir 'protobuf'
$gnsBuild = Join-Path $buildDir 'gns'; $gnsInstall = Join-Path $installDir 'gns'
$probeBuild = Join-Path $buildDir 'probe'
$common = @('-G Ninja', ('"-DCMAKE_MAKE_PROGRAM=' + $ninja + '"'),
    ('"-DCMAKE_C_COMPILER=' + $compiler + '"'), ('"-DCMAKE_CXX_COMPILER=' + $compiler + '"'),
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL') -join ' '
$gnsOptions = ($pins.gamenetworkingsockets.options.PSObject.Properties |
    Where-Object { $_.Name -notin @('CMAKE_BUILD_TYPE', 'CMAKE_MSVC_RUNTIME_LIBRARY') } |
    ForEach-Object { '-D' + $_.Name + '=' + $_.Value }) -join ' '
$pbOptions = ($pins.protobuf.options.PSObject.Properties |
    Where-Object { $_.Name -ne 'CMAKE_BUILD_TYPE' } |
    ForEach-Object { '-D' + $_.Name + '=' + $_.Value }) -join ' '

$steps = @(
    ('"' + $cmake + '" -S "' + $protobufSrc + '" -B "' + $pbBuild + '" ' + $common + ' ' + $pbOptions + ' -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "-DCMAKE_INSTALL_PREFIX=' + $pbInstall + '"'),
    ('"' + $cmake + '" --build "' + $pbBuild + '"'),
    ('"' + $cmake + '" --install "' + $pbBuild + '"'),
    ('"' + $cmake + '" -S "' + $gns + '" -B "' + $gnsBuild + '" ' + $common + ' ' + $gnsOptions + ' "-DCMAKE_PREFIX_PATH=' + $pbInstall + '" "-DCMAKE_INSTALL_PREFIX=' + $gnsInstall + '"'),
    ('"' + $cmake + '" --build "' + $gnsBuild + '"'),
    ('"' + $cmake + '" --install "' + $gnsBuild + '"'),
    ('"' + $cmake + '" -S "' + $probeSrc + '" -B "' + $probeBuild + '" ' + $common + ' "-DCMAKE_PREFIX_PATH=' + $gnsInstall + ';' + $pbInstall + '"'),
    ('"' + $cmake + '" --build "' + $probeBuild + '"')
)
if ($ReuseLibrariesFrom) { $steps = $steps[6..7] }
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
    pins = 'tools/network-probe/pins.json'
    gns_commit = $gnsCommit
    compiler = $compiler
    compiler_sha256 = $toolchain.files.compiler.sha256
    build_dir = $buildDir
    install_dir = $installDir
    reuse_libraries_from = $ReuseLibrariesFrom
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
if (-not $process.WaitForExit(3600000)) { throw "Build wrapper timed out: $batchPath" }
$process.Refresh()
$result.exit_code = $process.ExitCode
$result.elapsed_s = [math]::Round($timer.Elapsed.TotalSeconds, 1)
$result.finished_utc = [DateTime]::UtcNow.ToString('o')

if ($process.ExitCode -eq 0) {
    # Record the outputs and what they load, for packaging.
    $outputs = @(
        (Join-Path $gnsInstall 'bin\GameNetworkingSockets.dll'),
        (Join-Path $gnsInstall 'lib\GameNetworkingSockets.lib'),
        (Join-Path $gnsInstall 'lib\GameNetworkingSockets_s.lib'),
        (Join-Path $pbInstall 'lib\libprotobuf.lib'),
        (Join-Path $pbInstall 'bin\protoc.exe'),
        (Join-Path $probeBuild 'gns-probe.exe'),
        (Join-Path $probeBuild 'GameNetworkingSockets.dll'),
        (Join-Path $gnsBuild 'bin\test_crypto.exe'),
        (Join-Path $gnsBuild 'bin\test_connection.exe')
    )
    $result.outputs = @($outputs | ForEach-Object {
        if (Test-Path $_) {
            [ordered]@{ path = $_; bytes = (Get-Item $_).Length; sha256 = (Get-FileHash $_ -Algorithm SHA256).Hash.ToLower() }
        } else { [ordered]@{ path = $_; missing = $true } }
    })
    $depLog = Join-Path $logDir 'dependents.txt'
    foreach ($bin in @((Join-Path $gnsInstall 'bin\GameNetworkingSockets.dll'), (Join-Path $probeBuild 'gns-probe.exe'))) {
        & $dumpbin /nologo /dependents $bin | Out-File -Append -Encoding ASCII $depLog
    }
    & $dumpbin /nologo /headers (Join-Path $gnsInstall 'bin\GameNetworkingSockets.dll') | Select-String -Pattern 'machine|linker version|subsystem' | Out-File -Append -Encoding ASCII $depLog
}
$result | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $logDir 'result.json')
Get-Content (Join-Path $logDir 'stdout.log') -Tail 15
Get-Content (Join-Path $logDir 'stderr.log') -Tail 15
exit $process.ExitCode
