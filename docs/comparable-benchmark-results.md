# Comparable Benchmark Results

> Generated: 2026-03-15T19:26:32.678Z
> OS: Linux  |  CPUs: 16  |  Node.js: v22.12.0  |  isched build: Release

## Results

| Scenario                     | Apollo req/s   | isched req/s   | isched / Apollo ratio  | p95 isched (ms)   |
|------------------------------|----------------|----------------|------------------------|-------------------|
| hello throughput             |         1680.0 |         9007.6 |                  5.36× |                  — |
| concurrent version           |         3268.6 |        25606.4 |                  7.83× |                  — |
| p95 latency (version)        |              — |              — |                      — |            0.47 ms |
| WS healthChanged fan-out     |              — |              — |                      — |              30 ms |

### Notes

- **hello throughput**: `autocannon` — 1 connection, 5 s, `POST /graphql` `{ hello }`
- **concurrent version**: `autocannon` — 100 connections, 5 s sustained, `{ version }`
- **p95 latency (version)**: 1000 sequential `fetch` calls, sorted, `timings[950]`
- **WS healthChanged fan-out**: 50 simultaneous `graphql-transport-ws` subscribers,
  wall-clock until all receive first `healthChanged` event
- p95 / fan-out column shows isched only; Apollo value is informational only
