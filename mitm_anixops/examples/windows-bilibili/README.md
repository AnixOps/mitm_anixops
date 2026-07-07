# Windows Bilibili Homepage MITM Demo

This demo proves the Windows test shape:

```text
browser -> mitm_anixops MITM shim -> mihomo HTTP proxy -> www.bilibili.com
```

The module matches `https://www.bilibili.com/`, runs a local response script, removes CSP for the demo response, injects
CSS/JS, changes visible titles to `test`, and darkens cover images.

## Prerequisites

- Windows 10/11.
- A running mihomo HTTP or mixed proxy, for example `http://127.0.0.1:7890`.
- Node.js in `PATH`.
- Go and a MinGW-w64/MSYS2 UCRT64 GCC toolchain in `PATH`.
- A POSIX regex library for MinGW-w64, such as the MSYS2 UCRT64 `libsystre` package.

One MSYS2 UCRT64 setup path is:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libsystre
```

Use the UCRT64 shell or make sure its `bin` directory is before other toolchains in `PATH`.

## Build

From this directory in PowerShell:

```powershell
.\build-windows.ps1
```

This creates:

```text
..\..\build\e2e_mitm_proxy_shim.exe
```

## Run With An Existing Mihomo Proxy

If mihomo is already listening on `127.0.0.1:7890`:

```powershell
.\start-bilibili-demo.ps1 -MihomoProxy http://127.0.0.1:7890
```

The script prints the generated CA path. Import it into the current user's Trusted Root store manually, or run with
explicit CA installation:

```powershell
.\start-bilibili-demo.ps1 -MihomoProxy http://127.0.0.1:7890 -InstallCert
```

Then set the browser HTTP and HTTPS proxy to:

```text
127.0.0.1:19080
```

Open:

```text
https://www.bilibili.com/
```

Expected result:

- `document.title` becomes `test`.
- Many visible card titles become `test`.
- Cover images are rendered dark/black by injected CSS.
- The HTML response includes `X-AnixOps-Bilibili-Demo: applied`.

## Run With A Temporary Mihomo Process

If you want the script to start a direct-mode mihomo instance:

```powershell
.\start-bilibili-demo.ps1 -MihomoBin C:\path\to\mihomo-windows-amd64.exe -MihomoPort 19091
```

## Cleanup

Stop the script with `Ctrl+C`, then restore the browser proxy settings.

If you used `-InstallCert`, remove the generated test CA from:

```text
certmgr.msc -> Current User -> Trusted Root Certification Authorities -> Certificates
```

Look for the certificate subject:

```text
mitm_anixops e2e CA
```

## Notes

- This is a test harness, not a production Windows service.
- The shim dynamically generates leaf certificates from the temporary CA.
- Non-MITM `CONNECT` traffic is tunneled through mihomo so normal page assets do not fail just because they are outside
  the demo module's `hostname` list.
- For response scripts, the shim asks upstream for `identity` encoding so the local script receives text HTML instead
  of compressed bytes.
