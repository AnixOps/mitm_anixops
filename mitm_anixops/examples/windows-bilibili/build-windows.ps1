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
  $Command = Get-Command $Name -ErrorAction SilentlyContinue
  if (-not $Command) {
    throw "Missing command '$Name'. Install MSYS2 UCRT64/MinGW-w64 tools and make sure they are in PATH."
  }
  return $Command.Source
}

function Convert-ToGccPath($Path) {
  return ($Path -replace "\\", "/")
}

$MingwPrefix = $env:MINGW_PREFIX
if ([string]::IsNullOrWhiteSpace($MingwPrefix) -or -not (Test-Path $MingwPrefix)) {
  foreach ($Candidate in @("C:\msys64\ucrt64", "C:\msys64\mingw64")) {
    if (Test-Path (Join-Path $Candidate "bin\gcc.exe")) {
      $MingwPrefix = $Candidate
      break
    }
  }
}
if ([string]::IsNullOrWhiteSpace($MingwPrefix) -or -not (Test-Path $MingwPrefix)) {
  throw "Unable to locate an MSYS2 MinGW prefix. Expected C:\msys64\ucrt64 or set MINGW_PREFIX."
}

$MingwBin = Join-Path $MingwPrefix "bin"
$MingwInclude = Join-Path $MingwPrefix "include"
$MingwLib = Join-Path $MingwPrefix "lib"
$env:PATH = "$MingwBin;$env:PATH"

$Gcc = Require-Command "gcc"
$Ar = Require-Command "ar"
$Go = Require-Command "go"

Write-Host "MSYS2 prefix: $MingwPrefix"
Write-Host "gcc: $Gcc"
Write-Host "ar:  $Ar"
Write-Host "go:  $Go"
& $Gcc --version | Select-Object -First 1 | ForEach-Object { Write-Host $_ }

$RegexHeader = Join-Path $MingwInclude "regex.h"
if (-not (Test-Path $RegexHeader)) {
  throw "Missing regex.h at $RegexHeader. Install mingw-w64-ucrt-x86_64-libsystre."
}
Write-Host "regex.h: $RegexHeader"

foreach ($StaticLibName in @("libsystre.a", "libtre.a")) {
  $StaticLibPath = Join-Path $MingwLib $StaticLibName
  if (Test-Path $StaticLibPath) {
    Write-Host "${StaticLibName}: $StaticLibPath"
  }
}

New-Item -ItemType Directory -Force $BuildDir | Out-Null

Push-Location $Root
try {
  & $Gcc -Iinclude "-I$MingwInclude" -std=c99 -O2 -Wall -Wextra -Werror -c src/mitm_anixops.c -o $ObjectFile
  if ($LASTEXITCODE -ne 0) {
    throw "gcc failed"
  }

  & $Ar rcs $StaticLib $ObjectFile
  if ($LASTEXITCODE -ne 0) {
    throw "ar failed"
  }

  $env:CGO_ENABLED = "1"
  $env:CC = (Convert-ToGccPath $Gcc)
  $env:CGO_CFLAGS = "-I$(Convert-ToGccPath $MingwInclude)"
  $env:CGO_LDFLAGS = "-L$(Convert-ToGccPath $MingwLib) -Wl,-Bstatic -lsystre -ltre -Wl,-Bdynamic"
  $env:CGO_LDFLAGS_ALLOW = ".*"
  Push-Location (Join-Path $Root "e2e\shim")
  try {
    & $Go build -o $ShimExe .
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

$Objdump = Get-Command "objdump" -ErrorAction SilentlyContinue
if ($Objdump) {
  Write-Host "DLL dependencies:"
  & $Objdump.Source -p $ShimExe |
    Select-String "DLL Name" |
    ForEach-Object { Write-Host $_.Line.Trim() }
}
