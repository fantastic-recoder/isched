# Phase 0 Research - 009-raise-pass-improvement-ratio

## Decision 1: Cumulative ratio arithmetic and baseline carry-forward

- Decision: Use a strict cumulative ledger that starts at baseline `improving=1`, `completed=2`, and recalculates ratio after each newly completed pass as `improving/completed * 100`.
- Rationale: FR-001/FR-002 require baseline continuity and cumulative reporting; this prevents denominator manipulation and keeps SC-001 reproducible.
- Alternatives considered:
  - Recalculate ratio only over recovery-window passes: rejected because it discards baseline evidence and violates FR-001.
  - Maintain a manually edited percentage only: rejected because raw numerator/denominator are needed for auditability.

## Decision 2: Minimum recovery pass count from current baseline

- Decision: Plan for at least eight additional improving passes so cumulative status reaches at least `9/10`.
- Rationale: From baseline `1/2`, eight improving passes produce `9/10 = 90%`, the minimum compliant threshold required by SC-001 and SC-002.
- Alternatives considered:
  - Target fewer than eight passes with optimistic denominator adjustments: rejected because denominator changes require formal approved removal rules and are not default flow.
  - Count non-improving passes toward target: rejected because FR-005 disqualifies non-improving passes.

## Decision 3: Measurable improvement and measurement-noise handling

- Decision: A pass is improving only if at least one selected metric shows directionally favorable change that exceeds the pass noise floor, proven by repeatable baseline/post runs under the same workload and environment metadata.
- Rationale: The feature assumptions require repeatability and noise-aware interpretation; without repeatability, improvement claims are not auditable.
- Alternatives considered:
  - Accept single-run deltas: rejected because noisy conditions can produce false positives.
  - Require all selected metrics to improve: rejected as too strict for practical tuning and not required by FR-004.

## Decision 4: Regression policy for mixed metric outcomes

- Decision: If a pass improves one selected metric but regresses another selected metric, classify it as blocked/non-compliant until mitigation is recorded in the same pass evidence.
- Rationale: Edge-case requirements explicitly prohibit counting unmitigated regressions.
- Alternatives considered:
  - Count any net-improving pass regardless of regressions: rejected as incompatible with edge-case constraints and quality intent.
  - Ignore secondary metrics: rejected because selected metric set is part of declared pass scope.

## Decision 5: Gate-preservation strategy

- Decision: Reuse existing 008 gate workflow (`ctest`, affected-scope line/branch coverage >=80, schema validation, docs/spec updates) without threshold changes.
- Rationale: FR-006 fixes gates and thresholds; reusing established scripts minimizes workflow drift and preserves comparability.
- Alternatives considered:
  - Lower coverage gates for faster pass throughput: rejected because it violates preserved-gate requirements.
  - Replace artifact validation with manual review: rejected due to lower reproducibility.

## Decision 6: Auditable evidence model

- Decision: Define three contracts: per-pass artifact schema, cumulative ratio ledger schema, and final compliance decision record schema; each ledger row links pass identifiers to metric and gate evidence paths.
- Rationale: FR-008/FR-009/FR-010 and SC-004/SC-005 require independent reviewer reproducibility from records alone.
- Alternatives considered:
  - Keep evidence only in Markdown narrative: rejected because machine validation and consistency checks are weaker.
  - Store only final ratio summary: rejected because pass-level traceability is required.

## Decision 7: Validation flow integration with existing tooling

- Decision: Continue using `tools/refactor_pass/validate_pass_artifact.py` for schema checks and extend pass workflow to validate ledger and decision records at each pass acceptance checkpoint.
- Rationale: Existing validator and gate scripts already enforce artifact quality; extending the same path avoids introducing a parallel validation stack.
- Alternatives considered:
  - Introduce a new external validation framework: rejected as unnecessary overhead for current scope.
  - Validate ledger only at end of initiative: rejected because FR-009 requires up-to-date cumulative summary after each pass.

## Decision 8: Threat-model documentation scope

- Decision: Treat this feature as non-security-sensitive workflow/evidence refinement with no auth/session transport change; no new feature-scoped threat model is required unless implementation later touches security-sensitive surfaces.
- Rationale: Constitution requires threat-model updates only for security-sensitive changes; current scope is pass governance and metric evidence.
- Alternatives considered:
  - Force threat-model update unconditionally: considered optional but deferred to avoid noise without security-surface change.

## Implementation updates (US2/US3 completion)

- Preserved-gate evaluator now records explicit status fields (`ctestStatus`, `coverageStatus`, `schemaStatus`, `docsSpecStatus`) and enforces docs/spec evidence as a first-class gate.
- Coverage acceptance is now validated from both gate command exit code and artifact-declared line/branch percentages (`>=80`) to prevent silent threshold drift.
- Ordered workflow execution now logs `ExpandTestsAndVerify: FAIL` on gate failure and blocks downstream documentation closure steps.
- SC-002 evaluator now marks decisions as `Unresolved` when evidence refs are present but point to missing files (including fragment refs), improving audit reproducibility.
- Audit outputs are now complete for feature 009: `gate-status.json` per pass, `evidence-index.json`, `AUDIT_TRAIL.md`, and `audit-validation.txt`.

