# Release Checklist

Use this checklist before publishing a `mitm_anixops` source tag, binary artifact, or vendored library update.

1. Update `ANIXOPS_VERSION_*` in `include/mitm_anixops.h` and `anixops_version()` in `src/mitm_anixops.c` when the
   public ABI or documented behavior changes.
2. For intentional public C ABI changes, update `ci/abi_exports.txt`.
3. Wait for the GitHub Actions `build` workflow for the release commit to pass.
   Do not use local build or test output as release acceptance evidence.

4. Confirm the check output includes:

   ```text
   0 failed
   mitm_anixops check passed
   ```

5. Confirm the `build` workflow logs show the strategy-chain demo output covers:

   ```text
   mitm tcp decision=1
   mitm quic decision=2
   rewrite url action=1
   rewrite response-body action=12
   script kind=2
   ```

6. Confirm the `build` workflow output includes `make pkg-config-check` and
   that the staged smoke binary runs through the generated `mitm_anixops.pc`.
7. Confirm the `build` workflow output includes `make runner-check` and
   `anixops-mitm-runner scan --corpus tests/fixtures/corpus/manifest.json`,
   validates the JSON report, and reports `"sha256Matched":true`, expected diagnostic status counts, and
   `"passed":true`.
8. Confirm the `build` workflow output includes `make cmake-package-check` and
   that the staged CMake consumer configure/build/run smoke passes.
9. Confirm the `build` workflow output includes `make go-binding-check` and
   that the Go cgo wrapper tests pass through pkg-config.
10. Confirm the `build` workflow output includes `make rust-binding-check` and
   that the Rust wrapper tests pass through pkg-config.
11. Confirm `docs/compatibility/matrix.md`, `TODO.md`, and `docs/TODO.md`
   reflect the shipped behavior and known gaps.
12. For Windows artifacts, use the GitHub Actions artifact named:

   ```text
   anixops-mitm-windows-x64
   ```

13. Do not add proxy forwarding, TLS socketing, certificate installation, HTTP/2 parsing, compression handling, a
   JavaScript runtime, GUI code, or platform network extensions to the C library ABI. Keep those in explicit runner,
   shim, or platform-adapter layers.

## v1.0.0 Stable Release Gate

Use GitHub Actions evidence for stable release acceptance. Do not use local build or test output as release acceptance evidence.

1. Confirm the GitHub Actions `build` workflow passed on the release commit.
   This workflow is the source of truth for lint, format check, unit tests,
   parser fixtures, compatibility matrix checks, integration smoke tests, and
   package smoke checks.
2. Confirm the GitHub Actions `release-dry-run` workflow passed on `main` for
   the release commit and uploaded the `anixops-mitm-release-dry-run` workflow
   artifact with Linux x64 and Windows x64 packages.
3. Confirm the dry-run artifact contains `.sha256` sidecars, `manifest.json`
   metadata, and `release-notes.md` content with compatibility counts, known
   gaps, manual-intervention status, and rollback path.
4. Confirm the stable release-readiness gate has passed for `v1.0.0`. The gate
   is represented by:

   ```sh
   scripts/release-readiness-check.sh v1.0.0
   ```

   Expected stable evidence before publication:

   ```text
   release_readiness_status=passed
   release_readiness_blocking_markers=none
   release_readiness_planned_row_count=0
   ```

5. Do not publish `v1.0.0` while any of these markers remain pending in
   `docs/manual-intervention.md`: `branch-protection`, `protected-tags`, or
   `release-environment-approval`.
6. Confirm `docs/compatibility/matrix.md` has no `planned` rows and does not
   describe partial or unsupported behavior as supported.
7. Create the public release only through the GitHub Actions `release` workflow
   from a `v*` tag after same-commit main CI, release metadata, and
   the `github-release-publication` environment gate pass.
8. Confirm the `release` workflow uploads the `anixops-mitm-release-package`
   artifact and publishes only workflow-generated GitHub Release assets:
   Linux x64 tarball, Windows x64 zip, checksum sidecars, manifest, manifest
   checksum, and release notes.
9. If publication fails after a public tag exists, follow
   `docs/architecture/release-rollback-policy.md`; do not overwrite public
   tags or mutate published assets in place.
