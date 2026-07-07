# Release Checklist

Use this checklist before publishing a `mitm_anixops` source tag, binary artifact, or vendored library update.

1. Update `ANIXOPS_VERSION_*` in `include/mitm_anixops.h` and `anixops_version()` in `src/mitm_anixops.c` when the
   public ABI or documented behavior changes.
2. For intentional public C ABI changes, update `ci/abi_exports.txt`.
3. Run:

   ```sh
   sh scripts/check.sh
   ```

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

6. Confirm `docs/compatibility_matrix.md` and `docs/TODO.md` reflect the shipped behavior and known gaps.
7. For Windows artifacts, use the GitHub Actions artifact named:

   ```text
   anixops-mitm-windows-x64
   ```

8. Do not add proxy forwarding, TLS socketing, certificate installation, HTTP/2 parsing, compression handling, a
   JavaScript runtime, GUI code, or platform network extensions to the C library release.
