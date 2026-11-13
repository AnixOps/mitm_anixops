# Rewrite Recipes

This directory documents *recipes*: user-authored policy modules that combine
already-supported `mitm_anixops` primitives into one reproducible end-to-end
outcome.

A recipe is data, not code. It is expressed in the plugin grammar the policy
core already parses (Surge `.sgmodule`, Loon `.plugin`, Quantumult X `.snippet`,
portable `.conf`), plus tolerated `#!` metadata lines. Adding a recipe requires
no C ABI change, no new parser branch, and no compatibility matrix row, because
the recipe only reuses capabilities that are already `supported` or `partial`
in [../compatibility/matrix.md](../compatibility/matrix.md).

## Why Recipes Are A Separate Layer

The repository has three distinct documentation layers and they must stay
separate:

| Layer | Answers | Lives in |
| --- | --- | --- |
| Compatibility contract | What does the parser and policy core accept, and what proves it? | [`../compatibility/`](../compatibility/README.md) |
| Architecture contract | Who owns TLS, HTTP, framing, script execution, trust? | [`../architecture/`](../architecture/overview.md) |
| Recipe | How do I combine supported primitives to reach one concrete outcome? | this directory |

A recipe never claims capability. It cites the contract for every primitive it
uses. If a recipe needs something the policy core does not accept, the recipe
is incomplete, and the correct follow-up is a compatibility contract, not a
recipe-local workaround.

## Recipe Index

- [recipe-spec-v1.md](recipe-spec-v1.md): the declarative recipe module format
  (`game-rewrite/v1` and future families), its metadata keys, its composition
  and ordering rules, and its verification contract.
- [game-rewrite/README.md](game-rewrite/README.md): the game-rewrite recipe
  family. Authority model, recon loop, strategy selection, and guard policy.
- [game-rewrite/patterns.md](game-rewrite/patterns.md): the reusable mutation
  pattern catalog that most mini-game and mobile-game titles share.
- [game-rewrite/adding-a-game.md](game-rewrite/adding-a-game.md): the standard
  operating procedure for adding a new title, including the review checklist.
- [game-rewrite/wangzhe-dalong.md](game-rewrite/wangzhe-dalong.md): the worked
  example for a WeChat mini-game (`王者来打龙`), end to end.
- [game-rewrite/brief_zh.md](game-rewrite/brief_zh.md): Chinese presentation
  brief for the same material, following the
  [loon_mitm_full_compat_brief_zh.md](../loon_mitm_full_compat_brief_zh.md)
  precedent.

## Recipe Requirements

Every recipe document must state:

1. **Family and version**: for example `game-rewrite/v1`.
2. **Target scope**: the exact hostnames the recipe may observe, and the exact
   hostnames it must never observe.
3. **Authority model**: whether the observed values are client-authoritative,
   server-authoritative, or dual-write, and how that was determined.
4. **Primitive mapping**: which supported grammar line implements each step,
   with the contract file that proves the line is accepted.
5. **Budgets**: `max-size`, `timeout`, clamp ratio, cooldown, and replay pacing.
6. **Evidence**: the `anixops-mitm-runner` command that reproduces the outcome,
   and the expected stable output fields.
7. **Known gaps**: every step that depends on adapter-owned behavior (script
   runtime, TLS, HTTP framing, compression) instead of policy-core behavior.

## Recipe Extension Contract

Recipes are designed to grow by data, not by branching:

- A new title adds one document plus one module file. It adds no source change.
- A new mutation shape that existing grammar already expresses adds one entry to
  [game-rewrite/patterns.md](game-rewrite/patterns.md).
- A new mutation shape that existing grammar does not express requires a
  compatibility contract under `docs/compatibility/` first, with parser
  fixtures, positive and negative tests, and a matrix row. Only after that does
  the recipe gain a new step.
- Reserved `#!recipe.*` keys are frozen per family version. New keys are
  additive within a major version and must be ignorable by older readers.

## Documentation Only

This directory is documentation. It changes no parser behavior, no ABI symbol,
no fixture, and no compatibility status. The pull request that introduces it
therefore answers the compatibility questions in
[CONTRIBUTING.md](../../CONTRIBUTING.md) with "documentation only, no matrix row
changed" and points at the contract for every primitive it references.

Promoting a recipe snippet into `tests/fixtures/` is a separate, later pull
request that adds the fixture, the `config/...` test IDs, the compatibility
note paragraph, and the matrix row update together. See
[game-rewrite/adding-a-game.md](game-rewrite/adding-a-game.md) for that path.
