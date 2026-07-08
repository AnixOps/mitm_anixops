# Header Mutation Common Source Contract

Capability: request and response header mutation.

Ecosystem: `portable`, `loon`, `quantumultx`.

Status: `partial`.

## Purpose

This contract fixes the v1.0.0 source of truth for the common header mutation
subset. It covers parser and policy-core output for bounded header operations;
it does not claim platform HTTP parser, unbounded header map, or network
serialization behavior.

## Input Forms

The current common subset accepts:

- `[Header Rewrite]`, `[Remote Header Rewrite]`, `[Rewrite]`, and
  `[URL Rewrite]` section aliases;
- request header actions: `header-add`, `header-replace`, `header-del`, and
  `header-replace-regex`;
- response header actions: `response-header-add`, `response-header-replace`,
  `response-header-del`, and `response-header-replace-regex`;
- URL capture expansion in non-regex replacement values;
- current-header-value regex replacement for regex actions.

## Parser Output

The parser must produce:

- accepted diagnostics for supported header mutation lines;
- rejected diagnostics for invalid URL or header regexes;
- request or response phase header rewrite rules observable through the public
  ABI.

## Positive Case

Parser case:

```text
tests/fixtures/HeaderMutation.Common.conf
```

Expected behavior:

- config load succeeds;
- five header mutation rules are registered;
- request header add, replace, and delete behavior are observable through
  `anixops_rewrite_evaluate_header`;
- response header regex replacement and delete behavior are observable through
  `anixops_rewrite_evaluate_header`;
- phase separation prevents request header rules from matching response phase
  evaluation.

## Negative Case

Parser case:

```text
tests/fixtures/HeaderMutation.Common.Malformed.conf
```

Expected behavior:

- the invalid header regex rejects config load;
- a rejected rule diagnostic is recorded with section `Rewrite` and action
  `rewrite`;
- last error reports parse failure at the malformed line.

## Runtime And Security Boundary

This contract covers bounded policy-core header mutation decisions.

It does not implement:

- HTTP parsing, request serialization, or network I/O;
- unbounded platform header map behavior;
- hop-by-hop header filtering;
- cookie store semantics;
- streaming response framing;
- TLS interception or certificate lifecycle.

Those remain adapter, runner, or separate policy contracts.

## CI Evidence

Required CI evidence:

- `tests/test_config.c` registers
  `config/header_mutation_common_fixture_is_supported`;
- `tests/test_config.c` registers
  `config/header_mutation_common_fixture_rejects_invalid_regex`;
- GitHub Actions `linux-test` runs `sh scripts/check.sh` and must pass.

## Compatibility Matrix Row

Row:

```text
header mutation
```

The row remains `partial` until adapter-owned header map behavior, broader
ecosystem grammar, and full cookie/header edge-case corpus are covered.
