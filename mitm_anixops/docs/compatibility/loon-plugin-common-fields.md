# Loon Plugin Common Fields Source Contract

Capability: Loon plugin common fields.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes the first P1 parser milestone for the high-frequency Loon
plugin subset. It does not claim full Loon compatibility.

## Input Forms

The current common-field subset accepts:

- common `#!` metadata lines covered by
  [Loon Hashbang Metadata Source Contract](loon-hashbang-metadata.md);
- `[Plugin]` metadata lines covered by
  [Loon Plugin Metadata Source Contract](loon-plugin-metadata.md);
- `[Argument]` key/value defaults covered by
  [Loon Argument Section](loon-argument-section.md);
- `[URL Rewrite]` URL redirect and reject rules;
- `[Header Rewrite]` request/response header mutation rules;
- `[Script]` `http-request` and `http-response` rules with dispatch metadata
  covered by [Loon Script Metadata](loon-script-metadata.md);
- `[Script]` scheduled task descriptors covered by
  [Loon Task Metadata](loon-task-metadata.md);
- `[MITM] hostname = ...` with exact and deny host entries;
- `[MITM]` adapter-visible options covered by
  [Loon MITM Options](loon-mitm-options.md).

## Parser Output

The parser must produce:

- accepted diagnostics for supported rules;
- ignored metadata diagnostics for tolerated metadata;
- rejected diagnostics for malformed rules under strict compatibility profiles;
- rule counts for argument, rewrite, script, and MITM host entries.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.CommonFields.plugin
```

Expected behavior:

- config load succeeds;
- one argument is registered;
- URL and header rewrite rules are registered;
- two script rules are registered;
- two MITM host patterns are registered;
- adapter-visible MITM options are covered by a dedicated fixture;
- metadata is tolerated and does not block load;
- URL rewrite, header rewrite, script dispatch, and MITM allow/deny behavior are
  observable through public ABI calls.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.CommonFields.Malformed.plugin
```

Expected behavior:

- `ANIXOPS_COMPAT_LOON_STRICT` rejects the malformed rewrite line;
- a rejected rule diagnostic is recorded with section `Rewrite` and action
  `rewrite`;
- last error reports parse failure at the malformed line.

## Runtime And Security Boundary

This contract covers parser and policy-core output only.

It does not implement:

- TLS interception;
- certificate generation or trust installation;
- HTTP parser or body streaming;
- JavaScript execution;
- remote script download or cache refresh.

Those remain runner, runtime, or adapter responsibilities.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers `config/loon_common_fields_fixture_is_supported`;
- `tests/test_config.c` registers
  `config/loon_common_fields_strict_fixture_rejects_malformed_rule`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon plugin common fields
```

The row remains `partial` until broader grammar, scheduled-task runtime
behavior, and larger real-plugin corpus coverage exist.
