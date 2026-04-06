---
description: "Task list for feature 008-dod-mech-refactor"
---

# Tasks: Data-Oriented Refactor Passes

**Input**: Design documents from `/specs/008-dod-mech-refactor/`
**Prerequisites**: `plan.md` (required), `spec.md` (required), `research.md`, `data-model.md`, `contracts/refactor-pass-artifact.schema.json`, `quickstart.md`

**Tests**: Test and coverage gates are mandatory for this feature. Every pass must keep relevant tests green and meet affected-scope coverage >=80% line and >=80% branch.

**Organization**: Tasks are grouped by user story for independent implementation and validation, while preserving required pass workflow order: analyze -> data layout -> logic refactor -> tests/coverage -> perf/docs.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create reusable pass infrastructure and artifact locations used by all stories.

- [X] T001 Create pass artifact directory scaffold in `specs/008-dod-mech-refactor/artifacts/.gitkeep`
- [X] T002 Create reusable pass summary template in `specs/008-dod-mech-refactor/artifacts/templates/pass-summary-template.md`
- [X] T003 [P] Create reusable pass artifact JSON template in `specs/008-dod-mech-refactor/artifacts/templates/refactor-pass-artifact.template.json`
- [X] T004 [P] Create pass workflow checklist enforcing stage order in `specs/008-dod-mech-refactor/pass-workflow-checklist.md`
- [X] T005 Create baseline/post-pass perf capture script in `tools/refactor_pass/collect_perf.sh`
- [X] T006 [P] Create affected-scope coverage gate script (line+branch >=80%) in `tools/refactor_pass/run_affected_coverage.sh`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Add quality-gate and artifact validation mechanics that block all pass acceptance.

**CRITICAL**: No user story pass can be accepted until this phase is complete.

- [X] T007 Implement pass artifact schema validator CLI in `tools/refactor_pass/validate_pass_artifact.py`
- [X] T008 [P] Add pass gate orchestration script for tests+coverage+artifact validation in `tools/refactor_pass/verify_pass_gates.sh`
- [X] T009 [P] Add affected-scope declaration manifest template in `specs/008-dod-mech-refactor/artifacts/templates/affected-scope.template.txt`
- [X] T010 Add refactor-pass helper README with required command order in `tools/refactor_pass/README.md`
- [X] T011 Wire helper commands into quickstart workflow in `specs/008-dod-mech-refactor/quickstart.md`

**Checkpoint**: Shared mechanics exist to enforce workflow order, green tests, coverage gates, and required artifacts.

---

## Phase 3: User Story 1 - Optimize Critical Paths (Priority: P1) 🎯 MVP

**Goal**: Complete one hot-path optimization pass with measured non-regression/improvement while preserving externally visible behavior.

**Independent Test**: Execute a full pass for a selected hot path and verify: relevant tests green, affected-scope coverage >=80% line/branch, and artifact contract validation passes.

### Tests and Verification for User Story 1

- [X] T012 [US1] Define pass-01 target scope and relevant suites in `specs/008-dod-mech-refactor/artifacts/pass-01/affected-scope.txt`
- [X] T013 [US1] Capture baseline hot-path metrics for pass-01 in `specs/008-dod-mech-refactor/artifacts/pass-01/perf-baseline.txt`

### Implementation for User Story 1 (Ordered Pass Workflow)

- [X] T014 [US1] Document pass-01 analysis findings and selected bottlenecks in `specs/008-dod-mech-refactor/artifacts/pass-01/pass-summary.md`
- [X] T015 [US1] Document pass-01 data layout plan (before/after + index mapping) in `specs/008-dod-mech-refactor/artifacts/pass-01/pass-summary.md`
- [X] T016 [US1] Refactor selected hot-loop data/iteration layout in `src/main/cpp/isched/backend/isched_GqlExecutor.hpp`
- [X] T017 [US1] Apply branch and stateless hot-loop logic refactor in `src/main/cpp/isched/backend/isched_GqlExecutor.cpp`
- [X] T018 [US1] Expand behavior-preservation and edge-case tests for pass-01 scope in `src/test/cpp/isched/isched_gql_executor_tests.cpp`
- [X] T019 [US1] Add regression coverage for external behavior parity in `src/test/cpp/isched/isched_graphql_tests.cpp`
- [X] T020 [US1] Run relevant ctest suites and store green evidence in `specs/008-dod-mech-refactor/artifacts/pass-01/ctest-green.txt`
- [X] T021 [US1] Run affected-scope gcovr gate and store >=80 line/branch evidence in `specs/008-dod-mech-refactor/artifacts/pass-01/coverage.txt`
- [X] T022 [US1] Capture post-pass metrics and compare with baseline in `specs/008-dod-mech-refactor/artifacts/pass-01/perf-post.txt`
- [X] T023 [US1] Finalize pass-01 performance summary and branch-elimination rationale in `specs/008-dod-mech-refactor/artifacts/pass-01/performance-summary.md`
- [X] T024 [US1] Produce pass-01 artifact contract JSON in `specs/008-dod-mech-refactor/artifacts/pass-01/refactor-pass-artifact.json`
- [X] T025 [US1] Validate pass-01 artifact JSON against schema and record output in `specs/008-dod-mech-refactor/artifacts/pass-01/schema-validation.txt`

**Checkpoint**: User Story 1 is complete when pass-01 artifacts prove preserved behavior, green tests, coverage gates, and no unmitigated selected-metric regression.

---

## Phase 4: User Story 2 - Reshape Data for Locality (Priority: P2)

**Goal**: Convert a selected subsystem from pointer-oriented traversal to index-addressable contiguous/SoA-style processing while preserving behavior.

**Independent Test**: Complete one subsystem-focused pass proving pointer-to-index migration + SoA-style iteration with parity tests and validated artifacts.

### Tests and Verification for User Story 2

- [X] T026 [US2] Define pass-02 scope and index-migration targets in `specs/008-dod-mech-refactor/artifacts/pass-02/affected-scope.txt`
- [X] T027 [US2] Capture pass-02 baseline metrics for selected subsystem in `specs/008-dod-mech-refactor/artifacts/pass-02/perf-baseline.txt`

### Implementation for User Story 2 (Ordered Pass Workflow)

- [X] T028 [US2] Document pass-02 analysis outcomes and migration constraints in `specs/008-dod-mech-refactor/artifacts/pass-02/pass-summary.md`
- [X] T029 [US2] Document pass-02 SoA/index data layout design in `specs/008-dod-mech-refactor/artifacts/pass-02/pass-summary.md`
- [X] T030 [US2] Introduce index-oriented storage and mapping helpers in `src/main/cpp/isched/backend/isched_SubscriptionBroker.hpp`
- [X] T031 [US2] Replace pointer traversal with index-based contiguous iteration in `src/main/cpp/isched/backend/isched_SubscriptionBroker.cpp`
- [X] T032 [US2] Add guard clauses for invalid index/sentinel access in `src/main/cpp/isched/backend/isched_SubscriptionBroker.cpp`
- [X] T033 [US2] Expand parity and invalid-index tests for pass-02 scope in `src/test/cpp/isched/isched_subscription_broker_tests.cpp`
- [X] T034 [US2] Run relevant ctest suites and store green evidence in `specs/008-dod-mech-refactor/artifacts/pass-02/ctest-green.txt`
- [X] T035 [US2] Run affected-scope gcovr gate and store >=80 line/branch evidence in `specs/008-dod-mech-refactor/artifacts/pass-02/coverage.txt`
- [X] T036 [US2] Capture pass-02 post-pass metrics in `specs/008-dod-mech-refactor/artifacts/pass-02/perf-post.txt`
- [X] T037 [US2] Finalize pass-02 performance/locality summary in `specs/008-dod-mech-refactor/artifacts/pass-02/performance-summary.md`
- [X] T038 [US2] Produce and validate pass-02 artifact JSON in `specs/008-dod-mech-refactor/artifacts/pass-02/refactor-pass-artifact.json`

**Checkpoint**: User Story 2 is complete when pass-02 demonstrates SoA/index migration with preserved behavior and all acceptance gates satisfied.

---

## Phase 5: User Story 3 - Enforce Safe Incremental Delivery (Priority: P3)

**Goal**: Make pass acceptance repeatable and reject candidates that violate workflow order, tests, coverage, or artifact completeness.

**Independent Test**: Run gate automation against at least one completed pass and one intentionally incomplete pass to prove acceptance/rejection behavior.

### Tests and Verification for User Story 3

- [X] T039 [US3] Add gate acceptance/rejection script test cases in `tests/refactor_pass/test_verify_pass_gates.sh`

### Implementation for User Story 3

- [X] T040 [US3] Implement ordered pass runner (analyze -> data layout -> logic -> tests/coverage -> perf/docs) in `tools/refactor_pass/run_pass_workflow.sh`
- [X] T041 [US3] Add machine-readable gate status output for CI/local usage in `tools/refactor_pass/verify_pass_gates.sh`
- [X] T042 [US3] Integrate refactor-pass verification target in `CMakeLists.txt`
- [X] T043 [US3] Document pass rejection and remediation flow in `specs/008-dod-mech-refactor/quickstart.md`
- [X] T044 [US3] Record workflow dry-run acceptance log in `specs/008-dod-mech-refactor/artifacts/pass-acceptance-dry-run.log`

**Checkpoint**: User Story 3 is complete when pass workflow and quality gates are automatically enforced and reproducible.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Finalize cross-pass documentation and complete full-gate evidence.

- [X] T045 [P] Consolidate pass outcomes and metrics rollup in `specs/008-dod-mech-refactor/artifacts/passes-rollup.md`
- [X] T046 [P] Update feature spec measurable outcomes/evidence links in `specs/008-dod-mech-refactor/spec.md`
- [X] T047 Run full regression gate and archive output in `specs/008-dod-mech-refactor/artifacts/final-ctest-output.txt`
- [X] T048 Run final affected-scope coverage verification and archive output in `specs/008-dod-mech-refactor/artifacts/final-coverage.txt`
- [X] T049 Validate all pass artifact JSON files and archive index in `specs/008-dod-mech-refactor/artifacts/artifact-validation-index.txt`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: starts immediately.
- **Phase 2 (Foundational)**: depends on Phase 1 and blocks all story completion.
- **Phase 3 (US1)**: depends on Phase 2.
- **Phase 4 (US2)**: depends on Phase 2; recommended after US1 to reuse proven pass mechanics.
- **Phase 5 (US3)**: depends on Phase 2; should run after at least one completed pass artifact (US1 or US2).
- **Phase 6 (Polish)**: depends on all targeted stories being complete.

### User Story Dependency Graph

- **US1 (P1)** -> establishes first accepted pass and baseline workflow evidence.
- **US2 (P2)** -> builds on established mechanics, focuses on locality/index migration.
- **US3 (P3)** -> codifies enforcement and rejection/acceptance automation using produced artifacts.

### Within Each Pass (Mandatory Order)

- Analyze hot paths first.
- Redefine data layout second.
- Refactor logic third.
- Expand tests + run coverage gates fourth.
- Capture perf summary + docs/spec updates last.
- Mark pass accepted only after schema-validated artifact, green tests, and >=80% line/branch affected-scope coverage.

---

## Parallel Opportunities

- **Setup**: `T003` and `T004` can run in parallel; `T006` can run while `T005` is implemented.
- **Foundational**: `T008` and `T009` can run in parallel after `T007` starts.
- **US1**: `T018` and `T019` can proceed in parallel once `T017` stabilizes.
- **US2**: `T033` can begin after interface changes in `T030`; `T034`/`T035` can be prepared in parallel.
- **Polish**: `T045` and `T046` can run in parallel.

## Parallel Example: User Story 1

```bash
# Parallelize test updates after logic refactor lands
Task: T018 in src/test/cpp/isched/isched_gql_executor_tests.cpp
Task: T019 in src/test/cpp/isched/isched_graphql_tests.cpp

# Parallelize verification prep
Task: T020 capture ctest evidence
Task: T021 capture coverage evidence
```

## Parallel Example: User Story 2

```bash
# Parallelize verification tasks once refactor/test changes are ready
Task: T034 capture ctest evidence
Task: T035 capture coverage evidence
```

## Parallel Example: User Story 3

```bash
# Parallelize enforcement and documentation
Task: T041 machine-readable gate output
Task: T043 quickstart rejection/remediation documentation
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1 and Phase 2.
2. Deliver US1 through one accepted pass (`pass-01`) with all gates/evidence.
3. Stop and validate independent test criteria for US1 before starting US2.

### Incremental Delivery

1. Build shared pass tooling (Setup + Foundational).
2. Deliver US1 (hot-path optimization pass).
3. Deliver US2 (SoA/index locality pass).
4. Deliver US3 (workflow enforcement automation).
5. Finalize polish artifacts and final gate reports.

### Quality Gate Rule (Non-Negotiable)

- No pass is accepted or merged unless: tests are green, affected-scope line coverage >=80%, affected-scope branch coverage >=80%, and required artifact JSON validates against `specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json`.

