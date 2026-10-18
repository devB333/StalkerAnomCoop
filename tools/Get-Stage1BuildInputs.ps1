$ErrorActionPreference = 'Stop'
$workspace = Split-Path $PSScriptRoot -Parent
$source = Join-Path $workspace 'builds\engine-work'
$logDir = Join-Path $workspace 'builds\logs\S01-T01'
$toolchain = Get-Content (Join-Path $logDir 'toolchain.json') -Raw | ConvertFrom-Json
$msbuild = $toolchain.files.msbuild.path
$env:TEMP = Join-Path $workspace 'builds\tmp\toolchain-probe'
$env:TMP = $env:TEMP
$solution = Get-Content (Join-Path $source 'src\engine-vs2022.sln') -Raw
$projects = [regex]::Matches($solution, '(?m)^Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcxproj)", "([^"]+)"')
$results = @()
foreach ($project in $projects) {
    $name = $project.Groups[1].Value
    $path = Join-Path "$source\src" $project.Groups[2].Value
    $configuration = [regex]::Match($solution, [regex]::Escape($project.Groups[3].Value) + '\.DX11\|x64\.Build\.0 = ([^|\r\n]+)\|x64')
    if (-not $configuration.Success) { continue }
    $arguments = @(
        $path, '/nologo', ('/p:Configuration=' + $configuration.Groups[1].Value), '/p:Platform=x64',
        "/p:SolutionDir=$source\src\", '/p:PreferredToolArchitecture=x64',
        ('/p:WindowsTargetPlatformVersion=' + $toolchain.windows_sdk_version),
        ('/p:VCToolsVersion=' + $toolchain.msvc_tools_version),
        '-getProperty:ConfigurationType,PlatformToolset,VCToolsVersion,WindowsTargetPlatformVersion,OutDir,IntDir,TargetPath,PostBuildEventUseInBuild'
    )
    $raw = & $msbuild @arguments
    if ($LASTEXITCODE -ne 0) { throw "MSBuild evaluation failed for $name`: $raw" }
    $properties = ($raw -join "`n" | ConvertFrom-Json).Properties
    foreach ($field in @('OutDir', 'IntDir', 'TargetPath')) {
        $value = $properties.$field
        if (-not [IO.Path]::IsPathRooted($value)) { $value = Join-Path (Split-Path $path) $value }
        $resolved = [IO.Path]::GetFullPath($value)
        if (-not $resolved.StartsWith($source + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "$name $field escapes the working source: $resolved"
        }
    }
    if ($properties.PlatformToolset -ne 'v143' -or $properties.VCToolsVersion -ne $toolchain.msvc_tools_version) {
        throw "$name does not use the selected compiler."
    }
    $results += [ordered]@{ name = $name; project = $path; configuration = $configuration.Groups[1].Value; properties = $properties }
}
$results | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $logDir 'evaluated-projects.json')
Write-Output "Evaluated $($results.Count) projects; compiler pins match and output paths stay in the working checkout."
