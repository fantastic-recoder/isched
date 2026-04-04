# Tasks: Isched WebUI (`004-add-isched-webui`)

**Input**: Design documents from `/specs/004-add-isched-webui/`
**Prerequisites**: `plan.md` (required), `spec.md` (required), `research.md`, `data-model.md`, `contracts/webui-graphql-contract.md`, `quickstart.md`

**Tests**: Required by feature spec and constitution gates. Include backend integration/contract/security tests and Angular unit/component tests for each user story.

**Organization**: Tasks are grouped by phase and user story to preserve independent implementation and validation.

## Format: `[ID] [P?] [Story] Description`

- `[P]`: Task is parallelizable (different files, no incomplete dependency)
- `[Story]`: User story label (`[US1]`, `[US2]`, `[US3]`, `[US4]`) for story phases only
- Every task includes an explicit repository file path

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish WebUI baseline, GraphQL-only client shape, and local proxy defaults.

- [X] T001 Add Angular WebUI dependency and scripts baseline in `src/ui/package.json`
- [X] T002 Configure Tailwind + DaisyUI theme tokens and content scan in `src/ui/tailwind.config.js`
- [X] T003 [P] Configure strict standalone/signal-first defaults for app bootstrap in `src/ui/src/app/app.config.ts`
- [X] T004 [P] Add local `/graphql` HTTP+WS dev proxy in `src/ui/proxy.conf.json`
- [X] T005 [P] Wire GraphQL endpoint via same-origin relative path (no hard-coded backend origin) in `src/ui/src/app/services/graphql.service.ts`
- [X] T006 Add WebUI build/serve integration target to root build workflow in `CMakeLists.txt` (T017a: GET /graphql serves embedded UI, POST /graphql for GraphQL operations)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Implement security and scope foundations required by all stories.

**CRITICAL**: No user story implementation starts before this phase is complete.

- [X] T007 Define shared GraphQL error-code mapping (`UNAUTHENTICATED`, `FORBIDDEN`, `VALIDATION_FAILED`, `CONFLICT`, `CSRF_FAILED`, `CONTEXT_MISMATCH`, `TRANSIENT_NETWORK`) in `src/ui/src/app/services/graphql.service.ts`
- [X] T008 [P] Implement cookie-session bootstrap + sign-out primitives in `src/ui/src/app/services/auth.service.ts`
- [X] T009 [P] Implement CSRF token acquisition/propagation interceptor for all mutations in `src/ui/src/app/interceptors/auth.interceptor.ts`
- [X] T010 [P] Enforce GraphQL mutation CSRF and Origin/Referer checks in backend request pipeline `src/main/cpp/isched/backend/isched_AuthenticationMiddleware.cpp`
- [X] T011 Implement explicit organization-context guard state for admin mutations in `src/ui/src/app/services/org-context.service.ts`
- [X] T012 [P] Add backend context-mismatch rejection path for org-scoped mutations in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T013 [P] Add foundational security integration tests for CSRF, origin validation, and GraphQL-only endpoint behavior in `src/test/cpp/integration/test_webui_security_foundation.cpp`
- [X] T014 Add foundational UI tests for cookie-auth opaque JWT handling and no storage persistence in `src/ui/src/app/services/auth.service.spec.ts`
- [ ] T014a [P] Add security verification tests ensuring JWT values never appear in frontend logs/errors, backend logs, GraphQL error payloads, or telemetry fixtures in `src/test/cpp/integration/test_webui_jwt_leakage.cpp`

**Checkpoint**: Security, org-scope guardrails, and GraphQL-only foundations are in place.

---

## Phase 3: User Story 1 - First-Time Platform Bootstrap (Priority: P1) 🎯 MVP

**Goal**: Deliver one-time bootstrap flow that initializes platform once and then blocks repeat bootstrap attempts.

**Independent Test**: Start with uninitialized backend, complete bootstrap via WebUI, verify authenticated session and repeat-bootstrap block.

### Tests for User Story 1

- [ ] T015 [P] [US1] Add GraphQL contract test for `BootstrapStatus` and `CompleteBootstrap` in `src/test/cpp/integration/test_webui_bootstrap_contract.cpp`
- [ ] T016 [P] [US1] Add backend integration test for second-attempt bootstrap denial and redirect semantics in `src/test/cpp/integration/test_bootstrap_platform_admin.cpp`
- [X] T017 [P] [US1] Add Angular bootstrap page unit/component validation tests in `src/ui/src/app/pages/bootstrap/bootstrap.page.spec.ts`
- [X] T017a [P] [US1] Add non-dev runtime integration tests for backend embedded WebUI static serving + SPA fallback + bootstrap unauthenticated exception behavior in `src/test/cpp/integration/test_webui_embedded_serving.cpp`

### Implementation for User Story 1

- [X] T018 [P] [US1] Implement bootstrap GraphQL operations client in `src/ui/src/app/services/bootstrap.service.ts`
- [X] T019 [P] [US1] Implement typed reactive bootstrap form (standalone + `@if`) in `src/ui/src/app/pages/bootstrap/bootstrap.page.ts`
- [X] T020 [US1] Add bootstrap route gating and initialized-state redirect logic in `src/ui/src/app/app.routes.ts`
- [ ] T021 [US1] Implement backend `platformBootstrapStatus` and `completePlatformBootstrap` resolver behavior updates in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T022 [US1] Add bootstrap UX error surfacing for validation/auth/csrf failures in `src/ui/src/app/pages/bootstrap/bootstrap.page.html`
- [ ] T022a [US1] Add deterministic GraphQL error-code to field/global alert mapping assertions for bootstrap flow (`VALIDATION_FAILED` vs global auth/csrf/network codes) in `src/ui/src/app/pages/bootstrap/bootstrap.page.spec.ts`

**Checkpoint**: US1 is independently functional and validates one-time bootstrap constraints.

---

## Phase 4: User Story 2 - Organization and User Administration (Priority: P1)

**Goal**: Enable scope-correct organization create/edit and user create/edit lifecycle management.

**Independent Test**: Create organization, create/edit users in selected org, validate uniqueness and out-of-scope rejections.

### Tests for User Story 2

- [ ] T023 [P] [US2] Add GraphQL contract tests for `Organizations`, `CreateOrganization`, `UpdateOrganization`, `Users`, `CreateUser`, `UpdateUser` in `src/test/cpp/integration/test_webui_org_user_contract.cpp`
- [ ] T024 [P] [US2] Add integration tests for org-scope boundaries and per-org `loginId` uniqueness in `src/test/cpp/integration/test_user_management.cpp`
- [X] T025 [P] [US2] Add Angular organization/user page tests (typed forms, field/global errors) in `src/ui/src/app/pages/admin/organization-users.page.spec.ts`

### Implementation for User Story 2

- [X] T026 [P] [US2] Implement organization GraphQL data service in `src/ui/src/app/services/organization.service.ts`
- [X] T027 [P] [US2] Implement user GraphQL data service (org-scoped mutations) in `src/ui/src/app/services/user.service.ts`
- [X] T028 [P] [US2] Implement standalone organization management page with signal-first state in `src/ui/src/app/pages/admin/organizations.page.ts`
- [X] T029 [P] [US2] Implement standalone user management page with typed reactive forms and context guard prompts in `src/ui/src/app/pages/admin/users.page.ts`
- [ ] T030 [US2] Implement backend authorization/scope checks for organization create/update and user create/update resolvers in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [ ] T031 [US2] Add deactivation/reactivation behavior preserving assignments and toggling effectiveness in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T032 [US2] Add context-switch dirty-form warning UX to block accidental cross-org writes in `src/ui/src/app/pages/admin/users.page.html`

**Checkpoint**: US2 independently supports org/user admin with strict scope boundaries.

---

## Phase 5: User Story 3 - RBAC Roles and Assignments (Priority: P1)

**Goal**: Manage built-in/custom roles and role assignments with strict authorization and org boundaries.

**Independent Test**: Assign built-in role, create custom role, assign/unassign role, verify forbidden behavior and effective permissions.

### Tests for User Story 3

- [ ] T033 [P] [US3] Add GraphQL contract tests for `Roles`, `CreateCustomRole`, and `AssignRole` in `src/test/cpp/integration/test_webui_rbac_contract.cpp`
- [ ] T034 [P] [US3] Add integration tests for out-of-scope RBAC denial and disabled-user assignment inactivity in `src/test/cpp/integration/test_webui_rbac_scope.cpp`
- [X] T035 [P] [US3] Add Angular RBAC page tests for built-in/custom role flows and authz error surfacing in `src/ui/src/app/pages/admin/rbac.page.spec.ts`

### Implementation for User Story 3

- [X] T036 [P] [US3] Implement role/assignment GraphQL service in `src/ui/src/app/services/rbac.service.ts`
- [X] T037 [P] [US3] Implement standalone RBAC management page (`@for` role lists, typed forms for custom roles) in `src/ui/src/app/pages/admin/rbac.page.ts`
- [ ] T038 [US3] Implement backend role create/assign resolver checks for built-in immutability and org scope in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [ ] T039 [US3] Implement role assignment persistence/effectiveness transitions in `src/main/cpp/isched/backend/isched_DatabaseManager.cpp`
- [X] T040 [US3] Add global/field error mapping for RBAC conflict/forbidden/csrf responses in `src/ui/src/app/pages/admin/rbac.page.html`

**Checkpoint**: US3 independently enforces RBAC creation/assignment and authorization boundaries.

---

## Phase 6: User Story 4 - Local WebUI Development Workflow (Priority: P2)

**Goal**: Provide reliable standalone Angular dev workflow using proxy to local backend `/graphql`.

**Independent Test**: Follow docs from clean machine context to run backend + UI, verify `/graphql` proxy for HTTP and WS, and validate diagnostics.

### Tests for User Story 4

- [ ] T041 [P] [US4] Add dev-proxy smoke integration test for `/graphql` HTTP and WS routing expectations in `src/test/cpp/integration/test_webui_dev_proxy_contract.cpp`
- [ ] T041a [P] [US4] Constrain WS proxy test scope to upgrade/routing diagnostics only (no functional subscription business assertions unless explicitly added by story scope) in `src/test/cpp/integration/test_webui_dev_proxy_contract.cpp`
- [X] T042 [P] [US4] Add Angular e2e-oriented proxy diagnostics test harness in `src/ui/src/app/pages/dev/dev-proxy-health.page.spec.ts`

### Implementation for User Story 4

- [X] T043 [P] [US4] Add standalone dev diagnostics page for proxy/auth/csrf troubleshooting in `src/ui/src/app/pages/dev/dev-proxy-health.page.ts`
- [X] T044 [US4] Document backend-in-background + Angular proxy startup/verification workflow in `specs/004-add-isched-webui/quickstart.md`
- [X] T045 [US4] Add WebUI README section for local proxy workflow and no hard-coded origin rule in `src/ui/README.md`

**Checkpoint**: US4 independently validates local development proxy workflow.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Complete constitution gates, hardening, and release-ready verification.

- [X] T046 [P] Add feature threat model (cookie auth, CSRF, org-scope, proxy attack surface) in `specs/004-add-isched-webui/threat-model.md`
- [X] T047 [P] Update project-level threat model summary for WebUI additions in `docs/security-threat-model.md`
- [X] T048 Run end-to-end quickstart validation and capture evidence updates in `specs/004-add-isched-webui/quickstart.md`
- [X] T049 [P] Add final Angular route/component conformance check for standalone + modern template syntax in `src/ui/src/app/app.routes.ts`
- [ ] T050 Execute and record non-performance verification suite evidence (`ctest`, Angular tests, embedded-serving integration, JWT-leakage checks), then update command evidence notes in `specs/004-add-isched-webui/quickstart.md`
- [ ] T051 [P] Add performance/scalability integration harness for embedded static asset serving and representative admin GraphQL flows (50 concurrent virtual users for 5 minutes) in `src/test/cpp/integration/test_webui_performance.cpp`
- [ ] T052 Execute the T051 harness and capture SC-011 measurable performance evidence (p95 latencies and non-intentional error-rate threshold checks) in `specs/004-add-isched-webui/quickstart.md`
- [ ] T052a Capture SC-001 and SC-003 measurable UAT evidence using the defined sample protocols and thresholds, and record run data in `specs/004-add-isched-webui/evidence/sc001-sc003-uat.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- Phase 1 (Setup) -> starts immediately.
- Phase 2 (Foundational) -> depends on Phase 1 and blocks all user stories.
- Phase 3 (US1), Phase 4 (US2), Phase 5 (US3), Phase 6 (US4) -> each depends on Phase 2.
- Phase 7 (Polish) -> depends on completion of Phases 3-6.

### User Story Dependencies

- **US1 (P1, MVP)**: Depends only on Phase 2.
- **US2 (P1)**: Depends on Phase 2; can run parallel with US1 after shared foundation.
- **US3 (P1)**: Depends on Phase 2 and benefits from US2 user-management entities but remains independently testable.
- **US4 (P2)**: Depends on Phase 2; can run parallel with US1-US3.

### Critical Path

- `T001 -> T006 -> T010 -> T011 -> T012 -> T017a -> T021 -> T030 -> T038 -> T046 -> T048 -> T050`
- This path covers setup, security foundations, bootstrap enablement, org/user scope enforcement, RBAC enforcement, and mandatory threat-model completion.

### Within-Story Execution Rules

- For each story, complete tests and implementation tasks before story sign-off.
- Maintain order: service/model tasks before UI wiring; backend enforcement before final UX completion.
- Story is complete only when its independent test criteria pass.

---

## Parallel Execution Examples

### US1

```bash
Task T015 + T016 + T017 + T017a in parallel, then T018 + T019 in parallel, then T020/T021/T022/T022a sequential finish.
```

### US2

```bash
Task T023 + T024 + T025 in parallel, then T026 + T027 + T028 + T029 in parallel, then T030/T031/T032.
```

### US3

```bash
Task T033 + T034 + T035 in parallel, then T036 + T037 in parallel, then T038/T039/T040.
```

### US4

```bash
Task T041 + T041a + T042 in parallel, then T043, then T044 + T045 in parallel.
```

---

## Implementation Strategy

### MVP First (US1)

1. Complete Phase 1 and Phase 2.
2. Deliver Phase 3 (US1) end-to-end.
3. Validate bootstrap independent test and security gates.
4. Demo/deploy MVP increment.

### Incremental Delivery

1. Add US2 for org/user admin boundaries.
2. Add US3 for RBAC definitions and assignments.
3. Add US4 for developer proxy workflow robustness.
4. Finish Phase 7 cross-cutting hardening and threat-model updates.

### Constitution Gate Traceability

- **GraphQL-only**: T005, T010, T013, T023, T033, T041
- **Angular conventions**: T003, T019, T028, T029, T037, T049
- **Cookie auth + CSRF**: T008, T009, T010, T013, T014, T014a, T046, T050
- **Org scope boundaries**: T011, T012, T024, T030, T038
- **Proxy dev flow**: T004, T041, T041a, T043, T044, T045
- **Embedded WebUI serving**: T017a, T050
- **Perf/scalability evidence**: T051, T052
- **SC-001/SC-003 UAT evidence**: T052a
- **Threat-model updates**: T046, T047

