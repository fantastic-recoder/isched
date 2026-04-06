#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
script_under_test="$repo_root/tools/refactor_pass/evaluate_sc002_compliance.py"

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

write_ledger() {
  local out="$1"
  local ratio="$2"
  local improving="$3"
  local completed="$4"
  local entry_regression="$5"
  local evidence_json="$6"
  cat >"$out" <<EOF
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
      "regressionStatus": "$entry_regression",
      "improvingCount": 2,
      "completedCount": 3,
      "improvementRatioPct": 66.67,
      "evidenceRefs": $evidence_json,
      "recordedAt": "2026-04-06T00:00:00Z"
    }
  ],
  "currentSummary": {
    "improvingCount": $improving,
    "completedCount": $completed,
    "improvementRatioPct": $ratio,
    "sc002Compliant": false
  },
  "updatedAt": "2026-04-06T00:00:00Z"
}
EOF
}

existing_evidence="$tmp_dir/evidence-pass-03.json"
cat >"$existing_evidence" <<'EOF'
{"kind":"evidence"}
EOF

# Case 1: ratio >= 90 and no unmitigated regressions -> Met.
ledger_met="$tmp_dir/ledger-met.json"
output_met="$tmp_dir/decision-met.json"
write_ledger "$ledger_met" 90.0 9 10 None "[\"$existing_evidence\"]"
python3 "$script_under_test" \
  --ledger "$ledger_met" \
  --output "$output_met" \
  --evaluated-at "2026-04-06T00:00:01Z" >"$tmp_dir/met.out"
assert_contains "$output_met" '"sc002Status": "Met"'
assert_contains "$output_met" '"ratioPct": 90.0'

# Case 2: ratio below threshold -> NotMet.
ledger_notmet="$tmp_dir/ledger-notmet.json"
output_notmet="$tmp_dir/decision-notmet.json"
write_ledger "$ledger_notmet" 85.0 17 20 None "[\"$existing_evidence\"]"
python3 "$script_under_test" \
  --ledger "$ledger_notmet" \
  --output "$output_notmet" \
  --evaluated-at "2026-04-06T00:00:02Z" >"$tmp_dir/notmet.out"
assert_contains "$output_notmet" '"sc002Status": "NotMet"'
assert_contains "$output_notmet" '"ratioPct": 85.0'

# Case 3: missing evidence forces unresolved.
ledger_unresolved="$tmp_dir/ledger-unresolved.json"
output_unresolved="$tmp_dir/decision-unresolved.json"
write_ledger "$ledger_unresolved" 92.0 11 12 None '[]'
python3 "$script_under_test" \
  --ledger "$ledger_unresolved" \
  --output "$output_unresolved" \
  --evaluated-at "2026-04-06T00:00:03Z" >"$tmp_dir/unresolved.out"
assert_contains "$output_unresolved" '"sc002Status": "Unresolved"'
assert_contains "$output_unresolved" '"notes": "Missing evidence references'

# Case 4: unresolved when evidence refs do not resolve to existing files.
ledger_missing_files="$tmp_dir/ledger-missing-files.json"
output_missing_files="$tmp_dir/decision-missing-files.json"
write_ledger "$ledger_missing_files" 92.0 11 12 None '["/definitely/missing/evidence.json"]'
python3 "$script_under_test" \
  --ledger "$ledger_missing_files" \
  --output "$output_missing_files" \
  --evaluated-at "2026-04-06T00:00:04Z" >"$tmp_dir/missing-files.out"
assert_contains "$output_missing_files" '"sc002Status": "Unresolved"'
assert_contains "$output_missing_files" '"notes": "Missing evidence files'

echo "PASS: evaluate_sc002_compliance met/not-met/unresolved cases verified"

