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
- [x] Add a dedicated static format check CI job.
- [x] Add a dedicated static lint CI job.
- [x] Add a macOS policy-core smoke CI job.
- [x] Add manual-intervention register schema checks to CI.
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

## P4 MITM Policy And Certificate Lifecycle

- [x] Add certificate lifecycle architecture contract.
- [x] Add manual-intervention markers for CA trust, platform certificate stores,
  protected environments, and signing materials.
- [x] Add CI checks preventing accidental claims of automatic root trust or
  non-target hostname decryption.
- [x] Add adapter redaction policy contract and CI checks preventing default raw
  payload logging claims.

## P5 Integration Adapter

- [x] Update NetworkCore integration notes for the latest `v0.45.10-alpha`
  baseline and future v1.0.0 adapter boundary.
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
