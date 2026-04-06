# refactor_pass helper scripts

This directory provides helper scripts for the `008-dod-mech-refactor` pass workflow
and the `009-raise-pass-improvement-ratio` recovery workflow.

## Required command order

1. Capture baseline metrics
2. Implement data-layout and logic refactor
3. Run relevant tests
4. Run affected-scope coverage gate (line >=80 and branch >=80)
5. Capture post-pass metrics
6. Build and validate `refactor-pass-artifact.json`

## Commands

### 1) Collect baseline/post-pass perf

```bash
tools/refactor_pass/collect_perf.sh baseline specs/008-dod-mech-refactor/artifacts/pass-01/perf-baseline.txt ./cmake-build-debug/src/test/cpp/isched/isched_graphql_tests
tools/refactor_pass/collect_perf.sh post specs/008-dod-mech-refactor/artifacts/pass-01/perf-post.txt ./cmake-build-debug/src/test/cpp/isched/isched_graphql_tests
```

### 2) Run coverage gate for affected scope

```bash
tools/refactor_pass/run_affected_coverage.sh \
  specs/008-dod-mech-refactor/artifacts/pass-01/affected-scope.txt \
  specs/008-dod-mech-refactor/artifacts/pass-01/coverage.txt \
  specs/008-dod-mech-refactor/artifacts/pass-01/coverage.html
```

### 3) Validate artifact JSON

```bash
python3 tools/refactor_pass/validate_pass_artifact.py \
  specs/008-dod-mech-refactor/artifacts/pass-01/refactor-pass-artifact.json \
  --schema specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json
```

### 4) Run all pass gates

```bash
tools/refactor_pass/verify_pass_gates.sh \
  specs/008-dod-mech-refactor/artifacts/pass-01 \
  specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json \
  "isched_(gql_executor|graphql)_tests" \
  specs/008-dod-mech-refactor/artifacts/pass-01/affected-scope.txt
```

## 009 recovery workflow commands

### 1) Update the cumulative improvement ledger

```bash
python3 tools/refactor_pass/update_improvement_ledger.py \
  --ledger specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json \
  --pass-artifact specs/009-raise-pass-improvement-ratio/artifacts/pass-03/recovery-pass-artifact.json
```

### 2) Evaluate SC-002 compliance from ledger state

```bash
python3 tools/refactor_pass/evaluate_sc002_compliance.py \
  --ledger specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json \
  --output specs/009-raise-pass-improvement-ratio/artifacts/compliance-decision-record.json
```

### 3) Validate cross-artifact consistency for the recovery window

```bash
python3 tools/refactor_pass/validate_recovery_window.py \
  --ledger specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json \
  --artifacts-root specs/009-raise-pass-improvement-ratio/artifacts
```

