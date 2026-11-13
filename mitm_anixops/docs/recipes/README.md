# Rewrite Recipes

This directory documents *recipes*: user-authored policy modules that combine
already-supported `mitm_anixops` primitives into one reproducible outcome.

A recipe is data, not code. It is expressed in the plugin grammar the policy
core already parses (Surge `.sgmodule`, Loon `.plugin`, Quantumult X `.snippet`,
portable `.conf`), plus tolerated `#!` metadata lines. Adding a recipe requires
no C ABI change, no new parser branch, and no compatibility matrix row, because
the recipe only reuses capabilities that are already `supported` or `partial`
in [../compatibility/matrix.md](../compatibility/matrix.md).

## Why Recipes Are A Separate Layer

The repository has four distinct documentation layers and they must stay
separate:

| Layer | Answers | Lives in |
| --- | --- | --- |
| Compatibility contract | What does the parser and policy core accept, and what proves it? | [`../compatibility/`](../compatibility/README.md) |
| Architecture contract | Who owns TLS, HTTP, framing, script execution, trust? | [`../architecture/`](../architecture/overview.md) |
| Recipe format | How is a composed module written, read, verified, and versioned? | this directory |
| Recipe examples | What does one concrete module for one concrete target look like? | outside this repository, see [external-examples.md](external-examples.md) |

A recipe never claims capability. It cites the contract for every primitive it
uses. If a recipe needs something the policy core does not accept, the recipe is
incomplete, and the correct follow-up is a compatibility contract, not a
recipe-local workaround.

## Scope Of This Directory

This directory defines the *format* and the *procedure*. It intentionally
carries no target-specific module, no target hostname, and no target parameter
set. Those live in per-author repositories, listed in
[external-examples.md](external-examples.md).

Keeping examples outside this repository is deliberate:

- a concrete module tracks one service's payload shape, which changes far
  faster than a format does;
- a concrete module is its author's artifact, with the author's own review,
  licensing, and maintenance obligations;
- a format change is reviewable by this repository's maintainers, while a
  target change is reviewable by the module's author;
- the format stays useful for every target, not only the ones with a bundled
  example.

## Files

- [recipe-spec-v1.md](recipe-spec-v1.md): the declarative recipe module format.
  Metadata keys, primitive mapping, composition order, strategies, versioning,
  and the conformance checklist.
- [adding-a-recipe.md](adding-a-recipe.md): the procedure for writing one
  recipe, the verification commands, the review checklist, and the path that
  promotes a grammar shape into a compatibility contract.
- [external-examples.md](external-examples.md): where concrete modules live,
  the requirements an external repository must meet to be listed, and how to
  reference one from a deployment document.

## Recipe Requirements

Every recipe must state:

1. **Format version**: for example `recipe/v1`.
2. **Scope**: the exact hostnames the module may observe, and the exact
   hostnames it must never observe.
3. **Authority model**: whether the observed value is decided by the client,
   the server, or both, and how that was determined.
4. **Primitive mapping**: which supported grammar line implements each step,
   with the contract file that proves the line is accepted.
5. **Budgets**: `max-size`, `timeout`, step ratio, cooldown, and replay pacing.
6. **Evidence**: the `anixops-mitm-runner` command that reproduces the outcome,
   and the expected stable output fields.
7. **Known gaps**: every step that depends on adapter-owned behavior (script
   runtime, TLS, HTTP framing, compression) instead of policy-core behavior.

## Recipe Extension Contract

Recipes are designed to grow by data, not by branching:

- A new target adds one module in its author's repository. It adds no source
  change here.
- A new mutation shape that existing grammar already expresses adds one entry to
  the module author's pattern catalog and, once a second module needs it, to
  [recipe-spec-v1.md](recipe-spec-v1.md#primitive-mapping).
- A new mutation shape that existing grammar does not express requires a
  compatibility contract under `docs/compatibility/` first, with parser
  fixtures, positive and negative tests, and a matrix row. Only after that does
  the format gain a new mapping row.
- Reserved `#!recipe.*` keys are frozen per format version. New keys are
  additive within a major version and must be ignorable by older readers.

## Documentation Only

This directory is documentation. It changes no parser behavior, no ABI symbol,
no fixture, and no compatibility status. Parser tolerance for the `#!recipe*`
keys themselves is a separate capability with its own contract, fixtures, and
matrix row: [recipe-metadata](../compatibility/recipe-metadata.md). The pull request that introduces it
therefore answers the compatibility questions in
[CONTRIBUTING.md](../../CONTRIBUTING.md) with "documentation only, no matrix row
changed" and points at the contract for every primitive it references.

Promoting a grammar shape into `tests/fixtures/` is a separate, later pull
request that adds the fixture, the `config/...` test IDs, the compatibility note
paragraph, and the matrix row update together. See
[adding-a-recipe.md](adding-a-recipe.md#promotion-to-compatibility-fixtures) for
that path.
