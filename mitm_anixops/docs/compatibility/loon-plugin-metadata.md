# Loon Plugin Metadata Source Contract

Capability: Loon `[Plugin]` metadata tolerance and typed descriptor.

Ecosystem: `loon`.

Status: `partial`.

## Purpose

This contract fixes the P1 parser behavior for Loon-style `[Plugin]` metadata
sections. Metadata is recorded for operator visibility as ignored diagnostics,
and common descriptor fields are exposed through a read-only C ABI descriptor.
Metadata does not create policy-core runtime behavior.

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

The parser exposes these keys through `anixops_plugin_metadata_descriptor_t`:

- `name`
- `desc`
- `description` as the descriptor `desc`
- `author`
- `icon`
- `homepage`

Every non-empty line inside `[Plugin]` remains metadata-only input. Descriptor
fields use parser order, so a later value for the same field replaces an earlier
value.

## Parser Output

For `[Plugin]` metadata lines, the parser must emit ignored diagnostics with:

- section `Plugin`;
- action `line`;
- message `plugin metadata ignored`.

Common descriptor fields are copied into
`anixops_plugin_metadata_descriptor_t`. `[Plugin]` lines must not register
argument defaults, rewrite rules, script rules, MITM host patterns, task
descriptors, route choices, certificate settings, external downloads, or
platform UI behavior.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.PluginMetadata.plugin
```

Expected behavior:

- config load succeeds;
- each `[Plugin]` metadata line is recorded as an ignored diagnostic;
- common descriptor fields are available through
  `anixops_engine_copy_plugin_metadata_descriptor`;
- the later `[MITM]` entry still parses after metadata;
- no argument, rewrite, or script rules are created by `[Plugin]` metadata.

Descriptor parser case:

```text
tests/fixtures/Loon.MetadataDescriptor.plugin
```

Expected behavior:

- config load succeeds;
- `name`, `desc`, `author`, `icon`, and `homepage` are exposed through the
  descriptor API;
- descriptor extraction still records metadata-only ignored diagnostics;
- no argument, rewrite, script, task, or MITM rules are created by descriptor
  metadata.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.PluginMetadata.Unsupported.plugin
```

Expected behavior:

- config load succeeds under the Loon strict profile;
- `hostname`, `rewrite`, `script`, and `argument`-shaped lines inside
  `[Plugin]` are not claimed as supported policy syntax;
- unsupported keys do not populate descriptor fields;
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
- `tests/test_config.c` registers
  `config/loon_metadata_descriptor_fixture_exposes_typed_metadata`;
- `tests/test_config.c` registers
  `config/loon_metadata_descriptor_unsupported_keys_do_not_populate_descriptor`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Loon plugin metadata
```

The row remains `partial` because descriptor extraction is policy-core metadata
only and no platform UI, download, install prompt, or external open behavior is
claimed.
