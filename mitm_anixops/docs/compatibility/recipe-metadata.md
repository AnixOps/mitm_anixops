# Recipe Metadata Source Contract

Capability: recipe module v1 `#!recipe*` metadata tolerance.

Ecosystem: `loon`, `portable`.

Status: `partial`.

## Purpose

This contract fixes parser behavior for the `#!recipe*` metadata block defined
by [recipe module format v1](../recipes/recipe-spec-v1.md). The keys are
recorded as diagnostics so that `anixops-mitm-runner scan` can show one line
per recipe key, but they create no policy-core runtime behavior. A reader that
ignores every `#!recipe*` line gets the same rules, because recipe behavior
lives only in the standard sections.

## Input Forms

The parser tolerates exactly these v1 keys, compared case-insensitively:

- `#!recipe`
- `#!recipe.id`
- `#!recipe.version`
- `#!recipe.scope` (may repeat, one line per host)
- `#!recipe.authority`
- `#!recipe.strategy`
- `#!recipe.guard`
- `#!recipe.evidence`
- `#!recipe.reviewed`

Any other `#!recipe...` spelling (`#!recipes`, `#!recipe-id`,
`#!recipe.<unknown>`, `#!recipe.scope.<suffix>`) is an unsupported `#!` key and
is treated as a comment, exactly as in
[Loon Hashbang Metadata](loon-hashbang-metadata.md). New optional keys added
inside v1 are therefore ignorable by older builds without a diagnostic.

## Parser Output

For each tolerated recipe key the parser must emit an ignored diagnostic with:

- section `Plugin`;
- action equal to the metadata key, for example `recipe.scope`;
- message `#! metadata ignored`.

Recipe keys never populate `anixops_plugin_metadata_descriptor_t`. The policy
core does not validate the values; value checks are warnings emitted by
`anixops-mitm-runner scan` only, described below.

## Positive Case

Parser case:

```text
tests/fixtures/Loon.RecipeMetadata.plugin
```

Expected behavior under the Loon strict profile:

- config load succeeds;
- the nine recipe keys and `#!name` / `#!desc` are recorded as eleven ignored
  `Plugin` diagnostics;
- `[Argument]` and `[MITM]` still parse after the metadata block;
- the descriptor carries `#!name` only from this block; no recipe key reaches
  it;
- no rewrite or script rule is created by metadata.

## Negative Case

Parser case:

```text
tests/fixtures/Loon.RecipeMetadata.Unsupported.plugin
```

Expected behavior under the Loon strict profile:

- config load succeeds;
- near-miss spellings are not claimed as tolerated metadata diagnostics;
- only the `[MITM]` rule is recorded as accepted parser evidence.

## Runner Scan Warnings

When a file carries at least one tolerated recipe key, `anixops-mitm-runner
scan` appends a `recipeWarnings` array to its JSON output. Files without recipe
keys produce byte-identical output to earlier builds. Warnings never change the
scan exit status, the engine load status, or any loaded rule.

Each warning is `{"line":N,"key":"...","code":"...","value":"..."}`, where
`line` is `0` for a missing key. Warnings are emitted in this order:

| Code | Raised when |
| --- | --- |
| `missing-key` | one of `#!recipe`, `.id`, `.version`, `.scope`, `.authority`, `.strategy`, `.evidence` is absent |
| `invalid-value` | `#!recipe` is not `family/<digits>`, or `.authority`, `.strategy`, `.evidence` is outside its v1 enum |
| `missing-guard` | `.strategy` is `clamp` or `replay` and `#!recipe.guard` is absent |
| `scope-not-in-hostname` | a `#!recipe.scope` host is not listed in `[MITM] hostname` |
| `hostname-not-in-scope` | a `[MITM] hostname` host is not listed in `#!recipe.scope` |

Host comparison is case-insensitive and exact: globs are compared as text, `-`
deny entries and `%APPEND%` / `%INSERT%` markers are skipped, and
comma-separated lists are split on both sides.

Runner case:

```text
tests/fixtures/Loon.RecipeMetadata.Mismatch.plugin
```

Expected behavior, checked by `make runner-check`:

- scan exits with status `0`;
- `recipeWarnings` lists, in order, `invalid-value` for `recipe.authority`
  (`remote`), `missing-guard` for `clamp`, `scope-not-in-hostname` for
  `scope-only.recipe.loon.test`, and `hostname-not-in-scope` for
  `hostname-only.recipe.loon.test`;
- the positive fixture `Loon.RecipeMetadata.plugin` yields an empty
  `recipeWarnings` array, and a file without recipe keys has no
  `recipeWarnings` field.

## Runtime And Security Boundary

Recipe metadata must not:

- widen or narrow MITM hostnames (`#!recipe.scope` is not a hostname source);
- enforce, relax, or schedule anything named in `#!recipe.guard`;
- alter rewrite, header, body, script, routing, certificate, or trust behavior;
- fetch anything named in the metadata.

Scope and guard enforcement stay with the standard sections and the host, as
[recipe-spec-v1](../recipes/recipe-spec-v1.md#metadata-keys) states.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/loon_recipe_metadata_fixture_records_tolerated_keys`;
- `tests/test_config.c` registers
  `config/loon_recipe_metadata_unsupported_keys_are_not_claimed`;
- `make runner-check` asserts the runner scan warnings above;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
Recipe module metadata
```

The row is `partial` because the policy core only tolerates the keys and the
runner only warns; load-time rejection, glob-aware scope comparison, and guard
enforcement are not claimed.
