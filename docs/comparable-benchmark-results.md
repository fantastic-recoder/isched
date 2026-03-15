# Comparable Benchmark Results

> Generated: 2026-03-15T18:05:20.964Z
> OS: Linux  |  CPUs: 16  |  Node.js: v22.12.0  |  isched build: Release

## Results

| Scenario                     | Apollo req/s   | isched req/s   | isched / Apollo ratio  | p95 isched (ms)   |
|------------------------------|----------------|----------------|------------------------|-------------------|
| hello throughput             |         3866.2 |           24.6 |                  0.01× |                  — |
| concurrent version           |          976.6 |          198.8 |                  0.20× |                  — |
| p95 latency (version)        |              — |              — |                      — |            5.31 ms |
| WS healthChanged fan-out     |              — |              — |                      — |              38 ms |

### Notes

- **hello throughput**: `autocannon` — 1 connection, 5 s, `POST /graphql` `{ hello }`
- **concurrent version**: `autocannon` — 100 connections, 1000 total requests, `{ version }`
- **p95 latency (version)**: 1000 sequential `fetch` calls, sorted, `timings[950]`
- **WS healthChanged fan-out**: 50 simultaneous `graphql-transport-ws` subscribers,
  wall-clock until all receive first `healthChanged` event
- p95 / fan-out column shows isched only; Apollo values are informational only
  (Apollo p95 latency: **1.78 ms**; Apollo WS fan-out: **244 ms** — isched wins fan-out **6.4×**)
- **hello throughput** uses a single connection; cpp-httplib's thread-per-connection model means
  only one thread is active, which is why Apollo's async event-loop outperforms isched here.
  At 50 concurrent connections isched reaches ~775 req/s vs Apollo's ~2630 req/s.
