# Windows Bilibili Homepage MITM Demo

This demo proves the Windows test shape:

```text
browser -> mitm_anixops MITM shim -> mihomo HTTP proxy -> www.bilibili.com
```

The binary can run without a UI. With `--bilibili-demo`, it matches `https://www.bilibili.com/`, runs a built-in
response mutator, removes CSP for the demo response, injects CSS/JS, changes visible titles to `test`, and darkens
cover images.

## Prerequisites

- Windows 10/11.
- A running HTTP/mixed proxy, for example mihomo at `http://127.0.0.1:7890`; or a `ss://` link plus a local
  mihomo/sing-box executable.
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
..\..\build\anixops-mitm.exe
```

## Run With An Existing HTTP/Mixed Proxy

If mihomo is already listening on `127.0.0.1:7890`:

```powershell
.\start-bilibili-demo.ps1 -Upstream http://127.0.0.1:7890 -InstallCert -Debug
```

Or run the binary directly:

```powershell
..\..\build\anixops-mitm.exe --bilibili-demo --listen 127.0.0.1:19080 --upstream http://127.0.0.1:7890 --install-ca --debug
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

## Run With An `ss://` Link

If you have a Shadowsocks link and a mihomo binary:

```powershell
.\start-bilibili-demo.ps1 -Upstream "ss://BASE64_METHOD_PASSWORD@host:port#name" -CoreBin C:\path\to\mihomo.exe -InstallCert -Debug
```

The same command works with sing-box:

```powershell
.\start-bilibili-demo.ps1 -Upstream "ss://BASE64_METHOD_PASSWORD@host:port#name" -CoreBin C:\path\to\sing-box.exe -Core sing-box -InstallCert -Debug
```

Direct binary form:

```powershell
..\..\build\anixops-mitm.exe --bilibili-demo --upstream "ss://BASE64_METHOD_PASSWORD@host:port#name" --core-bin C:\path\to\mihomo.exe --install-ca --debug
```

The tool generates a temporary mihomo/sing-box config with a local mixed proxy, then uses that local proxy as the
AnixOps upstream.

## Debugging

Use `--debug` to print:

```text
CONNECT host, MITM intercept/bypass decision
TLS handshake status
request URL and method
upstream response status/content-type/content-encoding
matched built-in/script rule
response mutation result
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
- `--install-ca` installs the generated CA into the current user's root store. You can omit it and import the printed
  CA path manually.
- Non-MITM `CONNECT` traffic is tunneled through mihomo so normal page assets do not fail just because they are outside
  the demo module's `hostname` list.
- For response scripts, the shim asks upstream for `identity` encoding so the local script receives text HTML instead
  of compressed bytes.
