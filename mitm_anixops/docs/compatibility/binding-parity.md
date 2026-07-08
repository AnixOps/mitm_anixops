# Binding Parity Source Contract

Capability: C runner, Go wrapper, and Rust wrapper parity for shared policy
fixtures.

Ecosystem: `portable adapter boundary`.

Status: `partial`.

## Purpose

This contract keeps the no-UI C runner, Go cgo wrapper, and Rust FFI wrapper
anchored to one shared fixture. Each binding may expose different host-language
types, but the observed policy outputs must stay equivalent for the same config,
URL, phase, body, and script metadata.

## Input Forms

The current shared fixture is:

```text
tests/fixtures/BindingParity.Common.conf
```

It covers:

- MITM hostname parsing;
- request URL redirect;
- request header mutation;
- request body mutation;
- response header mutation;
- response body mutation;
- request and response script dispatch metadata;
- argument substitution for script metadata.

## Positive Case

Expected behavior:

- C runner `scan` loads the fixture and reports stable rule counts.
- C runner `trace` reports the same request/response header and script metadata
  that bindings expose through plan APIs.
- Go wrapper `TestGoBindingLoadsSharedParityFixture` loads the fixture and
  verifies redirect, request plan, response plan, body rewrite, header rewrite,
  script tag, script path, timeout, max-size, and argument output.
- Rust wrapper `rust_binding_loads_shared_parity_fixture` verifies the same
  policy outputs through the Rust safe wrapper.

## Negative Guards

This contract is not a production adapter claim. Existing negative coverage
continues to guard unsafe or mismatched behavior:

- phase mismatch remains covered by `PlanApiParity.PhaseMismatch.conf`;
- missing named-header checks remain covered in Go and Rust wrapper tests;
- malformed MITM host parsing remains covered by `MITM.Hostname.Malformed.conf`;
- live traffic mutation remains adapter-owned and deferred.

## Runtime And Security Boundary

This contract covers policy-core and binding parity only.

It does not implement:

- production HTTP/TLS interception;
- platform certificate lifecycle;
- platform header map mutation;
- production JavaScript runtime execution;
- streaming body mutation;
- host adapter redaction or telemetry export.

## CI Evidence

Required CI evidence:

- `make runner-check` runs C runner `scan` and `trace` checks against
  `BindingParity.Common.conf`;
- `make go-binding-check` runs `TestGoBindingLoadsSharedParityFixture`;
- `make rust-binding-check` runs `rust_binding_loads_shared_parity_fixture`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
binding parity
```

The row remains `partial` until parity expands to golden JSON trace fixtures,
named-header current-value behavior, and every release-package binding surface.
