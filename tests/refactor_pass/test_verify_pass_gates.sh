#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
script_under_test="$repo_root/tools/refactor_pass/verify_pass_gates.sh"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

stub_bin="$tmp_dir/bin"
mkdir -p "$stub_bin"

cat >"$stub_bin/ctest" <<'EOF'
#!/usr/bin/env bash
echo "stub ctest invoked: $*"
exit "${STUB_CTEST_RC:-0}"
EOF

cat >"$stub_bin/gcovr" <<'EOF'
#!/usr/bin/env bash
echo "lines: 95.0% (95 out of 100)"
echo "branches: 90.0% (90 out of 100)"
exit "${STUB_GCOVR_RC:-0}"
EOF

chmod +x "$stub_bin/ctest" "$stub_bin/gcovr"

assert_contains() {
  local file="$1"
  local needle="$2"
  if ! grep -Fq "$needle" "$file"; then
    echo "Assertion failed: '$needle' not found in $file" >&2
    echo "---- file contents ----" >&2
    cat "$file" >&2
    echo "-----------------------" >&2
    exit 1
  fi
}

run_case() {
  local name="$1"
  local ctest_rc="$2"
  local gcovr_rc="$3"
  local line_coverage="$4"
  local branch_coverage="$5"
  local docs_updated="$6"
  local expected_rc="$7"
  local expected_overall="$8"
  local expected_coverage="$9"
  local expected_docs="${10}"

  local case_dir="$tmp_dir/$name"
  mkdir -p "$case_dir"
  cat >"$case_dir/refactor-pass-artifact.json" <<EOF
{
  "passId": "stub-pass",
  "selectedMetrics": [
    {
      "metricId": "wallTimeNs",
      "direction": "LowerIsBetter",
      "noiseFloorPct": 1.0,
      "workloadId": "stub-workload"
    }
  ],
  "metricEvidence": {
    "baseline": {
      "wallTimeNs": 1000,
      "samples": 5,
      "environmentFingerprint": "linux-x86_64"
    },
    "postPass": {
      "wallTimeNs": 900,
      "samples": 5,
      "environmentFingerprint": "linux-x86_64"
    },
    "deltas": [
      {
        "metricId": "wallTimeNs",
        "baselineValue": 1000,
        "postPassValue": 900,
        "deltaValue": -100,
        "deltaPct": -10.0,
        "improved": true
      }
    ],
    "repeatability": {
      "runCount": 5,
      "variancePct": 0.5,
      "withinNoiseFloor": true
    },
    "evidenceFiles": ["$case_dir/perf-baseline.txt"]
  },
  "gates": {
    "testsGreen": true,
    "affectedLineCoveragePct": $line_coverage,
    "affectedBranchCoveragePct": $branch_coverage,
    "artifactValidation": true,
    "docsSpecUpdated": $docs_updated
  },
  "artifacts": {
    "sourceUpdates": ["src/main/cpp/isched/backend/isched_GqlExecutor.cpp"],
    "testUpdates": ["src/test/cpp/isched/isched_gql_executor_tests.cpp"],
    "performanceSummary": "specs/009-raise-pass-improvement-ratio/artifacts/pass-03/performance-summary.md",
    "docsSpecUpdates": ["specs/009-raise-pass-improvement-ratio/tasks.md"]
  },
  "classification": {
    "improvement": "Improving",
    "regressionStatus": "None",
    "countsTowardRecovery": true
  }
}
EOF
  cat >"$case_dir/scope.txt" <<'EOF'
src/main/cpp/isched/backend/isched_SubscriptionBroker\.cpp
EOF

  local status_file="$case_dir/status.json"

  set +e
  (
    cd "$repo_root"
    PATH="$stub_bin:$PATH" \
      STUB_CTEST_RC="$ctest_rc" \
      STUB_GCOVR_RC="$gcovr_rc" \
      "$script_under_test" \
      "$case_dir" \
      "$repo_root/specs/009-raise-pass-improvement-ratio/contracts/recovery-pass-artifact.schema.json" \
      "isched_stub_tests" \
      "$case_dir/scope.txt" \
      --status-file "$status_file"
  ) >"$case_dir/output.txt" 2>&1
  local actual_rc=$?
  set -e

  if [[ "$actual_rc" -ne "$expected_rc" ]]; then
    echo "Assertion failed for $name: expected rc=$expected_rc got rc=$actual_rc" >&2
    cat "$case_dir/output.txt" >&2
    exit 1
  fi

  assert_contains "$status_file" "\"overallStatus\": \"$expected_overall\""
  assert_contains "$status_file" "\"coverageStatus\": \"$expected_coverage\""
  assert_contains "$status_file" "\"docsSpecStatus\": \"$expected_docs\""
}

run_case "acceptance" 0 0 95.0 90.0 true 0 "PASS" "PASS" "PASS"
run_case "rejection_coverage_cmd" 0 1 95.0 90.0 true 1 "FAIL" "FAIL" "PASS"
run_case "rejection_coverage_threshold" 0 0 79.9 88.0 true 1 "FAIL" "FAIL" "PASS"
run_case "rejection_docs_spec" 0 0 95.0 90.0 false 1 "FAIL" "PASS" "FAIL"

echo "PASS: verify_pass_gates preserves tests/coverage/schema/docs gates"

