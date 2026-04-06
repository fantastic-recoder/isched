---
spec_id: SP-009
title: Improvement-Ratio Recovery Passes
status: completed
owner: @isched-dev
---

# Feature Specification: Improvement-Ratio Recovery Passes

**Feature Branch**: `009-raise-pass-improvement-ratio`  
**Created**: 2026-04-06  
**Status**: Completed  
**Input**: User description: "Create a follow-up feature specification in this repository to close the gap in `specs/008-dod-mech-refactor/spec.md` success criterion SC-002 (>=90% of completed passes show measurable improvement in at least one selected hot-path metric). The new feature should define additional refactor/performance pass work needed to raise improvement ratio from current 1/2 to compliant level, while preserving all existing gates (green tests, >=80 line/branch affected-scope coverage, artifact validation, docs/spec updates). Produce a concrete, testable spec with user stories, functional requirements, measurable outcomes, and edge cases aligned with repo conventions."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Recover SC-002 Compliance (Priority: P1)

As a performance refactor lead, I need to run additional targeted passes that produce measurable hot-path gains so the rolling improvement ratio reaches and stays at or above 90%.

**Why this priority**: This directly closes the currently unmet success criterion and determines whether the initiative can be accepted.

**Independent Test**: Starting from documented baseline status of `1/2` improved passes, execute additional passes and verify the rollup shows at least `9/10` improved passes (or higher ratio with more passes) with no unmitigated regression entries.

**Acceptance Scenarios**:

1. **Given** the initiative starts at `1/2` improved passes, **When** eight additional completed passes each improve at least one selected hot-path metric, **Then** the rollup reports at least `9/10` improved passes and SC-002 is satisfied.
2. **Given** a completed pass has no measurable metric improvement, **When** pass validation runs, **Then** the pass is marked non-improving and cannot be counted toward SC-002 recovery until a replacement improving pass is completed.

---

### User Story 2 - Preserve Existing Quality Gates (Priority: P2)

As a project maintainer, I need every recovery pass to preserve the same gating standards already used in feature 008 so quality does not regress while chasing ratio compliance.

**Why this priority**: Ratio recovery without quality gates could hide regressions and invalidate the original initiative outcomes.

**Independent Test**: For any one recovery pass, verify pass artifacts include green test results, affected-scope coverage at or above 80% for line and branch, artifact validation evidence, and docs/spec updates before the pass is accepted.

**Acceptance Scenarios**:

1. **Given** a recovery pass candidate is submitted, **When** gate checks run, **Then** the pass is rejected unless all existing gates are explicitly satisfied.
2. **Given** a pass improves performance but coverage falls below threshold, **When** acceptance is evaluated, **Then** the pass remains blocked until coverage is restored and revalidated.

---

### User Story 3 - Keep Evidence Auditable (Priority: P3)

As a reviewer, I need each recovery pass to produce consistent evidence so I can audit the improvement ratio calculation and final compliance decision without manual reconstruction.

**Why this priority**: Reliable auditability reduces ambiguity in pass acceptance and prevents disputes about whether SC-002 is actually met.

**Independent Test**: Review only pass artifacts and rollup files for the recovery window and verify that every counted pass has complete metric evidence and an explicit improve/not-improve classification.

**Acceptance Scenarios**:

1. **Given** recovery passes are completed, **When** a reviewer inspects the rollup, **Then** each pass has traceable links to its metric evidence, gate evidence, and classification outcome.
2. **Given** any recovery pass artifact is incomplete or conflicting, **When** rollup validation runs, **Then** compliance status is reported as unresolved until evidence is corrected.

### Edge Cases

- A pass improves one selected metric but regresses another selected metric; the pass is non-compliant unless the regression is mitigated within the same pass and documented as resolved.
- A pass changes code in affected scope but cannot produce a stable baseline due to noisy measurement conditions; the pass cannot be counted until baseline and post-pass data are repeatable.
- A planned hot path is discovered to have negligible runtime impact after analysis; the pass must be re-scoped to a higher-impact path before implementation proceeds.
- Completed passes exceed the minimum recovery count; the success ratio must still be recalculated using all completed passes, not only a chosen subset.
- Documentation updates are missing while all technical gates pass; the pass remains incomplete and cannot be included in ratio totals.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The recovery effort MUST start from the documented baseline of `1/2` improved passes from feature `008-dod-mech-refactor` and carry that baseline into ratio calculations.
- **FR-002**: The effort MUST define and execute additional refactor/performance passes until the cumulative improvement ratio is at least 90% of all completed passes.
- **FR-003**: From the current baseline, the recovery plan MUST include at least eight additional completed improving passes unless an approved recalculation method changes the denominator by formally removing previously completed passes.
- **FR-004**: Each completed recovery pass MUST demonstrate measurable improvement in at least one selected hot-path metric relative to that pass baseline and record metric values before and after the pass.
- **FR-005**: A pass with zero measured improvement across selected metrics MUST be classified as non-improving and excluded from the set of qualifying recovery passes.
- **FR-006**: Every recovery pass MUST preserve all existing gates from feature 008: relevant automated tests green, affected-scope line coverage >=80%, affected-scope branch coverage >=80%, artifact validation complete, and docs/spec updates complete.
- **FR-007**: Pass acceptance MUST be blocked when any required gate evidence is missing, inconsistent, or indicates failure.
- **FR-008**: The pass rollup MUST maintain, for each completed pass, pass identifier, selected metric set, baseline and post-pass values, improvement classification, regression status, and gate outcomes.
- **FR-009**: The rollup MUST provide a current cumulative ratio summary after each completed pass, including numerator, denominator, and percentage.
- **FR-010**: The final compliance report MUST explicitly state whether SC-002 is met and reference the exact evidence set used for that conclusion.

### Key Entities *(include if feature involves data)*

- **Recovery Pass**: A bounded refactor/performance iteration with selected hot path scope, metric evidence, and gate evidence.
- **Metric Evidence Record**: A per-pass record of selected hot-path metrics with baseline values, post-pass values, and calculated deltas.
- **Pass Gate Record**: A per-pass record of required gates (tests, coverage, artifact validation, docs/spec updates) and pass/fail state.
- **Improvement Ratio Ledger**: A cumulative record tracking improving-pass count, total completed-pass count, and resulting percentage after each pass.
- **Compliance Decision Record**: The final decision artifact declaring SC-002 met/not met with referenced evidence sources.

### Assumptions

- The current `1/2` baseline is accepted as authoritative for the start of this follow-up feature.
- Selected hot-path metrics for each pass are declared before implementation and remain unchanged during that pass.
- A measurable improvement means a directionally favorable numeric change beyond measurement noise under repeatable run conditions.
- Existing gate definitions and acceptance thresholds from feature 008 remain unchanged for this follow-up work.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The recovery effort reaches a cumulative improvement ratio of at least 90% across all completed passes, starting from the `1/2` baseline.
- **SC-002**: At least eight additional completed passes are recorded as improving from the current baseline, resulting in at least `9/10` improving completed passes.
- **SC-003**: 100% of completed recovery passes satisfy all preserved gates (green tests, >=80% line coverage in affected scope, >=80% branch coverage in affected scope, artifact validation, docs/spec updates).
- **SC-004**: 100% of completed recovery passes include auditable metric evidence and explicit improve/not-improve classification in the rollup.
- **SC-005**: The final compliance decision for SC-002 is reproducible by an independent reviewer using only the recorded pass artifacts and rollup data.
