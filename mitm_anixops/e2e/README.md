# E2E Fixtures

These fixtures prove the standalone ABI can be used in proxy/script data paths before the final iOS proxy core exists.

## Mihomo Proxy Chain

This fixture starts:

- official `MetaCubeX/mihomo` release binary as a black-box outbound HTTP proxy
- a local Go shim that calls `libmitm_anixops` through cgo
- a local HTTPS origin server
- `curl` as the client

The tested path is:

```text
curl -> mitm_anixops shim -> mihomo -> local HTTPS origin
```

The fixture uses `https://google.com:<local-port>` so the Host/SNI and rewrite matching behave like a real domain,
while `mihomo` maps `google.com` to `127.0.0.1` in its temporary config. This keeps the test deterministic and does
not depend on public internet reachability.

Run:

```sh
MIHOMO_BIN=/path/to/mihomo make e2e
```

`scripts/check.sh` also runs this fixture automatically when `MIHOMO_BIN` resolves to an executable binary.

In this workspace the default is:

```text
../downloads/mihomo-linux-amd64-compatible-v1.19.27
```

To install the pinned fixture binary used by CI:

```sh
sh scripts/ensure-mihomo.sh
```

Covered:

- loading a AnixOps-style `[Rewrite]` and `[MITM]` fixture through the C ABI
- trusted-cert MITM host decision for `google.com`
- live CONNECT interception with a test CA
- HTTPS URL rewrite returning `302 Location`
- no-rewrite pass-through via mihomo to a local HTTPS origin
- loading the real BiliUniverse Enhanced plugin in the same proxy path
- executing the `x/resource/show/tab/v2` response hook after MITM origin response

## BiliUniverse Script Runtime

This fixture starts:

- a small C matcher that loads pinned BiliUniverse `.plugin`, `.snippet`, and `.sgmodule` assets through
  `libmitm_anixops`
- a Node.js AnixOps-compatible script environment
- the real `response.bundle.js` asset from BiliUniverse Enhanced `v0.5.13`

The tested path is:

```text
BiliBili.Enhanced module asset -> mitm_anixops script dispatch -> Node AnixOps runner -> response.bundle.js
```

Run:

```sh
make bili-e2e
```

Run all E2E fixtures:

```sh
make e2e-all
```

Covered:

- loading the pinned real `BiliBili.Enhanced.plugin`, `.snippet`, and `.sgmodule` assets
- matching all four BiliUniverse Enhanced `http-response` hooks:
  `x/resource/show/tab/v2`, `x/v2/account/mine`, `x/v2/account/mine/ipad`,
  `x/v2/region/index`, and `x/v2/channel/region/list`
- resolving `$argument` from `[Argument]` and `#!arguments` defaults
- executing the real script bundle with `$request`, `$response`, `$argument`, `$persistentStore`, and `$done`
- asserting the response body is modified for tab, mine, iPad mine, region, and channel-region payloads

## Generic Script Contract

This fixture uses a tiny synthetic script to verify that the proxy adapter applies request and response fields returned
by `$done`:

```text
request header/body rewrite -> request script dispatch -> request header/body mutation -> local origin
response header/body rewrite -> response script dispatch -> status/header/body mutation -> MITM response
gzip/deflate response decode -> body rewrite/script mutation -> identity writeback
persistentStore write in request script -> persistentStore read in response script
response script timeout -> fail open with static-rewritten response
```

Run:

```sh
make script-contract-e2e
```

Covered:

- request header mutation
- request body mutation
- response status mutation
- response header mutation
- response body mutation
- `$request.url`, `$request.headers`, `$argument`, `$persistentStore`, and original `$response.body` propagation
- static request/response header and body rewrites before script dispatch
- response script timeout fail-open after static rewrites
- gzip/deflate response decode with identity writeback

Not covered yet:

- real iOS certificate installation/trust detection
- HTTP/2 frame-level rewrite
- QUIC downgrade behavior
- JavaScriptCore embedding on iOS
- brotli/zstd, chunked streaming, and full compressed body pipeline handling
- JQ rewrite execution through the proxy path
