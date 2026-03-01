param(
  [string]$BaseUrl = "http://127.0.0.1:18080",
  [string]$Token = ""
)

$ErrorActionPreference = "Stop"

function Invoke-Api {
  param(
    [string]$Method,
    [string]$Url,
    [string]$Body = "",
    [string]$ContentType = "application/json",
    [switch]$ExpectBinary
  )

  $headers = @{}
  if ($Token) {
    $headers["Authorization"] = "Bearer $Token"
  }

  if ($ExpectBinary) {
    $headers["Accept"] = "application/x-protobuf"
  } else {
    $headers["Accept"] = "application/json"
  }

  if ($Body) {
    return Invoke-WebRequest -UseBasicParsing -Method $Method -Uri $Url -Headers $headers -ContentType $ContentType -Body $Body
  }
  return Invoke-WebRequest -UseBasicParsing -Method $Method -Uri $Url -Headers $headers
}

$health = Invoke-Api -Method GET -Url "$BaseUrl/health"
Write-Host "health:" $health.StatusCode $health.Content

$ready = Invoke-Api -Method GET -Url "$BaseUrl/ready"
Write-Host "ready:" $ready.StatusCode $ready.Content

$list = Invoke-Api -Method GET -Url "$BaseUrl/persons?page=1&pageSize=5"
Write-Host "list:" $list.StatusCode $list.Content

$pbHealth = Invoke-Api -Method GET -Url "$BaseUrl/pb/health" -ExpectBinary
Write-Host "pb health:" $pbHealth.StatusCode
