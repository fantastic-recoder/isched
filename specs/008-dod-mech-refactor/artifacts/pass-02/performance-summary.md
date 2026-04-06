# pass-02 performance and locality summary

## Workload

- Command: `/usr/bin/env bash -lc 'cd cmake-build-debug && ctest --output-on-failure -R isched_subscription_broker_tests'`
- Environment: Linux debug build, `/usr/bin/time` fallback (`perf` unavailable on PATH)
- Samples: single-run baseline vs post capture (same command path)

## Baseline vs post-pass snapshot

- Baseline elapsed seconds: `0.84`
- Post-pass elapsed seconds: `0.85`
- Delta: `+0.01s` (~1.2% slower in this run)
- Baseline max RSS: `102844 KB`
- Post-pass max RSS: `103208 KB`

## Locality and index/SoA-style refactor impact

- Replaced pointer-oriented and ID-scan traversal with contiguous primary storage (`subscriptions` vector) and index-addressable lookups.
- Added `subscription_id -> index`, `session_id -> [index]`, and `topic -> [index]` maps to avoid full-collection scans on hot paths.
- `publish()` now iterates only `topic_index[topic]` candidates and guards invalid/sentinel indices via `getRecordByIndex()`.
- Removal path uses swap-with-last + index remap (`eraseSubscriptionAt`) to keep contiguous storage dense while preserving external behavior.

## Interpretation and acceptance note

- The single-run wall-time delta is small and within expected noise for fallback timing; no material regression signal is indicated.
- Functional parity and safety gates remain satisfied by existing evidence files: `ctest-green.txt` and `coverage.txt` (line 97.2%, branch 80.0%).

