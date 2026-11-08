# Implementation Plan: Plugin System (Boost.DLL)

**Branch**: `012-plugin-system-boost-dll` | **Date**: 2026-06-04 | **Spec**: `/home/groby/dev/isched/specs/012-plugin-system-boost-dll/spec.md`
**Input**: Updated feature specification from `/specs/012-plugin-system-boost-dll/spec.md`

## Summary

Deliver a runtime plugin system for request and resolver shared libraries using Boost.DLL with GraphQL-only administration, deterministic conflict rejection, persisted lifecycle state, restart restore, and a `platform_admin`-gated lifecycle subscription surface (User Story 6). Plan scope explicitly preserves prior remediations for FR-021 resolver optional-schema two-path flow and FR-022 `plugins_root` path policy, and now adds FR-023/FR-024/FR-025 design commitments for lifecycle event publish timing, payload mapping, and pre-stream authorization rejection.

## Technical Context

**Language/Version**: C++23  
**Primary Dependencies**: Boost.DLL + Boost.Filesystem, nlohmann::json, SQLite (`DatabaseManager`), `GqlExecutor::load_schema` with project PEGTL grammar (`isched_gql_grammar.hpp`) for SDL validation, spdlog  
**Storage**: `isched_system.db` (`plugin_registry`)  
**Testing**: Catch2 + CTest + coverage (>=80% line/branch on plugin-system files), GraphQL subscription integration checks for SC-017/SC-018  
**Target Platform**: Linux server runtime  
**Project Type**: Backend GraphQL service  
**Performance Goals**:
- `PluginManager::dispatch_request` p95 latency regression <= 5% vs baseline
- `GqlExecutor::execute` on plugin resolver paths p95 latency regression <= 5% vs baseline
- `PluginManager::restore_persisted_plugins` startup restore duration regression <= 5% vs baseline (same plugin set)
**Constraints**:
- GraphQL-only management interface (`/graphql`), no REST admin surface
- FR-021 resolver load behavior: two-path flow (schema-supplied via `ResolverPlugin::graphqlSchema()` vs empty no-schema sentinel), with manifest `graphqlSchemaSdl` informational only
- FR-022 path policy gate before any dynamic loading: absolute path + canonical containment under configured `plugins_root`
- FR-023/FR-024 lifecycle events publish only on successful load/unload state transitions with canonical payload mapping
- FR-025 authorization gate for lifecycle subscription must reject unauthorized callers before subscription stream establishment
- Role-based access via `platform_admin`
**Scale/Scope**: Dozens of plugins per instance; load-time route/resolver conflict checks; persisted restore on each startup

## Constitution Gates

### Pre-Phase 0 Gates

- [x] GraphQL-only external interface preserved (`/graphql` HTTP/WebSocket only)
- [x] C++23 + Conan + CMake/Ninja baseline preserved
- [x] Angular/WebUI/JWT/proxy gates: N/A (backend-only change)
- [x] Test strategy includes automated coverage and acceptance verification
- [x] Security documentation gate: plan requires **both** updates:
  - feature-scoped threat model in `specs/012-plugin-system-boost-dll/`
  - project threat model entry in `docs/security-threat-model.md`
- [x] Clean Code gate: prefer focused functions/polymorphic seams on hot paths with profiling evidence when trade-offs exist
- [x] Performance regression gate defined: baseline + post-change benchmark comparison with <=5% regression threshold on core paths
- [x] Lifecycle subscription security gate defined: unauthorized callers rejected before stream establishment (FR-025)

### Post-Phase 1 Gate Re-check

- [x] FR-021 remains explicit in `data-model.md` + contract
- [x] FR-022 `plugins_root` path policy remains first-step validation in design + contract
- [x] FR-023/FR-024 lifecycle publication semantics and payload fields explicitly defined in design + contract
- [x] FR-025 pre-stream authorization timing explicitly defined in design + contract
- [x] GraphQL-only management and `platform_admin` authorization preserved
- [x] Security deliverables still mandatory at both feature and project levels
- [x] No constitution violations identified

## Phase 0 — Research

Output: `/home/groby/dev/isched/specs/012-plugin-system-boost-dll/research.md`

Completed decisions:
1. FR-021 flow and authority decisions captured per FR-021.
2. FR-022 modeled as canonical path containment under `plugins_root` before Boost.DLL load.
3. Error taxonomy standardized for path-policy, SDL parse/conflict, and resolver registration failures.
4. Benchmark plan defined for baseline and post-change comparisons on dispatch, resolver execution, and restore startup paths.
5. Lifecycle subscription publish pipeline fixed: emit exactly once only after successful load/unload transition commit, never on pre-transition failures.
6. Lifecycle subscription auth timing fixed: `require_roles(platform_admin)` enforced during subscription resolver entry; unauthorized callers are rejected before stream handle creation.
7. SC-017 latency measurement method fixed: measure mutation completion timestamp to subscriber receipt timestamp across trial runs; require <=2s delivery in >=95% of successful transition events.

## Phase 1 — Design & Contracts

Outputs:
- `/home/groby/dev/isched/specs/012-plugin-system-boost-dll/data-model.md`
- `/home/groby/dev/isched/specs/012-plugin-system-boost-dll/contracts/graphql-plugin-api.md`
- `/home/groby/dev/isched/specs/012-plugin-system-boost-dll/quickstart.md`

Design commitments:
1. FR-021 resolver load orchestration and rollback behavior implemented per FR-021.
2. FR-022 explicit flow:
   - validate absolute path
   - canonicalize library/root
   - enforce canonical containment under `plugins_root`
   - reject before load/persist/activate on any failure.
3. Benchmarks:
   - capture baseline before implementation changes
   - rerun post-change with identical dataset/hardware
   - fail gate if any relevant metric regresses by >5%.
4. Security docs:
   - update feature threat-model notes in this spec directory
   - update summarized project entry in `docs/security-threat-model.md`.
5. User Story 6 lifecycle subscription architecture (FR-023/FR-024/FR-025):
   - introduce admin subscription field for plugin lifecycle events
   - source events from `PluginManager` transition success points only:
     - publish `LOADED` immediately after successful activation + persistence during `loadPlugin`
     - publish `UNLOADED` immediately after successful deactivation + persistence during `unloadPlugin`
   - no event publication on validation/auth/conflict/not-found/idempotent-no-transition failures
   - payload mapping contract:
     - `eventType` from transition kind (`LOADED`/`UNLOADED`)
     - `pluginId`, `pluginKind`, `pluginName`, `libraryPath` from activated/deactivated plugin manifest/record
     - `occurredAt` from server-side transition commit timestamp (UTC)
   - subscription auth timing:
     - run `require_roles(..., platform_admin)` before creating/returning event stream cursor
     - unauthorized subscription attempts fail immediately with authorization error and no stream side effects
6. SC-017/SC-018 test strategy:
   - successful paths: assert exact event count (+1 per successful load/unload transition)
   - negative paths: assert event counter unchanged (zero new events)
   - payload assertions: all FR-024 fields non-empty and equal to affected plugin record values
   - latency method: capture per-trial `(event_received_ts - mutation_completed_ts)` and compute p95 over N trials (N>=20); pass if p95 <= 2000ms and success-rate threshold >=95%

Agent context update executed:
- `.specify/scripts/bash/update-agent-context.sh copilot`

## Phase 2 — Implementation Planning (Stop)

Planned workstreams for task generation:
1. Enforce FR-022 path policy in `loadPlugin` preflight.
2. Implement resolver load orchestration and rollback safety per FR-021.
3. Preserve route/resolver conflict invariants and idempotent unload behavior.
4. Implement lifecycle subscription publish pipeline for FR-023/FR-024 with post-transition emit points only.
5. Enforce FR-025 auth check before lifecycle subscription stream establishment.
6. Extend tests for SC-013/SC-014/SC-015/SC-017/SC-018 + coverage threshold.
7. Run baseline/post-change performance benchmarks and enforce <=5% regression gate (including subscription event delivery timing evidence).
8. Apply required feature + project threat-model documentation updates.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| None | N/A | N/A |
