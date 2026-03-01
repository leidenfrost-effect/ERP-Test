param(
  [string]$BuildDir = "build",
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
ctest --test-dir $BuildDir -C $Config --output-on-failure
