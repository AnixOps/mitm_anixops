# Pull Request

## Goal

Describe the smallest user-visible or maintainer-visible change in this PR.

## Scope

List affected areas:

- [ ] C ABI / policy core
- [ ] parser fixtures
- [ ] runner / replay / trace
- [ ] script runtime contract
- [ ] MITM policy / certificate lifecycle
- [ ] bindings / packaging
- [ ] CI/CD / release
- [ ] documentation only

## Contract And Tests

- Source contract:
- Parser case:
- Positive test:
- Negative test:
- Compatibility matrix row:
- GitHub Actions CI run:
- Release dry-run or release run, if applicable:

If any item is not applicable, explain why. Do not mark a capability supported
without CI-covered evidence.

## Documentation

- [ ] README, ROADMAP, TODO, CHANGELOG, or CONTRIBUTING updated if needed
- [ ] `docs/compatibility/` updated if compatibility changed
- [ ] `docs/manual-intervention.md` updated if external or manual action is needed

## Security

- [ ] Does not automatically trust root certificates
- [ ] Does not log sensitive header/body content by default
- [ ] Does not decrypt non-target hostnames
- [ ] Does not bypass user authorization
- [ ] Does not add hidden interception behavior

## Manual Intervention

- [ ] No manual intervention required
- [ ] Manual intervention required and recorded in `mitm_anixops/docs/manual-intervention.md`
