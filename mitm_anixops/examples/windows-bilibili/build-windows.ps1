[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$ExampleDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = (Resolve-Path (Join-Path $ExampleDir "..\..")).Path
$BuildDir = Join-Path $Root "build"
$ObjectFile = Join-Path $BuildDir "mitm_anixops.o"
$StaticLib = Join-Path $BuildDir "libmitm_anixops.a"
$ShimExe = Join-Path $BuildDir "anixops-mitm.exe"

function Require-Command($Name) {
  if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
    throw "Missing command '$Name'. Install MSYS2 UCRT64/MinGW-w64 tools and make sure they are in PATH."
  }
}

Require-Command "gcc"
Require-Command "ar"
Require-Command "go"

New-Item -ItemType Directory -Force $BuildDir | Out-Null

Push-Location $Root
try {
  & gcc -Iinclude -std=c99 -O2 -Wall -Wextra -Werror -c src/mitm_anixops.c -o $ObjectFile
  if ($LASTEXITCODE -ne 0) {
    throw "gcc failed"
  }

  & ar rcs $StaticLib $ObjectFile
  if ($LASTEXITCODE -ne 0) {
    throw "ar failed"
  }

  $env:CGO_ENABLED = "1"
  Push-Location (Join-Path $Root "e2e\shim")
  try {
    & go build -o $ShimExe .
    if ($LASTEXITCODE -ne 0) {
      throw "go build failed"
    }
  } finally {
    Pop-Location
  }
} finally {
  Pop-Location
}

Write-Host "Built $ShimExe"
