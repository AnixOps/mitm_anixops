# Loon Inline Arguments Source Contract

Capability: Loon `#!arguments` inline defaults.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser behavior for Loon-style inline argument
defaults. Inline arguments are policy-core inputs for script argument template
resolution only; they do not create platform UI controls.

## Input Forms

The parser accepts:

```text
#!arguments = Name:value,Other:"quoted value"
```

Supported fields:

- comma-separated `name:value` pairs;
- optional whitespace around names and values;
- quoted values, with quotes removed before storing the default.

Malformed non-empty fields without a `:` separator are rejected. Empty
argument names are rejected.

## Parser Output

For valid inline arguments, the parser must:

- register one argument default per `name:value` pair;
- emit an accepted diagnostic with section `Argument`, action `arguments`,
  and message `inline arguments accepted`;
- leave script dispatch, rewrite, MITM, certificate, routing, and platform UI
  behavior unchanged.

Script argument templates may resolve `{Name}` and `{{{Name}}}` placeholders
from inline defaults. Runtime argument overrides supplied through
`anixops_engine_set_argument_value` take precedence over inline defaults.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.InlineArguments.plugin
```

Expected behavior:

- config load succeeds;
- two argument defaults are registered;
- the request script argument template resolves inline defaults;
- runtime argument override changes the resolved script argument;
- the fixture MITM hostname still parses as a normal host pattern.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.InlineArguments.Malformed.plugin
```

Expected behavior:

- `ANIXOPS_COMPAT_LOON_STRICT` rejects the malformed inline argument line;
- no argument defaults or MITM host patterns are registered after the failure;
- a rejected diagnostic is recorded with section `Argument`, action
  `arguments`, and a message that names the missing `:` separator.

## Runtime And Security Boundary

Inline arguments must not:

- create Loon or host-platform UI controls;
- persist values outside the policy-core engine;
- read secrets or external configuration;
- alter MITM, rewrite, body/header mutation, certificate, or routing behavior.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_inline_arguments_fixture_resolves_script_defaults`;
- `tests/test_config.c` registers
  `config/loon_inline_arguments_malformed_fixture_rejects_missing_separator`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon inline arguments
```

The row remains `partial` because only parser defaults and script argument
template resolution are claimed. Platform UI controls and broader argument
forms remain unimplemented.
