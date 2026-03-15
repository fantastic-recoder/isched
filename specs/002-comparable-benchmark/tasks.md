# Task List: Comparable Benchmark

**Feature Branch**: `002-comparable-benchmark`
**Updated**: 2026-03-15
**Status**: Ready

---

## Format Legend

```
[ ] T0XX  Short description  (owner file)
[x] T0XX  Completed task
[P] T0XX  Can run in parallel with its sibling tasks
```

**Constitutional rule**: the feature is considered shippable only when
`pnpm run benchmark:compare` exits 0 and produces `docs/comparable-benchmark-results.md`.
No ctest changes are required (benchmark is a developer tool, not a CI test suite).

**Path conventions**:
- Subproject root: `tools/comparable_benchmark/`
- TypeScript sources: `tools/comparable_benchmark/src/`
- Result artifacts: `tools/comparable_benchmark/results/` (git-ignored)
- Report output: `docs/comparable-benchmark-results.md`

---

## Phase A + E — Project Scaffold (workspace + subproject config)

> These are pure configuration files. Commit them together as the first scaffold commit; they
> unblock `pnpm install` and IDE support for all subsequent phases.

[x] T001  Add `pnpm-workspace.yaml` at repo root — `packages: ["tools/*"]`
[x] T002  Add root `package.json` with `benchmark:compare` workspace delegate script
[x] T003  Create `tools/comparable_benchmark/package.json` with name, version, scripts and dependencies:
           - **deps**: `@apollo/server`, `body-parser`, `express`, `graphql`, `graphql-subscriptions`, `graphql-ws`, `ws`, `autocannon`
           - **devDeps**: `tsx`, `typescript`, `@types/express`, `@types/node`, `@types/ws`
           - Note: `expressMiddleware` is a sub-path export of `@apollo/server` (`@apollo/server/express4`) — no extra package needed
[x] T004  Add `tools/comparable_benchmark/tsconfig.json`:
           `target: ES2022`, `moduleResolution: node16`, `esModuleInterop: true`, `strict: true`, `outDir: ./dist` (unused at runtime but needed for IDE support)
[x] T005  Add `tools/comparable_benchmark/results/.gitkeep` and update `.gitignore` to exclude `tools/comparable_benchmark/results/*.json`
[x] T006  Run `pnpm install` and verify lock file updates; confirm `node_modules` structure

---

## Phase B — Apollo Reference Server

[x] T007  Implement `tools/comparable_benchmark/src/apollo-server.ts`:
           - GraphQL schema matching `specs/001-universal-backend/contracts/graphql-schema.md` subset:
             `hello`, `version`, `uptime: Int`, `serverInfo` (8 fields), `health` queries **and** `healthChanged` subscription
           - Resolvers (values must match isched exactly, sourced from `isched_GqlExecutor.cpp`):
             - `hello` → `"Hello, GraphQL!"`  (**not** `"Hello, World!"`)
             - `version` → `"0.0.1"`
             - `uptime` → `Math.floor(process.uptime())` (integer seconds; this is a **root Query field**, separate from `serverInfo`)
             - `serverInfo` → `{ version: "0.0.1", host: "localhost", port: 18100, status: "RUNNING", startedAt: <epoch ms at startup>, activeTenants: 1, activeWebSocketSessions: 0, transportModes: ["http","websocket"] }`
             - `health` → `{ status: "UP" }` (**not** `"ok"`)
             - `healthChanged` subscription: `PubSub` immediately publishes `{ status: "UP", timestamp: new Date().toISOString() }` per subscriber on connect (matches `HealthChangedEvent { status: String!, timestamp: String! }`)
           - Uses `express` + `expressMiddleware` (required to expose `http.Server` for `graphql-ws` attachment)
           - `graphql-ws` `useServer` attaches to the same `http.Server`; HTTP **and** WS on port **18100**
           - Graceful SIGTERM/SIGINT handler that closes WS server then HTTP server within 2 s
[x] T008  Manual smoke-test: `tsx tools/comparable_benchmark/src/apollo-server.ts` + `curl` for HTTP + `wscat` for WS subscription
           (document result in a comment at the top of the file; not automated)

---

## Phase C — Benchmark Harness

[x] T009  Implement `tools/comparable_benchmark/src/benchmark.ts`:
           - `runThroughput(url, query)` — `autocannon({ connections: 1, duration: 5, method: "POST", … })`; returns `BenchmarkResult`
           - `runConcurrent(url, query)` — `autocannon({ connections: 100, amount: 1000, … })`; returns `BenchmarkResult`
           - `runP95Sequential(url, query, iterations = 1000)` — `fetch` loop with `performance.now()` per call; `timings.sort((a,b)=>a-b)[Math.floor(0.95*n)]`
           - `runWsFanOut(wsUrl, clientCount = 50)` — `graphql-ws` `createClient` per subscriber, `Promise.all` first `next`, elapsed ms
             - Apollo WS URL: `ws://127.0.0.1:18100/graphql`
             - isched WS URL: `ws://127.0.0.1:18093/graphql` (isched WS = HTTP port + 1)
[x] T010  Implement `tools/comparable_benchmark/src/runner.ts` — process management utilities:
           - `spawnServer(opts: { binary, args, env, readinessUrl })` — `spawn` with merged env; isched env must include `ISCHED_SERVER_PORT=18092 ISCHED_SERVER_HOST=127.0.0.1 SPDLOG_LEVEL=warn`; Apollo spawned as `node --import tsx/esm tools/comparable_benchmark/src/apollo-server.ts`
           - `warmUp(url, query, count = 100)` — fires `count` un-timed POST requests; awaits each
           - `killServer(proc)` — SIGTERM, wait ≤ 2 s with poll, SIGKILL if needed

---

## Phase D — Orchestration & Result Persistence

[x] T011  Implement main orchestration in `tools/comparable_benchmark/src/runner.ts` (main function):
           1. Check `ISCHED_BINARY` path exists; exit 1 with build instructions if not
           2. Start Apollo server child process; await readiness on `http://127.0.0.1:18100/graphql`
           3. Start isched child process (env vars, no CLI args); await readiness on `http://127.0.0.1:18092/graphql`
           4. For each server: warm-up (HTTP scenarios only) then measure 4 scenarios
              - isched WS fan-out uses `ws://127.0.0.1:18093/graphql`
              - Apollo WS fan-out uses `ws://127.0.0.1:18100/graphql`
           5. Write 8 JSON result files to `tools/comparable_benchmark/results/`
           6. Kill both server processes
           7. Generate report (calls `generateReport()` from `report.ts`)
           8. Run regression check; exit 1 if breached
[x] T012  Define `BenchmarkResult` TypeScript interface in `tools/comparable_benchmark/src/types.ts`:
           `{ scenario, server, requests, errors, durationMs, reqPerSecond: number|null, p95Ms: number|null, fanOutMs: number|null, hardwareContext, timestamp }`
[x] T013  Handle `ISCHED_BINARY` not found: print actionable error to stderr and exit 1
[x] T014  Handle port-already-in-use (EADDRINUSE): catch error from readiness probe connection refusal pattern; print which port and exit 1

---

## Phase E — Report Generator (`report.ts`)

[x] T015  Implement `tools/comparable_benchmark/src/report.ts`:
           - Reads the eight JSON result files from `tools/comparable_benchmark/results/`
           - Derives hardware context header (OS, CPU count, Node version, isched build type)
           - Writes `docs/comparable-benchmark-results.md` with the Markdown table (four rows: hello throughput, concurrent version, p95 latency, WS fan-out)
[x] T016  Add `report` npm script to `tools/comparable_benchmark/package.json` (`tsx src/report.ts`) for standalone re-generation
[x] T017  Wire report generation as the final step of the `benchmark:compare` script in `runner.ts`

---

## Phase F — Regression Threshold, README Tutorial & Polish

[x] T018  Implement regression check in `runner.ts`:
           - Compare isched req/s to Apollo req/s for HTTP throughput scenarios (WS fan-out comparison is informational only)
           - If `(isched - apollo) / apollo < -REGRESSION_THRESHOLD_PCT / 100`, print a warning and exit 1
           - `REGRESSION_THRESHOLD_PCT` defaults to 10; configurable via environment variable
[x] T019  Add `--dry-run` flag to `runner.ts`: validates binary exists and ports are free, then exits 0 without starting servers
[x] T020  Add a **"Comparable Benchmark" tutorial** section to `README.md` covering:
           - Prerequisites: Node.js ≥ 22, pnpm ≥ 10, built isched binary
           - How to build isched: `python3 configure.py`
           - How to install Node deps: `pnpm install`
           - How to run: `pnpm run benchmark:compare`
           - How to interpret the output table (ratio column, WS fan-out column, regression threshold)
           - How to override binary path: `ISCHED_BINARY=path/to/binary pnpm run benchmark:compare`
[x] T021  Run `pnpm run benchmark:compare` end-to-end; commit `docs/comparable-benchmark-results.md` with real numbers

---

## Constitutional Checklist

- [x] `pnpm install` succeeds with no unresolved peer dependencies
- [x] `pnpm run benchmark:compare` exits 0 when binary is present and all ports are free
- [x] `docs/comparable-benchmark-results.md` is created and contains **four** required rows (including WS fan-out)
- [x] Binary-missing path exits 1 with a human-readable error
- [x] Port-in-use path exits 1 with a human-readable error
- [x] `tools/comparable_benchmark/results/*.json` is listed in `.gitignore`
- [x] `isched_srv_main.cpp` amended to read `ISCHED_SERVER_PORT`/`HOST` env vars (spec required spawning isched on a configurable port; no CMake changes)
- [x] `README.md` contains the tutorial section for the benchmark
