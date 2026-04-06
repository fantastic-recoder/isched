#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
script_under_test="$repo_root/tools/refactor_pass/update_improvement_ledger.py"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

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

make_artifact() {
  local out_path="$1"
  local pass_id="$2"
  cat >"$out_path" <<EOF
{
  "passId": "$pass_id",
  "selectedMetrics": [
    {
      "metricId": "wallTimeNs",
      "direction": "LowerIsBetter",
      "noiseFloorPct": 1.0,
      "workloadId": "graphql-hotpath"
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
      "variancePct": 0.9,
      "withinNoiseFloor": true
    },
    "evidenceFiles": [
      "$pass_id/perf-baseline.txt",
      "$pass_id/perf-post.txt"
    ]
  },
  "gates": {
    "testsGreen": true,
    "affectedLineCoveragePct": 92.1,
    "affectedBranchCoveragePct": 88.4,
    "artifactValidation": true,
    "docsSpecUpdated": true,
    "gateEvidenceFiles": [
      "$pass_id/ctest-green.txt",
      "$pass_id/coverage.txt"
    ]
  },
  "classification": {
    "improvement": "Improving",
    "regressionStatus": "None",
    "countsTowardRecovery": true,
    "reason": "hot path latency reduced"
  },
  "artifacts": {
    "sourceUpdates": ["src/main/cpp/isched/backend/isched_GqlExecutor.cpp"],
    "testUpdates": ["src/test/cpp/isched/isched_gql_executor_tests.cpp"],
    "performanceSummary": "$pass_id/performance-summary.md",
    "docsSpecUpdates": ["specs/009-raise-pass-improvement-ratio/research.md"]
  }
}
EOF
}

ledger_path="$tmp_dir/improvement-ratio-ledger.json"

# Foundational behavior: creating/bootstrapping ledger carries baseline forward.
python3 "$script_under_test" --ledger "$ledger_path" --updated-at "2026-04-06T00:00:00Z" >"$tmp_dir/bootstrap.out"
assert_contains "$ledger_path" '"improvingCount": 1'
assert_contains "$ledger_path" '"completedCount": 2'
assert_contains "$ledger_path" '"improvementRatioPct": 50.0'

# US1: baseline immutability guard.
python3 - <<'PY' "$ledger_path"
import json
import pathlib
import sys
path = pathlib.Path(sys.argv[1])
data = json.loads(path.read_text(encoding="utf-8"))
data["baseline"]["completedCount"] = 1
path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
PY
set +e
python3 "$script_under_test" --ledger "$ledger_path" >"$tmp_dir/immutability.out" 2>&1
immutability_rc=$?
set -e
if [[ $immutability_rc -eq 0 ]]; then
  echo "Expected baseline immutability case to fail" >&2
  cat "$tmp_dir/immutability.out" >&2
  exit 1
fi
assert_contains "$tmp_dir/immutability.out" "Baseline is immutable"

# Restore clean baseline for progression tests.
rm -f "$ledger_path"
python3 "$script_under_test" --ledger "$ledger_path" --updated-at "2026-04-06T00:00:01Z" >/dev/null

# US1: cumulative progression from baseline 1/2 to 9/10.
pass_ids=(pass-03 pass-04 pass-05 pass-06 pass-07 pass-08 pass-09 pass-10)
artifact_args=()
for pass_id in "${pass_ids[@]}"; do
  artifact_path="$tmp_dir/${pass_id}.json"
  make_artifact "$artifact_path" "$pass_id"
  artifact_args+=(--pass-artifact "$artifact_path")
done

python3 "$script_under_test" \
  --ledger "$ledger_path" \
  "${artifact_args[@]}" \
  --updated-at "2026-04-06T00:00:02Z" >"$tmp_dir/progression.out"

python3 - <<'PY' "$ledger_path"
import json
import pathlib
import sys
path = pathlib.Path(sys.argv[1])
ledger = json.loads(path.read_text(encoding="utf-8"))
summary = ledger["currentSummary"]
assert summary["improvingCount"] == 9, summary
assert summary["completedCount"] == 10, summary
assert summary["improvementRatioPct"] == 90.0, summary
assert summary["sc002Compliant"] is True, summary
assert len(ledger["entries"]) == 8, ledger["entries"]
PY

# US1: denominator manipulation is rejected (duplicate pass append).
set +e
python3 "$script_under_test" \
  --ledger "$ledger_path" \
  --pass-artifact "$tmp_dir/pass-03.json" >"$tmp_dir/duplicate.out" 2>&1
duplicate_rc=$?
set -e
if [[ $duplicate_rc -eq 0 ]]; then
  echo "Expected duplicate pass append to fail" >&2
  cat "$tmp_dir/duplicate.out" >&2
  exit 1
fi
assert_contains "$tmp_dir/duplicate.out" "Pass already exists in ledger"

echo "PASS: update_improvement_ledger baseline, progression, and denominator protections verified"

