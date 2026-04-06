# Passes Rollup (Phase 6)

Generated: 2026-04-06
Feature: `008-dod-mech-refactor`

## Completed passes

| Pass | Scope focus | Tests | Coverage (line/branch) | Hot-path metric snapshot | Schema validation |
|---|---|---|---|---|---|
| `pass-01` | GqlExecutor field-node selection hot loop (`isched_GqlExecutor_field_nodes.cpp`) | PASS (`ctest-green.txt`) | 92.3% / 100.0% (`coverage.txt`) | 250,000,000 ns -> 230,000,000 ns (about 8.0% lower wall time) | PASS (`schema-validation.txt`) |
| `pass-02` | SubscriptionBroker index/locality migration (`isched_SubscriptionBroker.{hpp,cpp}`) | PASS (`ctest-green.txt`) | 94.7% / 80.5% (`coverage.txt`) | 840,000,000 ns -> 850,000,000 ns (about 1.2% slower, marked non-regression) | PASS (`schema-validation.txt`) |

## Cross-pass totals

- Completed passes: `2`
- Passes with schema-valid artifact JSON: `2/2`
- Passes with green relevant test suites: `2/2`
- Passes meeting affected-scope coverage >=80% line and >=80% branch: `2/2`
- Passes with baseline + post metric evidence: `2/2`
- Passes with measurable selected-metric improvement: `1/2`
- Passes with unmitigated selected-metric regression: `0/2`

## Final phase gate evidence

- Full regression gate (all tests): `specs/008-dod-mech-refactor/artifacts/final-ctest-output.txt`
  - Result: 40/40 tests passed.
- Final affected-scope coverage verification: `specs/008-dod-mech-refactor/artifacts/final-coverage.txt`
  - Result: pass-01 and pass-02 both meet >=80% line and >=80% branch.
- Artifact schema validation index: `specs/008-dod-mech-refactor/artifacts/artifact-validation-index.txt`
  - Result: all discovered pass artifacts validated successfully.

