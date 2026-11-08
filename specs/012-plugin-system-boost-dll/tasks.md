# Tasks: SP-012 Plugin System (Boost.DLL)

**Input**: Design documents from `/home/groby/dev/isched/specs/012-plugin-system-boost-dll/`  
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/graphql-plugin-api.md`, `quickstart.md`

**Tests**: Automated tests are required by the specification (FR-016, SC-001..SC-018).  
**Organization**: Tasks are grouped by user story and ordered for executable implementation.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish implementation baseline, benchmark harness, and initial artifacts.

- [X] T001 Create plugin-system benchmark harness script for dispatch/GraphQL/restore paths in `tools/benchmarks/plugin_system_perf.sh`
- [X] T002 Add benchmark target wiring and dependencies for plugin performance harness in `src/test/cpp/isched/CMakeLists.txt`
- [X] T003 [P] Create benchmark dataset/runbook for repeatable measurements in `specs/012-plugin-system-boost-dll/benchmarks/README.md`
- [X] T004 Capture and store pre-change baseline metrics (p95 + startup duration) in `specs/012-plugin-system-boost-dll/benchmarks/baseline.json`
- [X] T005 [P] Create feature-scoped requirements traceability scaffold in `specs/012-plugin-system-boost-dll/requirements-traceability.md`
- [X] T006 [P] Create feature-scoped threat model scaffold in `specs/012-plugin-system-boost-dll/threat-model.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core plugin contracts, persistence, GraphQL API surface, and shared validation infrastructure.

**⚠️ CRITICAL**: No user-story completion is allowed before this phase is complete.

- [X] T007 Implement `PluginKind` enum and GraphQL mapping constants in `src/main/cpp/isched/backend/isched_PluginSystem.hpp`
- [X] T008 Implement/normalize `PluginManifest` fields (`abi_version`, `openApiFragmentJson`, resolver and route metadata) in `src/main/cpp/isched/backend/isched_PluginSystem.hpp`
- [X] T009 Implement C-linkage factory contracts (`isched_create_request_plugin`, `isched_create_resolver_plugin`) and plugin interfaces in `src/main/cpp/isched/backend/isched_PluginSystem.hpp`
- [X] T010 Implement manifest JSON serialization/deserialization updates for all required fields in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T011 Implement `plugin_registry` schema initialization and migration path in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T012 Implement `DatabaseManager` plugin-registry CRUD/query APIs in `src/main/cpp/isched/backend/isched_DatabaseManager.hpp`
- [X] T013 Implement `DatabaseManager` plugin-registry CRUD/query API definitions in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T014 Add `GqlExecutor::unregister_resolver` API contract in `src/main/cpp/isched/backend/isched_GqlExecutor.hpp`
- [X] T015 Implement `GqlExecutor::unregister_resolver` behavior in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T016 Implement `PluginManager::inject_loaded_plugin` and base lifecycle scaffolding in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T017 Expose GraphQL enum/type/input updates for plugin operations plus lifecycle subscription type placeholders in `src/main/cpp/isched/backend/isched_builtin_server_schema.graphql`
- [X] T018 Implement GraphQL resolver skeletons for `loadPlugin`, `unloadPlugin`, `plugins`, `plugin(id:)`, and `pluginLifecycleEvents` in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T019 Implement `require_roles` protection (`platform_admin`) for plugin queries/mutations and subscription entry checks in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T020 [P] Add foundational tests for manifest round-trip, DB CRUD, GraphQL schema fields, and auth pre-checks in `src/test/cpp/isched/isched_plugin_system_tests.cpp`

**Checkpoint**: Foundation ready for independent user-story implementation.

---

## Phase 3: User Story 1 - Load and activate a request plugin at runtime (Priority: P1) 🎯 MVP

**Goal**: Load request plugins via GraphQL, enforce route/path safety, dispatch at `/plugin/*`, and support deterministic unload.

**Independent Test**: Load a request plugin via `loadPlugin`, call its `/plugin/*` route, verify persisted record and successful response; verify unload removes route and marks unloaded.

### Tests for User Story 1

- [X] T021 [P] [US1] Add request-plugin load + route dispatch success test in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T022 [P] [US1] Add FR-006 deterministic request route conflict tests covering method normalization behavior, trailing-slash normalization behavior, segment-count non-conflict behavior, templated-vs-static per-segment compatibility conflicts, and case-sensitive segment policy in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T023 [P] [US1] Add FR-022/SC-015 path-policy tests (relative path, absolute outside root, canonicalization failure, symlink escape) in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T024 [P] [US1] Add fail-fast negative tests for FR-022 path-policy and FR-019 invalid `openApiFragmentJson` parse rejection, asserting load is rejected, descriptive parse error is returned, no DLL load/plugin activation occurs, and no registry/persistence partial side effects in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T025 [P] [US1] Add request-plugin crash containment test (HTTP 500 + auto-unload + persisted unload state) in `src/test/cpp/isched/isched_plugin_system_tests.cpp`

### Implementation for User Story 1

- [X] T026 [US1] Implement request-plugin dynamic load and factory resolution in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T027 [US1] Implement FR-006 deterministic request-route normalization and compatibility conflict detection (method normalization, trailing-slash normalization, segment-count non-conflict, templated-vs-static per-segment compatibility, case-sensitive segments) and registration in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T028 [US1] Implement `/plugin/*` dispatch in `HttpSession::handle_request()` via `PluginManager::dispatch_request` in `src/main/cpp/isched/backend/isched_Server.cpp`
- [X] T029 [US1] Implement FR-022 preflight absolute-path check and canonical path resolution in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T030 [US1] Implement FR-022 canonical-in-root containment and symlink escape rejection in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T031 [US1] Enforce FR-022 fail-fast ordering before dynamic load/persist/in-memory activation in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T032 [US1] Implement request-plugin unload lifecycle (route deregistration, idempotence, DB `is_loaded`/timestamps) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T033 [US1] Implement request handler exception containment and automatic unload semantics in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T034 [US1] Complete final `loadPlugin`/`unloadPlugin` GraphQL mutation handler wiring and verify contract-level error assertions for conflict/validation/not-found paths in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp` and `src/test/cpp/isched/isched_plugin_system_tests.cpp`

**Checkpoint**: US1 satisfies request-plugin runtime load/dispatch/unload plus FR-022 path policy.

---

## Phase 4: User Story 2 - Load and activate a resolver plugin at runtime (Priority: P1)

**Goal**: Support resolver-plugin runtime loading with explicit FR-021 two-path schema behavior and rollback-safe error handling.

**Independent Test**: Load resolver plugin (schema-supplied and no-schema variants), run GraphQL operations against registered fields, verify success/failure semantics and rollback guarantees.

### Tests for User Story 2

- [X] T035 [P] [US2] Add FR-021 schema-supplied success test (non-empty `ResolverPlugin::graphqlSchema()` is parsed/validated via `GqlExecutor::load_schema` using `isched_gql_grammar` (PEGTL), then merged/registered/queryable, taking precedence over manifest `graphqlSchemaSdl` metadata) in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T036 [P] [US2] Add FR-021 schema-supplied invalid-SDL rejection test with no partial state in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T037 [P] [US2] Add FR-021 schema conflict rejection test (core/plugin type-field conflicts) with no partial merge in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T038 [P] [US2] Add FR-021 empty-string no-schema success test for existing unclaimed fields in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T039 [P] [US2] Add FR-021 empty-string no-schema rejection tests (field-not-found + resolver-conflict) in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T040 [P] [US2] Add FR-021 rollback/error semantics test (registration/merge failure triggers unload and no persist mutation) in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T041 [P] [US2] Add resolver throwable auto-unload + GraphQL error-node tests in `src/test/cpp/isched/isched_plugin_system_tests.cpp`

### Implementation for User Story 2

- [X] T042 [US2] Implement resolver-plugin dynamic load and binding preflight in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T043 [US2] Implement FR-021 branch selection on `ResolverPlugin::graphqlSchema()` empty/non-empty, with `graphqlSchema()` as authoritative when present over manifest `graphqlSchemaSdl` metadata in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T044 [US2] Implement schema-supplied flow (FR-021 SDL parsing/validation via `GqlExecutor::load_schema` using `isched_gql_grammar` (PEGTL), plus conflict detection and merge-before-register) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T045 [US2] Implement no-schema flow (existing-field check + unclaimed-resolver check before register) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T046 [US2] Implement FR-021 rollback semantics for parse/merge/register failures (undo schema/resolvers, unload library, no partial persistence) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T047 [US2] Implement resolver unload path using `unregisterResolvers` + `GqlExecutor::unregister_resolver` in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T048 [US2] Make US2 test suite green and align resolver test doubles in `src/test/cpp/isched/isched_plugin_system_tests.cpp`

**Checkpoint**: US2 satisfies FR-021 full behavior, resolver conflict checks, and rollback-safe runtime lifecycle.

---

## Phase 5: User Story 3 - Discover loaded plugins and metadata (Priority: P2)

**Goal**: Provide authorized GraphQL discovery of plugin records and manifest metadata.

**Independent Test**: Query `plugins` and `plugin(id:)` as `platform_admin`, verify full records/manifest JSON; verify non-admin access is denied.

### Tests for User Story 3

- [X] T049 [P] [US3] Add `plugins` query tests (empty state, loaded list; include kind filtering only if explicitly required by FR-010/spec acceptance criteria) in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T050 [P] [US3] Add `plugin(id:)` query tests (found/not-found and manifest payload) in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T051 [P] [US3] Add plugin discovery RBAC tests for non-admin callers in `src/test/cpp/isched/isched_gql_executor_auth_resolvers_tests.cpp`

### Implementation for User Story 3

- [X] T052 [US3] Implement `plugins` and `plugin(id:)` resolver data mapping from `DatabaseManager` records in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T053 [US3] Ensure manifest metadata exposure includes `openApiFragmentJson` and ABI info via `manifestJson` in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T054 [US3] Update GraphQL plugin API contract examples and error catalog for discovery endpoints in `specs/012-plugin-system-boost-dll/contracts/graphql-plugin-api.md`
- [X] T055 [US3] Make US3 tests green across plugin and auth resolver suites in `src/test/cpp/isched/isched_plugin_system_tests.cpp`

**Checkpoint**: US3 provides secure operational visibility into plugin registry state.

---

## Phase 6: User Story 4 - Plugin state survives server restarts (Priority: P2)

**Goal**: Restore persisted loaded plugins on startup with retry metadata and non-crashing failure behavior.

**Independent Test**: Load plugin, simulate restart, verify automatic restore; verify unload-state exclusion and retry metadata for failed restores.

### Tests for User Story 4

- [X] T056 [P] [US4] Add startup restore success tests for persisted loaded request/resolver plugins in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T057 [P] [US4] Add startup restore exclusion tests for records with `is_loaded = false` in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T058 [P] [US4] Add restore failure retry-metadata tests (`last_restore_error`, `restore_attempts`, `last_restore_attempt_at`) in `src/test/cpp/isched/isched_plugin_system_tests.cpp`

### Implementation for User Story 4

- [X] T059 [US4] Implement `PluginManager::restore_persisted_plugins()` reload loop and continue-on-error behavior in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T060 [US4] Implement restore-failure metadata persistence updates in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T061 [US4] Wire startup restore invocation during server bootstrap in `src/main/cpp/isched/backend/isched_Server.cpp`
- [X] T062 [US4] Make US4 tests green and stabilize restart fixtures in `src/test/cpp/isched/isched_plugin_system_tests.cpp`

**Checkpoint**: US4 satisfies durable plugin lifecycle and restore retry semantics.

---

## Phase 7: User Story 5 - Confidence through automated tests (Priority: P3)

**Goal**: Achieve required plugin-system coverage and confidence gates for regression safety.

**Independent Test**: `ctest --output-on-failure` passes plugin-related suites and coverage report shows >=80% line/branch on plugin-system files.

### Tests for User Story 5

- [X] T063 [P] [US5] Add `inject_loaded_plugin()` bypass-loader tests for request/resolver lifecycle in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T064 [P] [US5] Add edge-case tests (missing exports, duplicate manifest identity/path, invalid shared library load error, and ABI-major mismatch rejection with descriptive error and no registry side effects) in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T065 [P] [US5] Add `loadPlugin`/`unloadPlugin` authorization error-contract fail-fast tests proving no plugin read/mutation side effects in `src/test/cpp/isched/isched_gql_executor_auth_resolvers_tests.cpp`

### Implementation for User Story 5

- [X] T066 [US5] Update plugin test target fixtures/stubs and linking as needed in `src/test/cpp/isched/CMakeLists.txt`
- [X] T067 [US5] Execute plugin-focused regression suites and fix failures in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T068 [US5] Run coverage build and publish >=80% plugin-system coverage evidence in `specs/012-plugin-system-boost-dll/coverage-report.md`

**Checkpoint**: US5 enforces long-term automated quality gates.

---

## Phase 8: User Story 6 - Monitor plugin lifecycle changes in real time (Priority: P2)

**Goal**: Provide an admin-only GraphQL lifecycle subscription that emits exactly one canonical payload event for each successful load/unload transition, with no events on failed/non-transition paths.

**Independent Test**: Subscribe as `platform_admin`, execute successful and failing `loadPlugin`/`unloadPlugin` operations, verify exact event-count semantics, payload field/value mapping, and latency pass criteria.

### Tests for User Story 6

- [X] T076 [P] [US6] Add subscription integration test asserting exactly one `LOADED`/`UNLOADED` event per successful transition (SC-017) in `src/test/cpp/isched/isched_plugin_system_subscription_integration_tests.cpp`
- [X] T077 [P] [US6] Add negative-path integration tests asserting zero new lifecycle events for failed load validation, conflict rejection, unload-not-found, and idempotent already-unloaded operations (SC-018), and explicitly verify unauthorized `pluginLifecycleEvents` subscription attempts are rejected before stream establishment (FR-025) in `src/test/cpp/isched/isched_plugin_system_subscription_integration_tests.cpp`
- [X] T078 [P] [US6] Add payload acceptance tests asserting required fields (`eventType`, `pluginId`, `pluginKind`, `pluginName`, `libraryPath`, `occurredAt`) are non-empty and value-matched to affected plugin records (SC-018) in `src/test/cpp/isched/isched_plugin_system_subscription_integration_tests.cpp`
- [X] T079 [P] [US6] Add lifecycle latency acceptance harness measuring `(subscriber_received_ts - mutation_completed_ts)` across N>=20 successful transitions and asserting p95 <= 2000ms with >=95% trials within 2s (SC-017) in `src/test/cpp/isched/isched_plugin_system_subscription_integration_tests.cpp`

### Implementation for User Story 6

- [X] T080 [US6] Implement GraphQL schema additions for `PluginLifecycleEventType`, `PluginLifecycleEvent`, and `pluginLifecycleEvents` subscription field in `src/main/cpp/isched/backend/isched_builtin_server_schema.graphql`
- [X] T081 [US6] Implement FR-025 pre-stream RBAC enforcement (`require_roles(platform_admin)` before stream creation) in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T082 [US6] Implement lifecycle publish hook for successful load transitions (emit one `LOADED` after activation+persistence commit) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T083 [US6] Implement lifecycle publish hook for successful unload transitions (emit one `UNLOADED` after deactivation+persistence commit) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T084 [US6] Implement FR-024 lifecycle payload construction/mapping helper for `eventType`, `pluginId`, `pluginKind`, `pluginName`, `libraryPath`, and UTC `occurredAt` in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T085 [US6] Wire PluginManager lifecycle publisher to GraphQL subscription broadcaster and ensure failed/non-transition paths short-circuit without enqueueing events in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`

**Checkpoint**: US6 satisfies FR-023/FR-024/FR-025 and acceptance coverage for SC-017/SC-018.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Constitution deliverables, performance gating, final verification, and release-ready documentation.

- [X] T069 Complete feature threat model with abuse cases, mitigations, and residual risks in `specs/012-plugin-system-boost-dll/threat-model.md`
- [X] T070 Update project-level threat model with SP-012 plugin controls in `docs/security-threat-model.md`
- [X] T071 [P] Update implementation/verification runbook for FR-021/FR-022 plus FR-023/FR-024/FR-025 and SC-017/SC-018 lifecycle checks in `specs/012-plugin-system-boost-dll/quickstart.md`
- [X] T072 Execute post-change plugin performance benchmark runs and store metrics in `specs/012-plugin-system-boost-dll/benchmarks/post-change.json`
- [X] T073 Enforce performance gate (<=5% regression) and publish comparison analysis in `specs/012-plugin-system-boost-dll/benchmarks/perf-gate-report.md`
- [X] T074 Update requirement-to-task-and-test traceability (FR/SC matrix, including FR-025 and SC-017/SC-018) in `specs/012-plugin-system-boost-dll/requirements-traceability.md`
- [X] T075 Run full test regression (`ctest --output-on-failure`) and record evidence in `specs/012-plugin-system-boost-dll/validation-report.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: no dependencies.
- **Phase 2 (Foundational)**: depends on Phase 1; blocks all user stories.
- **Phase 3 (US1)**: depends on Phase 2.
- **Phase 4 (US2)**: depends on Phase 2; should start after T014-T016 and T029-T031 are available.
- **Phase 5 (US3)**: depends on Phase 2; independent of US4/US5.
- **Phase 6 (US4)**: depends on Phase 2 and lifecycle load/unload readiness from US1/US2.
- **Phase 7 (US5)**: depends on US1-US4 implementation being complete.
- **Phase 8 (US6)**: depends on Phase 2 and lifecycle transition implementation from US1/US2.
- **Phase 9 (Polish)**: depends on all user stories.

### User Story Dependency Graph

- **US1 (P1)** -> enables stable load/unload and path-security baseline.
- **US2 (P1)** -> extends runtime loading to resolver path with FR-021 flows.
- **US3 (P2)** -> can proceed after Foundational; operationally benefits from US1/US2 data.
- **US4 (P2)** -> depends on plugin lifecycle behavior from US1/US2.
- **US5 (P3)** -> depends on implementation completion of US1-US4.
- **US6 (P2)** -> depends on stable load/unload transitions (US1/US2) and GraphQL auth foundations (Phase 2).

Recommended completion order: **US1 -> US2 -> US3 -> US4 -> US6 -> US5**.

### Within Each User Story

- Add/extend tests first (or in parallel), but story is complete only when tests pass.
- Implement core lifecycle logic before integration wiring.
- Ensure rollback/no-partial-state semantics before marking load flows complete.

---

## Parallel Opportunities

- **Phase 1**: T003, T005, T006 can run in parallel.
- **Phase 2**: T012, T014, T017, T020 can run in parallel after T011 baseline schema work.
- **US1**: T021-T025 are parallel test tasks; T029-T031 are sequential implementation gates.
- **US2**: T035-T041 are parallel test tasks; T043-T046 are sequential FR-021 orchestration tasks.
- **US3**: T049-T051 can run in parallel.
- **US4**: T056-T058 can run in parallel.
- **US5**: T063-T065 can run in parallel.
- **US6**: T076-T079 can run in parallel; T081 must precede T085 stream establishment wiring.
- **Phase 9**: T071 and T074 can run in parallel with benchmark execution prep.

### Parallel Example: User Story 1

```bash
Task: T021 (request load/dispatch success test)
Task: T022 (route conflict tests)
Task: T023 (FR-022 path-policy tests)
Task: T025 (request crash containment test)
```

### Parallel Example: User Story 2

```bash
Task: T035 (schema-supplied success test)
Task: T036 (invalid SDL rejection test)
Task: T038 (empty-string no-schema success test)
Task: T039 (no-schema rejection tests)
```

### Parallel Example: User Story 3

```bash
Task: T049 (plugins query tests)
Task: T050 (plugin(id) query tests)
Task: T051 (discovery RBAC tests)
```

### Parallel Example: User Story 4

```bash
Task: T056 (restore success tests)
Task: T057 (is_loaded=false exclusion tests)
Task: T058 (restore retry metadata tests)
```

### Parallel Example: User Story 5

```bash
Task: T063 (inject_loaded_plugin tests)
Task: T064 (edge-case loader tests)
Task: T065 (auth fail-fast side-effect tests)
```

### Parallel Example: User Story 6

```bash
Task: T076 (exactly-one-event transition integration test)
Task: T077 (no-event negative-path integration tests)
Task: T078 (payload field/value mapping checks)
Task: T079 (2s p95 latency measurement harness)
```

---

## Implementation Strategy

### MVP First (US1)

1. Complete Phase 1 and Phase 2.
2. Complete US1 (Phase 3) including FR-022 validation gates and tests.
3. Validate with plugin-system test target before expanding to resolver flows.

### Incremental Delivery

1. Foundation complete -> deliver US1.
2. Add US2 FR-021 resolver paths.
3. Add US3 discovery APIs.
4. Add US4 restore durability.
5. Add US6 lifecycle subscription stream semantics and acceptance checks.
6. Add US5 coverage hardening.
7. Finish with security + performance + full regression in Phase 9.

---

## FR Traceability Matrix (FR-001 .. FR-025)

- **FR-001**: T007, T017
- **FR-002**: T008, T010, T053
- **FR-003**: T009, T026, T028
- **FR-004**: T009, T042, T047
- **FR-005**: T009, T026, T042
- **FR-006**: T022, T027
- **FR-007**: T039, T042, T045
- **FR-008**: T021, T028
- **FR-009**: T018, T034, T065
- **FR-010**: T017, T049, T050, T052
- **FR-011**: T011, T012, T013, T032, T060
- **FR-012**: T056, T057, T058, T059, T060, T061
- **FR-013**: T016, T063
- **FR-014**: T014, T015, T047
- **FR-015**: T012, T013, T052, T060
- **FR-016**: T063, T064, T067, T068
- **FR-017**: T019, T051, T065
- **FR-018**: T008, T010, T026, T042, T064
- **FR-019**: T008, T024, T026, T053
- **FR-020**: T025, T033, T041, T047
- **FR-021**: T035, T036, T037, T038, T039, T040, T043, T044, T045, T046
- **FR-022**: T023, T024, T029, T030, T031
- **FR-023**: T076, T079, T080, T082, T083, T085
- **FR-024**: T077, T078, T082, T083, T084, T085
- **FR-025**: T019, T077, T081, T085

### Success-Criteria Focus Mapping (partial)

- **SC-010**: T064
- **SC-013 / SC-014**: T035-T040, T043-T046
- **SC-015**: T023, T024, T029, T030, T031
- **SC-017**: T076, T079, T082, T083
- **SC-018**: T077, T078, T084, T085
- **Performance gate (<=5% regression)**: T001, T004, T072, T073
- **Constitution threat-model deliverables**: T069, T070
