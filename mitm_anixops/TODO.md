# TODO

This file tracks the next small increments toward `v1.0.0`.

Long-term direction is in [ROADMAP.md](ROADMAP.md). Compatibility scope is in
[docs/compatibility/](docs/compatibility/). Alpha-stage library gaps remain in
[docs/TODO.md](docs/TODO.md).

## P0 Repository Baseline

- [x] Publish the latest Alpha release before starting the v1.0.0 track.
- [x] Add a repository audit and architecture baseline.
- [x] Add v1.0.0 roadmap, TODO, CHANGELOG, CONTRIBUTING, compatibility docs,
  and manual intervention register.
- [x] Add CI governance checks for the new baseline files.
- [x] Add a dedicated compatibility matrix CI job.
- [x] Add a compatibility evidence check for documented fixture paths and
  parser-test IDs.
- [x] Extend compatibility evidence checks to cover fenced/plain fixture paths
  and script bundle manifest fixtures, plus orphan top-level fixture detection.
- [x] Extend compatibility evidence checks to verify registered `config/...`,
  `script/...`, `rewrite/...`, `mitm/...`, and `abi/...` test IDs across the C
  test registry.
- [x] Extend compatibility evidence checks to verify documented Make targets,
  CI-covered script paths, and Go/Rust binding test names.
- [x] Extend compatibility evidence checks to verify documented
  `tests/test_*.c` file references.
- [x] Add a dedicated static format check CI job.
- [x] Add a dedicated static lint CI job.
- [x] Add a macOS policy-core smoke CI job.
- [x] Add manual-intervention register schema checks to CI.
- [x] Require placeholder confirmation evidence on pending required
  manual-intervention markers.
- [x] Require unique status, scope, release-boundary, evidence, and pending
  next-action fields for required manual-intervention markers.
- [x] Require confirmation evidence on completed manual-intervention markers.
- [x] Enforce compatibility-matrix status semantics for non-support guard rows.
- [x] Add PR template fields for source contract, test, compatibility matrix,
  changelog, and manual-intervention impact.
- [x] Add a release dry-run design before changing release automation.
- [x] Align ROADMAP current-track markers and repository audit test-status
  language with current CI evidence.

## P1 Plugin Parser

- [x] Define Loon plugin source contract for common sections and fields.
- [x] Define Quantumult X source contract for rewrite/task/mitm subsets.
- [x] Define Surge module source contract for module metadata, scripts, MITM,
  and rewrite subsets.
- [x] Add Loon plugin metadata parser fixtures and positive/negative tests.
- [x] Add Loon hashbang metadata parser fixture and positive/negative tests.
- [x] Add Loon metadata descriptor ABI, fixture, and positive/negative tests
      for `#!` and `[Plugin]` common display fields.
- [x] Add Loon argument section parser fixtures and positive/negative tests.
- [x] Add Loon inline-arguments parser fixtures and positive/negative tests.
- [x] Add Loon script metadata parser fixtures and positive/negative tests.
- [x] Add Loon task metadata parser fixtures and positive/negative tests.
- [x] Add Loon unsupported script-type guard and negative test.
- [x] Add Loon MITM option parser fixtures and positive/negative tests.
- [x] Add Loon MITM certificate-material non-support guard and negative test.
- [x] Add Loon `[Rule]` DOMAIN-SUFFIX reject parser fixtures and
      positive/negative tests.
- [x] Add Loon `[Rule]` DOMAIN reject parser fixtures and positive/negative
      tests.
- [x] Add Loon `[Rule]` DOMAIN-KEYWORD reject parser fixtures and
      positive/negative tests.
- [x] Add Loon `[Rule]` FINAL reject parser fixtures and positive/negative
      tests.
- [x] Add Loon `[Rule]` URL-REGEX reject parser fixtures and
      positive/negative tests.
- [x] Add Loon `[Rule]` route-selection non-support guard fixture and test.
- [x] Add Quantumult X `url echo-response` parser fixtures and
  positive/negative tests for the common-config contract.
- [x] Add Quantumult X `url response-body-replace-regex` parser fixtures and
  positive/negative tests for the common-config/body-mutation contracts.
- [x] Add Quantumult X `url header-add` parser fixtures and positive/negative
  tests for the common-config/header-mutation contracts.
- [x] Add Quantumult X `url header-replace` parser fixtures and
  positive/negative tests for the common-config/header-mutation contracts.
- [x] Add Quantumult X `url response-header-add` parser fixtures and
  positive/negative tests for the common-config/header-mutation contracts.
- [x] Add Quantumult X `url response-header-replace` parser fixtures and
  positive/negative tests for the common-config/header-mutation contracts.
- [x] Add Quantumult X `url response-header-del` parser fixtures and
  positive/negative tests for the common-config/header-mutation contracts.
- [x] Add Quantumult X `url response-header-replace-regex` parser fixtures and
  positive/negative tests for the common-config/header-mutation contracts.
- [x] Add Quantumult X `url header-replace-regex` parser fixtures and
  positive/negative tests for the common-config/header-mutation contracts.
- [x] Add Quantumult X `url header-del` parser fixtures and positive/negative
  tests for the common-config/header-mutation contracts.
- [x] Add Quantumult X MITM option parser fixtures and positive/negative
  tests.
- [x] Add Quantumult X MITM certificate-material and validation-bypass
  non-support guard and negative test.
- [x] Add Quantumult X task metadata parser fixtures and positive/negative
  tests.
- [x] Add Quantumult X event task metadata parser fixture and
  positive/negative tests.
- [x] Add Quantumult X unsupported event task guard and negative test.
- [x] Add Surge `[URL Rewrite]` `response-body-replace-regex` parser fixtures
  and positive/negative tests for the common-config/body-mutation contracts.
- [x] Add Surge `[URL Rewrite]` `response-body-json-replace` parser fixtures
  and positive/negative tests for the common-config/body-mutation contracts.
- [x] Add Surge `[URL Rewrite]` header mutation parser fixtures and
  positive/negative tests for the common-config/header-mutation contracts.
- [x] Add Surge requirement metadata parser fixtures and positive/negative
  tests.
- [x] Add Surge task metadata parser fixtures and positive/negative tests.
- [x] Add Surge event task metadata parser fixture and positive/negative tests.
- [x] Add Surge event unknown-name parser guard and negative test.
- [x] Add Surge unsupported script-type guard and negative test.
- [x] Add Surge MITM certificate-material non-support guard and negative test.
- [x] Add Stash HTTP MITM host parser fixtures and positive/negative tests.
- [x] Normalize Stash HTTP MITM `host:*` port-wildcard entries to host-only
  policy patterns.
- [x] Add Stash HTTP `force-http-engine` parser fixtures and
  positive/negative tests.
- [x] Add Stash HTTP MITM certificate-material non-support guard and negative
  test.
- [x] Add Stash `http.url-rewrite` reject parser fixtures and
  positive/negative tests.
- [x] Add Stash `http.url-rewrite` 302/307 redirect parser fixtures and
  positive/negative tests.
- [x] Add Stash `http.url-rewrite` 301/303/308 redirect parser fixtures and
  positive/negative tests.
- [x] Add Shadowrocket common-config parser fixture and positive/negative tests.
- [x] Add Shadowrocket MITM certificate-material non-support guard and negative
  test.
- [x] Add Shadowrocket `[Rule]` URL-regex reject parser fixtures and
  positive/negative tests.
- [x] Add Shadowrocket `[Rule]` DOMAIN reject parser fixtures and
  positive/negative tests.
- [x] Add Shadowrocket `[Rule]` DOMAIN-KEYWORD reject parser fixtures and
  positive/negative tests.
- [x] Add Shadowrocket `[Rule]` DOMAIN-SUFFIX reject parser fixtures and
  positive/negative tests.
- [x] Add Shadowrocket `[Rule]` FINAL reject parser fixtures and
  positive/negative tests.
- [x] Add reject/direct/proxy policy-intent parser fixtures and
  positive/negative tests.
- [x] Add cron/task non-support parser fixtures and guard tests.
- [ ] Add one parser fixture and one positive/negative parser test per new
  grammar unit.
- [ ] Extend compatibility matrix rows only after the corresponding CI test
  exists.

## P2 Rule Matching Engine

- [x] Define request rewrite source contract with parser fixtures and CI-covered
  positive/negative tests.
- [x] Add portable/common request redirect status parser fixtures and
  positive/negative tests for the request-rewrite contract.
- [x] Add portable/common request URL rewrite 200 parser fixture and
  positive/runtime tests for the request-rewrite contract.
- [x] Add Loon `[URL Rewrite]` request redirect/reject parser fixtures and
  positive/negative tests for the request-rewrite contract.
- [x] Add Quantumult X `url` request redirect/reject parser fixtures and
  positive/negative tests for the request-rewrite contract.
- [x] Add Surge `[URL Rewrite]` request redirect/reject parser fixtures and
  positive/negative tests for the request-rewrite contract.
- [x] Add Shadowrocket `[URL Rewrite]` request redirect/reject parser fixtures
  and positive/negative tests for the request-rewrite contract.
- [x] Define header mutation source contract with parser fixtures and CI-covered
  positive/negative tests.
- [x] Add portable/common request header regex parser fixtures and
  positive/negative tests for the header-mutation contract.
- [x] Add portable/common response header add/replace parser fixtures and
  positive/negative tests for the header-mutation contract.
- [x] Add Loon `[Header Rewrite]` header mutation parser fixtures and
  positive/negative tests for the header-mutation contract.
- [x] Add Shadowrocket `[URL Rewrite]` header mutation parser fixtures and
  positive/negative tests for the header-mutation contract.
- [x] Define response rewrite source contract with parser fixtures and
  CI-covered positive/negative tests.
- [x] Add portable/common echo-response parser fixtures and positive/negative
  tests for the response-rewrite contract.
- [x] Add Loon `[URL Rewrite]` response echo/body-regex parser fixtures and
  positive/negative tests for the response-rewrite contract.
- [x] Add Quantumult X `url` response echo/body-regex parser fixtures and
  positive/negative tests for the response-rewrite contract.
- [x] Add Surge `[URL Rewrite]` response body-regex parser fixtures and
  positive/negative tests for the response-rewrite contract.
- [x] Add Shadowrocket `[URL Rewrite]` response mock body parser fixtures and
  positive/negative tests for the response-rewrite contract.
- [x] Define body mutation source contract with parser fixtures and CI-covered
  positive/negative tests.
- [x] Add Loon `[Body Rewrite]` body mutation parser fixtures and
  positive/negative tests for the body-mutation contract.
- [x] Add Loon `[Body Rewrite]` request body JSON mutation parser fixtures and
  positive/negative tests for the body-mutation contract.
- [x] Add Loon `[Body Rewrite]` response body JSON mutation parser fixtures
  and positive/negative tests for the body-mutation contract.
- [x] Add portable/common response body JSON mutation parser fixtures and
  positive/negative tests for the body-mutation contract.
- [x] Add portable/common request body JSON mutation parser fixtures and
  positive/negative tests for the body-mutation contract.
- [x] Add portable/common request and response body JQ mutation parser fixtures
  and positive/negative tests for the body-mutation contract.
- [x] Add portable/common HTTP JQ alias mutation parser fixtures and
  positive/negative tests for the body-mutation contract.
- [x] Add optional libjq output-byte budgeting with fail-open runtime tests and
  C, Go, and Rust binding coverage.
- [x] Add optional libjq output-value enumeration budgeting with fail-open
  runtime tests and C, Go, and Rust binding coverage.
- [x] Add optional POSIX libjq wall-clock timeout isolation with fail-open
  runtime tests and C, Go, and Rust binding coverage.
- [x] Add optional POSIX libjq child memory ceilings with fail-open runtime
  tests and C, Go, and Rust binding coverage.
- [x] Add configurable bounded per-engine libjq compiled-filter cache reuse,
  explicit invalidation, and C, Go, and Rust binding observability tests.
- [x] Add optional libjq advanced-filter fixtures and positive/negative tests
  for predicates, slices, recursive selectors, computed filters, `del`, `map`,
  `with_entries`, `walk`, `test`, `capture`, assignment, and iterator pipes.
- [x] Add Surge `[URL Rewrite]` request/response JQ body mutation fixtures and
  positive/negative tests for the body-mutation contract.
- [x] Add Shadowrocket `[URL Rewrite]` response body regex mutation parser
  fixtures and positive/negative tests for the body-mutation contract.
- [x] Define trace schema for URL, host, header, body, script trigger, and
  policy intent decisions.
- [x] Add runner MITM decision golden JSON trace fixtures for TCP allow and
  QUIC fallback.
- [x] Add runner negative MITM decision golden JSON trace fixtures for
  certificate, deny-host, and no-host-match bypasses.
- [x] Add runner disabled and malformed MITM decision golden JSON trace
  fixtures.
- [x] Add negative fixture proving unsupported direct/proxy policy intent stays
  ignored until routing semantics are contracted.
- [x] Define reject/direct/proxy policy-intent contract with CI-covered reject
  subset and unsupported route tests.
- [x] Add golden tests proving plan API and legacy evaluate API parity.
- [x] Add current-header plan parity checks for C, Go, and Rust surfaces.
- [x] Add negative tests for phase mismatches.
- [x] Add negative tests for malformed hosts.

## P3 Script And Runtime Compatibility

- [x] Define script runtime source contract for `$request`, `$response`,
  `$argument`, `$persistentStore`, `$done`, timeout, exception, and double done.
- [x] Define cron/task trigger as parser metadata until runtime tests exist.
- [x] Add CI-covered parser guards proving unsupported cron/task-like lines do
  not register as HTTP script triggers.
- [x] Add cron/task descriptor parser fixtures, public ABI, and
  positive/negative tests.
- [x] Record any QuickJS, JavaScriptCore, or new runtime dependency decision
  before implementation.
- [x] Add a script trigger compatibility guard proving body trigger tokens keep
  `requires_body=1` in dispatch metadata.
- [x] Add a script trigger attribute-boundary guard proving comma-delimited
  direct script path attributes preserve `script_path`, quoted values, and
  hyphen/underscore field aliases.
- [x] Add a CI/CD script trigger evidence gate requiring every registered
  `script/...` C test to be recorded in the compatibility matrix and source
  contract evidence.
- [x] Add a cron/task compatibility guard proving quoted attr-list task
  `type` values register as task descriptors and do not become HTTP script
  triggers.
- [x] Add script runtime error reporting evidence for exception and timeout
  fail-open replay paths.
- [x] Add a script runtime security review gate for no-embedded-engine,
  pending production runtime/redaction markers, source-contract evidence, and
  forbidden overclaim checks.

## P4 MITM Policy And Certificate Lifecycle

- [x] Add certificate lifecycle architecture contract.
- [x] Add manual-intervention markers for CA trust, platform certificate stores,
  protected environments, and signing materials.
- [x] Add CI checks preventing accidental claims of automatic root trust or
  non-target hostname decryption.
- [x] Add adapter redaction policy contract and CI checks preventing default raw
  payload logging claims.
- [x] Tighten MITM hostname policy normalization and wildcard boundary tests so
  `*.` patterns match child subdomains without also matching the bare base
  domain.

## P5 Integration Adapter

- [x] Update NetworkCore integration notes for the latest `v0.45.10-alpha`
  baseline and future v1.0.0 adapter boundary.
- [x] Add an integration adapter readiness gate that verifies NetworkCore
  boundary markers, alpha artifact contents, runner/proxy shim targets,
  binding/package checks, E2E entrypoints, workflow registration, and forbidden
  production adapter overclaims.
- [x] Add binding parity fixtures for C runner, Go wrapper, and Rust wrapper.
- [x] Add named-header current-value parity checks for Go and Rust wrappers.
- [x] Add runner golden JSON trace fixtures for binding parity.
- [x] Define Stash and Shadowrocket app-profile compatibility as migration
  notes unless parser fixtures and tests are added.
- [x] Add Stash and Shadowrocket migration guard fixtures and parser tests.

## P6 Release Hardening

- [x] Add release dry-run workflow.
- [x] Add tag-triggered release workflow that builds artifacts in GitHub
  Actions.
- [x] Generate checksums, manifest, release notes, and release summary in
  GitHub Actions.
- [x] Add same-commit CI gate before release publication.
- [x] Add release rollback and replacement policy.
- [x] Add tag-only GitHub Release asset publication after release gates pass.
- [x] Add stable release-readiness gate blocking v1.0.0 while required manual
  markers remain pending.
- [x] Add release artifact compatibility status counts to manifest, notes, and
  summaries.
- [x] Add Windows x64 release artifact generation and checksum coverage to the
  tag-triggered release workflow.
- [x] Align release dry-run Windows x64 artifact metadata with the
  tag-triggered release artifact set.
- [x] Add a v1.0.0 acceptance evidence check to keep release readiness,
  compatibility, manual-intervention, CI, and release-gate evidence auditable.
- [x] Add an alpha fixture package check so release artifacts copy all
  top-level compatibility fixtures.
- [x] Add a repository governance contract for branch protection, protected
  release tags, and release environment approval evidence.
- [x] Align v1 release docs with GitHub Actions-only evidence and the v1
  compatibility matrix path.
- [x] Add a release metadata static check for artifact, checksum, manifest,
  release-note, compatibility-count, and publication-boundary evidence.
- [x] Add a CI trigger static check for PR, main push, tag, manual dry-run, and
  tag-release workflow coverage.
- [x] Add a release checklist static check covering v1 stable release workflow,
  artifact, manifest, checksum, release-note, and manual gate evidence.
- [x] Allow required manual-intervention markers to transition from pending to
  confirmed only when confirmation evidence is recorded.
- [x] Add a manual-intervention transition fixture check for confirmed marker
  evidence validation.
- [x] Run manual-intervention evidence and transition gates directly in release
  and release dry-run workflows before release-readiness checks.
- [x] Run release checklist and metadata static gates directly in release and
  release dry-run workflows before release-readiness checks.
- [x] Enforce unique release compatibility summary count keys in release and
  release dry-run workflow metadata checks.
- [x] Add a compatibility status summary static check so release count outputs
  must be unique, numeric, and total-consistent before publication metadata is
  generated.
- [x] Run integration adapter readiness directly in CI, release dry-run, and
  tag-triggered release readiness gates.
- [x] Add adapter readiness evidence fields to release and dry-run manifests,
  release notes, summaries, and metadata gates.
- [x] Add release manifest schema-version evidence to release and dry-run
  manifests, release notes, summaries, and metadata gates.
- [x] Add release digest algorithm and checksum sidecar format evidence to
  release and dry-run manifests, release notes, summaries, and metadata gates.
- [x] Add release workflow run ID and URL evidence to release and dry-run
  manifests, release notes, summaries, and metadata gates.
- [x] Add release publication gate evidence to release and dry-run manifests,
  release notes, summaries, and metadata gates.
- [x] Add release source-mode evidence to release and dry-run manifests,
  release notes, summaries, and metadata gates.
- [x] Add a production MITM roadmap gate that fixes the v2.0.0 contract
  freeze, v2.8.0 beta, v2.9.0 RC, and v3.0.0 production-ready milestones while
  blocking earlier production-ready claims.
- [x] Add CI run ID, URL, and conclusion evidence to release and dry-run
  manifests, release notes, summaries, and metadata gates.
- [x] Add artifact count and platform evidence to release and dry-run
  manifests, release notes, summaries, and metadata gates.
- [x] Add a release publication verifier for public GitHub Release assets,
  manifest fields, checksum sidecars, release notes, release workflow run
  evidence, CI run evidence, artifact count, and artifact platforms.
- [x] Add an offline fixture test for the release publication verifier covering
  public-asset metadata success and checksum-mismatch failure paths.
- [x] Add a post-publication release verifier evidence artifact so tag
  workflows archive `release-publication-verify.env` after public asset
  validation passes.
- [x] Add a machine-readable release evidence index for the latest public
  stable release URL, commit, CI run, release run, asset count, artifact
  platforms, and publication evidence artifact.
- [x] Refresh the release evidence index to the latest public stable tag while
  retaining the previous stable release evidence entry.
- [x] Add a release notes change-summary gate requiring feature additions and
  BUG fixes sections in release and release-dry-run artifacts.
- [x] Record release notes change-summary evidence in the machine-readable
  release evidence index for the latest public stable tag.
- [x] Add a release evidence index freshness gate so stable patch releases
  require `latestStable` to match the previous stable patch for the target
  version.
- [x] Roll the release evidence index freshness policy forward to the latest
  public stable tag after `v1.3.6` publication.
- [x] Extend the release evidence index freshness gate across declared
  minor-boundary release targets such as `v1.4.0`.
- [x] Roll the release evidence index freshness policy forward to the latest
  public stable tag after `v1.4.0` publication.
- [x] Roll the release evidence index freshness policy forward to the latest
  public stable tag after `v1.4.1` publication.
- [x] Require release evidence index entries to stay sorted newest-to-oldest.
- [x] Roll the release evidence index freshness policy forward to the latest
  public stable tag after `v1.4.2` publication.
- [x] Add release evidence index negative fixture coverage for unsorted
  newest-to-oldest entries.
- [x] Roll the release evidence index freshness policy forward to the latest
  public stable tag after `v1.4.3` publication.
- [x] Require retained release evidence index patch entries to stay contiguous.
- [x] Roll the release evidence index freshness policy forward to the latest
  public stable tag after `v1.4.4` publication.
- [x] Require every retained release evidence entry to carry publication
  evidence artifact and file fields.
- [x] Roll the release evidence index freshness policy forward to the latest
  public stable tag after `v1.4.5` publication.
- [x] Add a release sensitive-material gate for generated Linux tarballs and
  Windows zip artifacts before publication.
- [x] Confirm repository-level `main` branch protection, `v*` tag ruleset, and
  `github-release-publication` environment approval evidence for `v1.0.0`.
- [x] Fix tag-triggered release publication so `gh release create` has explicit
  repository context outside a checkout-backed Git working tree.
