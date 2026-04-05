# Tasks: Isched WebUI (`004-add-isched-webui`)

**Input**: Design documents from `/specs/004-add-isched-webui/`  
**Prerequisites**: `plan.md` (required), `spec.md` (required), `research.md`, `data-model.md`, `contracts/webui-graphql-contract.md`, `quickstart.md`

**Tests**: Required by specification for each story (contract, integration, UI, and e2e where applicable).

**Organization**: Tasks are grouped by phase and user story so each story remains independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- `[P]` indicates parallelizable work (different files, no unfinished prerequisite dependency).
- `[Story]` labels (`[US1]`..`[US4]`) are used only in user story phases.
- Every task includes an exact file path.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish baseline Angular/WebUI + backend embedding + GraphQL proxy conventions.

- [X] T001 Align WebUI scripts/dependencies for Angular 21 + Playwright + Tailwind/DaisyUI in `src/ui/package.json`
- [X] T002 Configure Tailwind and DaisyUI scanning/theme baseline in `src/ui/tailwind.config.js`
- [X] T003 [P] Enforce standalone + strict app bootstrap defaults in `src/ui/src/app/app.config.ts`
- [X] T004 [P] Configure dev proxy for `/graphql` HTTP + WebSocket in `src/ui/proxy.conf.json`
- [X] T005 [P] Ensure GraphQL client uses same-origin `/graphql` path only in `src/ui/src/app/services/graphql.service.ts`
- [X] T006 Add/verify embedded WebUI build and static asset integration in `CMakeLists.txt`
- [X] T007 Add/verify SPA fallback static route serving behavior in `src/main/cpp/isched/backend/isched_Server.cpp`
- [X] T007b Add/verify canonical WebUI routing redirects (`GET /` and `GET /graphql` -> `/isched`) and coverage in `src/main/cpp/isched/backend/isched_Server.cpp`, `src/test/cpp/integration/test_webui_embedded_serving.cpp`, and `src/test/cpp/integration/test_admin_ui.cpp`
- [X] T007a [P] Enforce Angular unit tests as a required gate in the CMake/CI npm build step so the build fails when `ng test` is not green (FR-013b) in `CMakeLists.txt`
- [X] T068 [P] Add integration coverage for embedded route-contract distinctions (missing-asset `404` JSON envelope vs SPA fallback route behavior) in `src/test/cpp/integration/test_webui_embedded_serving.cpp`
- [X] T069 [P] Add integration coverage for embedded static asset hardening/cache contract (`CSP`, `X-Content-Type-Options`, `X-Frame-Options`, `ETag`/`304`) in `src/test/cpp/integration/test_webui_embedded_serving.cpp`
- [X] T070 Add backend embedded-serving contract updates for startup Admin UI log line, missing-asset JSON `404` envelopes, SPA-route fallback boundaries, security headers, and conditional `ETag`/`304` handling in `src/main/cpp/isched/backend/isched_Server.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Security, auth/session, error contracts, org-scope, and shared testing primitives needed by all stories.

**CRITICAL**: Complete this phase before starting user stories.

- [X] T008 Define stable GraphQL error-code normalization (`VALIDATION_FAILED`, `UNAUTHENTICATED`, `FORBIDDEN`, `CSRF_FAILED`, `CONTEXT_MISMATCH`, `CONFLICT`, `TRANSIENT_NETWORK`) in `src/ui/src/app/services/graphql.service.ts`
- [X] T009 [P] Implement cookie-session auth primitives without token exposure in `src/ui/src/app/services/auth.service.ts`
- [X] T010 [P] Implement CSRF token/header mutation interceptor in `src/ui/src/app/interceptors/auth.interceptor.ts`
- [X] T011 [P] Enforce backend CSRF double-submit + strict Origin/Referer validation in `src/main/cpp/isched/backend/isched_AuthenticationMiddleware.cpp`
- [X] T012 Implement explicit selected-organization context guard service in `src/ui/src/app/services/org-context.service.ts`
- [X] T013 [P] Enforce `CONTEXT_MISMATCH` for stale/mismatched org-scoped mutations in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T014 [P] Add shared GraphQL auth/CSRF test helper API in `src/test/cpp/isched/isched_graphql_test_helpers.hpp`
- [X] T015 Refactor mutation-focused backend integration tests to use shared helper in `src/test/cpp/integration/`
- [X] T016 [P] Add foundational backend security tests for GraphQL-only + CSRF/origin protections in `src/test/cpp/integration/test_webui_security_foundation.cpp`
- [X] T017 [P] Add frontend auth-storage tests proving no JWT persistence in browser storage in `src/ui/src/app/services/auth.service.spec.ts`
- [X] T018 [P] Add JWT leakage regression tests for backend/frontend logs and GraphQL error payloads in `src/test/cpp/integration/test_webui_jwt_leakage.cpp`
- [X] T018a [P] Add integration tests for temporary auth lockout (5 failed attempts in 15 minutes, 15-minute auto-unlock, deterministic lockout error mapping) in `src/test/cpp/integration/test_auth_lockout_policy.cpp`
- [X] T018b Implement backend failed-attempt rolling-window tracking and temporary lockout enforcement in `src/main/cpp/isched/backend/isched_AuthenticationMiddleware.cpp`
- [X] T018c [P] Implement WebUI lockout error surfacing with retry timing guidance for auth flows in `src/ui/src/app/services/auth.service.ts`
- [X] T071 [P] Add integration tests for bootstrap/auth rate-limit configuration-chain precedence (bootstrap/auth-specific override -> feature default -> global fallback) in `src/test/cpp/integration/test_auth_lockout_policy.cpp`
- [X] T072 Implement bootstrap/auth rate-limit configuration-chain resolution and diagnostics logging in `src/main/cpp/isched/backend/isched_AuthenticationMiddleware.cpp` and `src/main/cpp/isched/shared/isched_Configuration.cpp`

**Checkpoint**: Shared security and scope controls are complete; stories can proceed independently.

---

## Phase 3: User Story 1 - First-Time Platform Bootstrap (Priority: P1) 🎯 MVP

**Goal**: Provide one-time bootstrap that initializes platform once, authenticates the initial admin, and blocks repeat bootstrap.

**Independent Test**: In a clean data directory, complete bootstrap once via UI, verify authenticated state, and confirm second bootstrap is denied.

### Tests for User Story 1

- [X] T019 [P] [US1] Add GraphQL contract tests for `BootstrapStatus` and `CompleteBootstrap` in `src/test/cpp/integration/test_webui_bootstrap_contract.cpp`
- [X] T020 [P] [US1] Add backend integration tests for bootstrap repeat-denial and redirect semantics in `src/test/cpp/integration/test_bootstrap_platform_admin.cpp`
- [X] T021 [P] [US1] Add Angular bootstrap page validation/error mapping tests in `src/ui/src/app/pages/bootstrap/bootstrap.page.spec.ts`
- [X] T022 [P] [US1] Add embedded-serving integration tests for static assets + SPA fallback + bootstrap route availability in `src/test/cpp/integration/test_webui_embedded_serving.cpp`
- [X] T023 [P] [US1] Add Playwright backend lifecycle harness with temporary `--data-dir` and `ISCHED_BUILD_DIR`-configurable binary path (FR-018, FR-018a) in `src/ui/e2e/global-setup.ts`
- [X] T024 [P] [US1] Add Playwright bootstrap e2e flow covering form visibility, full bootstrap→auto-login→sign-out→login round-trip in `src/ui/e2e/bootstrap.spec.ts`
- [X] T074 [P] [US1] Add post-bootstrap dashboard minimum-content e2e assertions for FR-020/SC-017 (health status badge + Organizations/Users/RBAC quicklinks) in `src/ui/e2e/bootstrap.spec.ts`
- [X] T076 [P] [US1] Add dashboard minimum-content unit tests that trace FR-020 link targets and health-card rendering in `src/ui/src/app/pages/dashboard/dashboard.page.spec.ts`
- [X] T077 [P] [US1] Add accessibility-focused tests for bootstrap/login keyboard-only navigation and required accessible labels for FR-021/SC-019 in `src/ui/src/app/pages/bootstrap/bootstrap.page.spec.ts` and `src/ui/src/app/pages/login/login.spec.ts`

### Implementation for User Story 1

- [X] T025 [P] [US1] Implement bootstrap GraphQL service operations in `src/ui/src/app/services/bootstrap.service.ts`
- [X] T026 [P] [US1] Implement standalone typed bootstrap form and submit flow in `src/ui/src/app/pages/bootstrap/bootstrap.page.ts`
- [X] T027 [US1] Implement bootstrap route gating and initialized redirect behavior in `src/ui/src/app/app.routes.ts`
- [X] T028 [US1] Implement `platformBootstrapStatus` and `completePlatformBootstrap` resolver logic in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T029 [US1] Implement bootstrap field/global error rendering for validation/auth/csrf/conflict states in `src/ui/src/app/pages/bootstrap/bootstrap.page.html`
- [X] T075 [US1] Implement FR-020 dashboard minimum content (system health summary card + Organizations/Users/RBAC quicklinks) in `src/ui/src/app/pages/dashboard/dashboard.page.ts` and `src/ui/src/app/pages/dashboard/dashboard.page.html`
- [X] T078 [US1] Implement FR-021 accessibility semantics for bootstrap/login interactive controls (programmatic labels, keyboard focus order, visible focus treatment) in `src/ui/src/app/pages/bootstrap/bootstrap.page.html` and `src/ui/src/app/pages/login/login.html`

**Checkpoint**: Bootstrap is complete, one-time, and independently testable.

---

## Phase 4: User Story 2 - Organization and User Administration (Priority: P1)

**Goal**: Deliver scoped organization/user create-edit-deactivate workflows with optimistic concurrency and per-org uniqueness.

**Independent Test**: Create organization, create/edit/deactivate/reactivate users in selected org, and verify out-of-scope and conflict cases are rejected.

### Tests for User Story 2

- [ ] T030 [P] [US2] Add GraphQL contract tests for organization/user list and mutation operations in `src/test/cpp/integration/test_webui_org_user_contract.cpp`
- [ ] T031 [P] [US2] Add integration tests for org scope checks, per-org `loginId` uniqueness, and `CONFLICT` revisions in `src/test/cpp/integration/test_user_management.cpp`
- [ ] T032 [P] [US2] Add server-side pagination/filter/sort validation tests for organizations/users in `src/test/cpp/integration/test_webui_server_side_paging.cpp`
- [ ] T033 [P] [US2] Add Angular organizations/users page tests for typed forms and deterministic error surfacing in `src/ui/src/app/pages/admin/organization-users.page.spec.ts`
- [X] T079 [P] [US2] Add FR-021/SC-019 accessibility tests for organization/user CRUD journeys (keyboard-only completion + accessible labels for interactive controls) in `src/ui/src/app/pages/admin/organizations.page.spec.ts` and `src/ui/src/app/pages/admin/users.page.spec.ts`

### Implementation for User Story 2

- [ ] T034 [P] [US2] Implement organization GraphQL query/mutation client with paging/filter/sort args in `src/ui/src/app/services/organization.service.ts`
- [ ] T035 [P] [US2] Implement user GraphQL query/mutation client with org context + expected revision in `src/ui/src/app/services/user.service.ts`
- [ ] T036 [P] [US2] Implement standalone organizations management page in `src/ui/src/app/pages/admin/organizations.page.ts`
- [ ] T037 [P] [US2] Implement standalone users management page with typed forms and deactivation/reactivation UX in `src/ui/src/app/pages/admin/users.page.ts`
- [ ] T038 [US2] Implement backend organization/user resolver authorization and scope enforcement in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [ ] T039 [US2] Implement database behavior for user deactivation preserving role assignments and effectiveness transitions in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [ ] T040 [US2] Implement context-switch dirty-edit confirmation UX for user/org pages in `src/ui/src/app/pages/admin/users.page.html`
- [X] T080 [US2] Implement FR-021 accessibility refinements for organization/user CRUD pages (labels, keyboard interaction, focus management) in `src/ui/src/app/pages/admin/organizations.page.html` and `src/ui/src/app/pages/admin/users.page.html`

**Checkpoint**: Organization and user administration works independently with strict scope and concurrency behavior.

---

## Phase 5: User Story 3 - RBAC Roles and Assignments (Priority: P1)

**Goal**: Deliver built-in role assignment, custom role CRUD (minus delete), and scoped role assignment management.

**Independent Test**: Assign built-in role, create/update custom role, assign/unassign role, and verify forbidden/out-of-scope/disabled-user behavior.

### Tests for User Story 3

- [ ] T041 [P] [US3] Add GraphQL contract tests for roles and role-assignment mutations in `src/test/cpp/integration/test_webui_rbac_contract.cpp`
- [ ] T042 [P] [US3] Add integration tests for RBAC scope denial, built-in immutability, and disabled-user effective assignment rules in `src/test/cpp/integration/test_webui_rbac_scope.cpp`
- [ ] T043 [P] [US3] Add server-side pagination/filter/sort validation tests for roles/assignments in `src/test/cpp/integration/test_webui_rbac_paging.cpp`
- [ ] T044 [P] [US3] Add Angular RBAC page tests for role forms and error-code surfacing in `src/ui/src/app/pages/admin/rbac.page.spec.ts`
- [X] T081 [P] [US3] Add FR-021/SC-019 accessibility tests for RBAC create/edit/assign flows (keyboard-only path + screen-reader labels) in `src/ui/src/app/pages/admin/rbac.page.spec.ts`

### Implementation for User Story 3

- [ ] T045 [P] [US3] Implement roles and assignments GraphQL client in `src/ui/src/app/services/rbac.service.ts`
- [ ] T046 [P] [US3] Implement standalone RBAC management page with typed role forms in `src/ui/src/app/pages/admin/rbac.page.ts`
- [ ] T047 [US3] Implement backend role and assignment resolver checks for scope, revision, and built-in immutability in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [ ] T048 [US3] Implement role-assignment persistence and effective-permission behavior in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [ ] T049 [US3] Implement RBAC error surfacing for `FORBIDDEN`/`CONFLICT`/`CSRF_FAILED`/`CONTEXT_MISMATCH` in `src/ui/src/app/pages/admin/rbac.page.html`
- [X] T082 [US3] Implement FR-021 accessibility refinements for RBAC management interactions in `src/ui/src/app/pages/admin/rbac.page.html`

**Checkpoint**: RBAC story is independently testable and scope-safe.

---

## Phase 6: User Story 4 - Local WebUI Development Workflow (Priority: P2)

**Goal**: Provide standalone local WebUI workflow with backend proxying and clear troubleshooting.

**Independent Test**: Follow docs to run backend + UI locally, verify `/graphql` HTTP/WS proxy behavior, and confirm clear diagnostics for misconfiguration.

### Tests for User Story 4

- [ ] T050 [P] [US4] Add backend integration tests for local dev proxy routing assumptions (`/graphql` HTTP + WS upgrade expectations) in `src/test/cpp/integration/test_webui_dev_proxy_contract.cpp`
- [ ] T051 [P] [US4] Add Angular diagnostics page tests for proxy/auth/csrf troubleshooting states in `src/ui/src/app/pages/dev/dev-proxy-health.page.spec.ts`

### Implementation for User Story 4

- [X] T052 [P] [US4] Implement standalone proxy health diagnostics page in `src/ui/src/app/pages/dev/dev-proxy-health.page.ts`
- [X] T053 [US4] Ensure backend CLI supports `--data-dir` and `--data-dir=<path>` for isolated local runs in `src/main/cpp/isched/backend/isched_srv_main.cpp`
- [X] T054 [US4] Ensure backend runtime honors configured work/data directory isolation for temporary test data in `src/main/cpp/isched/backend/isched_Server.cpp`
- [X] T055 [US4] Update WebUI local development instructions and proxy verification steps in `specs/004-add-isched-webui/quickstart.md`
- [X] T056 [US4] Update frontend developer guide for proxy-based GraphQL-only workflow in `src/ui/README.md`

**Checkpoint**: Local workflow implementation is in place; verification remains open until T050 and T051 are complete.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Complete non-functional gates, evidence, and release hardening.

- [ ] T057 [P] Update feature threat model for cookie auth, CSRF, org context, and embedded UI surfaces in `specs/004-add-isched-webui/threat-model.md`
- [ ] T058 [P] Update project-level security threat model with WebUI controls in `docs/security-threat-model.md`
- [ ] T067 Implement the single dedicated FR-010f release-blocking task: remove all `AuthService.accessToken` reads/writes, complete cookie-only session handling migration, and update/pass related Angular unit tests plus auth integration/e2e tests in `src/ui/src/app/services/auth.service.ts`
- [ ] T059 [P] Add performance/scalability harness for static assets + representative admin GraphQL flows (50 VUs, 5 minutes) in `src/test/cpp/integration/test_webui_performance.cpp`
- [ ] T060 Add scale dataset and assertions for 10,000 users / 1,000 roles pagination/filter/sort behavior in `src/test/cpp/integration/test_webui_scale_baseline.cpp`
- [ ] T061 Add audit event integration tests for success/failure admin mutations and required fields/immutability in `src/test/cpp/integration/test_webui_audit_events.cpp`
- [ ] T062 Add audit retention verification tests for 90-day queryability baseline in `src/test/cpp/integration/test_webui_audit_retention.cpp`
- [ ] T063 Capture SC-001 and SC-003 UAT timing/outcome evidence in `specs/004-add-isched-webui/evidence/sc001-sc003-uat.md`
- [ ] T064 Capture SC-011 and SC-011a performance/scale evidence in `specs/004-add-isched-webui/evidence/sc011-performance.md`
- [ ] T065 Capture SC-015 availability and RTO<=60 recovery drill procedure/evidence in `docs/operations/isched-webui-recovery.md`
- [ ] T066 Run full verification suite and record command outputs and pass criteria in `specs/004-add-isched-webui/quickstart.md`
- [X] T073 Capture focused validation evidence for startup Admin UI log contract, embedded-route `404`/SPA split, security headers, `ETag`/`304`, and rate-limit config chain precedence in `specs/004-add-isched-webui/evidence/`
- [ ] T083 Add FR-021/SC-019 WCAG 2.1 AA verification trace (keyboard-only journeys + interactive-control labeling evidence) in `specs/004-add-isched-webui/evidence/sc019-accessibility.md`
- [ ] T084 Add FR-020/SC-017 dashboard minimum-content verification trace (health badge + admin quicklinks) in `specs/004-add-isched-webui/evidence/sc017-dashboard-minimum-content.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- Phase 1 -> no dependencies.
- Phase 2 -> depends on Phase 1 and blocks all user stories.
- Phase 3 (US1), Phase 4 (US2), Phase 5 (US3), Phase 6 (US4) -> each depends on Phase 2.
- Phase 7 -> depends on completion of targeted story phases.

### User Story Dependency Graph

- US1 -> depends only on Phase 2 (MVP path).
- US2 -> depends only on Phase 2.
- US3 -> depends on Phase 2; can run in parallel with US2 if shared resolver work is coordinated.
- US4 -> depends only on Phase 2.

### Suggested Completion Order

- Baseline order: US1 -> US2 -> US3 -> US4.
- Parallel-capable order (team): US1 + US2 + US4 after Phase 2, then US3 once shared resolver/database changes are stable.

### Within-Story Rules

- Add/finish story tests before marking story complete.
- Implement service/data layer before route/page wiring.
- Implement backend enforcement before final UI success criteria sign-off.

---

## Parallel Execution Examples

### US1

```bash
# Parallel test tasks
T019 T020 T021 T022 T023 T024 T074 T076 T077

# Parallel implementation tasks after tests are in place
T025 T026 T075
```

### US2

```bash
# Parallel test tasks
T030 T031 T032 T033 T079

# Parallel implementation tasks
T034 T035 T036 T037
```

### US3

```bash
# Parallel test tasks
T041 T042 T043 T044 T081

# Parallel implementation tasks
T045 T046
```

### US4

```bash
# Parallel test/implementation start
T050 T051 T052

# Documentation tasks can run in parallel once runtime behavior is verified
T055 T056
```

---

## Implementation Strategy

### MVP First (US1)

1. Complete Phase 1 and Phase 2.
2. Complete US1 tasks (T019-T029, T074-T078).
3. Validate US1 independent test criteria in isolated `--data-dir` environment.
4. Demo/release MVP bootstrap path.

### Incremental Delivery

1. Deliver US2 for core org/user administration.
2. Deliver US3 for RBAC definitions and assignments.
3. Deliver US4 for developer workflow quality.
4. Complete Phase 7 evidence and operational readiness gates.

### Validation Checklist

- All tasks follow strict checklist format: `- [ ] T### [P?] [US?] Description with file path`.
- Story labels appear only in user story phases.
- Phase ordering and dependencies preserve independent story testing.

