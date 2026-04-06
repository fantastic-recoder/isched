#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
script_under_test="$repo_root/tools/refactor_pass/run_pass_workflow.sh"

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

assert_not_contains() {
  local file="$1"
  local needle="$2"
  if grep -Fq "$needle" "$file"; then
    echo "Assertion failed: '$needle' unexpectedly found in $file" >&2
    echo "---- file contents ----" >&2
    cat "$file" >&2
    echo "-----------------------" >&2
    exit 1
  fi
}

pass_dir="$tmp_dir/pass-03"
mkdir -p "$pass_dir"

echo "baseline" >"$pass_dir/perf-baseline.txt"
cat >"$pass_dir/pass-summary.md" <<'EOF'
This pass includes data layout updates for index locality.
EOF
cat >"$pass_dir/recovery-pass-artifact.json" <<'EOF'
{
  "passId": "pass-03",
  "gates": {
    "testsGreen": true,
    "affectedLineCoveragePct": 91.0,
    "affectedBranchCoveragePct": 88.0,
    "artifactValidation": true,
    "docsSpecUpdated": true
  },
  "artifacts": {
    "docsSpecUpdates": ["specs/009-raise-pass-improvement-ratio/tasks.md"]
  }
}
EOF

missing_scope="$pass_dir/missing-scope.txt"

set +e
"$script_under_test" \
  "$pass_dir" \
  "isched_stub_tests" \
  "$missing_scope" \
  --schema "$repo_root/specs/009-raise-pass-improvement-ratio/contracts/recovery-pass-artifact.schema.json" >"$tmp_dir/workflow.out" 2>&1
actual_rc=$?
set -e

if [[ $actual_rc -eq 0 ]]; then
  echo "Expected workflow gate block case to fail" >&2
  cat "$tmp_dir/workflow.out" >&2
  exit 1
fi

log_file="$pass_dir/run-pass-workflow.log"
assert_contains "$log_file" "AnalyzeHotPaths: PASS"
assert_contains "$log_file" "RedefineDataLayout: PASS"
assert_contains "$log_file" "RefactorLogic: PASS"
assert_contains "$log_file" "ExpandTestsAndVerify: FAIL"
assert_not_contains "$log_file" "VerifyAndDocument: PASS"

echo "PASS: run_pass_workflow blocks ordered flow when preserved gates fail"

