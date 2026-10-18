# Builds the distribution configuration (no dev overlay or hot-reload, static runtime and libraries) and
# assembles a standalone folder: dist\SpaceTravel\ with the exe, shaders\, data\ and a readme.
#   .\package.ps1        # folder only
#   .\package.ps1 -Zip   # folder + dist\SpaceTravel.zip
param([switch]$Zip)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

& .\build.ps1 -Config dist
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$bin = Join-Path $PSScriptRoot 'build\dist\Release'
$out = Join-Path $PSScriptRoot 'dist\SpaceTravel'
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null

Copy-Item (Join-Path $bin 'SpaceTravel.exe') $out
Get-ChildItem $bin -Filter *.dll | Copy-Item -Destination $out  # none with static linking, kept as a safety net
Copy-Item -Recurse (Join-Path $PSScriptRoot 'shaders') (Join-Path $out 'shaders')
Copy-Item -Recurse (Join-Path $PSScriptRoot 'data') (Join-Path $out 'data')

@'
SPACE TRAVEL
============

A real-time demo in C++ and OpenGL 4.6, in the spirit of the Amiga and PC demoscene.
Requires Windows 10/11 x64 and a GPU with OpenGL 4.6 support.

Run SpaceTravel.exe. The demo starts fullscreen and loops forever.

  Esc            quit
  F11            toggle fullscreen
  M              mute music
  Space          pause
  Left / Right   seek 5 seconds
  PgUp / PgDn    previous / next scene
  F12            save a screenshot next to the exe

Options: --windowed, --width N --height N, --no-vsync, --mute

The scene sequence lives in data\demo.timeline (plain text, documented at its top).
Shaders are plain GLSL in shaders\.
'@ | Set-Content (Join-Path $out 'README.txt')

if ($Zip) {
    $archive = Join-Path $PSScriptRoot 'dist\SpaceTravel.zip'
    Compress-Archive -Path $out -DestinationPath $archive -Force
    Write-Host "Packaged $archive"
}
Write-Host "Packaged $out"
