# Quickstart - 008-dod-mech-refactor

This quickstart runs one complete refactor pass for a selected backend hot path.

## 1) Prepare build and baseline

```bash
cd /home/groby/dev/isched
python3 configure.py
cd /home/groby/dev/isched/cmake-build-debug && ctest --output-on-failure
```

Collect baseline metrics for the selected hot path under fixed input.

```bash
cd /home/groby/dev/isched
perf stat -e cycles,instructions,branches,branch-misses ./cmake-build-debug/src/test/cpp/isched/isched_graphql_tests
```

Record baseline values in the pass performance summary.

## 2) Execute pass workflow (FR-001 order)

1. Analyze hot path and identify branch/memory bottlenecks.
2. Redefine data layout toward SoA for selected frequently iterated state.
3. Replace pointer traversal with index-based references where behavior can be preserved.
4. Refactor logic into stateless system-style operations on flat/index-addressable collections.
5. Apply branch-elimination strategies when profiling indicates benefit.
6. Expand tests for behavior parity, invalid index guards, and SoA alignment.

## 3) Verify quality gates

Run relevant tests for the pass scope.

```bash
cd /home/groby/dev/isched/cmake-build-debug && ctest --output-on-failure -R "isched_(gql_executor|graphql)_tests"
```

Run the affected-scope coverage gate with the scope manifest.

```bash
cd /home/groby/dev/isched
tools/refactor_pass/run_affected_coverage.sh \
  specs/008-dod-mech-refactor/artifacts/pass-01/affected-scope.txt \
  specs/008-dod-mech-refactor/artifacts/pass-01/coverage.txt \
  specs/008-dod-mech-refactor/artifacts/pass-01/coverage.html
```

Capture post-pass metrics with the same workload and compare to baseline.

```bash
cd /home/groby/dev/isched
tools/refactor_pass/collect_perf.sh post \
  specs/008-dod-mech-refactor/artifacts/pass-01/perf-post.txt \
  ./cmake-build-debug/src/test/cpp/isched/isched_graphql_tests
```

## 4) Produce pass artifacts

A pass is complete only when all items are present:

- Refactored source updates (`.hpp/.cpp`) for selected scope.
- Test updates proving behavior preservation and edge-case coverage.
- Performance summary with baseline/post-pass metrics and branch-elimination rationale.
- Coverage evidence proving >=80% line and >=80% branch in affected scope.
- Documentation/spec updates in the same pass.

Validate and gate the pass artifact before acceptance.

```bash
cd /home/groby/dev/isched
python3 tools/refactor_pass/validate_pass_artifact.py \
  specs/008-dod-mech-refactor/artifacts/pass-01/refactor-pass-artifact.json \
  --schema specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json

tools/refactor_pass/verify_pass_gates.sh \
  specs/008-dod-mech-refactor/artifacts/pass-01 \
  specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json \
  "isched_(gql_executor|graphql)_tests" \
  specs/008-dod-mech-refactor/artifacts/pass-01/affected-scope.txt
```

## 5) Acceptance checklist

- Relevant tests are green.
- Affected-scope line coverage >=80%.
- Affected-scope branch coverage >=80%.
- No unmitigated regressions in selected hot-path metrics.
- Required artifacts committed together for review.

## 6) Rejection and remediation flow

If a pass candidate is rejected, use this deterministic recovery sequence:

1. Inspect gate status in `gate-status.json` (`overallStatus`, then gate-specific statuses and exit codes).
2. Open the referenced gate outputs (`ctestOutput`, `coverageOutput`, `schemaOutput`) and classify the failure:
   - test regression (`ctestStatus=FAIL`)
   - coverage shortfall (`coverageStatus=FAIL`)
   - artifact contract/schema mismatch (`schemaStatus=FAIL`)
3. Apply remediation in workflow order:
   - fix behavior/test regressions first,
   - then raise affected-scope line/branch coverage to >=80,
   - then correct artifact JSON completeness/shape.
4. Re-run consolidated verification and overwrite the pass evidence files:

```bash
cd /home/groby/dev/isched
tools/refactor_pass/verify_pass_gates.sh \
  specs/008-dod-mech-refactor/artifacts/pass-02 \
  specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json \
  "isched_subscription_broker_tests|test_graphql_subscriptions" \
  specs/008-dod-mech-refactor/artifacts/pass-02/affected-scope.txt \
  --status-file specs/008-dod-mech-refactor/artifacts/pass-02/gate-status.json \
  | tee specs/008-dod-mech-refactor/artifacts/pass-02/verify-pass-gates.txt
```

5. Accept only when `overallStatus` is `PASS` and all three gate statuses are `PASS`.

