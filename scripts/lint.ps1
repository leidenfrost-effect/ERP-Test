param(
  [switch]$CheckOnly,
  [switch]$RunClangTidy
)

$ErrorActionPreference = "Stop"

$filePatterns = @("*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx")
$roots = @("api", "core", "infra", "tests", "proto")

function Get-SourceFiles {
  $rg = Get-Command rg -ErrorAction SilentlyContinue
  if ($rg) {
    $globs = @()
    foreach ($pattern in $filePatterns) {
      $globs += "-g"
      $globs += $pattern
    }
    $args = @("--files") + $roots + $globs
    $result = & rg @args
    return $result | Where-Object { $_ -and (Test-Path $_) }
  }

  $files = @()
  foreach ($root in $roots) {
    if (Test-Path $root) {
      $files += Get-ChildItem -Recurse -Path $root -File -Include $filePatterns | ForEach-Object { $_.FullName }
    }
  }
  return $files
}

$files = Get-SourceFiles
if (-not $files -or $files.Count -eq 0) {
  Write-Host "No source files found for lint."
  exit 0
}

$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clangFormat) {
  Write-Host "clang-format not found. Skipping format checks."
} else {
  if ($CheckOnly) {
    & $clangFormat.Source --dry-run --Werror --style=file $files
  } else {
    & $clangFormat.Source -i --style=file $files
  }
}

if ($RunClangTidy) {
  $clangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue
  if (-not $clangTidy) {
    Write-Host "clang-tidy not found. Skipping static analysis."
    exit 0
  }

  if (-not (Test-Path "build/compile_commands.json")) {
    throw "build/compile_commands.json not found. Configure the project first."
  }

  foreach ($file in $files) {
    & $clangTidy.Source $file -p build
  }
}
