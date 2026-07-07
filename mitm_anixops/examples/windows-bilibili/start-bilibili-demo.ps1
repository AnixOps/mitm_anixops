[CmdletBinding()]
param(
  [string]$Listen = "127.0.0.1:19080",
  [string]$MihomoProxy = "http://127.0.0.1:7890",
  [string]$MihomoBin = "",
  [int]$MihomoPort = 19091,
  [string]$CaCert = (Join-Path $env:TEMP "anixops-bilibili-demo-ca.pem"),
  [string]$CaKey = (Join-Path $env:TEMP "anixops-bilibili-demo-ca.key"),
  [switch]$InstallCert
)

$ErrorActionPreference = "Stop"

$ExampleDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = (Resolve-Path (Join-Path $ExampleDir "..\..")).Path
$ShimExe = Join-Path $Root "build\e2e_mitm_proxy_shim.exe"
$Plugin = Join-Path $ExampleDir "bilibili-homepage.plugin"
$ScriptFile = Join-Path $ExampleDir "bilibili_homepage_demo.js"
$Runner = Join-Path $Root "e2e\script_runtime\anixops_runner.js"
$ScriptUrl = "https://local.anixops.test/bilibili_homepage_demo.js"

if (-not (Test-Path $ShimExe)) {
  throw "Missing $ShimExe. Run .\build-windows.ps1 first."
}
if (-not (Get-Command "node" -ErrorAction SilentlyContinue)) {
  throw "Missing node.exe in PATH. Install Node.js before running the demo."
}

$work = Join-Path $env:TEMP ("anixops-bilibili-demo-" + $PID)
New-Item -ItemType Directory -Force $work | Out-Null
$mihomoProcess = $null
$shimProcess = $null

try {
  if ($MihomoBin -ne "") {
    if (-not (Test-Path $MihomoBin)) {
      throw "Mihomo binary not found: $MihomoBin"
    }
    $mihomoHome = Join-Path $work "mihomo-home"
    New-Item -ItemType Directory -Force $mihomoHome | Out-Null
    $mihomoConfig = Join-Path $work "mihomo.yaml"
@"
mixed-port: $MihomoPort
bind-address: 127.0.0.1
allow-lan: false
mode: rule
log-level: warning
ipv6: false
rules:
  - MATCH,DIRECT
"@ | Set-Content -Encoding ASCII $mihomoConfig

    $mihomoOutLog = Join-Path $work "mihomo.out.log"
    $mihomoErrLog = Join-Path $work "mihomo.err.log"
    $mihomoProcess = Start-Process -FilePath $MihomoBin `
      -ArgumentList @("-d", $mihomoHome, "-f", $mihomoConfig) `
      -RedirectStandardOutput $mihomoOutLog `
      -RedirectStandardError $mihomoErrLog `
      -PassThru `
      -WindowStyle Hidden
    $MihomoProxy = "http://127.0.0.1:$MihomoPort"
    Start-Sleep -Seconds 1
  }

  Remove-Item -Force -ErrorAction SilentlyContinue $CaCert, $CaKey

  $shimOutLog = Join-Path $work "shim.out.log"
  $shimErrLog = Join-Path $work "shim.err.log"
  $shimArgs = @(
    "--listen", $Listen,
    "--mihomo-proxy", $MihomoProxy,
    "--config", $Plugin,
    "--script-runner", $Runner,
    "--script-path-url", $ScriptUrl,
    "--script-path-file", $ScriptFile,
    "--ca-cert", $CaCert,
    "--ca-key", $CaKey
  )
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
    throw "Shim did not write CA certificate. See $shimErrLog"
  }

  if ($InstallCert) {
    & certutil.exe -user -addstore Root $CaCert | Out-Host
    if ($LASTEXITCODE -ne 0) {
      throw "certutil failed"
    }
  }

  Write-Host "MITM shim:     http://$Listen"
  Write-Host "Upstream:      $MihomoProxy"
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
  if ($mihomoProcess -and -not $mihomoProcess.HasExited) {
    Stop-Process -Id $mihomoProcess.Id -Force
  }
}
