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

cat >"$stub_bin/python3" <<'EOF'
#!/usr/bin/env bash
echo "stub python3 invoked: $*"
exit "${STUB_PYTHON_RC:-0}"
EOF

chmod +x "$stub_bin/ctest" "$stub_bin/gcovr" "$stub_bin/python3"

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
  local python_rc="$4"
  local expected_rc="$5"
  local expected_overall="$6"
  local expected_coverage="$7"

  local case_dir="$tmp_dir/$name"
  mkdir -p "$case_dir"
  cat >"$case_dir/refactor-pass-artifact.json" <<'EOF'
{"passId":"stub-pass"}
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
      STUB_PYTHON_RC="$python_rc" \
      "$script_under_test" \
      "$case_dir" \
      "$repo_root/specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json" \
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
}

run_case "acceptance" 0 0 0 0 "PASS" "PASS"
run_case "rejection_coverage" 0 1 0 1 "FAIL" "FAIL"

echo "PASS: verify_pass_gates acceptance/rejection cases passed"

