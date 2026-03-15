# Comparable Benchmark Results

> Generated: 2026-03-15T17:12:33.954Z
> OS: Linux  |  CPUs: 16  |  Node.js: v22.12.0  |  isched build: Debug

## Results

| Scenario                     | Apollo req/s   | isched req/s   | isched / Apollo ratio  | p95 isched (ms)   |
|------------------------------|----------------|----------------|------------------------|-------------------|
| hello throughput             |         2263.8 |           24.6 |                  0.01× |                  — |
| concurrent version           |          977.5 |           89.9 |                  0.09× |                  — |
| p95 latency (version)        |              — |              — |                      — |         3038.85 ms |
| WS healthChanged fan-out     |              — |              — |                      — |              55 ms |

### Notes

- **hello throughput**: `autocannon` — 1 connection, 5 s, `POST /graphql` `{ hello }`
- **concurrent version**: `autocannon` — 100 connections, 1000 total requests, `{ version }`
- **p95 latency (version)**: 1000 sequential `fetch` calls, sorted, `timings[950]`
- **WS healthChanged fan-out**: 50 simultaneous `graphql-transport-ws` subscribers,
  wall-clock until all receive first `healthChanged` event
- p95 / fan-out column shows isched only; Apollo value is informational only
