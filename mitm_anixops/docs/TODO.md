# TODO And Non-Goals

This file tracks remaining work for `mitm_anixops` as an embeddable MITM/rewrite policy-chain library. It is not a
proxy client roadmap.

## Before A Stable Release

- Keep `ci/abi_exports.txt` in sync with every intentional C ABI change.
- Decide whether unsupported-but-recognized rule actions should remain ignored or return `ANIXOPS_ERR_PARSE` in a future
  major/minor version.

## Compatibility Gaps

- JQ-style JSON body rewrite actions.
- Full NSRegularExpression/PCRE compatibility. The current implementation uses POSIX ERE.
- Full Quantumult X rewrite grammar beyond the tested `url script-response-body` and common reject/rewrite actions.
- Full Surge rule grammar beyond tested `type=`, `pattern=`, `script-path=`, `requires-body=`, `tag=`, and `argument=`.
- Exact AnixOps/Loon behavior for every malformed line. Current behavior ignores incomplete non-action lines and reports
  regex compilation failures.
- Exact rewrite/script ordering for all edge cases in larger platform clients.

## Explicit Non-Goals For This Stage

- Full proxy forwarding implementation.
- TLS socket handling.
- Dynamic leaf certificate generation.
- System certificate installation or platform trust UI.
- HTTP/2 frame parsing.
- Compression, decompression, chunked transfer decoding, or body streaming.
- JavaScript runtime embedding.
- GUI client features.
- iOS/macOS Network Extension implementation.
- Platform-specific packet capture or TUN routing.

## Adapter Responsibilities

Adapters such as NetworkCore or another proxy client must own:

- Network IO and upstream proxy routing.
- TLS session handling and certificate generation/trust decisions.
- HTTP/1.1 and HTTP/2 message parsing and serialization.
- Body buffering, decoding, recompression, transfer framing, and size limits.
- Header map storage, case-insensitive lookup, and multi-value semantics.
- Script asset download/cache policy and JavaScript execution.
- Concurrency control around each `anixops_engine_t`, or an engine snapshot strategy.
