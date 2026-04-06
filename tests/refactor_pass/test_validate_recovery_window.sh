#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
script_under_test="$repo_root/tools/refactor_pass/validate_recovery_window.py"

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

artifacts_root="$tmp_dir/artifacts"
mkdir -p "$artifacts_root/pass-03"

cat >"$artifacts_root/pass-03/recovery-pass-artifact.json" <<'EOF'
{
  "passId": "pass-03",
  "classification": {
    "improvement": "Improving",
    "regressionStatus": "None",
    "countsTowardRecovery": true
  }
}
EOF

ledger_path="$tmp_dir/improvement-ratio-ledger.json"
cat >"$ledger_path" <<'EOF'
{
  "baseline": {
    "improvingCount": 1,
    "completedCount": 2,
    "sourceRef": "specs/008-dod-mech-refactor/artifacts/passes-rollup.md"
  },
  "entries": [
    {
      "passId": "pass-03",
      "classification": "Improving",
      "regressionStatus": "None",
      "improvingCount": 2,
      "completedCount": 3,
      "improvementRatioPct": 66.67,
      "evidenceRefs": ["specs/009-raise-pass-improvement-ratio/artifacts/pass-03/recovery-pass-artifact.json"],
      "recordedAt": "2026-04-06T00:00:00Z"
    }
  ],
  "currentSummary": {
    "improvingCount": 2,
    "completedCount": 3,
    "improvementRatioPct": 66.67,
    "sc002Compliant": false
  },
  "updatedAt": "2026-04-06T00:00:00Z"
}
EOF

# Case 1: matching decision should pass cross-validation.
decision_ok="$tmp_dir/decision-ok.json"
cat >"$decision_ok" <<'EOF'
{
  "decisionId": "sc002-2026-04-06T00:00:01Z",
  "featureRef": "009-raise-pass-improvement-ratio",
  "sc002Status": "NotMet",
  "numerator": 2,
  "denominator": 3,
  "ratioPct": 66.67,
  "evaluatedAt": "2026-04-06T00:00:01Z",
  "ledgerRef": "specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json",
  "evidenceSet": ["specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json"],
  "unmitigatedRegressionPasses": []
}
EOF

python3 "$script_under_test" \
  --ledger "$ledger_path" \
  --artifacts-root "$artifacts_root" \
  --decision "$decision_ok" >"$tmp_dir/pass.out"
assert_contains "$tmp_dir/pass.out" "PASS: recovery window artifacts are cross-consistent"

# Case 2: decision/ledger mismatch must fail.
decision_bad="$tmp_dir/decision-bad.json"
cat >"$decision_bad" <<'EOF'
{
  "decisionId": "sc002-2026-04-06T00:00:02Z",
  "featureRef": "009-raise-pass-improvement-ratio",
  "sc002Status": "NotMet",
  "numerator": 2,
  "denominator": 3,
  "ratioPct": 70.0,
  "evaluatedAt": "2026-04-06T00:00:02Z",
  "ledgerRef": "specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json",
  "evidenceSet": ["specs/009-raise-pass-improvement-ratio/artifacts/improvement-ratio-ledger.json"],
  "unmitigatedRegressionPasses": []
}
EOF

set +e
python3 "$script_under_test" \
  --ledger "$ledger_path" \
  --artifacts-root "$artifacts_root" \
  --decision "$decision_bad" >"$tmp_dir/fail.out" 2>&1
bad_rc=$?
set -e
if [[ $bad_rc -eq 0 ]]; then
  echo "Expected decision mismatch validation to fail" >&2
  cat "$tmp_dir/fail.out" >&2
  exit 1
fi
assert_contains "$tmp_dir/fail.out" "Decision ratioPct does not match ledger summary"

echo "PASS: validate_recovery_window enforces ledger/decision consistency"

