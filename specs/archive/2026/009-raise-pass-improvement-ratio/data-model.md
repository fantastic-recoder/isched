# Data Model - 009-raise-pass-improvement-ratio

## Entity: RecoveryPass

- Description: One completed recovery pass executed to improve at least one selected hot-path metric while preserving all existing gates.
- Fields:
  - `passId` (string, required): Unique pass identifier (for example `pass-03-gql-executor-cacheline`).
  - `status` (enum, required): `Planned | InProgress | GateBlocked | Completed`.
  - `selectedMetrics` (array<MetricTarget>, required, minItems=1).
  - `metricEvidence` (MetricEvidenceRecord, required once completed).
  - `gateRecord` (PassGateRecord, required once completed).
  - `improvementClassification` (enum, required once completed): `Improving | NonImproving | Blocked`.
  - `regressionStatus` (enum, required once completed): `None | Mitigated | Unmitigated`.
  - `artifactRefs` (ArtifactReferenceSet, required once completed).
- Validation rules:
  - `Completed` requires gate record pass + complete metric evidence.
  - `improvementClassification=Improving` requires at least one metric delta above noise floor and `regressionStatus != Unmitigated`.

## Entity: MetricTarget

- Description: A selected metric tracked for a pass.
- Fields:
  - `metricId` (string, required): Canonical name (for example `wallTimeNs`, `cycles`, `branchMisses`).
  - `direction` (enum, required): `LowerIsBetter | HigherIsBetter`.
  - `noiseFloorPct` (number, required, >=0).
  - `workloadId` (string, required): Stable workload label.
- Validation rules:
  - `metricId + workloadId` must be unique within a pass.

## Entity: MetricEvidenceRecord

- Description: Baseline and post-pass measurements proving improvement/no-improvement.
- Fields:
  - `baseline` (MetricSnapshot, required).
  - `postPass` (MetricSnapshot, required).
  - `deltaByMetric` (array<MetricDelta>, required, minItems=1).
  - `repeatabilityChecks` (array<RepeatabilityCheck>, required, minItems=1).
  - `evidenceFiles` (array<string>, required, minItems=1).
- Validation rules:
  - Baseline and post-pass must use identical workload and environment fingerprint.
  - `repeatabilityChecks` must indicate stable variance within declared threshold.

## Entity: PassGateRecord

- Description: Required gate outcomes preserved from feature 008.
- Fields:
  - `testsGreen` (boolean, required, must be `true` for acceptance).
  - `affectedLineCoveragePct` (number, required, 0..100).
  - `affectedBranchCoveragePct` (number, required, 0..100).
  - `artifactValidation` (boolean, required, must be `true` for acceptance).
  - `docsSpecUpdated` (boolean, required, must be `true` for acceptance).
  - `gateEvidenceFiles` (array<string>, required, minItems=1).
- Validation rules:
  - Coverage percentages must both be `>=80` to pass.
  - Any missing gate evidence forces `RecoveryPass.status=GateBlocked`.

## Entity: ImprovementRatioLedger

- Description: Ordered cumulative ledger used to compute SC-002 compliance.
- Fields:
  - `baseline` (LedgerBaseline, required): Starts at `improving=1`, `completed=2`.
  - `entries` (array<LedgerEntry>, required): One entry per completed pass in chronological order.
  - `currentSummary` (LedgerSummary, required): Running totals and ratio after latest entry.
  - `updatedAt` (datetime, required).
- Validation rules:
  - `entries` cannot skip completed pass identifiers.
  - `currentSummary.completedCount = baseline.completedCount + entries.length`.
  - `currentSummary.improvementRatioPct = improvingCount/completedCount * 100` (rounded by declared rule).

## Entity: ComplianceDecisionRecord

- Description: Final SC-002 decision artifact with evidence references.
- Fields:
  - `decisionId` (string, required).
  - `sc002Status` (enum, required): `Met | NotMet | Unresolved`.
  - `numerator` (integer, required, >=0).
  - `denominator` (integer, required, >=1).
  - `ratioPct` (number, required, 0..100).
  - `evaluatedAt` (datetime, required).
  - `evidenceSet` (array<string>, required, minItems=1).
  - `notes` (string, optional).
- Validation rules:
  - `sc002Status=Met` requires `ratioPct>=90` and zero unmitigated regressions in referenced entries.
  - `sc002Status=Unresolved` required when any evidence reference is missing or conflicting.

## Supporting Value Objects

- `MetricSnapshot`: `{ wallTimeNs, cycles, instructions, branches, branchMisses, samples, environmentFingerprint }`.
- `MetricDelta`: `{ metricId, baselineValue, postPassValue, deltaValue, deltaPct, improved }`.
- `RepeatabilityCheck`: `{ runCount, variancePct, withinNoiseFloor }`.
- `ArtifactReferenceSet`: `{ artifactJsonPath, perfSummaryPath, coveragePath, ctestPath, docsDiffPath }`.
- `LedgerBaseline`: `{ improvingCount=1, completedCount=2, sourceRef }`.
- `LedgerEntry`: `{ passId, classification, regressionStatus, improvingCount, completedCount, ratioPct, evidenceRefs }`.
- `LedgerSummary`: `{ improvingCount, completedCount, ratioPct, sc002Compliant }`.

## Relationships

- `RecoveryPass` 1..1 -> `MetricEvidenceRecord`
- `RecoveryPass` 1..1 -> `PassGateRecord`
- `ImprovementRatioLedger` 1..* -> `LedgerEntry`
- `ComplianceDecisionRecord` 1..1 -> `ImprovementRatioLedger`
- `ComplianceDecisionRecord` *..* -> `RecoveryPass` (via evidence refs)

## State Transitions

- `Planned -> InProgress`: Selected metrics and workload declared.
- `InProgress -> GateBlocked`: Any preserved gate fails or evidence is incomplete.
- `InProgress -> Completed`: Gates pass, evidence is complete, and classification assigned.
- `GateBlocked -> InProgress`: Remediation applied and gates rerun.
- `Completed -> Completed` (ledger update): Ledger entry appended and cumulative summary recalculated.

