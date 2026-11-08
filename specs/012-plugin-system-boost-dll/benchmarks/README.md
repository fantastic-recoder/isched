# SP-012 Benchmark Runbook

This directory stores performance-gate evidence for SP-012.

## Harness

- Script: `tools/benchmarks/plugin_system_perf.sh`
- Inputs: built `isched_plugin_system_tests` binary from a configured CMake build directory
- Measured scenarios:
  - `dispatch_request` (`[dispatch][load][plugin][us1]`)
  - `graphql_resolver_path` (`[fr021][plugin][schema][us2]`)
  - `restore_persisted_plugins` (`[plugin][restore][us4]`)
- Metric: process-duration samples in milliseconds with p95/mean/min/max

## Reproduction

```bash
cd /home/groby/dev/isched
cmake --build build/release --parallel

# baseline capture
./tools/benchmarks/plugin_system_perf.sh \
  --build-dir /home/groby/dev/isched/build/release \
  --output /home/groby/dev/isched/specs/012-plugin-system-boost-dll/benchmarks/baseline.json \
  --iterations 20

# post-change capture
./tools/benchmarks/plugin_system_perf.sh \
  --build-dir /home/groby/dev/isched/build/release \
  --output /home/groby/dev/isched/specs/012-plugin-system-boost-dll/benchmarks/post-change.json \
  --iterations 20
```

## Notes

- Run baseline and post-change on the same machine profile.
- Compare p95 values using `perf-gate-report.md`.

