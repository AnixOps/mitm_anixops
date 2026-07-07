[CmdletBinding()]
param(
  [string]$Listen = "127.0.0.1:19080",
  [string]$Upstream = "http://127.0.0.1:7890",
  [string]$CoreBin = "",
  [string]$Core = "auto",
  [string]$CaCert = (Join-Path $env:TEMP "anixops-bilibili-demo-ca.pem"),
  [string]$CaKey = (Join-Path $env:TEMP "anixops-bilibili-demo-ca.key"),
  [switch]$InstallCert
)

$ErrorActionPreference = "Stop"

$ExampleDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = (Resolve-Path (Join-Path $ExampleDir "..\..")).Path
$ShimExe = Join-Path $Root "build\anixops-mitm.exe"

if (-not (Test-Path $ShimExe)) {
  throw "Missing $ShimExe. Run .\build-windows.ps1 first, or use the release zip."
}

$work = Join-Path $env:TEMP ("anixops-bilibili-demo-" + $PID)
New-Item -ItemType Directory -Force $work | Out-Null
$shimProcess = $null

try {
  Remove-Item -Force -ErrorAction SilentlyContinue $CaCert, $CaKey

  $shimOutLog = Join-Path $work "shim.out.log"
  $shimErrLog = Join-Path $work "shim.err.log"
  $shimArgs = @(
    "--listen", $Listen,
    "--upstream", $Upstream,
    "--bilibili-demo",
    "--ca-cert", $CaCert,
    "--ca-key", $CaKey
  )
  if ($InstallCert) {
    $shimArgs += "--install-ca"
  }
  $enableShimDebug = $PSBoundParameters.ContainsKey("Debug") -and [bool]$PSBoundParameters["Debug"]
  if ($enableShimDebug) {
    $shimArgs += "--debug"
  }
  if ($CoreBin -ne "") {
    $shimArgs += @("--core-bin", $CoreBin, "--core", $Core)
  }

  $shimProcess = Start-Process -FilePath $ShimExe `
    -ArgumentList $shimArgs `
    -RedirectStandardOutput $shimOutLog `
    -RedirectStandardError $shimErrLog `
    -PassThru `
    -WindowStyle Hidden

  for ($i = 0; $i -lt 100; $i++) {
    if ((Test-Path $CaCert) -and ((Get-Item $CaCert).Length -gt 0)) {
      break
    }
    Start-Sleep -Milliseconds 100
  }
  if (-not (Test-Path $CaCert)) {
    throw "AnixOps did not write CA certificate. See $shimErrLog"
  }

  Write-Host "MITM proxy:    http://$Listen"
  Write-Host "Upstream:      $Upstream"
  Write-Host "CA cert:       $CaCert"
  Write-Host "Shim stdout:   $shimOutLog"
  Write-Host "Shim stderr:   $shimErrLog"
  Write-Host ""
  Write-Host "Set the browser HTTP and HTTPS proxy to $Listen, then open https://www.bilibili.com/"
  Write-Host "Expected result: page title becomes test, visible card titles become test, and cover images are dark."
  Write-Host "Press Ctrl+C to stop."

  Wait-Process -Id $shimProcess.Id
} finally {
  if ($shimProcess -and -not $shimProcess.HasExited) {
    Stop-Process -Id $shimProcess.Id -Force
  }
}
