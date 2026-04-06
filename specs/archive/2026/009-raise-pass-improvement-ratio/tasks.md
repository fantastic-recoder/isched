---
description: "Task list for feature 009-raise-pass-improvement-ratio"
---

# Tasks: 009-raise-pass-improvement-ratio

**Input**: Design documents from `/home/groby/dev/isched/specs/009-raise-pass-improvement-ratio/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `quickstart.md`, `contracts/`

**Tests**: This feature requires executable verification for ratio math, gate preservation, and audit reproducibility before story completion.

**Organization**: Tasks are grouped by user story so each story can be implemented and validated independently.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on incomplete tasks)
- **[Story]**: User story label (`[US1]`, `[US2]`, `[US3]`)
- Every task includes an explicit file path

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Initialize feature-scoped artifact layout and runner entry points.

- [X] T001 Create recovery artifact root and README in `specs/009-raise-pass-improvement-ratio/artifacts/README.md`
- [X] T002 [P] Create pass folders `pass-03` through `pass-10` under `specs/009-raise-pass-improvement-ratio/artifacts/`
- [X] T003 [P] Add per-pass artifact template in `specs/009-raise-pass-improvement-ratio/artifacts/templates/recovery-pass-artifact.template.json`
- [X] T004 [P] Add cumulative ledger template in `specs/009-raise-pass-improvement-ratio/artifacts/templates/improvement-ratio-ledger.template.json`
- [X] T005 [P] Add compliance decision template in `specs/009-raise-pass-improvement-ratio/artifacts/templates/compliance-decision-record.template.json`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish reusable governance tooling that all stories depend on.

**⚠️ CRITICAL**: Complete this phase before any user story work.

- [X] T006 Implement baseline carry-forward and entry append CLI in `tools/refactor_pass/update_improvement_ledger.py`
- [X] T007 [P] Implement SC-002 decision evaluator CLI in `tools/refactor_pass/evaluate_sc002_compliance.py`
- [X] T008 [P] Implement cross-artifact audit validator in `tools/refactor_pass/validate_recovery_window.py`
- [X] T009 Add shell test harness for ledger update script in `tests/refactor_pass/test_update_improvement_ledger.sh`
- [X] T010 [P] Add shell test harness for compliance evaluator in `tests/refactor_pass/test_evaluate_sc002_compliance.sh`
- [X] T011 [P] Document new 009 workflow commands in `tools/refactor_pass/README.md`

**Checkpoint**: Foundation ready - user stories can proceed.

---

## Phase 3: User Story 1 - Recover SC-002 Compliance (Priority: P1) 🎯 MVP

**Goal**: Raise cumulative improving-pass ratio from baseline `1/2` to `>=90%` (minimum `9/10`) using all completed passes.

**Independent Test**: Starting from baseline ledger `1/2`, append recovery pass outcomes and verify ledger summary reaches at least `9/10` with no denominator cherry-picking.

### Tests for User Story 1

- [X] T012 [P] [US1] Add baseline immutability regression case in `tests/refactor_pass/test_update_improvement_ledger.sh`
- [X] T013 [P] [US1] Add cumulative ratio progression case (`1/2` to `9/10`) in `tests/refactor_pass/test_update_improvement_ledger.sh`
- [X] T014 [P] [US1] Add denominator-manipulation rejection case in `tests/refactor_pass/test_update_improvement_ledger.sh`

### Implementation for User Story 1

- [X] T015 [US1] Seed baseline ledger (`improving=1`, `completed=2`) in `specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json`
- [X] T016 [US1] Record pass-03 improving artifact in `specs/009-raise-pass-improvement-ratio/artifacts/pass-03/recovery-pass-artifact.json`
- [X] T017 [P] [US1] Record pass-04 improving artifact in `specs/009-raise-pass-improvement-ratio/artifacts/pass-04/recovery-pass-artifact.json`
- [X] T018 [P] [US1] Record pass-05 improving artifact in `specs/009-raise-pass-improvement-ratio/artifacts/pass-05/recovery-pass-artifact.json`
- [X] T019 [P] [US1] Record pass-06 improving artifact in `specs/009-raise-pass-improvement-ratio/artifacts/pass-06/recovery-pass-artifact.json`
- [X] T020 [P] [US1] Record pass-07 improving artifact in `specs/009-raise-pass-improvement-ratio/artifacts/pass-07/recovery-pass-artifact.json`
- [X] T021 [P] [US1] Record pass-08 improving artifact in `specs/009-raise-pass-improvement-ratio/artifacts/pass-08/recovery-pass-artifact.json`
- [X] T022 [P] [US1] Record pass-09 improving artifact in `specs/009-raise-pass-improvement-ratio/artifacts/pass-09/recovery-pass-artifact.json`
- [X] T023 [P] [US1] Record pass-10 improving artifact in `specs/009-raise-pass-improvement-ratio/artifacts/pass-10/recovery-pass-artifact.json`
- [X] T024 [US1] Append pass-03..pass-10 entries and recompute cumulative summary in `specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json`
- [X] T025 [US1] Capture ratio timeline and `>=90%` milestone evidence in `specs/009-raise-pass-improvement-ratio/artifacts/passes-rollup.md`

**Checkpoint**: User Story 1 independently proves SC-002 recovery math from baseline to compliant ratio.

---

## Phase 4: User Story 2 - Preserve Existing Quality Gates (Priority: P2)

**Goal**: Keep all feature-008 gates intact for every recovery pass (tests, affected line/branch coverage, schema validation, docs/spec updates).

**Independent Test**: For any recovery pass artifact, gate verification fails if any preserved gate is missing/failing and passes only when all required evidence is present.

### Tests for User Story 2

- [X] T026 [P] [US2] Add docs/spec gate failure case in `tests/refactor_pass/test_verify_pass_gates.sh`
- [X] T027 [P] [US2] Add affected line/branch `<80%` rejection case in `tests/refactor_pass/test_verify_pass_gates.sh`
- [X] T028 [P] [US2] Add ordered workflow gate-block case in `tests/refactor_pass/test_run_pass_workflow.sh`

### Implementation for User Story 2

- [X] T029 [US2] Enforce preserved gate checks and explicit status fields in `tools/refactor_pass/verify_pass_gates.sh`
- [X] T030 [US2] Integrate 009 schemas and gate outputs in `tools/refactor_pass/run_pass_workflow.sh`
- [X] T031 [US2] Add per-pass gate status template in `specs/009-raise-pass-improvement-ratio/artifacts/templates/gate-status.template.json`
- [X] T032 [US2] Persist gate-status evidence for pass-03..pass-10 in `specs/009-raise-pass-improvement-ratio/artifacts/`

**Checkpoint**: User Story 2 independently demonstrates gate preservation with reject/accept behavior unchanged from feature 008.

---

## Phase 5: User Story 3 - Keep Evidence Auditable (Priority: P3)

**Goal**: Produce reviewer-auditable ledger and compliance decision records that are reproducible from artifacts alone.

**Independent Test**: Using only artifacts in `specs/009-raise-pass-improvement-ratio/artifacts/`, an independent reviewer can reproduce pass classifications, ratio totals, and final SC-002 decision.

### Tests for User Story 3

- [X] T033 [P] [US3] Add missing-evidence unresolved-decision case in `tests/refactor_pass/test_evaluate_sc002_compliance.sh`
- [X] T034 [P] [US3] Add ledger/decision consistency validation case in `tests/refactor_pass/test_validate_recovery_window.sh`

### Implementation for User Story 3

- [X] T035 [US3] Generate final SC-002 decision artifact in `specs/009-raise-pass-improvement-ratio/artifacts/compliance-decision-record.json`
- [X] T036 [US3] Build evidence index for all counted passes in `specs/009-raise-pass-improvement-ratio/artifacts/evidence-index.json`
- [X] T037 [US3] Record auditor-oriented traceability guide in `specs/009-raise-pass-improvement-ratio/artifacts/AUDIT_TRAIL.md`
- [X] T038 [US3] Run cross-artifact audit validation and save output in `specs/009-raise-pass-improvement-ratio/artifacts/audit-validation.txt`

**Checkpoint**: User Story 3 independently proves auditable, reproducible compliance decisions.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final consistency checks, docs alignment, and end-to-end validation.

- [X] T039 [P] Reconcile feature guidance and command examples in `specs/009-raise-pass-improvement-ratio/quickstart.md`
- [X] T040 [P] Update feature implementation notes and decisions in `specs/009-raise-pass-improvement-ratio/research.md`
- [X] T041 Validate all recovery artifacts against contracts in `specs/009-raise-pass-improvement-ratio/contracts/`
- [X] T042 Run full quickstart flow and capture execution evidence in `specs/009-raise-pass-improvement-ratio/artifacts/quickstart-validation.log`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies.
- **Phase 2 (Foundational)**: Depends on Phase 1; blocks all user stories.
- **Phase 3 (US1)**: Depends on Phase 2; establishes baseline carry-forward and cumulative ratio compliance.
- **Phase 4 (US2)**: Depends on Phase 2; may run in parallel with late US1 artifact authoring once foundational scripts exist.
- **Phase 5 (US3)**: Depends on US1 ledger completion and US2 gate evidence availability.
- **Phase 6 (Polish)**: Depends on completion of all user stories.

### User Story Dependency Graph

- **US1 (P1)** -> Required first for ratio-governance outputs (`improvement-ratio-ledger.json`, `passes-rollup.md`).
- **US2 (P2)** -> Independent gate-preservation enforcement; must complete before final compliance sign-off.
- **US3 (P3)** -> Depends on US1 + US2 artifacts to produce final auditable decision.

### Within-Story Ordering Rules

- Add/extend story-specific verification tasks before story sign-off.
- Create/refresh artifact templates before per-pass artifact authoring.
- Update cumulative ledger after each completed pass, never by subset recomputation.
- Mark story complete only when its Independent Test passes using only that story's outputs.

---

## Parallel Execution Examples

### User Story 1

```bash
# Parallel artifact authoring after baseline ledger is seeded:
Task T017  # pass-04 artifact
Task T018  # pass-05 artifact
Task T019  # pass-06 artifact
Task T020  # pass-07 artifact
Task T021  # pass-08 artifact
Task T022  # pass-09 artifact
Task T023  # pass-10 artifact
```

### User Story 2

```bash
# Parallel gate failure scenarios:
Task T026  # docs/spec gate rejection
Task T027  # coverage threshold rejection
Task T028  # ordered workflow gate-block
```

### User Story 3

```bash
# Parallel audit-focused tests:
Task T033  # unresolved decision on missing evidence
Task T034  # ledger/decision consistency check
```

---

## Implementation Strategy

### MVP First (US1 only)

1. Complete Phase 1 and Phase 2.
2. Complete US1 tasks through `T025`.
3. Validate US1 independent test (baseline `1/2` carried forward to `>=9/10`).
4. Pause for stakeholder review before gate/audit expansion.

### Incremental Delivery

1. Deliver US1 ratio recovery.
2. Add US2 gate-preservation enforcement and rejection tests.
3. Add US3 audit ledger + decision reproducibility.
4. Run Phase 6 full quickstart validation and publish final evidence.

### Suggested MVP Scope

- `US1` only (`T012`-`T025`) is the minimum slice that closes SC-002 ratio compliance.

---

## Notes

- `[P]` tasks are parallelizable because they target separate files/artifacts.
- Cumulative ratio governance is always computed across all completed passes, including baseline `1/2` carry-forward.
- A pass with missing gate evidence, unresolved regression, or non-improving metrics remains non-qualifying until corrected.
- Final acceptance requires schema-valid artifacts, preserved gates, and reproducible compliance decision evidence.

