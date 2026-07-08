# Repository Governance Contract

This contract defines the repository-level controls that must be manually
confirmed before `v1.0.0` publication. These controls live outside the source
tree, so the repository can only enforce the contract shape and block release
readiness until the external state is confirmed.

Current mode:

```text
repository-governance-contract-status=accepted
repository-governance-automation-mode=static-contract-and-manual-confirmation
repository-governance-branch-protection=manual-confirmation-required
repository-governance-protected-tags=manual-confirmation-required
repository-governance-release-environment=manual-confirmation-required
repository-governance-v1-publication-status=blocked-until-manual-confirmation
```

## Required Manual Controls

### Main Branch Protection

Before `v1.0.0`, the `main` branch must have repository rules or branch
protection that prevent unreviewed or failing changes from becoming release
inputs.

Minimum expected controls:

- required status checks include the GitHub Actions `build` workflow;
- pull request or equivalent reviewed-change policy is enabled;
- force pushes and branch deletion are blocked;
- bypass permissions are limited to repository maintainers who own release
  operations;
- any exception is recorded in `docs/manual-intervention.md`.

The manual marker is `branch-protection-status`. It must remain `pending` until
a maintainer verifies the GitHub repository settings and records confirmation
evidence in `docs/manual-intervention.md`.

### Protected Release Tags

Before `v1.0.0-release`, `v*` tags must be protected by tag protection or a
repository ruleset.

Minimum expected controls:

- creation and update of `v*` tags is limited to release maintainers;
- deletion and force updates of public release tags are blocked;
- the immutable tag and asset policy in
  [Release Rollback And Replacement Policy](release-rollback-policy.md) remains
  the default;
- any emergency exception is recorded before publication.

The manual marker is `protected-tags-status`. It must remain `pending` until a
maintainer verifies the GitHub repository settings and records confirmation
evidence in `docs/manual-intervention.md`.

### Release Publication Environment

The release workflow already targets the `github-release-publication`
environment for tag publication. Before `v1.0.0-release`, maintainers must
decide whether that environment requires explicit approval, and record the
decision.

Minimum expected controls:

- the `github-release-publication` environment exists in GitHub;
- required reviewers or an explicit no-approval rationale are documented;
- release secrets, if ever added, are scoped to the environment and never
  committed to the repository;
- manual approval, if required, cannot skip checksum, manifest, release-note,
  same-commit CI, or stable readiness gates.

The manual marker is `release-environment-approval-status`. It must remain
`pending` until a maintainer verifies the GitHub environment settings and
records confirmation evidence in `docs/manual-intervention.md`.

## Automation Boundary

`scripts/repository-governance-check.sh` verifies that this contract, the
manual markers, and the release workflow hooks exist. It reports the current
governance state as `blocked` while any required marker is still pending. The
script does not call GitHub APIs and does not treat source-tree checks as proof
that external repository settings are configured.

When a maintainer completes an external control, update the matching marker in
`docs/manual-intervention.md` from `pending` to `confirmed`, replace
`not-yet-confirmed` evidence with a redacted URL or audit note, and keep any
credential, reviewer identity detail, signing material, or private policy out
of the repository.

## Failure Rules

Repository governance evidence is invalid when:

- this contract is missing or not accepted;
- any required manual marker is missing;
- a required marker has a status other than `pending` or `confirmed`;
- a `confirmed` marker still has `not-yet-confirmed` evidence;
- `release.yml` stops using the `github-release-publication` environment;
- release publication no longer requires same-commit CI and stable readiness
  gates.
