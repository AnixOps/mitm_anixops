# Release Checklist

Use this checklist before publishing a `mitm_anixops` source tag, binary artifact, or vendored library update.

1. Update `ANIXOPS_VERSION_*` in `include/mitm_anixops.h` and `anixops_version()` in `src/mitm_anixops.c` when the
   public ABI or documented behavior changes.
2. For intentional public C ABI changes, update `ci/abi_exports.txt`.
3. Run:

   ```sh
   sh scripts/check.sh
   ```

   GitHub Actions runs the same check entrypoint on Linux after installing the pinned mihomo fixture.

4. Confirm the check output includes:

   ```text
   0 failed
   mitm_anixops check passed
   ```

5. Confirm the strategy-chain demo output covers:

   ```text
   mitm tcp decision=1
   mitm quic decision=2
   rewrite url action=1
   rewrite response-body action=12
   script kind=2
   ```

6. Confirm the output includes `make pkg-config-check` and that the staged smoke binary runs through the generated
   `mitm_anixops.pc`.
7. Confirm `make runner-check` includes `anixops-mitm-runner scan --corpus tests/fixtures/corpus/manifest.json`,
   validates the JSON report, and reports `"sha256Matched":true`, expected diagnostic status counts, and
   `"passed":true`.
8. Confirm the output includes `make cmake-package-check` and that the staged CMake consumer configure/build/run smoke
   passes. If `cmake` is unavailable in a constrained local environment, treat the explicit skip as a local limitation
   and rerun on a machine with CMake before publishing Alpha artifacts.
9. Confirm the output includes `make go-binding-check` and that the Go cgo wrapper tests pass through pkg-config.
10. Confirm the output includes `make rust-binding-check` and that the Rust wrapper tests pass through pkg-config.
11. Confirm `docs/compatibility_matrix.md` and `docs/TODO.md` reflect the shipped behavior and known gaps.
12. For Windows artifacts, use the GitHub Actions artifact named:

   ```text
   anixops-mitm-windows-x64
   ```

13. Do not add proxy forwarding, TLS socketing, certificate installation, HTTP/2 parsing, compression handling, a
   JavaScript runtime, GUI code, or platform network extensions to the C library ABI. Keep those in explicit runner,
   shim, or platform-adapter layers.
