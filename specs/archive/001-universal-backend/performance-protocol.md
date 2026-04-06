# Performance Protocol: Universal Backend

**Feature**: `001-universal-backend`  
**Status**: Normative for FR-012 and SC-006  
**Updated**: 2026-04-04

## Purpose

This document defines the canonical two-tier performance validation protocol used to evaluate FR-012 and SC-006.

- Tier 1 (in-process): fast regression guard for development
- Tier 2 (HTTP-level `/graphql`): normative acceptance gate for release claims

## Environment Baseline

- OS: Linux
- Hardware class: Ryzen 7 or Intel i5 class (or stronger)
- Build profile: run and report build type (`Debug` or `Release`) explicitly
- Server endpoint under test: `POST /graphql`
- Authentication: valid JWT for authenticated queries

## Tier 1: In-Process Regression Guard

### Objective

Catch regressions quickly in CI/developer loops.

### Minimum procedure

1. Start server in-process test harness.
2. Execute 1000 sequential `{ version }` queries.
3. Measure per-request wall time and compute p95.
4. Store result as regression metric.

### Gate interpretation

- Tier 1 failures indicate regression risk and must be investigated.
- Tier 1 does **not** replace Tier 2 release acceptance.

## Tier 2: HTTP-Level Acceptance Gate (Normative)

### Objective

Provide release-gating evidence for FR-012/SC-006 with externally observable transport behavior.

### Fixed profile

- Transport: HTTP `POST /graphql`
- Query mix (default):
  - 60% `{ version }`
  - 25% `{ hello }`
  - 15% lightweight introspection (`{ __schema { queryType { name } } }`)
- Concurrency: 100 concurrent HTTP clients
- Warmup: 30 seconds warmup before timed sampling
- Timed sample window: 120 seconds
- Percentile method: nearest-rank p95 over all successful request latencies in the timed window

### Pass/fail criteria

- p95 latency <= 20 ms
- application-level error rate == 0 for requests in the timed window
- no unauthorized fallback paths (all authenticated operations must enforce JWT checks)

### Required reporting fields

Each acceptance run must document:

- hardware model and core/thread count
- memory size
- OS/kernel version
- build type and git commit
- exact command/workload harness version
- request counts, p50/p95/p99, max latency, error count

## Relationship to `docs/performance.md`

- This file is the canonical protocol definition and acceptance gate.
- `docs/performance.md` is the release-facing summary of measured results.
- If thresholds, query mix, or percentile method change, update this file first and then update `docs/performance.md` accordingly.

