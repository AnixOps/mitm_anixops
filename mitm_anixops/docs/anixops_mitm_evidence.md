# AnixOps MITM/Rewrite Evidence Map

This document separates confirmed reverse-engineering evidence from compatibility choices made by
`mitm_anixops`.

## Confirmed From App Resources

- `example.lcf` contains `[Rewrite]`, `[Script]`, and `[MITM]` sections.
- `[MITM]` supports `hostname` and `skip-server-cert-verify`.
- The syntax highlighter recognizes:
  - URL rewrite sections: `[Rewrite]`, `[URL Rewrite]`, `[Remote Rewrite]`.
  - MITM keys: `hostname`, `skip-server-cert-verify`, `ca-p12`, `ca-passphrase`.
  - URL/body/header/mock/reject rewrite actions.
- The main app contains switches such as `kEnableRewriteKey`, `kEnableMitmKey`,
  `kEnableH2MitmKey`, and `kDisableMitmQUICKey`.

Local references:

- `/root/crack/anixops/extracted/AnixOps/Payload/AnixOps.app/example.lcf`
- `/root/crack/anixops/extracted/AnixOps/Payload/AnixOps.app/highlight.min.js`
- `/root/crack/anixops/ghidra_index/AnixOpsProject/programs/AnixOps/strings.jsonl`

## Confirmed From `AnixOpsTunnelProvider`

Ghidra headless decompilation outputs are under:

```text
/root/crack/anixops/reports/decompile_mitm2
```

Confirmed runtime shape:

- `Decare` owns runtime configuration and MITM gates.
- `Decare::matchMitmHost:` performs the MITM hostname decision instead of letting every socket decide independently.
- `Decare::reloadMITMCert` checks MITM enable/certificate state before updating certificate statistics/cache.
- HTTP/1 cleartext and HTTPS MITM paths have parallel proxy socket implementations:
  - `LNHTTPProxySocket`
  - `LNHTTPSProxySocket`
- HTTP/2 has a separate path:
  - `LNHTTP2ProxySocket`
  - `HTTP2Stream`
- H2 URL rewrite mutates pseudo headers:
  - `:path`
  - `:authority`
- H2 redirect helpers exist for `302` and `307`.
- H2 mock response generation creates a fresh `HTTP2Stream`, defaulting status to `200` if the rewrite does not supply one.
- Request-side H2 script/rewrite work happens before notifying the delegate with finished request headers.
- Response-side H2 rewrite can modify response body through `URLRewrite` and then records statistics/session state.
- Rewrite action strings include `request-body-replace-regex`, `response-body-replace-regex`, `mock-request-body`, and
  `mock-response-body` in both the app and tunnel-provider indexed strings.
- Header rewrite action strings include `header-replace`, `header-add`, `header-del`, `header-replace-regex`,
  `response-header-del`, `response-header-replace`, `response-header-add`, and `response-header-replace-regex`.
- `Decare::setUpHTTPSProxy:` allocates `LNGCDHTTPSProxyServer` with port `0x27f7`.
- `Decare::setUpH2Proxy:` allocates `LNGCDH2ProxyServer` with port `0xa584`.
- `LNMitMRemoteSocket::generateHeaderData:` builds an internal `MITM ... ANIXOPSMITMFIX` handshake payload.
- `LNMitMRemoteSocket::didReadData:from:` waits for `ANIXOPSMITM200Established`; on success it notifies the tunnel
  delegate with `didReadySwitchDataWithSocket:`.
- `LNMitmProxyRemoteSocket::generateHeaderData` builds a CONNECT-style header with extra AnixOps metadata and records
  certificate/statistics data.
- `LNMitmProxyRemoteSocket::socket:didGetPeerCert:alpnHasH2:` notifies `didReadySwitchDataWithSocket:` after the
  upstream TLS handshake succeeds.

Key decompiled functions:

- `Decare::matchMitmHost:` at `10008f634`
- `Decare::reloadMITMCert` at `10008c400`
- `Decare::reloadConfigWith:reloadRules:reloadScript:` at `100090d30`
- `Decare::setUpHTTPSProxy:` at `1000938bc`
- `Decare::setUpH2Proxy:` at `10009399c`
- `LNHTTPProxySocket::searchMatchRewrite` at `1000b8fdc`
- `LNHTTPSProxySocket::searchMatchRewrite` at `10009eae0`
- `LNHTTP2ProxySocket::dealwithURLRewriteIn:` at `100121a44`
- `LNHTTP2ProxySocket::executeRequestScriptAndRewrite:` at `100123264`
- `LNHTTP2ProxySocket::executeResponseScriptAndRewrite:` at `100123474`
- `LNTunnel::didReceiveMitmHandshake:` at `10012516c`
- `LNMitMRemoteSocket::generateHeaderData:` at `10012ebbc`
- `LNMitMRemoteSocket::didReadData:from:` at `10012f26c`
- `LNMitmProxyRemoteSocket::generateHeaderData` at `10006a2a0`
- `LNMitmProxyRemoteSocket::socket:didGetPeerCert:alpnHasH2:` at `10006ac34`
- `HTTP2Stream::rewriteUrl:` at `10014548c`
- `HTTP2Stream::generateMockResponseStreamByRewrite:` at `100147694`
- `HTTP2Stream::h2302ResponseStream:` at `100151738`
- `HTTP2Stream::h2307ResponseStream:` at `100151898`

## Compatibility Choices In This Library

The library implements the decision/model layer, not AnixOps's socket runtime:

- MITM host matching supports exact host, wildcard host, and deny patterns with `-` or `!`.
- Certificate trust is an input supplied by the platform adapter.
- QUIC-for-MITM returns a decision (`ANIXOPS_MITM_REJECT_QUIC`) rather than dropping packets itself.
- URL rewrite uses POSIX ERE with a tested leading `(?i)` case-insensitive prefix, PCRE shorthand classes, absolute
  anchors, non-capturing group capture numbering, named capture groups, named capture replacement, and quoted literal matching subset. This is
  portable C, but not full NSRegularExpression/PCRE compatibility.
- Mock and regex body rewrite operate on already-buffered plain text. The platform adapter still owns body buffering,
  decompression, transfer framing, and HTTP writeback.
- Header rewrite returns structured operations, and `anixops_rewrite_apply_headers` can apply supported operations to a
  bounded case-insensitive header list with independent `Set-Cookie` entries. The platform adapter still owns HTTP
  parsing, unbounded storage, original casing policy, and writeback.
- `*.example.com` matches both `example.com` and subdomains in this library. Treat this as a documented
  compatibility choice until dynamic AnixOps behavior is tested.

## Unknown Or Not Yet Reimplemented

- Exact Objective-C selector mapping for every `objc_msgSend` thunk in the decompiled output.
- Exact request/response rewrite vs script ordering for every body-buffered edge case.
- Exact behavior for chunked/gzip/streaming response body rewrites.
- Exact HTTP/2 ALPN downgrade and QUIC rejection mechanics.
- Dynamic leaf certificate generation and cache format.
- JavaScript runtime semantics.

## Verification Rule

New behavior should only be promoted from compatibility choice to confirmed behavior after one of:

- A decompiled AnixOps function directly confirms it.
- A controlled dynamic test against AnixOps confirms it.
- A public AnixOps configuration example documents it clearly.
