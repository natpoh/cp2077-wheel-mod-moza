# Build the project: patched mod_settings.dll (vendor/mod_settings/) followed by
# direct_wheel.dll itself.
#
# Requires:
#   - Visual Studio 2022 with the "Desktop development with C++" workload
#   - CMake 3.21+ on PATH, OR the "C++ CMake tools for Windows" VS workload
#     (which bundles a cmake.exe under the VS install dir)
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File build.ps1
#   powershell -ExecutionPolicy Bypass -File build.ps1 -Config Debug

param(
  [ValidateSet("Debug", "Release", "RelWithDebInfo")]
  [string]$Config = "Release",

  [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

function Resolve-CMake {
  $candidates = @(
    (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -First 1).Source,
    "${Env:ProgramFiles}\CMake\bin\cmake.exe",
    "${Env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "${Env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "${Env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  )
  foreach ($c in $candidates) {
    if ($c -and (Test-Path $c)) { return $c }
  }
  throw "CMake not found. Install CMake 3.21+ or add the 'C++ CMake tools for Windows' VS workload."
}

$cmake = Resolve-CMake
Write-Host "Using CMake: $cmake"

# ---------- submodule init -------------------------------------------------
# Pulls vendor/RED4ext.SDK and vendor/mod_settings (and mod_settings' own
# nested deps: spdlog, Detours, RED4ext.SDK fork, cpcmake, archive_xl,
# red_lib). Recursive so the second-level submodules under vendor/mod_settings
# also resolve in one pass.

if (-not (Test-Path "vendor\RED4ext.SDK\CMakeLists.txt")) {
  Write-Host "Initializing top-level submodules..."
  git submodule update --init --recursive
  if ($LASTEXITCODE -ne 0) { throw "git submodule update --init --recursive failed." }
}
if (-not (Test-Path "vendor\mod_settings\CMakeLists.txt")) {
  Write-Host "Initializing vendor/mod_settings submodule..."
  git submodule update --init --recursive vendor/mod_settings
  if ($LASTEXITCODE -ne 0) { throw "git submodule update --init --recursive vendor/mod_settings failed." }
}
# Even if vendor/mod_settings/ is checked out, its own nested submodules
# (deps/) may not be. Force-init them.
if (-not (Test-Path "vendor\mod_settings\deps\red4ext.sdk\CMakeLists.txt")) {
  Write-Host "Initializing vendor/mod_settings nested submodules..."
  git -C vendor/mod_settings submodule update --init --recursive
  if ($LASTEXITCODE -ne 0) { throw "nested submodule init in vendor/mod_settings failed." }
}

# ---------- patched mod_settings.dll ----------------------------------------
# vendor/mod_settings/ is our fork of jackhumbert/mod_settings with a
# `ModSettings.hidden` runtime property + empty-category UI filter.
# Builds via its own CMake project; output lands in
# vendor/mod_settings/build/Release/mod_settings.dll where deploy.ps1
# expects it.

$msDll       = "vendor\mod_settings\build\Release\mod_settings.dll"
$msBuildDir  = "vendor\mod_settings\build"
$msNeedBuild = -not (Test-Path $msDll)
if (-not $msNeedBuild) {
  $msDllTime = (Get-Item $msDll).LastWriteTime
  $msSrcTimes = Get-ChildItem -Recurse -Path "vendor\mod_settings\src","vendor\mod_settings\include","vendor\mod_settings\CMakeLists.txt" -File -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty LastWriteTime
  if ($msSrcTimes) {
    $msNewest = ($msSrcTimes | Measure-Object -Maximum).Maximum
    if ($msNewest -gt $msDllTime) { $msNeedBuild = $true }
  }
}
if ($msNeedBuild) {
  Write-Host "Building patched mod_settings.dll (vendor/mod_settings/)..."
  & $cmake -S vendor/mod_settings -B $msBuildDir -A x64
  if ($LASTEXITCODE -ne 0) { throw "mod_settings CMake configure failed." }
  & $cmake --build $msBuildDir --config Release
  if ($LASTEXITCODE -ne 0) { throw "mod_settings CMake build failed." }
  if (-not (Test-Path $msDll)) { throw "mod_settings build succeeded but $msDll not found." }
} else {
  Write-Host "mod_settings.dll is up to date; skipping mod_settings build."
}

# ---------- direct_wheel.dll -----------------------------------------------------

Write-Host "Configuring direct_wheel..."
& $cmake -S . -B $BuildDir -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

Write-Host "Building direct_wheel..."
& $cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

$dll = Join-Path $BuildDir "direct_wheel\$Config\direct_wheel.dll"
if (Test-Path $dll) {
  Write-Host ""
  Write-Host "Built: $dll"
  Write-Host "Built: $msDll"
  Write-Host ""
  Write-Host "Run deploy.ps1 to package or push to a game install."
} else {
  throw "Build succeeded but direct_wheel.dll was not found at $dll"
}
