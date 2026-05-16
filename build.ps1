# Build the project: direct_wheel.dll.
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
    "${Env:ProgramFiles}\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
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

  Write-Host ""
  Write-Host "Run deploy.ps1 to package or push to a game install."
} else {
  throw "Build succeeded but direct_wheel.dll was not found at $dll"
}
