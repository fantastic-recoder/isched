# Comparable Benchmark Results

> Generated: 2026-03-15T19:17:39.310Z
> OS: Linux  |  CPUs: 16  |  Node.js: v22.12.0  |  isched build: Release

## Results

| Scenario                     | Apollo req/s   | isched req/s   | isched / Apollo ratio  | p95 isched (ms)   |
|------------------------------|----------------|----------------|------------------------|-------------------|
| hello throughput             |         1317.6 |         8905.8 |                  6.76× |                  — |
| concurrent version           |          973.7 |          979.4 |                  1.01× |                  — |
| p95 latency (version)        |              — |              — |                      — |            4.02 ms |
| WS healthChanged fan-out     |              — |              — |                      — |              27 ms |

### Notes

- **hello throughput**: `autocannon` — 1 connection, 5 s, `POST /graphql` `{ hello }`
- **concurrent version**: `autocannon` — 100 connections, 1000 total requests, `{ version }`
- **p95 latency (version)**: 1000 sequential `fetch` calls, sorted, `timings[950]`
- **WS healthChanged fan-out**: 50 simultaneous `graphql-transport-ws` subscribers,
  wall-clock until all receive first `healthChanged` event
- p95 / fan-out column shows isched only; Apollo value is informational only
