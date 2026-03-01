param(
  [string]$DbPath = ".\\build\\persons.db",
  [int]$Port = 18080,
  [int]$Concurrency = 4,
  [string]$Token = "",
  [string]$Tokens = "",
  [string]$ApiExe = ".\\build\\api\\Release\\person_api.exe"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ApiExe)) {
  throw "API executable was not found: $ApiExe"
}

if ($Tokens) {
  $env:PERSON_API_TOKENS = $Tokens
  $env:PERSON_API_TOKEN = ""
} elseif ($Token) {
  $env:PERSON_API_TOKEN = $Token
  $env:PERSON_API_TOKENS = ""
}

& $ApiExe $DbPath $Port $Concurrency
