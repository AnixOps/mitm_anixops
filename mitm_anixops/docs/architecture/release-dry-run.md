# Release Dry-Run Source Contract

This contract defines the release dry-run that must exist before v1.0.0 release
automation is allowed to publish assets.

Current mode:

```text
release-dry-run-current-mode=contract-only
release-dry-run-workflow=not-defined
release-dry-run-publication=blocked
release-dry-run-next-action=add-workflow-dispatch-dry-run-before-tag-publication
```

## Purpose

The release dry-run proves the release pipeline without creating a GitHub
Release or uploading public release assets. It must run in GitHub Actions and
must not consume local build output.

## Triggers

The future dry-run workflow must support:

- `workflow_dispatch` from `main`;
- version input using `vMAJOR.MINOR.PATCH`, `vMAJOR.MINOR.PATCH-alpha.N`, or
  `vMAJOR.MINOR.PATCH-rc.N`;
- optional artifact target selection only after each target has a source
  contract.

Tag-triggered publication remains a separate path and must stay blocked until
the dry-run passes for the same release design.

## Required Jobs

The future dry-run workflow must include these logical gates:

1. `release-policy`: validate version format, branch, event, and source docs.
2. `release-ci-gate`: read the same-commit `main` CI result with
   `actions: read`, or run an equivalent full CI gate in the same workflow.
3. `artifact-contract`: output artifact names, target platforms, staging paths,
   checksum algorithm, manifest path, and release-note path.
4. `package-*`: build each dry-run artifact in GitHub Actions.
5. `checksum-*`: generate SHA-256 sidecars with two-space file-name records.
6. `manifest-*`: generate JSON manifest and manifest checksum.
7. `release-notes-dry-run`: generate notes containing compatibility scope,
   known gaps, manual-intervention status, and rollback path.
8. `publish-eligibility-dry-run`: aggregate gates and report whether a tag
   publication would be eligible.
9. `summary`: publish a GitHub Step Summary with artifact, checksum, manifest,
   CI, compatibility, and manual-intervention fields.

## Required Outputs

The dry-run must output these fields before a tag release workflow is added:

```text
release_version
release_kind
release_commit
ci_run_id
ci_run_url
ci_run_conclusion
artifact_name
artifact_path
artifact_sha256
artifact_sha256_file
manifest_name
manifest_path
manifest_sha256
manifest_sha256_file
release_notes_path
manual_intervention_status
publish_eligibility_status
publish_blocking_reason
```

## Failure Rules

The dry-run must fail when:

- version format is invalid;
- run is not from `main`;
- same-commit CI success cannot be proven;
- artifact is missing, empty, or created outside the workflow;
- checksum sidecar format is invalid;
- manifest omits commit, version, artifact names, checksum values, or supported
  compatibility scope;
- release notes omit known gaps, rollback path, or manual-intervention status;
- a pending manual-intervention marker is required for the requested target.

## Publication Boundary

Dry-run mode must not:

- call `gh release create`;
- upload GitHub Release assets;
- overwrite tags;
- sign, notarize, or publish store assets;
- read platform signing secrets.

When this contract is implemented, `workflow_dispatch` dry-runs may upload
workflow artifacts for inspection, but public release assets remain blocked
until the tag-triggered release workflow and publish eligibility gate are added.

## GitHub Actions Evidence

P0 governance checks must prove this contract exists. A later P6 increment must
add the workflow and use GitHub Actions logs, artifacts, checksums, manifests,
and step summaries as acceptance evidence.
