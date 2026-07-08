# Contributing

`mitm_anixops` is moving toward a v1.0.0 MITM plugin compatibility layer. Changes
must be small, testable, documented, and verified by GitHub Actions.

## Local vs CI Responsibilities

Local machines may be used for:

- reading files and repository history;
- editing source, tests, docs, and workflows;
- checking diffs and Git status;
- committing, pushing, and observing GitHub Actions.

GitHub Actions is the required environment for:

- lint and format checks;
- build and link checks;
- unit tests;
- parser fixture tests;
- compatibility matrix tests;
- integration smoke tests;
- packaging and release dry-runs;
- release publication.

Do not use a local build or local test as final acceptance evidence.

## Feature Workflow

For every feature-sized change:

1. Write or update the source contract, test, or fixture first.
2. Implement one minimal behavior unit.
3. Add positive and negative coverage.
4. Update `docs/compatibility/`.
5. Update `CHANGELOG.md`.
6. Record manual or external blockers in `docs/manual-intervention.md`.
7. Push and use GitHub Actions as the acceptance result.

## Compatibility Changes

Any parser, matching, rewrite, script, MITM policy, or packaging change must
answer:

- Which plugin ecosystem is affected: Loon, Quantumult X, Surge, Stash,
  Shadowrocket, or portable AnixOps?
- What source contract defines the expected behavior?
- What parser case proves the input is recognized?
- What positive test proves expected behavior?
- What negative test proves rejection, ignore, fail-open, or unsupported
  behavior?
- Which compatibility matrix row changed?

## Security Rules

Security-sensitive behavior must be conservative by default:

- Do not automatically trust root certificates.
- Do not log sensitive headers or bodies by default.
- Do not decrypt non-target hostnames.
- Do not bypass user authorization.
- Do not implement hidden interception, covert capture, or malicious traffic
  manipulation features.
- Keep TLS, certificate store, HTTP parser, HTTP/2, compression/framing, and
  platform permission handling in explicit runner, shim, or adapter layers.

## Dependencies

Avoid new dependencies unless a source contract explains:

- why the dependency is needed;
- alternatives considered;
- license impact;
- whether it affects default builds or only optional runtime builds;
- how GitHub Actions verifies it.

## Releases

Release artifacts must be produced by GitHub Actions. Future v1.0.0 release
automation must include:

- tag-triggered workflow;
- same-commit CI gate;
- artifact build;
- checksum generation;
- manifest generation;
- release notes generation;
- GitHub Release upload;
- failure blocking before publication.
