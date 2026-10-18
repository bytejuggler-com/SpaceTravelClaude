# Configures and builds the demo using the CMake + vcpkg bundled with Visual Studio 2022.
#   .\build.ps1                 # Debug build (dev overlay, hot-reload)
#   .\build.ps1 -Config release # RelWithDebInfo build (dev overlay, hot-reload)
#   .\build.ps1 -Config dist    # distribution build: no dev features, static libraries (see package.ps1)
#   .\build.ps1 -Run            # build, then launch
param(
    [ValidateSet('debug', 'release', 'dist')][string]$Config = 'debug',
    [switch]$Run
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
if (-not $vs) { throw 'Visual Studio 2022 not found' }
if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = Join-Path $vs 'VC\vcpkg' }

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmake) { $cmake.Source } else { Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }

# Pin the vcpkg registry to the current upstream commit the first time we build.
$vcpkgConfig = Join-Path $PSScriptRoot 'vcpkg-configuration.json'
if ((Get-Content $vcpkgConfig -Raw) -match 'RESOLVED_BY_BUILD_SCRIPT') {
    $sha = (git ls-remote https://github.com/microsoft/vcpkg refs/heads/master).Split("`t")[0]
    if ($sha -notmatch '^[0-9a-f]{40}$') { throw "Could not resolve vcpkg baseline (got '$sha')" }
    (Get-Content $vcpkgConfig -Raw).Replace('RESOLVED_BY_BUILD_SCRIPT', $sha) | Set-Content $vcpkgConfig -NoNewline
    Write-Host "Pinned vcpkg baseline to $sha"
}

$configurePreset = if ($Config -eq 'dist') { 'vs2022-dist' } else { 'vs2022' }
& $cmake --preset $configurePreset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build --preset $Config
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Run) {
    $exe = switch ($Config) {
        'debug' { 'build\vs2022\Debug\SpaceTravel.exe' }
        'release' { 'build\vs2022\RelWithDebInfo\SpaceTravel.exe' }
        'dist' { 'build\dist\Release\SpaceTravel.exe' }
    }
    & (Join-Path $PSScriptRoot $exe)
}
