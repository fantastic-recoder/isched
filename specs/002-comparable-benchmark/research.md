# Research: Comparable Benchmark

**Feature Branch**: `002-comparable-benchmark`
**Updated**: 2026-03-15

---

## 1. Apollo Server 4

### Package

```
@apollo/server  >=4.0.0
```

Apollo Server 4 dropped the monolithic `apollo-server` package in favour of framework-specific integrations. The **standalone** adapter (`startStandaloneServer`) uses Node's built-in `http` module and is the simplest path with no additional router framework required.

```ts
import { ApolloServer } from "@apollo/server";
import { startStandaloneServer } from "@apollo/server/standalone";

const server = new ApolloServer({ typeDefs, resolvers });
const { url } = await startStandaloneServer(server, { listen: { port: 18100 } });
```

### Subscription support

Apollo Server 4 does **not** bundle a WebSocket server. GraphQL over WS requires adding:

```
graphql-ws   # spec-compliant graphql-transport-ws protocol
ws           # Node.js WebSocket server
```

The WS server attaches to an `http.Server` instance alongside Apollo:

```ts
import { createServer } from "http";
import { WebSocketServer } from "ws";
import { useServer } from "graphql-ws/lib/use/ws";

const httpServer = createServer(expressApp);
const wsServer  = new WebSocketServer({ server: httpServer, path: "/graphql" });
useServer({ schema }, wsServer);
httpServer.listen(18100); // HTTP and WS on the same port
```

> **Note**: Apollo's standalone adapter wraps an internal `http.Server` and does not expose it. Therefore the Apollo reference server uses **Express middleware** (`expressMiddleware`) rather than the `startStandaloneServer` shortcut, so the `http.Server` instance is accessible for attaching the WS server. Apollo HTTP stays on **18100**; the same `http.Server` handles WS upgrade requests on the same port.

---

## 2. HTTP Benchmarking Approach

### Option A — `autocannon` (npm)

`autocannon` is a Node.js HTTP benchmarking tool (similar role to `wrk`/`hey`). It supports:
- duration-based (e.g. 5 seconds) and request-count-based modes
- concurrent connections
- custom headers and POST body (required for GraphQL)
- JSON output (`--json` flag)

```bash
autocannon -d 5 -c 1 --json \
  -m POST -H "Content-Type: application/json" \
  -b '{"query":"{ hello }"}' \
  http://localhost:18100/graphql
```

**Selected**: autocannon is the primary benchmark tool because:
1. Full programmatic API (can run inside the same Node.js process)
2. JSON output is directly consumable without parsing
3. Established project, well-maintained, MIT licence

### Option B — External `hey` / `wrk` (shell)

Would require those binaries to be present on the machine. Not selected: adds a system dependency and complicates CI setup.

### Option C — Raw `fetch` loop in TypeScript

Yields accurate sequential timing but cannot drive concurrent load as efficiently as autocannon's native C++ internals. Retained as fallback for p95 sequential latency measurement where simplicity matters more than driver overhead.

**Selected for p95 sequential latency**: custom `fetch` loop records per-request timing to compute p95 without autocannon's min-max bucketing.

---

## 3. pnpm Workspace Strategy

### Chosen approach — root pnpm workspace

A `pnpm-workspace.yaml` is added at the repo root. All Node.js tooling subprojects (current and future) live under `tools/`. The workspace declaration:

```yaml
packages:
  - "tools/*"
```

A root `package.json` provides the convenience script:

```json
{
  "scripts": {
    "benchmark:compare": "pnpm --filter comparable_benchmark run benchmark:compare"
  }
}
```

`tools/comparable_benchmark/` is the first workspace package; more tooling packages will follow.

This is the **first Node.js artefact** in the repo, so no existing workspace config conflicts.

---

## 4. TypeScript Configuration

- **`tsx`** (fast TypeScript runner using esbuild under the hood, no config needed)

**Selected**: `tsx` — faster startup, no tsconfig.json juggling for ESM vs CJS, already widely used in Node 22 projects.

```json
{
  "scripts": {
    "benchmark:compare": "tsx src/runner.ts"
  }
}
```

---

## 5. GraphQL Schema — Apollo Reference Server

The operations that isched exposes as built-ins (see `specs/001-universal-backend/contracts/graphql-schema.md`), augmented with the subscription:

```graphql
type Query {
  hello: String
  version: String
  uptime: Int
  serverInfo: ServerInfo!
  health: HealthStatus!
}

type Subscription {
  healthChanged: HealthChangedEvent!
}

type ServerInfo {
  version: String
  host: String
  port: Int
  status: String
  startedAt: Int
  activeTenants: Int
  activeWebSocketSessions: Int
  transportModes: [String]
}

type HealthStatus {
  status: String
  timestamp: String
  components: [HealthComponent]
}

type HealthChangedEvent {
  status: String!
  timestamp: String!
}
```

> Schema sourced from `specs/001-universal-backend/contracts/graphql-schema.md` (version 3.0.0).
> The benchmark only queries the subset used in the four benchmark scenarios; full schema fidelity
> is not required for the Apollo reference server.

### Resolver implementation strategy

Resolvers return values that **exactly match isched's resolver output** (sourced from `isched_GqlExecutor.cpp`):

| Field | isched value | Apollo implementation |
|-------|-------------|------------------------|
| `hello` | `"Hello, GraphQL!"` | static string `"Hello, GraphQL!"` |
| `version` | `"0.0.1"` | static string `"0.0.1"` |
| `serverInfo.version` | `"0.0.1"` | static `"0.0.1"` |
| `serverInfo.host` | `"localhost"` | static `"localhost"` |
| `serverInfo.port` | `8080` | static `18100` (Apollo's own port) |
| `serverInfo.status` | `"RUNNING"` | static `"RUNNING"` |
| `serverInfo.startedAt` | epoch ms | `Date.now()` captured at startup |
| `serverInfo.activeTenants` | `1` | static `1` |
| `serverInfo.activeWebSocketSessions` | `0` | static `0` |
| `serverInfo.transportModes` | `["http","websocket"]` | static array |
| `health.status` | `"UP"` | static `"UP"` (**not** `"ok"`) |
| `healthChanged.status` | `"UP"` | static `"UP"` |
| `healthChanged.timestamp` | ISO 8601 string | `new Date().toISOString()` |

> **Key finding**: `serverInfo` does **not** expose `uptime` — that is a separate root query field `{ uptime }`. `serverInfo` has 8 fields as shown above. `health.status` is `"UP"`, not `"ok"`.

Using `graphql-subscriptions` `PubSub` — the subscription resolver immediately publishes one `{ status: "UP", timestamp: new Date().toISOString() }` event after a subscriber connects, matching the isched T052-004 behavior.

---

## 6. isched Process Management

### Starting the isched binary

The isched server binary has **no CLI arguments**. Configuration is exclusively via environment variables with the `ISCHED_` prefix. The transformation rule is:

```
ISCHED_SERVER_PORT=18092  →  server.port = 18092
ISCHED_SERVER_HOST=127.0.0.1  →  server.host = 127.0.0.1
```

The harness spawns:

```ts
const isched = spawn(
  process.env.ISCHED_BINARY ?? "cmake-build-debug/isched_server",
  [],
  {
    env: {
      ...process.env,
      ISCHED_SERVER_PORT: "18092",
      ISCHED_SERVER_HOST: "127.0.0.1",
      SPDLOG_LEVEL: "warn",        // suppress info logs during benchmark
    },
  }
);
```

### Starting the Apollo server child process

```ts
const apollo = spawn(
  "node",
  ["--import", "tsx/esm", "tools/comparable_benchmark/src/apollo-server.ts"]
);
```

Apollo uses the same port (18100) for both HTTP and WS upgrade requests.

### Readiness probe

Before starting timed benchmarks the harness polls the server by sending `POST /graphql` with body `{"query":"{ health { status } }"}` at 100 ms intervals until HTTP 200 is received, with a 10-second timeout. Both servers expose **only `POST /graphql`** as the HTTP endpoint (there is no separate `GET /health` route on isched).

---

## 7. WS Subscription Benchmark

### Protocol: `graphql-transport-ws`

Both isched and Apollo use the `graphql-transport-ws` sub-protocol. The Node.js client package `graphql-ws` provides the client API:

```ts
import { createClient } from "graphql-ws";
import WebSocket from "ws";

const client = createClient({
  url: "ws://127.0.0.1:18100/graphql",
  webSocketImpl: WebSocket,
});
```

### Benchmark procedure (mirrors T052-004)

1. Create 50 `graphql-ws` clients.
2. Start a wall-clock timer.
3. Subscribe all 50 clients to `subscription { healthChanged { status timestamp } }`.
4. Await the first `next` message on each client.
5. Stop timer when all 50 have received their first event.
6. Record `fanOutMs`; pass threshold ≤ 500 ms.

The Apollo server must publish an initial `healthChanged` event immediately upon each subscriber connecting (via `PubSub.publish` inside the subscription `subscribe` resolver).

---

## 8. Result JSON Schema

```jsonc
{
  "scenario": "hello-throughput",      // string
  "server": "apollo" | "isched",       // string
  "requests": 12345,                   // total requests completed
  "errors": 0,                         // non-2xx or network errors
  "durationMs": 5000,                  // wall-clock duration of measurement
  "reqPerSecond": 2469.0,              // requests / durationMs * 1000  (null for WS fan-out)
  "p95Ms": null,                       // p95 latency ms (null when not measured)
  "fanOutMs": null,                    // WS fan-out elapsed ms (null for HTTP scenarios)
  "hardwareContext": {
    "os": "linux",
    "cpuCount": 8,
    "nodeVersion": "22.12.0",
    "buildType": "Debug"
  },
  "timestamp": "2026-03-15T10:30:00Z"
}
```

---

## 9. Output Markdown Table Format

The report generator (`src/report.ts`) reads all six JSON files and produces:

```markdown
# Comparable Benchmark Results

Generated: 2026-03-15T10:30:00Z
Hardware: Linux, 8 CPUs | Node 22.12.0 | isched Debug build

| Scenario              | Apollo req/s | isched req/s | Ratio  | isched p95 (ms) | WS fan-out (ms) |
|-----------------------|:------------:|:------------:|:------:|:---------------:|:---------------:|
| hello throughput      |    2,469     |    3,812     |  1.54× |        —        |        —        |
| concurrent version    |    1,821     |    2,950     |  1.62× |        —        |        —        |
| p95 latency (version) |      —       |      —       |   —    |      8 ms       |        —        |
| WS healthChanged      |      —       |      —       |   —    |        —        |      210 ms     |
```

---

## 10. Open Questions / Future Work

| # | Topic | Decision Needed |
|---|-------|-----------------|
| 1 | WS subscription comparison | Included in this spec (T052-004 mirror) |
| 2 | Hard CI gate threshold | Configurable via env `REGRESSION_THRESHOLD_PCT` (default 10) |
| 3 | Apollo version pinning | Pin `@apollo/server` to `^4.11` in `package.json` |
| 4 | Warm-up for isched | Same 100-request warm-up applied to isched before measurement window |
| 5 | CI integration | Deferred; local developer tool only for now |
