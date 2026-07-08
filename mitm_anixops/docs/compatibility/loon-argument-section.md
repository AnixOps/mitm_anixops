# Loon Argument Section Source Contract

Capability: Loon `[Argument]` section defaults.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser behavior for Loon-style `[Argument]`
defaults. Argument values are policy-core inputs for script argument template
resolution only; they do not create host-platform UI controls or persistence.

## Input Forms

The parser accepts key/value defaults inside `[Argument]`:

```text
[Argument]
Mode = select,prod
Token = input,"quoted token"
Flag = switch,true
```

Supported behavior:

- the key before `=` is the argument name;
- the first comma-separated field after `=` is treated as argument kind
  metadata and is not exposed as policy behavior;
- the second comma-separated field is stored as the default value;
- quoted default values are unquoted before storage.

Malformed non-empty argument lines without `=` are rejected by strict
compatibility profiles.

## Parser Output

For valid argument lines, the parser must:

- register one argument default per non-empty argument name;
- emit an accepted diagnostic with section `Argument`, action `argument`, and
  message `argument rule accepted`;
- leave script dispatch, rewrite, MITM, certificate, routing, task, and
  platform UI behavior unchanged.

Script argument templates may resolve `{Name}`, `{{{Name}}}`, and list-style
`[{Name},{Other}]` placeholders from `[Argument]` defaults. Runtime argument
overrides supplied through `anixops_engine_set_argument_value` take precedence
over defaults.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.ArgumentSection.plugin
```

Expected behavior:

- config load succeeds;
- three argument defaults are registered;
- the request script argument template resolves `[Argument]` defaults;
- runtime argument override changes the resolved script argument;
- the fixture MITM hostname still parses as a normal host pattern.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.ArgumentSection.Malformed.plugin
```

Expected behavior:

- `ANIXOPS_COMPAT_LOON_STRICT` rejects the malformed `[Argument]` line;
- no argument defaults, script rules, or MITM host patterns are registered after
  the failure;
- a rejected diagnostic is recorded with section `Argument`, action
  `argument`, and strict-profile rejection context.

## Runtime And Security Boundary

`[Argument]` defaults must not:

- create Loon or host-platform UI controls;
- persist values outside the policy-core engine;
- read secrets or external configuration;
- alter MITM, rewrite, body/header mutation, certificate, task, or routing
  behavior.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_argument_section_fixture_resolves_script_defaults`;
- `tests/test_config.c` registers
  `config/loon_argument_section_malformed_fixture_rejects_missing_equals`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon argument section
```

The row remains `partial` because only parser defaults and script argument
template resolution are claimed. Platform UI controls and broader argument
forms remain unimplemented.
