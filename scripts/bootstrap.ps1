param(
  [string]$BuildDir = "build",
  [string]$ToolchainFile = "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/vcpkg/scripts/buildsystems/vcpkg.cmake",
  [switch]$ClearProxy
)

$ErrorActionPreference = "Stop"

if ($ClearProxy) {
  $env:HTTP_PROXY = ""
  $env:HTTPS_PROXY = ""
  $env:ALL_PROXY = ""
}

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
  $fallback = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  if (-not (Test-Path $fallback)) {
    throw "cmake executable was not found."
  }
  $cmakePath = $fallback
} else {
  $cmakePath = $cmake.Source
}

& $cmakePath -S . -B $BuildDir -DCMAKE_TOOLCHAIN_FILE="$ToolchainFile"
