# External Recipe Examples

Concrete recipe modules live outside this repository. This page lists the ones
that follow the format, states the requirements for being listed, and explains
how a deployment document references one.

## Why Examples Live Outside

The format in [recipe-spec-v1.md](recipe-spec-v1.md) is stable and slow-moving.
A concrete module tracks one target's payload shape, which changes without
notice. Mixing the two puts a fast-moving artifact under a slow-moving review
process, and it makes this repository's maintainers responsible for content they
did not author.

Splitting them gives each artifact the review it deserves:

| Artifact | Author | Review | Cadence |
| --- | --- | --- | --- |
| Format and procedure | this repository's maintainers | this repository's pull requests | slow |
| A concrete module | the module's author | the module author's own review | fast, per target change |

## Listing Requirements

An external module repository is listed here when it satisfies all of:

- [ ] modules use only grammar already proven by this repository's fixtures, as
      listed in [recipe-spec-v1.md](recipe-spec-v1.md#primitive-mapping);
- [ ] every module declares `#!recipe`, `#!recipe.id`, `#!recipe.scope`,
      `#!recipe.authority`, `#!recipe.strategy`, and `#!recipe.guard`;
- [ ] the `#!recipe.scope` set and the `[MITM] hostname` set are identical;
- [ ] every module documents scope, authority probe outcomes, verification
      commands, known gaps, and a reversal step;
- [ ] `anixops-mitm-runner scan` and `trace` results are recorded in the module
      document;
- [ ] the repository states its own license and its own maintenance status.

A listed repository is a reference, not an endorsement. This repository's
maintainers do not review, test, or support the content of a listed repository,
and a listed module's behavior is its author's responsibility.

## Registry

| Repository | Scope | Modules | Status |
| --- | --- | --- | --- |
| `anixops-recipes` | community recipe modules | worked examples and a shared pattern catalog | reference |

Adding a row is a documentation pull request to this file. A row carries the
repository name, a one-line scope, the module count, and the status word. A row
carries no hostname and no parameter set, because those belong to the module.

## Referencing A Module From A Deployment Document

A deployment document references a module by identity, not by content:

```text
Recipe: anixops.recipe.example.settle (recipe/v1)
Source: <external repository URL>
Pinned: <commit or release tag>
Verified with: anixops-mitm-runner scan / trace / replay
```

Pinning matters. A module that is fetched from a moving branch can change
between two runs of the same verification command, which breaks the determinism
requirement in
[adding-a-recipe.md](adding-a-recipe.md#step-7-verify).

## Delisting

A row is removed when the repository stops meeting a listing requirement, when
its modules no longer parse against the current grammar, or at its author's
request. Delisting changes this page only; it changes no library behavior.
