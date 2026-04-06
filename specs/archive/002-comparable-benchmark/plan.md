# Implementation Plan: Comparable Benchmark

**Feature Branch**: `002-comparable-benchmark`
**Created**: 2026-03-15
**Status**: Ready

---

> **Note**: This plan covers the TypeScript/Node.js subproject `tools/comparable_benchmark/` and the
> thin root-level pnpm workspace glue. It does **not** modify the C++ isched source; the isched
> server binary is treated as a black-box subprocess.

---

## Summary

Introduce a `comparable_benchmark/` subproject that:

1. Starts an **Apollo Server 4** reference GraphQL server (TypeScript, HTTP + WS on port 18100) with the same four built-in queries and `healthChanged` subscription as isched.
2. Starts the compiled **isched server** binary (port 18092) as a child process.
3. Runs **four** matching benchmark scenarios against each server using **autocannon** (throughput/concurrent), a custom **`fetch` loop** (p95 latency), and **`graphql-ws` client** (WS fan-out).
4. Saves per-scenario JSON result files to `tools/comparable_benchmark/results/`.
5. Generates a combined Markdown report at `docs/comparable-benchmark-results.md`.
6. Is invoked from the repo root via `pnpm run benchmark:compare`.

---

## Technical Context

| Aspect             | Choice                                                                 |
|--------------------|------------------------------------------------------------------------|
| Language (subproject) | TypeScript 5 executed with `tsx` (no compile step)                 |
| Runtime            | Node.js ≥ 22 (already available on dev machines)                      |
| Package manager    | pnpm 10 — workspace declared in `pnpm-workspace.yaml` at repo root    |
| GraphQL server     | `@apollo/server` ^4.11 + `expressMiddleware` (from `@apollo/server/express4`) |
| HTTP server        | `express` ^4 + `body-parser` ^1 (required by `expressMiddleware`) |
| WS server          | `ws` ^8 + `graphql-ws` ^5 (`useServer` from `graphql-ws/lib/use/ws`) |
| Subscription model | `graphql-subscriptions` ^2 (`PubSub`) |
| Benchmark driver   | `autocannon` ^7 (HTTP throughput + concurrent load)                   |
| p95 latency driver | Custom `fetch` loop in `src/benchmark.ts`                             |
| WS client driver   | `graphql-ws` ^5 client + `ws` ^8 for subscription fan-out |
| Apollo WS URL      | `ws://127.0.0.1:18100/graphql` (same port as HTTP) |
| isched WS URL      | `ws://127.0.0.1:18093/graphql` (isched WS port = HTTP port + 1) |
| Result storage     | JSON files in `tools/comparable_benchmark/results/` (git-ignored)    |
| Report output      | `docs/comparable-benchmark-results.md` (committed when updated)       |
| isched binary path | `cmake-build-debug/isched_server` (override via `ISCHED_BINARY` env)  |
| isched port config | Env vars: `ISCHED_SERVER_PORT=18092 ISCHED_SERVER_HOST=127.0.0.1`     |
| Apollo HTTP/WS port | 18100 (HTTP + WS on same port via `http.Server` upgrade)             |
| isched HTTP port   | 18092                                                                  |
| isched WS port     | 18093                                                                  |

---

## Subproject Directory Layout

```
tools/
  comparable_benchmark/
    package.json              # name: "comparable_benchmark", scripts, deps + devDeps
    tsconfig.json             # target: ES2022, moduleResolution: node16, esModuleInterop, strict
    src/
      apollo-server.ts        # Apollo Server 4 + graphql-ws server + resolvers + subscription
      benchmark.ts            # autocannon wrapper + fetch-loop p95 + graphql-ws fan-out helper
      runner.ts               # orchestrates start→warmup→measure→stop→report
      report.ts               # reads JSON results, writes Markdown table
      types.ts                # shared TypeScript interface BenchmarkResult
    results/                  # .gitignore'd output directory
      .gitkeep
```

Root additions:
```
pnpm-workspace.yaml         # packages: ["tools/*"]  ← add first, unblocks workspace installs
package.json                # root; scripts: { "benchmark:compare": "pnpm -F comparable_benchmark run benchmark:compare" }
```

---

## Dependency Manifest

### `tools/comparable_benchmark/package.json`

```jsonc
{
  "name": "comparable_benchmark",
  "version": "0.1.0",
  "private": true,
  "type": "module",
  "scripts": {
    "benchmark:compare": "tsx src/runner.ts",
    "report": "tsx src/report.ts",
    "start:apollo": "tsx src/apollo-server.ts"
  },
  "dependencies": {
    "@apollo/server": "^4.11.0",
    "autocannon": "^7.15.0",
    "body-parser": "^1.20.0",
    "express": "^4.19.0",
    "graphql": "^16.8.0",
    "graphql-subscriptions": "^2.0.0",
    "graphql-ws": "^5.16.0",
    "ws": "^8.17.0"
  },
  "devDependencies": {
    "@types/express": "^4.17.0",
    "@types/node": "^22.0.0",
    "@types/ws": "^8.5.0",
    "tsx": "^4.19.0",
    "typescript": "^5.4.0"
  }
}
```

> `expressMiddleware` is exported from **`@apollo/server/express4`** (sub-path export inside the
> `@apollo/server` package itself — no separate package needed).

---

## Benchmark Scenarios (mirroring T052 HTTP subset)

| # | Name                   | Method            | autocannon params                  | Metric captured                                        |
|---|------------------------|-------------------|------------------------------------|-------------------------------------------------------|
| 1 | hello throughput        | autocannon        | `duration: 5, connections: 1`      | `requests.average` → req/s                            |
| 2 | concurrent version      | autocannon        | `connections: 100, amount: 1000`   | total elapsed ms (`finish - start`), `errors`         |
| 3 | p95 latency (version)   | `fetch` loop      | 1000 sequential POST requests      | sort timings array → `timings[950]` = p95 ms          |
| 4 | WS healthChanged fan-out| `graphql-ws` client | 50 concurrent subscribers        | wall-clock from first `subscribe` to last `next` event |

GraphQL body for all HTTP scenarios: `Content-Type: application/json`, `POST /graphql`.
HTTP benchmark URL: `http://127.0.0.1:{port}/graphql`.
WS URL: Apollo `ws://127.0.0.1:18100/graphql` | isched `ws://127.0.0.1:18093/graphql`.

Each HTTP scenario runs: warm-up (100 requests, un-timed) → measurement window. WS fan-out has no warm-up (connection establishment is part of the measurement).

---

## Build & Run Commands

```bash
# One-time: install subproject dependencies (run from repo root)
pnpm install

# Run the full benchmark suite (requires cmake-build-debug/isched_server to exist)
pnpm run benchmark:compare

# Override binary path
ISCHED_BINARY=./cmake-build-debug/isched_server pnpm run benchmark:compare

# Run only report generation (if JSON results already exist)
pnpm --filter comparable_benchmark run report

# Validate setup without starting servers
pnpm run benchmark:compare -- --dry-run
```

No CMake changes are required. The isched binary is expected to already be built (`python3 configure.py` from repo root).

---

## Phases

> **Sequencing note**: Phases A and E are pure configuration files with no code. They can (and
> should) be committed together as a single scaffold commit. Phases B–D are the bulk of the work
> and should each get their own commit. Phase F is the polish gate before the feature is done.

### Phase A + E — Scaffold (workspace config + subproject layout)
Create `pnpm-workspace.yaml`, root `package.json`, `tools/comparable_benchmark/package.json`,
`tsconfig.json`, `results/.gitkeep`, and update `.gitignore`. Run `pnpm install`. No functional
code yet — this commit unblocks all subsequent phases.

### Phase B — Apollo Reference Server
Implement `apollo-server.ts`:
- `express` app with `body-parser.json()` middleware
- `ApolloServer` with schema (4 queries + 1 subscription)
- `expressMiddleware` bound to `app` at `/graphql`
- `http.createServer(app)` exposed so `WebSocketServer` can attach
- `graphql-ws` `useServer` on `{ path: "/graphql" }`
- `server.listen(18100)` — single port for HTTP + WS upgrade
- `PubSub` subscription resolver publishes `{ status: "UP" }` immediately on each new subscriber
- SIGTERM/SIGINT: close WS server → `server.close()` (≤ 2 s)

Smoke-test: `tsx tools/comparable_benchmark/src/apollo-server.ts` + `curl` POST + `wscat -c ws://…`.

### Phase C — Benchmark Harness (`benchmark.ts` + process utils in `runner.ts`)
Implement:
1. `runThroughput(url, query)` — `autocannon({ url, connections: 1, duration: 5, method: "POST", headers, body })` → `BenchmarkResult`
2. `runConcurrent(url, query)` — `autocannon({ url, connections: 100, amount: 1000, method: "POST", … })` → `BenchmarkResult`
3. `runP95Sequential(url, query, iterations = 1000)` — `for` loop with `performance.now()` per request; `timings.sort(); timings[Math.floor(0.95 * n)]` → `BenchmarkResult`
4. `runWsFanOut(wsUrl, clientCount = 50)` — create `clientCount` `graphql-ws` clients, subscribe all, `Promise.all` waiting for first `next`, record total elapsed → `BenchmarkResult`
5. `spawnServer(binary, args, readinessUrl, env)` — `child_process.spawn`, poll `readinessUrl` every 100 ms up to 10 s
6. `warmUp(url, query, count = 100)` — fire-and-forget POST loop
7. `killServer(proc)` — SIGTERM + 2 s timeout + SIGKILL fallback

### Phase D — Orchestration & Persistence (`runner.ts` main)
Main function sequence:
1. Check `ISCHED_BINARY` exists; exit 1 with clear message if not
2. Start Apollo (`tsx apollo-server.ts`), await readiness
3. Start isched (env: `ISCHED_SERVER_PORT=18092 ISCHED_SERVER_HOST=127.0.0.1 SPDLOG_LEVEL=warn`), await readiness
4. For each server: warm-up (HTTP only) → run 4 scenarios → write JSON to `results/`
5. Kill both servers
6. Call `generateReport()` → writes `docs/comparable-benchmark-results.md`
7. Run regression check; exit 1 if threshold breached

### Phase E — (merged into Phase A, see above)

### Phase F — Report, Error Handling & README Tutorial
- `report.ts`: read 8 JSON files, build 4-row Markdown table + hardware header
- Error paths: port EADDRINUSE detection, binary-not-found message
- `--dry-run` flag
- `README.md` tutorial section
- Real benchmark run → commit `docs/comparable-benchmark-results.md`

---

## Risks & Known Constraints

| Risk | Mitigation |
|------|------------|
| `expressMiddleware` requires `body-parser` to be applied before it | Add `app.use(bodyParser.json())` before `app.use("/graphql", expressMiddleware(...))` |
| `graphql-ws` `useServer` intercepts WS upgrades — HTTP requests unaffected | Both share the same `http.Server`; only `Upgrade: websocket` requests are routed to WS |
| isched WS port is HTTP port + 1 (18093), not 18100 | `runWsFanOut` must use `ws://127.0.0.1:18093/graphql` for isched, `ws://127.0.0.1:18100/graphql` for Apollo |
| isched Debug build is slower than Release — numbers reflect Debug | Hardware context header in report notes "Debug build"; `CMAKE_BUILD_TYPE` field in JSON |
| `autocannon` `amount` mode waits for all requests to complete | `errors` field in result detects connection refusals; non-zero errors invalidate the run |
| Apollo cold JIT vs isched native | 100-request unmetered warm-up per server before measurement window |
| Port collision with other local services | `--dry-run` flag probes ports before starting; actionable error if EADDRINUSE |

---

## Constraints

- **No C++ changes**: isched C++ source and CMake are not modified by this feature.
- **No new ctest tests**: the benchmark is a developer tool, not a CI test suite.
- **Git-ignore results/**: raw JSON result files must not be committed; only the generated Markdown report is committed.
- **Node ≥ 22 only**: `fetch` is used as a global (stable since Node 18); no polyfill needed.
- **isched has no CLI args**: configuration is via env vars only; the harness must set `ISCHED_SERVER_PORT` and `ISCHED_SERVER_HOST` in the child process environment.
- **First Node.js artefact**: `pnpm-workspace.yaml` is new; `tools/*` glob accommodates future tooling packages.
