# Tasks: RATE_LIMITED + Auth Bootstrap Consistency

**Input**: Design documents from `/specs/005-rate-limited-auth-bootstrap/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/graphql-auth-bootstrap-consistency.md`

**Tests**: Automated coverage is required by `FR-010`; include backend integration, Angular unit tests, and Playwright E2E tasks before story completion.

**Organization**: Tasks are grouped by user story for independent implementation and validation.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Align feature-specific validation docs and local run surfaces before code changes.

- [X] T001 Refresh feature validation matrix and focused run steps in `specs/005-rate-limited-auth-bootstrap/quickstart.md`
- [X] T002 [P] Align GraphQL auth/bootstrap error contract examples in `specs/005-rate-limited-auth-bootstrap/contracts/graphql-auth-bootstrap-consistency.md`
- [X] T003 [P] Verify `/graphql` HTTP+WS proxy routing assumptions for this feature in `src/ui/proxy.conf.json`
- [X] T004 [P] Add/adjust focused auth/bootstrap verification scripts in `src/ui/package.json`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Build shared state and contract normalization used by all stories.

**CRITICAL**: Complete this phase before starting user story implementation.

- [X] T005 Create shared auth/bootstrap domain types (`AuthAttemptOutcome`, `UserFacingAlert`, `SessionBootstrapState`, `BootstrapEligibilityState`, `AuthFlowFlightState`) in `src/ui/src/app/services/auth-bootstrap.models.ts`
- [X] T006 [P] Add GraphQL error normalization utilities (including `extensions.code` and `extensions.retryAfterMs`) in `src/ui/src/app/services/graphql.service.ts`
- [X] T007 [P] Add deterministic alert-mapping helpers for auth/bootstrap categories in `src/ui/src/app/services/auth-alert.mapper.ts`
- [X] T008 Implement startup session/bootstrap state store (signal-backed) in `src/ui/src/app/services/session-bootstrap-state.service.ts`
- [X] T009 Integrate startup bootstrap-first route resolution orchestration in `src/ui/src/app/app.routes.ts`
- [X] T010 Implement one-time first-guard session revalidation state in `src/ui/src/app/guards/auth.guard.ts`

**Checkpoint**: Shared frontend/backend contract handling is in place; user stories can proceed.

---

## Phase 3: User Story 1 - Deterministic Lockout Guidance (Priority: P1) 🎯 MVP

**Goal**: Deliver deterministic `RATE_LIMITED` handling with actionable retry guidance for all auth lockout outcomes.

**Independent Test**: Trigger repeated failed sign-ins and verify `RATE_LIMITED` always maps to dedicated UI guidance, with and without retry metadata.

### Tests for User Story 1

- [X] T011 [P] [US1] Extend lockout integration assertions for deterministic `RATE_LIMITED` envelopes in `src/test/cpp/integration/test_rate_limiting.cpp`
- [X] T012 [P] [US1] Add Angular auth service tests for metadata-aware and fallback lockout guidance in `src/ui/src/app/services/auth.service.spec.ts`
- [X] T013 [P] [US1] Add lockout UX regression coverage for deterministic guidance in `src/ui/e2e/rate-limiting.spec.ts`

### Implementation for User Story 1

- [X] T014 [US1] Implement `RATE_LIMITED`-specific auth outcome mapping in `src/ui/src/app/services/auth.service.ts`
- [X] T015 [US1] Add dedicated lockout alert state and submit-result handling in `src/ui/src/app/pages/login/login.ts`
- [X] T016 [US1] Render deterministic lockout guidance (retry metadata + fallback copy) in `src/ui/src/app/pages/login/login.html`
- [X] T017 [US1] Ensure backend login resolver emits canonical lockout code/metadata consistently in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`

**Checkpoint**: User Story 1 is independently testable and delivers deterministic operator lockout guidance.

---

## Phase 4: User Story 2 - Stable Session Bootstrap on App Start (Priority: P2)

**Goal**: Make startup routing deterministic across seed/non-seed and session-valid/session-invalid combinations.

**Independent Test**: Run startup permutations and verify first route correctness, including bootstrap precedence and first-guard revalidation behavior.

### Tests for User Story 2

- [X] T018 [P] [US2] Add startup routing permutation coverage (seed/session matrix) in `src/ui/src/app/app.spec.ts`
- [X] T019 [P] [US2] Add one-time guard revalidation behavior tests in `src/ui/src/app/guards/auth.guard.spec.ts`
- [X] T020 [P] [US2] Extend backend seed-mode precedence checks for startup consistency in `src/test/cpp/integration/test_seed_mode.cpp`

### Implementation for User Story 2

- [X] T021 [US2] Implement app-init bootstrap eligibility resolution before auth destination in `src/ui/src/app/app.ts`
- [X] T022 [US2] Enforce bootstrap-first gating and deterministic login fallback in `src/ui/src/app/app.routes.ts`
- [X] T023 [US2] Implement first guarded-navigation revalidation and redirect-on-invalid behavior in `src/ui/src/app/guards/auth.guard.ts`
- [X] T024 [US2] Align bootstrap status query usage and state propagation in `src/ui/src/app/services/bootstrap.service.ts`
- [X] T025 [US2] Ensure sign-out clears all in-memory auth/session indicators for subsequent guards in `src/ui/src/app/services/auth.service.ts`

**Checkpoint**: User Stories 1 and 2 both work independently with deterministic startup routing behavior.

---

## Phase 5: User Story 3 - Predictable Bootstrap-to-Auth Transition (Priority: P3)

**Goal**: Ensure bootstrap completion, immediate auth handoff, and failure recovery stay deterministic with single-flight suppression.

**Independent Test**: Complete bootstrap in clean environments and verify success path to dashboard, failure fallback to login, and duplicate-submit suppression.

### Tests for User Story 3

- [ ] T026 [P] [US3] Add bootstrap completion success/failure transition tests in `src/ui/src/app/pages/bootstrap/bootstrap.page.spec.ts`
- [ ] T027 [P] [US3] Add duplicate-submit single-flight tests for sign-in interactions in `src/ui/src/app/pages/login/login.spec.ts`
- [ ] T028 [P] [US3] Add E2E coverage for bootstrap handoff and bootstrap-unavailable redirect notice in `src/ui/e2e/bootstrap.spec.ts`
- [ ] T029 [P] [US3] Add backend bootstrap-unavailable behavior regression checks in `src/test/cpp/integration/test_bootstrap_platform_admin.cpp`

### Implementation for User Story 3

- [ ] T030 [US3] Implement single-flight suppression for repeated sign-in submissions in `src/ui/src/app/pages/login/login.ts`
- [ ] T031 [US3] Implement single-flight suppression for bootstrap completion submits in `src/ui/src/app/pages/bootstrap/bootstrap.page.ts`
- [ ] T032 [US3] Implement bootstrap success-to-auth chaining with deterministic login fallback in `src/ui/src/app/pages/bootstrap/bootstrap.page.ts`
- [ ] T033 [US3] Add immediate bootstrap-unavailable redirect and notice handoff path in `src/ui/src/app/pages/bootstrap/bootstrap.page.ts`
- [ ] T034 [US3] Render bootstrap-unavailable and recovery notices in `src/ui/src/app/pages/bootstrap/bootstrap.page.html`

**Checkpoint**: All three user stories are independently functional and verifiable.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final hardening, documentation, and full-regression readiness.

- [ ] T035 [P] Update feature threat model with final mitigations/evidence in `specs/005-rate-limited-auth-bootstrap/threat-model.md`
- [ ] T036 [P] Update project-level security summary for this feature in `docs/security-threat-model.md`
- [ ] T037 [P] Add feature completion note and validation outcomes in `CHANGELOG.md`
- [ ] T038 Run/record full feature regression command matrix in `specs/005-rate-limited-auth-bootstrap/quickstart.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- Phase 1 (Setup): no dependencies.
- Phase 2 (Foundational): depends on Phase 1; blocks all story work.
- Phase 3 (US1): depends on Phase 2; MVP scope.
- Phase 4 (US2): depends on Phase 2; can run in parallel with US1 after foundation, but priority order is US1 then US2.
- Phase 5 (US3): depends on Phase 2; can run in parallel with US1/US2 after foundation, but priority order is US1 then US2 then US3.
- Phase 6 (Polish): depends on completion of selected user stories.

### User Story Dependencies

- US1 (P1): no dependency on other stories after foundational work.
- US2 (P2): no hard dependency on US1, but shares auth/bootstrap state infrastructure from Phase 2.
- US3 (P3): no hard dependency on US1/US2, but reuses Phase 2 state and routing infrastructure.

### Within Each User Story

- Add/extend automated tests before marking story complete.
- Implement contract/state mapping before UI rendering updates.
- Complete service/guard logic before E2E stabilization and sign-off.

---

## Parallel Opportunities

- Setup: T002, T003, T004 can run in parallel.
- Foundational: T006 and T007 can run in parallel; T008-T010 follow after shared helpers.
- US1: T011, T012, T013 can run in parallel; T014 and T017 can run in parallel on frontend/backend.
- US2: T018, T019, T020 can run in parallel; T022 and T024 can run in parallel once T021 is in place.
- US3: T026, T027, T028, T029 can run in parallel; T030 and T031 can run in parallel.
- Polish: T035, T036, T037 can run in parallel before T038 final validation pass.

---

## Parallel Example: User Story 1

```bash
# Parallel US1 verification tasks
Task: "T011 [US1] Extend lockout integration assertions in src/test/cpp/integration/test_rate_limiting.cpp"
Task: "T012 [US1] Add Angular auth service tests in src/ui/src/app/services/auth.service.spec.ts"
Task: "T013 [US1] Add E2E lockout guidance coverage in src/ui/e2e/rate-limiting.spec.ts"

# Parallel US1 implementation split
Task: "T014 [US1] Implement RATE_LIMITED mapping in src/ui/src/app/services/auth.service.ts"
Task: "T017 [US1] Normalize backend lockout envelope in src/main/cpp/isched/backend/isched_GqlExecutor.cpp"
```

## Parallel Example: User Story 2

```bash
# Parallel US2 test development
Task: "T018 [US2] Startup routing matrix tests in src/ui/src/app/app.spec.ts"
Task: "T019 [US2] Guard revalidation tests in src/ui/src/app/guards/auth.guard.spec.ts"
Task: "T020 [US2] Seed-mode precedence tests in src/test/cpp/integration/test_seed_mode.cpp"

# Parallel US2 implementation after startup flow wiring
Task: "T022 [US2] Route gating updates in src/ui/src/app/app.routes.ts"
Task: "T024 [US2] Bootstrap status propagation in src/ui/src/app/services/bootstrap.service.ts"
```

## Parallel Example: User Story 3

```bash
# Parallel US3 test suite additions
Task: "T026 [US3] Bootstrap transition tests in src/ui/src/app/pages/bootstrap/bootstrap.page.spec.ts"
Task: "T027 [US3] Sign-in single-flight tests in src/ui/src/app/pages/login/login.spec.ts"
Task: "T028 [US3] Bootstrap E2E transition coverage in src/ui/e2e/bootstrap.spec.ts"
Task: "T029 [US3] Bootstrap-unavailable backend regression in src/test/cpp/integration/test_bootstrap_platform_admin.cpp"

# Parallel US3 implementation split
Task: "T030 [US3] Sign-in single-flight in src/ui/src/app/pages/login/login.ts"
Task: "T031 [US3] Bootstrap single-flight in src/ui/src/app/pages/bootstrap/bootstrap.page.ts"
```

---

## Implementation Strategy

### MVP First (US1)

1. Complete Phases 1-2.
2. Deliver Phase 3 (US1) and run its independent tests.
3. Demo deterministic lockout guidance before expanding scope.

### Incremental Delivery

1. Foundation complete (Phases 1-2).
2. Ship US1, then US2, then US3 in priority order.
3. Run Phase 6 polish and full regression before merge.

### Implementation Readiness Checklist

- All tasks follow `- [ ] T### [P?] [US?] Description with file path` format.
- Every user story has independent tests and implementation tasks.
- Dependency order is explicit and supports MVP-first or parallel staffing.
- Contract, data-model, and threat-model updates are represented in executable tasks.

