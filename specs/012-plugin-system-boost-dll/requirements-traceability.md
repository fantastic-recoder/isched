# SP-012 Requirements Traceability Matrix

**Updated**: 2026-05-25

## Functional Requirements (FR)

| Requirement | Tasks | Primary automated evidence |
|---|---|---|
| FR-021 Resolver two-path schema flow | T035-T046, T071, T074 | `isched_plugin_system_tests` (schema-supplied success/fail, no-schema success/fail, rollback assertions) |
| FR-022 Plugin path policy under `plugins_root` | T023-T024, T029-T031, T071, T074 | `isched_plugin_system_tests` path-policy suites (relative/outside-root/canonicalization failure/symlink escape) |
| FR-023 Lifecycle event publication timing | T076, T079, T080, T082-T083, T085, T071, T074 | `isched_plugin_system_subscription_integration_tests` transition event-count + latency checks |
| FR-024 Lifecycle payload mapping | T077-T078, T082-T085, T071, T074 | `isched_plugin_system_subscription_integration_tests` payload field/value mapping tests |
| FR-025 Subscription pre-stream authorization | T077, T081, T085, T071, T074 | Unauthorized subscription rejection checks in `isched_plugin_system_subscription_integration_tests` |
| Performance gate (<=5% p95 regression) | T001-T004, T072-T073 | `benchmarks/baseline.json`, `benchmarks/post-change.json`, `benchmarks/perf-gate-report.md` |

## Success Criteria (SC)

| Success Criterion | Tasks | Evidence |
|---|---|---|
| SC-017 lifecycle latency + exactly-once transition events | T076, T079, T082, T083, T085, T071, T074 | Subscription integration tests (N>=20 trials, p95 <= 2000ms and >=95% within target) |
| SC-018 no-event negative paths + payload correctness | T077, T078, T084, T085, T071, T074 | Subscription integration tests for failed/non-transition zero-event paths and payload assertions |

## Verification Execution Evidence

- Full regression command: `cd /home/groby/dev/isched/build/release && ctest --output-on-failure`
- Result snapshot (2026-05-25): **PASS (55/55 tests)**
- Plugin-focused suites covered:
  - `isched_plugin_system_tests`
  - `isched_plugin_system_subscription_integration_tests`
  - `isched_gql_executor_auth_resolvers_tests`

## Notes

- Traceability refreshed after Phase 9 benchmark + full regression evidence capture.
- Performance benchmarking gate artifacts are tracked separately under `specs/012-plugin-system-boost-dll/benchmarks/`.

