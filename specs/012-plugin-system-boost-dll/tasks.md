# Tasks: SP-012 Plugin System (Boost.DLL) — Gap Closure

**Input**: Design documents from `/home/groby/dev/isched/specs/012-plugin-system-boost-dll/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/graphql-plugin-api.md, quickstart.md

**Scope note**: Core plugin system is already implemented. This task list covers only remaining gaps: FR-017, FR-018, FR-019, FR-006 template routes, FR-020 crash containment.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish a red/green baseline for the gap-closure work.

- [X] T001 Capture baseline plugin behavior and add FR-017..FR-020 gap TODO anchors in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)
- [X] T002 Confirm current plugin and auth test targets are green from `build/release` before changes (`ctest -R isched_plugin_system_tests`, `ctest -R isched_gql_executor_auth_resolvers_tests`)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared interface and type changes required before story-specific implementation.

- [X] T003 Add plugin ABI constants and `PluginManifest::abi_version` default in `src/main/cpp/isched/backend/isched_PluginSystem.hpp`
- [X] T004 Add `IResolverRegistrar` interface and update `ResolverPlugin` method signatures in `src/main/cpp/isched/backend/isched_PluginSystem.hpp`
- [X] T005 [P] Add/adjust resolver registrar type aliases and pass-through compatibility methods in `src/main/cpp/isched/backend/isched_GqlExecutor.hpp`
- [X] T006 Update `PluginManager` declarations for crash-unload helpers and non-const `dispatch_request` in `src/main/cpp/isched/backend/isched_PluginSystem.hpp`
- [X] T007 Update `StubResolverPlugin` signatures to `IResolverRegistrar&` in `src/test/cpp/isched/isched_plugin_system_tests.cpp`

**Checkpoint**: Shared ABI + registrar contracts are in place; user-story work can proceed.

---

## Phase 3: User Story 1 — Request plugin safety/compatibility gap closure (Priority: P1) 🎯 MVP

**Goal**: Request plugins enforce ABI + OpenAPI validation, support template route conflict/matching, and auto-unload on handler crash.

**Independent Test**: `ctest -R isched_plugin_system_tests` passes cases for T-ABI-01..05, T-OAI-01..05, T-RT-01..09, and T-CR-01..02.

### Tests for User Story 1

- [X] T008 [US1] Add failing ABI-validation tests (major mismatch, bad semver, valid minor/patch) in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)
- [X] T009 [US1] Add failing OpenAPI load-time validation tests (invalid JSON reject, empty accepted, `{}` accepted) in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)
- [X] T010 [US1] Add failing template route conflict/dispatch tests (overlap, segment count, method split, slash normalization) in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)
- [X] T011 [US1] Add failing request-plugin crash containment tests (HTTP 500 + auto-unload + route release) in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)

### Implementation for User Story 1

- [X] T012 [US1] Implement `extract_semver_major`, `validate_abi_version`, and `validate_openapi_fragment` helpers in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T013 [US1] Wire ABI/OpenAPI checks into `load_request_plugin()` and manifest JSON round-trip (`manifest_to_json` / `manifest_from_json`) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T014 [US1] Implement template-route helpers (`is_template_segment`, `split_path_segments`, `routes_overlap`, `route_matches_request`) and use them in conflict detection/dispatch in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T015 [US1] Implement request crash containment (`dispatch_request` try/catch, `crash_unload_request_plugin`, DB/state cleanup) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T016 [US1] Update plugin dispatch call path for non-const manager dispatch in `src/main/cpp/isched/backend/isched_Server.cpp`
- [X] T017 [US1] Make US1 tests pass in `src/test/cpp/isched/isched_plugin_system_tests.cpp` and verify with `ctest -R isched_plugin_system_tests`

**Checkpoint**: Request-plugin path satisfies FR-018, FR-019, FR-006(template), and FR-020(request side).

---

## Phase 4: User Story 2 — Resolver plugin crash containment and ABI enforcement (Priority: P1)

**Goal**: Resolver plugins register through crash-safe wrappers, auto-unload on resolver exception, and enforce ABI compatibility.

**Independent Test**: `ctest -R isched_plugin_system_tests` passes resolver crash tests and resolver ABI enforcement tests without uncaught exceptions.

### Tests for User Story 2

- [X] T018 [US2] Add failing resolver crash containment tests (GraphQL error node + auto-unload + idempotent crash unload) in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)
- [X] T019 [US2] Add failing resolver ABI validation and manifest `abi_version` JSON round-trip tests in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)

### Implementation for User Story 2

- [X] T020 [US2] Implement resolver-registrar adapter behavior needed by plugin loading paths in `src/main/cpp/isched/backend/isched_GqlExecutor.hpp`
- [X] T021 [US2] Implement `CrashSafeRegistrar` and `crash_unload_resolver_plugin` wiring in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T022 [US2] Update resolver load/unload flow to use `IResolverRegistrar` (`load_resolver_plugin`, rollback, unload path) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T023 [US2] Finalize resolver test stub and compile fixes in `src/test/cpp/isched/isched_plugin_system_tests.cpp`
- [X] T024 [US2] Make US2 tests pass and verify with `ctest -R isched_plugin_system_tests`

**Checkpoint**: Resolver-plugin path satisfies FR-020(resolver side) and FR-018 parity.

---

## Phase 5: User Story 3 — Secure plugin administration and discovery (Priority: P2)

**Goal**: Plugin GraphQL queries/mutations are restricted to `platform_admin` users.

**Independent Test**: Non-admin context receives authorization errors for `plugins`, `plugin`, `loadPlugin`, and `unloadPlugin`; admin context succeeds.

### Tests for User Story 3

- [X] T025 [US3] Add failing authorization tests for plugin query/mutation fields in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)
- [X] T026 [P] [US3] Add/adjust RBAC resolver tests for plugin fields in `src/test/cpp/isched/isched_gql_executor_auth_resolvers_tests.cpp` (target: `ctest -R isched_gql_executor_auth_resolvers_tests`)

### Implementation for User Story 3

- [X] T027 [US3] Add `require_roles` guards for `plugins`, `plugin`, `loadPlugin`, and `unloadPlugin` in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T028 [US3] Make US3 authorization tests pass and verify with `ctest -R isched_plugin_system_tests && ctest -R isched_gql_executor_auth_resolvers_tests`

**Checkpoint**: FR-017 enforcement is complete and contract-aligned.

---

## Phase 6: User Story 4 — Startup restore remains stable with new validations (Priority: P2)

**Goal**: Startup restore remains non-crashing and deterministic under new ABI/OpenAPI validation behavior.

**Independent Test**: Restore path handles invalid or missing persisted plugin libraries without crashing; valid entries still restore.

### Tests for User Story 4

- [X] T029 [US4] Add restore regression coverage for validation-era edge cases in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)

### Implementation for User Story 4

- [X] T030 [US4] Harden restore error-path logging/guard behavior (if required by tests) in `src/main/cpp/isched/backend/isched_PluginSystem.cpp`
- [X] T031 [US4] Make US4 regression tests pass and verify with `ctest -R isched_plugin_system_tests`

**Checkpoint**: Startup restore behavior remains safe after gap closure.

---

## Phase 7: User Story 5 — Confidence through automated test coverage (Priority: P3)

**Goal**: Complete and validate expanded plugin-system coverage for all gap areas.

**Independent Test**: Full plugin test matrix passes and coverage workflow can be executed with documented commands.

### Tests for User Story 5

- [X] T032 [US5] Refactor/update legacy OpenAPI silent-skip test to align with load-time validation + inject path behavior in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)
- [X] T033 [US5] Add final assertion coverage for all planned cases (T-SEC, T-ABI, T-OAI, T-RT, T-CR) in `src/test/cpp/isched/isched_plugin_system_tests.cpp` (target: `ctest -R isched_plugin_system_tests`)

### Implementation for User Story 5

- [X] T034 [US5] Ensure plugin test target declaration remains aligned for execution in `src/test/cpp/isched/CMakeLists.txt`
- [X] T035 [US5] Run and stabilize plugin-focused regression gate from `build/release` using `ctest -R isched_plugin_system_tests --output-on-failure`

**Checkpoint**: Automated confidence gate for plugin gap-closure work is complete.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Final security/documentation and end-to-end validation.

- [X] T036 Update SP-012 threat model summary section in `docs/security-threat-model.md`
- [X] T037 [P] Update implementation/verification guidance for gap closure in `specs/012-plugin-system-boost-dll/quickstart.md`
- [X] T038 Run full release validation from `build/release` with `ctest --output-on-failure`

---

## Dependencies & Execution Order

### Phase Dependencies

- Phase 1 → Phase 2 → Phases 3–7 → Phase 8
- Phase 2 blocks all user stories because it introduces shared ABI/registrar interfaces.

### User Story Dependencies

- **US1 (P1)**: starts after Phase 2.
- **US2 (P1)**: starts after Phase 2; depends on shared registrar contract from Phase 2.
- **US3 (P2)**: can start after Phase 2; safest after US1/US2 compile stabilization.
- **US4 (P2)**: after US1/US2 because restore behavior depends on new validation logic.
- **US5 (P3)**: after US1–US4 implementation is stable.

Recommended completion order: **US1 → US2 → US3 → US4 → US5**.

---

## Parallel Opportunities

- **Foundational**: T005 can run in parallel with T006 (different files).
- **US3**: T026 can run in parallel with T027.
- **Polish**: T037 can run in parallel with T036.

### Parallel Example: US3

```bash
# Parallelizable once Phase 2 is complete:
Task: T026 (src/test/cpp/isched/isched_gql_executor_auth_resolvers_tests.cpp)
Task: T027 (src/main/cpp/isched/backend/isched_GqlExecutor.cpp)
```

### Parallel Example: Polish

```bash
Task: T036 (docs/security-threat-model.md)
Task: T037 (specs/012-plugin-system-boost-dll/quickstart.md)
```

---

## Implementation Strategy

### MVP First (US1)

1. Finish Phase 1 and Phase 2.
2. Deliver US1 completely (tests + implementation).
3. Validate with `ctest -R isched_plugin_system_tests --output-on-failure`.

### Incremental Delivery

1. Add US2 crash containment.
2. Add US3 authorization guards.
3. Add US4 restore regressions.
4. Finish US5 confidence/coverage tasks.
5. Close with Phase 8 polish and full test pass.

---

## Validation

```bash
cd /home/groby/dev/isched
cmake --preset conan-release -S . -B build/release
cmake --build build/release --parallel
cd build/release
ctest -R isched_plugin_system_tests --output-on-failure
ctest -R isched_gql_executor_auth_resolvers_tests --output-on-failure
ctest --output-on-failure
```
