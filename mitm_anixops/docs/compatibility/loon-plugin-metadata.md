# Loon Plugin Metadata Source Contract

Capability: Loon `[Plugin]` metadata tolerance.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser behavior for Loon-style `[Plugin]` metadata
sections. Metadata is recorded for operator visibility as ignored diagnostics,
but it does not create policy-core runtime behavior.

## Input Forms

The parser tolerates key/value lines inside:

```text
[Plugin]
name = Example
desc = Metadata only
author = Example Author
icon = https://assets.example/icon.png
homepage = https://docs.example/plugin
```

The parser does not interpret individual `[Plugin]` keys. Every non-empty line
inside `[Plugin]` is treated as metadata-only input.

## Parser Output

For `[Plugin]` metadata lines, the parser must emit ignored diagnostics with:

- section `Plugin`;
- action `line`;
- message `plugin metadata ignored`.

`[Plugin]` lines must not register argument defaults, rewrite rules, script
rules, MITM host patterns, task descriptors, route choices, certificate
settings, or platform UI behavior.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.PluginMetadata.plugin
```

Expected behavior:

- config load succeeds;
- each `[Plugin]` metadata line is recorded as an ignored diagnostic;
- the later `[MITM]` entry still parses after metadata;
- no argument, rewrite, or script rules are created by `[Plugin]` metadata.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.PluginMetadata.Unsupported.plugin
```

Expected behavior:

- config load succeeds under the Loon strict profile;
- `hostname`, `rewrite`, `script`, and `argument`-shaped lines inside
  `[Plugin]` are not claimed as supported policy syntax;
- no argument defaults, rewrite rules, script rules, or MITM host patterns are
  registered.

## Runtime And Security Boundary

`[Plugin]` metadata must not:

- download icons or homepages;
- open external URLs;
- create platform install prompts or settings UI;
- alter MITM, rewrite, header, body, script, routing, certificate, or trust
  behavior;
- expose hidden traffic capture or payload logging behavior.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_plugin_metadata_fixture_records_ignored_lines`;
- `tests/test_config.c` registers
  `config/loon_plugin_metadata_unsupported_keys_are_not_claimed`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon plugin metadata
```

The row remains `partial` because metadata is tolerated only as parser
diagnostics and no platform UI behavior is claimed.
