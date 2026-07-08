# Surge Requirement Metadata Source Contract

Capability: Surge requirement metadata tolerance.

Ecosystem: `surge`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser behavior for Surge-style `#!requirement`
metadata. Requirement metadata is recorded for operator visibility, but it does
not create policy-core runtime gates or platform capability checks.

## Input Forms

The parser tolerates these requirement-adjacent metadata keys in Surge modules:

- `#!name`
- `#!desc`
- `#!system`
- `#!requirement`
- `#!arguments-desc`

`#!arguments` is not metadata; it is parsed as inline argument defaults by the
Surge common-config contract.

## Parser Output

For tolerated metadata lines, the parser must emit ignored diagnostics with:

- section `Plugin`;
- action equal to the metadata key;
- message `#! metadata ignored`.

Unsupported requirement-like `#!` keys are treated as comments, not as
tolerated metadata. Bare requirement-like assignment lines outside a supported
section remain ignored parser input and must not create rules, MITM hosts,
scripts, or task descriptors.

## Positive Case

Parser case:

```text
tests/fixtures/Surge.RequirementMetadata.sgmodule
```

Expected behavior:

- config load succeeds;
- tolerated requirement metadata keys are recorded as ignored `Plugin`
  diagnostics;
- the later `[MITM]` entry still parses after metadata;
- no argument, rewrite, script, or task descriptors are created by metadata.

## Negative Case

Parser case:

```text
tests/fixtures/Surge.RequirementMetadata.Unsupported.sgmodule
```

Expected behavior:

- config load succeeds under the Surge strict profile;
- unsupported requirement-like `#!` keys are not claimed as tolerated metadata
  diagnostics;
- a bare `requirement = ...` line outside a supported section is ignored;
- only the `[MITM]` rule is recorded as accepted parser evidence.

## Runtime And Security Boundary

Surge requirement metadata must not:

- gate rule execution by Surge version, OS version, device, or entitlement;
- download dependencies or remote modules;
- alter MITM, rewrite, header, body, script, routing, certificate, or trust
  behavior;
- expose platform UI or install behavior from the C policy core.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/surge_requirement_metadata_fixture_records_tolerated_keys`;
- `tests/test_config.c` registers
  `config/surge_requirement_metadata_unsupported_keys_are_not_claimed`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Surge requirement metadata
```

The row remains `partial` because requirement metadata is tolerated only as
parser diagnostics and does not claim platform requirement gating behavior.
