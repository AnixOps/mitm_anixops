# Loon Hashbang Metadata Source Contract

Capability: Loon hashbang metadata tolerance.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser behavior for common Loon-style `#!`
metadata. Metadata is recorded as diagnostics for operator visibility, but it
does not create policy-core runtime behavior.

## Input Forms

The parser tolerates these metadata keys:

- `#!name`
- `#!desc`
- `#!description`
- `#!author`
- `#!category`
- `#!icon`
- `#!homepage`
- `#!date`
- `#!system`
- `#!requirement`
- `#!arguments-desc`

`#!arguments` is not metadata; it is parsed as inline argument defaults and is
covered by the common Loon/Surge argument behavior.

## Parser Output

For tolerated metadata lines, the parser must emit ignored diagnostics with:

- section `Plugin`;
- action equal to the metadata key;
- message `#! metadata ignored`.

Unsupported `#!` keys are treated as comments, not as tolerated metadata. They
must not produce accepted policy rules or metadata diagnostics.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.HashbangMetadata.plugin
```

Expected behavior:

- config load succeeds;
- each tolerated metadata key is recorded as an ignored `Plugin` diagnostic;
- `[Argument]` and `[MITM]` entries still parse after metadata;
- no rewrite or script rules are created by metadata.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.HashbangMetadata.Unsupported.plugin
```

Expected behavior:

- config load succeeds under the Loon strict profile;
- unsupported `#!` keys are not claimed as tolerated metadata diagnostics;
- only the `[MITM]` rule is recorded as accepted parser evidence.

## Runtime And Security Boundary

Hashbang metadata must not:

- download icons or homepages;
- open external URLs;
- alter MITM, rewrite, header, body, script, routing, certificate, or trust
  behavior;
- expose platform UI behavior from the C policy core.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_hashbang_metadata_fixture_records_tolerated_keys`;
- `tests/test_config.c` registers
  `config/loon_hashbang_metadata_unsupported_keys_are_not_claimed`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon hashbang metadata
```

The row remains `partial` because metadata is tolerated only as parser
diagnostics and does not claim platform UI behavior.
