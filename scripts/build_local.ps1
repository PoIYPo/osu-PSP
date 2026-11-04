# Local dev helper for Windows: sets up a portable GCC/CMake inside .tools/,
# then configures, builds and tests the host targets. No admin rights needed.
#
#   powershell -ExecutionPolicy Bypass -File scripts/build_local.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/build_local.ps1 -Dump "testdata/1234.osu"
param(
    [switch]$Clean,
    [switch]$SkipTests,
    [string]$Dump
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Test-Path '.tools\mingw64\bin\g++.exe')) {
    Write-Host 'Portable toolchain missing, downloading (~315 MB, one time)...' -ForegroundColor Cyan
    node .tools/fetch.mjs
    if ($LASTEXITCODE -ne 0) { throw 'toolchain download failed' }
    & tar.exe -xf .tools\dl\mingw.zip -C .tools
    & tar.exe -xf .tools\dl\cmake.zip -C .tools
}

$env:PATH = "$root\.tools\mingw64\bin;$env:PATH"
$cmakeDir = Get-ChildItem .tools -Directory -Filter 'cmake-*' | Select-Object -First 1
$cmake = Join-Path $cmakeDir.FullName 'bin\cmake.exe'

if ($Clean -and (Test-Path build)) { Remove-Item -Recurse -Force build }

& $cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'configure failed' }

& $cmake --build build -j 8
if ($LASTEXITCODE -ne 0) { throw 'build failed' }

if (-not $SkipTests) {
    & .\build\osu_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
}

if ($Dump) {
    & .\build\osu_dump.exe $Dump --objects
}
