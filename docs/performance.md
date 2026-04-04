# isched Performance Benchmarks

> **Last updated:** 2026-03-15  
> **Branch:** `001-universal-backend`  
> **Build type:** Debug (cmake-build-debug)  
> **Hardware:** Developer workstation (x86_64, Linux)

This document records measured results from the automated benchmark suite
(`src/test/cpp/performance/benchmark_suite.cpp`, CTest target `benchmark_suite`).
The suite runs as part of `ctest --output-on-failure` and must pass at every commit.

Canonical benchmark protocol and release-gate criteria are defined in
`specs/001-universal-backend/performance-protocol.md`. This document is the
release-facing summary of measured outcomes.

---

## Benchmarks

### T052-002 — Hello Query Throughput

| Metric          | Result      | Requirement  |
|-----------------|-------------|--------------|
| Window          | 5 seconds   | 5 seconds    |
| Mode            | Sequential  | Sequential   |
| Requests issued | > 5 000     | ≥ 5 000      |
| Throughput      | > 1 000 req/s | ≥ 1 000 req/s |

Query: `{ hello }`  
Client: single `httplib::Client` (keep-alive), one in-process HTTP call per iteration.

---

### T052-003 — Concurrent GraphQL POST

| Metric              | Result     | Requirement |
|---------------------|------------|-------------|
| Concurrency         | 100 threads × 10 requests | 100 × 10  |
| Total requests      | 1 000      | 1 000       |
| Application errors  | 0          | 0           |
| Wall-clock time     | ≤ 5 s      | ≤ 10 s      |

Query: `{ version }`  
Each thread owns its own `httplib::Client`.  
Transient TCP-accept-queue saturation at burst onset is retried (up to 5 attempts
with 20 ms × attempt back-off) before counting an error; this is normal O/S behaviour
at extreme concurrency and does not constitute an application failure.

---

### T052-004 — WebSocket Subscription Fan-Out

| Metric                  | Result   | Requirement |
|-------------------------|----------|-------------|
| Simultaneous connections | 50       | 50          |
| Event type              | `healthChanged` initial snapshot | any |
| All 50 receive event    | ≤ 50 ms  | ≤ 500 ms    |

Each client connects, performs the `graphql-transport-ws` handshake, subscribes
to `healthChanged`, and receives the server-pushed initial health snapshot.
The 500 ms wall-clock window covers the receive phase across all 50 clients.

---

### T052-005 — Introspection Under Load

| Metric                  | Result     | Requirement |
|-------------------------|------------|-------------|
| Concurrent threads      | 10         | 10          |
| Query                   | `{ __schema { types { name } } }` | same |
| Per-request latency     | < 250 ms   | ≤ 500 ms    |
| Errors                  | 0          | 0           |

Three sequential warm-up requests precede the timed concurrent phase to allow
lazy schema-serialisation overhead to amortise.  
Note: the first cold-start introspection traverses all schema types and can take
200–250 ms; warm requests are typically < 50 ms each.

---

### T052-006 — p95 Latency (FR-012)

| Metric        | Result    | Requirement |
|---------------|-----------|-------------|
| Requests      | 1 000     | 1 000       |
| Mode          | Sequential (keep-alive) | Sequential |
| p95 latency   | ≤ 10 ms   | ≤ 20 ms     |
| p99 latency   | ≤ 15 ms   | — (informational) |
| max latency   | ≤ 20 ms   | — (informational) |

Query: `{ version }`  
Durations measured as wall-clock time around `httplib::Client::Post()`.

---

## Release History

| Date       | Version / Commit | Changes                              |
|------------|------------------|--------------------------------------|
| 2026-03-15 | T052 (initial)   | First measurement on debug build     |

---

## Notes

- All measurements are from **Debug** builds; Release builds will be faster.
- The benchmark server starts fresh for each test case (port 18092 HTTP, 18093 WS).
- Server thread pool starts at 8 threads; adaptive growth to 120 under WS load.
- To re-measure: `ctest --test-dir cmake-build-debug -R benchmark_suite --output-on-failure`
