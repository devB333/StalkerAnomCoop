$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$vsRoot = Join-Path $workspace 'builds\vs2022'
$logDir = Join-Path $workspace 'builds\logs\S01-T01'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$env:TEMP = Join-Path $workspace 'builds\tmp\toolchain-probe'
$env:TMP = $env:TEMP
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
$msvcVersion = (Get-Content (Join-Path $vsRoot 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
$msvcRoot = Join-Path $vsRoot ('VC\Tools\MSVC\' + $msvcVersion)
$sdkRoot = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots').KitsRoot10
$sdkVersion = '10.0.26100.0'
$msbuild = Join-Path $vsRoot 'MSBuild\Current\Bin\amd64\MSBuild.exe'
$files = [ordered]@{
    compiler = Join-Path $msvcRoot 'bin\Hostx64\x64\cl.exe'
    linker = Join-Path $msvcRoot 'bin\Hostx64\x64\link.exe'
    msbuild = $msbuild
    atl_header = Join-Path $msvcRoot 'atlmfc\include\atlbase.h'
    mfc_header = Join-Path $msvcRoot 'atlmfc\include\afxwin.h'
    sdk_windows_header = Join-Path $sdkRoot "Include\$sdkVersion\um\Windows.h"
    sdk_kernel_library = Join-Path $sdkRoot "Lib\$sdkVersion\um\x64\kernel32.lib"
    sdk_ucrt_library = Join-Path $sdkRoot "Lib\$sdkVersion\ucrt\x64\ucrt.lib"
    sdk_resource_compiler = Join-Path $sdkRoot "bin\$sdkVersion\x64\rc.exe"
}
$identities = [ordered]@{}
foreach ($entry in $files.GetEnumerator()) {
    $file = Get-Item $entry.Value
    $identities[$entry.Key] = [ordered]@{
        path = $file.FullName
        file_version = $file.VersionInfo.FileVersion
        sha256 = (Get-FileHash $file.FullName -Algorithm SHA256).Hash.ToLower()
    }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$instances = & $vswhere -all -products '*' -format json | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw 'vswhere failed.' }
$instance = @($instances | Where-Object { $_.installationPath -eq $vsRoot })
if ($instance.Count -ne 1 -or -not $instance[0].isComplete) { throw 'VS instance is missing or incomplete.' }
$msbuildVersion = & $msbuild -nologo -version
if ($LASTEXITCODE -ne 0) { throw 'MSBuild version probe failed.' }
$report = [ordered]@{
    recorded_utc = [DateTime]::UtcNow.ToString('o')
    visual_studio_version = $instance[0].installationVersion
    visual_studio_display_version = $instance[0].catalog.productDisplayVersion
    installation_path = $vsRoot
    platform_toolset = 'v143'
    msvc_tools_version = $msvcVersion
    msbuild_version = ($msbuildVersion -join '').Trim()
    windows_sdk_version = $sdkVersion
    windows_sdk_root = $sdkRoot
    files = $identities
}
$report | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $logDir 'toolchain.json')
$report | ConvertTo-Json -Depth 8
