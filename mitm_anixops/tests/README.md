# Test Fixture Policy

`mitm_anixops` keeps tests separate from library logic:

- Library code lives in `src/` and public ABI in `include/`.
- Test harness code lives in `tests/`.
- Commit/CI entrypoint is `scripts/check.sh`.
- The shared-library export allowlist lives in `ci/abi_exports.txt`; every intentional C ABI change must update it.

## Rules

- Every new feature must add or update tests in the same change.
- Every feature test must include at least one happy path and one boundary or rejection path.
- Boundary cases should cover null inputs, empty inputs, malformed config, unsupported phase, first-match behavior,
  capacity limits, missing script fields, unsupported phases, and security gates where relevant.
- Tests should assert public API behavior, not private implementation details.
- New config compatibility behavior should update `docs/compatibility_matrix.md`; unsupported or deferred behavior should
  update `docs/TODO.md`.
- Reverse-engineering evidence belongs in `docs/`, not in test assertions, unless it affects public compatibility.

## Test Files

- `test_abi.c`: ABI stability, status text, last-error copy behavior, null argument handling, lifecycle defaults.
- `test_config.c`: AnixOps config parsing, malformed input boundaries, and config line diagnostics.
- `test_mitm.c`: MITM host/certificate/QUIC gate behavior.
- `test_rewrite.c`: URL rewrite matching, action mapping, header rewrite dispatch, and mock/regex body rewrite application.
- `test_script.c`: module argument parsing, HTTP script matching, and script dispatch metadata.
- `fixtures/`: stable third-party and representative compatibility fixtures used by tests.
- `test_main.c`: test registration and process exit status.
- `test_harness.h`: lightweight assertion macros and shared declarations.
- `examples/strategy_chain_demo.c`: minimal public-ABI demo checked by `make demo-check`.

## Required Command

```sh
sh scripts/check.sh
```

That command must pass before a commit is accepted. It includes the BiliUniverse script-runtime E2E fixture and the
generic request/response script contract, plus the pure-C strategy-chain demo. It also runs the mihomo proxy-chain
fixture when `MIHOMO_BIN` points to an executable binary.

The mihomo proxy-chain fixture can also be run directly:

```sh
make e2e
```
