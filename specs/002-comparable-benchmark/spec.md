# Feature Specification: Comparable Benchmark

**Feature Branch**: `002-comparable-benchmark`
**Created**: 2026-03-15
**Status**: Draft

---

## Clarifications

**Q1**: Which Apollo Server version should the reference implementation target?
**A1**: Apollo Server 4 (`@apollo/server` package) with the standalone HTTP adapter, as it is the current production-ready release.

**Q2**: Where in the repository does the TypeScript subproject live?
**A2**: `tools/comparable_benchmark/`. The `tools/` directory is the intended home for all Node.js / scripting artefacts in this repo; more tooling subprojects are planned there.

**Q3**: Should the Apollo server and isched share any ports?
**A3**: No. Apollo HTTP + WS share **18100** (single `http.Server` with `graphql-ws` upgrade); isched HTTP on **18092**, isched WS on **18093**.

**Q4**: Which GraphQL operations must the Apollo server support for a fair comparison?
**A4**: All operations that isched exposes as built-ins: `{ hello }`, `{ version }`, `{ uptime }`, `{ serverInfo { version host port status startedAt activeTenants activeWebSocketSessions transportModes } }`, `{ health { status } }`, **and** `subscription { healthChanged { status timestamp } }` for the WS scenario.

**Q5**: How are results persisted and reported?
**A5**: Each benchmark scenario writes a JSON file to `tools/comparable_benchmark/results/`. A report generator reads all JSON files and produces a single Markdown table at `docs/comparable-benchmark-results.md`. The report file **is committed** to version control so performance history is tracked across major changes.

**Q6**: Should the benchmark tool fail if isched performs worse than Apollo?
**A6**: The tool exits with non-zero only if isched degrades by more than a configurable threshold (default 10 % below Apollo). Default is informational; hard gate is enabled by setting `REGRESSION_THRESHOLD_PCT`.

**Q7**: How is the Apollo server process managed?
**A7**: The Apollo server is started as a child process by the TypeScript harness (Node.js `child_process`) and killed after results are captured. Apollo HTTP and WS share port 18100 (single `http.Server`).

**Q8**: How is the isched server configured (port, etc.)?
**A8**: The isched server has **no CLI arguments**. All configuration is via environment variables with the `ISCHED_` prefix, transformed as `ISCHED_SERVER_PORT` → `server.port`. The harness spawns the binary with the appropriate env vars set. `ISCHED_BINARY` controls the path to the compiled binary (default: `cmake-build-debug/isched_server`).

**Q9**: Should the WS subscription scenario be included in this spec?
**A9**: Yes. Include a basic WS subscription throughput scenario mirroring T052-004: 50 simultaneous `graphql-transport-ws` clients subscribing to `healthChanged { status timestamp }`, all receiving the initial event (containing `{ status: "UP", timestamp: "<ISO8601>" }`) within 500 ms. Apollo WS uses the `graphql-ws` package on **the same port as HTTP (18100)**; isched WS is on 18093.

**Q10**: Is CI integration required?
**A10**: No — local developer tool only. CI integration is planned but not in scope. The README must include a detailed tutorial (prerequisites, build steps, run steps, interpreting output).

---

## User Scenarios & Testing

### User Story 1 — Cross-Stack Performance Comparison Table

**As a developer**
I want to run a single `pnpm run benchmark:compare` command from the repo root
in order to obtain a side-by-side Markdown table comparing isched and Apollo Server throughput and latency.

#### Acceptance Scenarios

**Scenario 1-A — Happy path: table produced**
- Given the `cmake-build-debug/isched_server` binary exists
- And Node.js ≥ 22 and pnpm ≥ 10 are available
- When I execute `pnpm run benchmark:compare` in the repo root
- Then a Markdown file `docs/comparable-benchmark-results.md` is created (or overwritten)
- And the file contains a table with columns: *Scenario*, *Apollo req/s*, *isched req/s*, *isched / Apollo ratio*, *p95 isched (ms)*
- And the table contains at least **four** rows (hello throughput, concurrent version, p95 latency, WS fan-out)

**Scenario 1-B — isched binary missing**
- Given `cmake-build-debug/isched_server` does not exist
- When I execute `pnpm run benchmark:compare`
- Then the command exits with a non-zero status
- And a human-readable error message is printed to stderr explaining that the binary was not found and how to build it

**Scenario 1-C — Port already in use**
- Given another process holds port 18100, 18092, or 18093
- When the harness attempts to start a server on the occupied port
- Then the harness exits with a non-zero status and a clear error message

---

### User Story 2 — Apollo Reference Server

**As a developer**
I want a minimal TypeScript Apollo Server that implements the same four GraphQL operations **plus the `healthChanged` subscription** as isched's built-ins
in order to ensure benchmark queries are semantically equivalent and the comparison is fair.

#### Acceptance Scenarios

**Scenario 2-A — Schema parity**
- Given the Apollo reference server is running on port 18100 (HTTP) / WS on same port
- When I send `{ hello }` → result must be `{ "data": { "hello": "Hello, GraphQL!" } }`
- And `{ version }` → result must be `{ "data": { "version": "0.0.1" } }`
- And `{ serverInfo { version host port status } }` → result must contain those four fields with matching types
- And `{ health { status } }` → result must be `{ "data": { "health": { "status": "UP" } } }` (isched returns `"UP"`, not `"ok"`)
- And WS `subscription { healthChanged { status timestamp } }` → first `next` message must contain `{ "healthChanged": { "status": "UP", "timestamp": "<ISO8601>" } }`

**Scenario 2-B — Server starts and stops cleanly**
- Given the harness spawns the Apollo server
- When benchmarking is complete
- Then the server process exits within 2 seconds of receiving SIGTERM/kill

---

### User Story 3 — JSON Result Artifacts

**As a developer**
I want the benchmark harness to save intermediate JSON result files
in order to allow post-processing, CI archiving, and future trend analysis.

#### Acceptance Scenarios

**Scenario 3-A — Artifact structure**
- When a benchmark run completes
- Then `tools/comparable_benchmark/results/` contains:
  - `apollo-hello-throughput.json`
  - `apollo-concurrent-version.json`
  - `apollo-p95-latency.json`
  - `apollo-ws-fanout.json`
  - `isched-hello-throughput.json`
  - `isched-concurrent-version.json`
  - `isched-p95-latency.json`
  - `isched-ws-fanout.json`
- And each file contains at minimum: `scenario`, `requests`, `durationMs`, `reqPerSecond`, `p95Ms` (or `null` where not applicable)
- And WS fan-out files contain `fanOutMs` instead of `reqPerSecond`

---

## Edge Cases

- **CPU throttling on CI machines**: throughput ratios may be unreliable; the tool should print the hardware context header (OS, CPU count, Node version, build type).
- **isched startup delay**: the harness must wait for the HTTP health endpoint to return 200 before starting timed measurements (poll with 100 ms interval, 10 s timeout). isched config via env: `ISCHED_SERVER_PORT=18092 ISCHED_SERVER_HOST=127.0.0.1`.
- **Stale result files**: old JSON results from previous runs are overwritten, not accumulated, to avoid misleading aggregations.
- **Apollo cold-start vs warm JIT**: the harness sends 100 un-timed warm-up requests to both servers before each server's measurement window starts.
- **`graphql-transport-ws` protocol**: WS benchmark uses the `graphql-transport-ws` sub-protocol (same as isched T052-004). The `graphql-ws` npm package is used as both the Apollo server-side WS adapter and the client-side connection driver.
- **WS fan-out metric**: for the subscription scenario the measured value is total wall-clock elapsed until all 50 clients receive their first `next` message; req/s does not apply.
