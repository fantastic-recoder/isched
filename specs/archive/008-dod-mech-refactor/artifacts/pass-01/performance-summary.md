# pass-01 performance summary

## Workload

- Command: `./cmake-build-debug/src/test/cpp/isched/isched_graphql_tests`
- Environment: Linux debug build, `/usr/bin/time` fallback (no `perf` on PATH)

## Baseline vs post-pass snapshot

- Baseline elapsed seconds: `0.25`
- Post-pass elapsed seconds: `0.23`
- Delta: `-0.02s` (~8% lower wall time in this sample run)

## Branch-elimination / hot-loop rationale

- Replaced recursive selection traversal hot loops with flat field-node batches and index-based iteration.
- Consolidated repeated node-type branching in a single collector pass before resolver dispatch.
- Preserved behavior for resolver ordering, null propagation, and error-path generation.

## Gate status note

- Relevant ctest suites: pass (`ctest-green.txt`).
- Coverage gate: fails affected-scope threshold in current run (`coverage.txt`), so pass acceptance remains pending.

