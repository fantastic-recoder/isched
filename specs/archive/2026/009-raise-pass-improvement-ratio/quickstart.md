# Quickstart - 009-raise-pass-improvement-ratio

This quickstart executes recovery passes that raise SC-002 improvement ratio from baseline `1/2` to `>=90%` while preserving all existing gates.

## 1) Prepare workspace and baseline evidence

```bash
cd /home/groby/dev/isched
python3 configure.py
cd /home/groby/dev/isched/cmake-build-debug && ctest --output-on-failure
```

Confirm baseline evidence source from feature 008:

- `specs/008-dod-mech-refactor/artifacts/passes-rollup.md`
- Baseline totals must start at `improving=1`, `completed=2`.

## 2) Initialize recovery artifact layout

Create a pass directory for each new pass and keep one cumulative ledger file.

```bash
cd /home/groby/dev/isched
mkdir -p specs/009-raise-pass-improvement-ratio/artifacts/pass-03
mkdir -p specs/009-raise-pass-improvement-ratio/artifacts/pass-04
```

Recommended files per pass:

- `recovery-pass-artifact.json`
- `perf-baseline.txt`
- `perf-post.txt`
- `coverage.txt`
- `ctest-green.txt`
- `schema-validation.txt`
- `gate-status.json`

## 3) Run one recovery pass with preserved gates

Run tests and affected-scope coverage:

```bash
cd /home/groby/dev/isched/cmake-build-debug && ctest --output-on-failure -R "isched_(gql_executor|graphql|subscription_broker)_tests"
cd /home/groby/dev/isched
tools/refactor_pass/run_affected_coverage.sh \
  specs/009-raise-pass-improvement-ratio/artifacts/pass-03/affected-scope.txt \
  specs/009-raise-pass-improvement-ratio/artifacts/pass-03/coverage.txt \
  specs/009-raise-pass-improvement-ratio/artifacts/pass-03/coverage.html
```

Capture baseline and post metrics for the same workload:

```bash
cd /home/groby/dev/isched
tools/refactor_pass/collect_perf.sh baseline \
  specs/009-raise-pass-improvement-ratio/artifacts/pass-03/perf-baseline.txt \
  ./cmake-build-debug/src/test/cpp/isched/isched_graphql_tests

tools/refactor_pass/collect_perf.sh post \
  specs/009-raise-pass-improvement-ratio/artifacts/pass-03/perf-post.txt \
  ./cmake-build-debug/src/test/cpp/isched/isched_graphql_tests
```

Validate artifact JSON against recovery pass contract:

```bash
cd /home/groby/dev/isched
python3 tools/refactor_pass/validate_pass_artifact.py \
  specs/009-raise-pass-improvement-ratio/artifacts/pass-03/recovery-pass-artifact.json \
  --schema specs/009-raise-pass-improvement-ratio/contracts/recovery-pass-artifact.schema.json
```

Run the ordered workflow wrapper (includes preserved gate verification and writes `gate-status.json`):

```bash
cd /home/groby/dev/isched
tools/refactor_pass/run_pass_workflow.sh \
  specs/009-raise-pass-improvement-ratio/artifacts/pass-03 \
  "isched_(gql_executor|graphql|subscription_broker)_tests" \
  specs/009-raise-pass-improvement-ratio/artifacts/pass-03/affected-scope.txt \
  --schema specs/009-raise-pass-improvement-ratio/contracts/recovery-pass-artifact.schema.json
```

## 4) Update and validate cumulative ratio ledger

After each completed pass, append a new ledger entry and recompute cumulative totals.

- Include `passId`, classification, regression status, cumulative numerator/denominator, ratio percentage, and evidence references.
- Never remove completed passes from denominator unless formally approved and documented.

Validate ledger and final decision artifacts:

```bash
cd /home/groby/dev/isched
python3 tools/refactor_pass/validate_pass_artifact.py \
  specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json \
  --schema specs/009-raise-pass-improvement-ratio/contracts/improvement-ratio-ledger.schema.json

python3 tools/refactor_pass/validate_pass_artifact.py \
  specs/009-raise-pass-improvement-ratio/artifacts/compliance-decision-record.json \
  --schema specs/009-raise-pass-improvement-ratio/contracts/compliance-decision-record.schema.json
```

## 5) Acceptance criteria for each pass and final compliance

A pass counts toward recovery only when all are true:

- Improvement classification is `Improving`.
- No unmitigated selected-metric regression remains.
- Tests are green.
- Affected-scope coverage is `>=80%` line and `>=80%` branch.
- Artifact validation passes.
- Docs/spec updates are included.

Final SC-002 compliance target from baseline:

- Minimum accepted outcome: `9/10` improving completed passes (`>=90%`).
- If more passes are completed, ratio must stay `>=90%` across all completed passes.

## 6) Final US3 audit outputs

Generate and validate the final SC-002 decision record:

```bash
cd /home/groby/dev/isched
python3 tools/refactor_pass/evaluate_sc002_compliance.py \
  --ledger specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json \
  --output specs/009-raise-pass-improvement-ratio/artifacts/compliance-decision-record.json

python3 tools/refactor_pass/validate_recovery_window.py \
  --ledger specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json \
  --artifacts-root specs/009-raise-pass-improvement-ratio/artifacts \
  --decision specs/009-raise-pass-improvement-ratio/artifacts/compliance-decision-record.json
```

Capture one-shot quickstart evidence output:

```bash
cd /home/groby/dev/isched
{
  for t in tests/refactor_pass/*.sh; do "$t"; done
  for p in specs/009-raise-pass-improvement-ratio/artifacts/pass-{03..10}/recovery-pass-artifact.json; do
    python3 tools/refactor_pass/validate_pass_artifact.py "$p" --schema specs/009-raise-pass-improvement-ratio/contracts/recovery-pass-artifact.schema.json
  done
  python3 tools/refactor_pass/validate_pass_artifact.py specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json --schema specs/009-raise-pass-improvement-ratio/contracts/improvement-ratio-ledger.schema.json
  python3 tools/refactor_pass/validate_pass_artifact.py specs/009-raise-pass-improvement-ratio/artifacts/compliance-decision-record.json --schema specs/009-raise-pass-improvement-ratio/contracts/compliance-decision-record.schema.json
  python3 tools/refactor_pass/validate_recovery_window.py --ledger specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json --artifacts-root specs/009-raise-pass-improvement-ratio/artifacts --decision specs/009-raise-pass-improvement-ratio/artifacts/compliance-decision-record.json
} 2>&1 | tee specs/009-raise-pass-improvement-ratio/artifacts/quickstart-validation.log
```

