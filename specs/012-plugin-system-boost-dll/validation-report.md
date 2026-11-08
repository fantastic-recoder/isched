# SP-012 Validation Report

**Date**: 2026-05-25

## Full Regression

Command:

```bash
cd /home/groby/dev/isched/build/release
ctest --output-on-failure
```

Outcome:

- **PASS**: 55/55 tests passed
- **Total runtime**: 104.72s
- **Failures**: 0

## Plugin-Focused Evidence

- `isched_plugin_system_tests`: PASS
- `isched_plugin_system_subscription_integration_tests`: PASS
- `isched_gql_executor_auth_resolvers_tests`: PASS

## Performance Gate

- `benchmarks/perf-gate-report.md`: **PASS** (all tracked regressions <= 5%)

