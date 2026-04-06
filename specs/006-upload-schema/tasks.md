# Tasks: Tenant Admin Schema Upload

**Input**: Design documents from `/home/groby/dev/isched/specs/006-upload-schema/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/graphql-schema-upload.md`, `quickstart.md`

**Tests**: Automated coverage is required for SC-001 through SC-010 plus constitution-mandated performance/scalability verification on core upload/list/fetch paths with explicit measurable thresholds. Canonical deterministic null-on-miss behavior and controlled-restart durability must each have explicit automated verification tasks.

**Organization**: Tasks are grouped by user story so each story can be implemented and tested independently.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare build/test scaffolding for schema upload/list/fetch behavior and performance verification.

- [X] T001 Add/verify `test_graphql_schema_upload` target and `add_test` wiring in `src/test/cpp/isched/CMakeLists.txt`
- [X] T002 [P] Extend integration test scaffold for authenticated tenant contexts in `src/test/cpp/integration/test_graphql_schema_upload.cpp`
- [X] T003 [P] Add shared fixture helpers for schema document assertions in `src/test/cpp/isched/isched_gql_executor_tests.cpp`
- [X] T004 [P] Add reusable backend-boundary latency measurement helpers (steady-clock capture + percentile computation) in `src/test/cpp/integration/test_graphql_schema_upload.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Implement shared schema contract, persistence primitives, and validation/config plumbing used by all stories.

**⚠️ CRITICAL**: Complete this phase before user story work.

- [X] T005 Create/upgrade `schema_documents` table DDL (name/content/hash/timestamps/updatedBy) and tenant-local indexes in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T006 [P] Declare schema-document DB APIs (`insert`, `replace`, `list`, `getByName`) with metadata projections in `src/main/cpp/isched/backend/isched_DatabaseManager.hpp`
- [X] T007 Implement schema-document DB APIs with tenant-scoped SQL and atomic replace transaction boundaries in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T008 [P] Add GraphQL SDL contract for upload/list/fetch and list summary fields (`name`, `createdAt`, `updatedAt`, `updatedBy`) in `src/main/cpp/isched/backend/isched_builtin_server_schema.graphql`
- [X] T009 Implement shared name validator enforcing regex+length `[A-Za-z0-9._-]{1,128}` and case-sensitive semantics in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T010 Implement shared content validator with configurable max size lookup and default `1 MB` fallback in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T011 Add configuration plumbing/documented constant for active schema upload max bytes (deployment override + default) in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T012 Add reusable GraphQL error mapping helpers for `UNAUTHENTICATED`, `FORBIDDEN`, `VALIDATION_FAILED`, and `CONFLICT` in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`

**Checkpoint**: Foundation ready for independent story implementation.

---

## Phase 3: User Story 1 - Upload a New Schema Document (Priority: P1) 🎯 MVP

**Goal**: Allow authenticated `tenant_admin` users to upload schemas with explicit overwrite semantics, deterministic validation/error behavior, and measurable upload performance.

**Independent Test**: Tenant admin can upload a valid schema, duplicate without overwrite returns conflict, duplicate with overwrite succeeds, invalid name/content/SDL is rejected, non-admin and unauthenticated callers are denied, concurrent overwrite writes resolve to last successful commit atomically, and upload path meets backend latency threshold.

### Tests for User Story 1

- [X] T013 [P] [US1] Add executor tests for upload authn/authz and conflict-without-overwrite behavior in `src/test/cpp/isched/isched_gql_executor_tests.cpp`
- [X] T014 [P] [US1] Add integration tests for valid upload, non-admin rejection, and unauthenticated rejection in `src/test/cpp/integration/test_graphql_schema_upload.cpp`
- [X] T015 [US1] Add validation tests for regex+length name rules, case-sensitive duplicates (`Billing` vs `billing`), and max-size boundary/default-`1 MB` behavior in `src/test/cpp/isched/isched_gql_executor_tests.cpp`
- [X] T016 [US1] Add overwrite concurrency test proving atomic last-successful-write-wins commit ordering in `src/test/cpp/integration/test_graphql_schema_upload.cpp`
- [X] T017 [US1] Add upload performance/scalability test proving >=95% of valid upload requests (N>=100, each <= active size limit) complete with definitive GraphQL outcome in <2s at backend request boundary in `src/test/cpp/integration/test_graphql_schema_upload.cpp`

### Implementation for User Story 1

- [X] T018 [US1] Implement `uploadSchemaDocument(input:)` resolver auth checks (`tenant_admin` role + authenticated context) in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T019 [US1] Implement upload input validation pipeline (name regex+length, case-sensitive handling, content non-empty, active-size-cap, SDL parse) in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T020 [US1] Implement conflict detection and explicit overwrite gate (`overwrite=false` conflict, `overwrite=true` replacement) in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T021 [US1] Implement atomic overwrite commit path and ensure final persisted document follows last-successful-write-wins semantics in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T022 [US1] Persist and return upload metadata (`name`, `createdAt`, `updatedAt`, `updatedBy`) from resolver success payloads in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`

**Checkpoint**: US1 is independently functional and testable.

---

## Phase 4: User Story 2 - List Available Schemas (Priority: P2)

**Goal**: Allow authenticated tenant members to list tenant-local schema summaries only with fixed metadata contract and measurable list-path scalability.

**Independent Test**: List returns only tenant-local documents, returns empty list when none exist, each row has exactly `name`, `createdAt`, `updatedAt`, `updatedBy` without `sizeBytes`, and list path meets backend latency threshold under representative tenant dataset size.

### Tests for User Story 2

- [X] T023 [P] [US2] Add executor tests for `schemaDocuments` list behavior (empty, populated, tenant-isolated) in `src/test/cpp/isched/isched_gql_executor_tests.cpp`
- [X] T024 [US2] Add list contract tests asserting exact metadata fields and explicit absence of `sizeBytes` in `src/test/cpp/integration/test_graphql_schema_upload.cpp`
- [X] T025 [US2] Add list performance/scalability test proving >=95% of list queries against a tenant seeded with >=200 schemas complete in <500ms at backend request boundary in `src/test/cpp/integration/test_graphql_schema_upload.cpp`

### Implementation for User Story 2

- [X] T026 [US2] Implement `schemaDocuments` resolver using authenticated tenant scope only in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T027 [US2] Implement DB list query projection returning only `name`, `createdAt`, `updatedAt`, and `updatedBy` in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T028 [US2] Align GraphQL list type and resolver field mapping to exclude `sizeBytes` from list output in `src/main/cpp/isched/backend/isched_builtin_server_schema.graphql`

**Checkpoint**: US1 and US2 are independently functional and testable.

---

## Phase 5: User Story 3 - Retrieve a Specific Schema Document (Priority: P3)

**Goal**: Allow authenticated tenant members to fetch full schema content by exact name within tenant scope with canonical null-on-miss behavior, restart durability, and measurable fetch-path scalability.

**Independent Test**: Existing schema fetch returns full content, missing schema returns deterministic null-on-miss with no GraphQL error, lookups remain tenant-isolated, name matching is case-sensitive, uploaded schemas remain retrievable after controlled restart, and fetch path meets backend latency threshold under representative dataset size.

### Tests for User Story 3

- [X] T029 [P] [US3] Add executor tests for `schemaDocument(name:)` success, deterministic null-on-miss without GraphQL error, tenant isolation, and case-sensitive lookup behavior in `src/test/cpp/isched/isched_gql_executor_tests.cpp`
- [X] T030 [US3] Add integration tests for fetch-by-name success, canonical null-on-miss/no-error behavior, and cross-tenant non-resolution in `src/test/cpp/integration/test_graphql_schema_upload.cpp`
- [X] T031 [US3] Add dedicated controlled-restart durability integration test (upload -> controlled restart -> fetch same tenant/name/content) in `src/test/cpp/integration/test_graphql_schema_upload.cpp`
- [X] T032 [US3] Add fetch performance/scalability test proving >=95% of by-name fetch queries against a tenant with >=200 schemas complete in <300ms at backend request boundary in `src/test/cpp/integration/test_graphql_schema_upload.cpp`

### Implementation for User Story 3

- [X] T033 [US3] Implement `schemaDocument(name:)` resolver authn guard, shared-name validation usage, and tenant-scoped lookup in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T034 [US3] Implement exact-name lookup SQL path and canonical null-on-miss behavior in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T035 [US3] Ensure fetch response mapping returns full schema content plus metadata fields in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`

**Checkpoint**: All stories are independently functional and testable.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, threat-model updates, performance evidence capture, and full validation closure.

- [X] T036 [P] Create/update feature threat model with authz/isolation/overwrite-race controls in `specs/006-upload-schema/threat-model.md`
- [X] T037 [P] Add feature linkage and mitigations summary in `docs/security-threat-model.md`
- [X] T038 [P] Add changelog entry for schema upload/list/fetch behavior and clarified contracts in `CHANGELOG.md`
- [X] T039 Update feature quickstart with explicit regex/case/size/concurrency/null-on-miss/restart and upload-list-fetch performance-threshold verification notes in `specs/006-upload-schema/quickstart.md`
- [X] T040 Record regression and performance verification evidence (`ctest --output-on-failure` and core-path threshold results) in `specs/006-upload-schema/quickstart.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies.
- **Phase 2 (Foundational)**: Depends on Phase 1 and blocks all user stories.
- **Phase 3 (US1)**: Depends on Phase 2.
- **Phase 4 (US2)**: Depends on Phase 2; can proceed independently of US3.
- **Phase 5 (US3)**: Depends on Phase 2; can proceed independently of US2.
- **Phase 6 (Polish)**: Depends on completed story phases.

### User Story Dependencies

- **US1 (P1)**: Independent after foundational work; recommended MVP scope.
- **US2 (P2)**: Independent after foundational work; no hard dependency on US3.
- **US3 (P3)**: Independent after foundational work; no hard dependency on US2.

### Recommended Delivery Order

1. Complete Phase 1 and Phase 2.
2. Deliver **US1 (MVP)** with SC-001/SC-002/SC-003/SC-008/SC-009 and upload performance threshold coverage.
3. Deliver US2 with SC-004/SC-005/SC-010 and list scalability threshold coverage.
4. Deliver US3 with SC-004/SC-006, canonical null-on-miss, controlled-restart durability, and fetch scalability threshold coverage.
5. Execute Phase 6 documentation + full-regression/performance closeout.

---

## Parallel Opportunities

- **Setup**: T002, T003, and T004 can run in parallel after T001.
- **Foundational**: T006 and T008 can run in parallel after T005; T009 and T010 can run in parallel before T011/T012 integration.
- **US1**: T013 and T014 can run in parallel; T015/T016/T017 are distinct test concerns in shared file and can be split by section ownership.
- **US2**: T023 and T024 can run in parallel; T025 can run once seed-data helper portions of T024 are ready.
- **US3**: T029 and T030 can run in parallel; T031 and T032 can run after baseline fetch harness is in place.
- **Polish**: T036, T037, and T038 can run in parallel.

---

## Parallel Example: User Story 1

```bash
# Parallel test authoring
Task T013: src/test/cpp/isched/isched_gql_executor_tests.cpp
Task T014: src/test/cpp/integration/test_graphql_schema_upload.cpp

# Then extend behavior/performance coverage
Task T015: src/test/cpp/isched/isched_gql_executor_tests.cpp
Task T016: src/test/cpp/integration/test_graphql_schema_upload.cpp
Task T017: src/test/cpp/integration/test_graphql_schema_upload.cpp
```

## Parallel Example: User Story 2

```bash
# Parallel verification tasks
Task T023: src/test/cpp/isched/isched_gql_executor_tests.cpp
Task T024: src/test/cpp/integration/test_graphql_schema_upload.cpp

# Performance + implementation follow-up
Task T025: src/test/cpp/integration/test_graphql_schema_upload.cpp
Task T026 -> T027 -> T028
```

## Parallel Example: User Story 3

```bash
# Parallel verification tasks
Task T029: src/test/cpp/isched/isched_gql_executor_tests.cpp
Task T030: src/test/cpp/integration/test_graphql_schema_upload.cpp

# Durability + performance checks, then implementation
Task T031: src/test/cpp/integration/test_graphql_schema_upload.cpp
Task T032: src/test/cpp/integration/test_graphql_schema_upload.cpp
Task T033 -> T034 -> T035
```

---

## Implementation Strategy

### MVP First (US1)

1. Complete Phase 1 and Phase 2.
2. Complete US1 tasks T013-T022.
3. Validate clarified rules: name regex/length, case-sensitive uniqueness, size limit default `1 MB`, overwrite-race semantics, and upload latency threshold.
4. Commit MVP once tests are green.

### Incremental Delivery

1. US1 (upload) -> validate -> commit.
2. US2 (list contract + list scalability threshold) -> validate -> commit.
3. US3 (fetch exact name + null-on-miss + controlled restart + fetch scalability threshold) -> validate -> commit.
4. Polish/docs/perf evidence -> full `ctest --output-on-failure` -> commit.

### Team Parallelization

1. DB track: `src/main/cpp/isched/backend/isched_DatabaseManager.hpp` and `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`.
2. Resolver/schema track: `src/main/cpp/isched/backend/isched_GqlExecutor.cpp` and `src/main/cpp/isched/backend/isched_builtin_server_schema.graphql`.
3. Test/perf track: `src/test/cpp/isched/isched_gql_executor_tests.cpp`, `src/test/cpp/integration/test_graphql_schema_upload.cpp`, and `src/test/cpp/isched/CMakeLists.txt`.
