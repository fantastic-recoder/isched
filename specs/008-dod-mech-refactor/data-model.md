# Data Model - 008-dod-mech-refactor

## Entity: RefactorPass

- Description: One bounded refactor iteration executed under the mandated stage order.
- Fields:
  - `passId` (string, required): Unique identifier for the pass (for example `pass-01-gql-executor-hotloop`).
  - `scope` (array<string>, required): Files/modules in affected scope.
  - `status` (enum, required): `Planned | InProgress | GateFailed | Accepted`.
  - `workflowStage` (enum, required): `AnalyzeHotPaths | RedefineDataLayout | RefactorLogic | ExpandTests | VerifyAndDocument`.
  - `startedAt` (datetime, optional).
  - `completedAt` (datetime, optional).
- Validation rules:
  - Stage progression must be monotonic in mandated order.
  - `status=Accepted` requires all gates passed and artifact set complete.

## Entity: HotPathRecord

- Description: Baseline/post-pass metric record for one selected hot path.
- Fields:
  - `pathId` (string, required).
  - `component` (string, required).
  - `workloadId` (string, required): Stable benchmark input/workload identifier.
  - `baseline` (MetricSnapshot, required).
  - `postPass` (MetricSnapshot, required).
  - `actionsApplied` (array<string>, required): Branch elimination, SoA migration, index replacement actions.
  - `regressionDetected` (boolean, required).
- Validation rules:
  - Baseline and post-pass workloads must match.
  - `regressionDetected=true` blocks pass acceptance unless explicitly mitigated and approved.

## Entity: MetricSnapshot

- Description: Performance metrics captured for one run profile.
- Fields:
  - `wallTimeNs` (integer, required, >0).
  - `cycles` (integer, optional, >=0).
  - `instructions` (integer, optional, >=0).
  - `branches` (integer, optional, >=0).
  - `branchMisses` (integer, optional, >=0).
  - `samples` (integer, required, >=1).
  - `environment` (string, required): Host/build metadata for repeatability.
- Validation rules:
  - Values must be collected under consistent build profile and workload.

## Entity: DataLayoutPlan

- Description: Pre/post representation of layout and reference model for selected scope.
- Fields:
  - `layoutId` (string, required).
  - `targetComponent` (string, required).
  - `beforeLayout` (string, required): AoS/pointer-oriented description.
  - `afterLayout` (string, required): SoA/index-oriented description.
  - `indexMappingRules` (array<string>, required).
  - `alignmentConstraints` (array<string>, optional).
  - `exceptions` (array<string>, optional): Feasibility/performance exceptions with justification.
- Validation rules:
  - `afterLayout` must document contiguous iteration strategy for selected hot loops or provide explicit exception rationale.

## Entity: CoverageGateResult

- Description: Affected-scope coverage status for pass acceptance.
- Fields:
  - `lineCoveragePct` (number, required, 0-100).
  - `branchCoveragePct` (number, required, 0-100).
  - `thresholdLinePct` (number, required, default 80).
  - `thresholdBranchPct` (number, required, default 80).
  - `reportPath` (string, required).
  - `passed` (boolean, required).
- Validation rules:
  - `passed=true` only if line and branch values both meet or exceed thresholds.

## Entity: PassArtifactSet

- Description: Required deliverables proving pass completeness.
- Fields:
  - `sourceUpdates` (array<string>, required): Refactored `.hpp/.cpp` paths.
  - `testUpdates` (array<string>, required).
  - `performanceSummaryPath` (string, required).
  - `docsSpecUpdates` (array<string>, required).
  - `allTestsGreen` (boolean, required).
- Validation rules:
  - `sourceUpdates`, `testUpdates`, and `docsSpecUpdates` must be non-empty.
  - `allTestsGreen=true` required before acceptance.

## Relationships

- `RefactorPass` 1..* -> `HotPathRecord`
- `RefactorPass` 1..1 -> `DataLayoutPlan`
- `RefactorPass` 1..1 -> `CoverageGateResult`
- `RefactorPass` 1..1 -> `PassArtifactSet`

## State Transitions

- `Planned -> InProgress`: Pass starts with explicit scope and baseline capture plan.
- `InProgress -> GateFailed`: Any of tests, coverage, or performance non-regression gate fails.
- `InProgress -> Accepted`: All required artifacts complete, tests green, coverage gate passes, and no unmitigated selected-metric regression.
- `GateFailed -> InProgress`: Additional remediation changes applied; gates rerun.

