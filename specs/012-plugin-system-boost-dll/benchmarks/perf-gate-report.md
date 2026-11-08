# SP-012 Performance Gate Report

**Date**: 2026-05-25  
**Gate**: p95 regression must be <= 5% for each tracked path

## Inputs

- Baseline: `benchmarks/baseline.json`
- Post-change: `benchmarks/post-change.json`
- Harness: `tools/benchmarks/plugin_system_perf.sh` (20 iterations per scenario)

## Comparison (p95 ms)

| Scenario | Baseline p95 (ms) | Post-change p95 (ms) | Regression | Gate |
|---|---:|---:|---:|---|
| dispatch_request | 8 | 7 | -12.50% | PASS |
| graphql_resolver_path | 13 | 12 | -7.69% | PASS |
| restore_persisted_plugins | 22 | 21 | -4.55% | PASS |

## Result

**PASS** — all tracked p95 regressions are within the <=5% threshold.

## Notes

- All measured paths improved versus baseline in this capture pair.
- Baseline was captured immediately before post-change capture on the same host profile.

