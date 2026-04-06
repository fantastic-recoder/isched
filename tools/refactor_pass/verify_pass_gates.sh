#!/usr/bin/env bash
set -uo pipefail

if [[ $# -lt 4 ]]; then
  cat >&2 <<'EOF'
Usage: verify_pass_gates.sh <pass-dir> <schema-path> <ctest-regex> <coverage-scope-file> [--status-file <path>]
Example:
  tools/refactor_pass/verify_pass_gates.sh \
    specs/008-dod-mech-refactor/artifacts/pass-01 \
    specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json \
    "isched_(gql_executor|graphql)_tests" \
    specs/008-dod-mech-refactor/artifacts/pass-01/affected-scope.txt
EOF
  exit 2
fi

pass_dir="$1"
schema_path="$2"
ctest_regex="$3"
scope_file="$4"
shift 4

status_file="${pass_dir}/gate-status.json"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --status-file)
      if [[ $# -lt 2 ]]; then
        echo "missing value for --status-file" >&2
        exit 2
      fi
      status_file="$2"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

mkdir -p "$pass_dir"
ctest_out="$pass_dir/ctest-green.txt"
coverage_out="$pass_dir/coverage.txt"
schema_out="$pass_dir/schema-validation.txt"
mkdir -p "$(dirname "$status_file")"

write_status() {
  local overall="$1"
  local ctest_status="$2"
  local coverage_status="$3"
  local schema_status="$4"
  local ctest_rc="$5"
  local coverage_rc="$6"
  local schema_rc="$7"
  cat >"$status_file" <<EOF
{
  "passDir": "$pass_dir",
  "overallStatus": "$overall",
  "ctestStatus": "$ctest_status",
  "coverageStatus": "$coverage_status",
  "schemaStatus": "$schema_status",
  "ctestExitCode": $ctest_rc,
  "coverageExitCode": $coverage_rc,
  "schemaExitCode": $schema_rc,
  "ctestOutput": "$ctest_out",
  "coverageOutput": "$coverage_out",
  "schemaOutput": "$schema_out",
  "timestamp": "$(date --iso-8601=seconds)"
}
EOF
}

ctest_rc=0
coverage_rc=0
schema_rc=0

(
  cd cmake-build-debug
  ctest --output-on-failure -R "$ctest_regex"
) 2>&1 | tee "$ctest_out"
ctest_rc=${PIPESTATUS[0]}

tools/refactor_pass/run_affected_coverage.sh "$scope_file" "$coverage_out" "$pass_dir/coverage.html"
coverage_rc=$?
python3 tools/refactor_pass/validate_pass_artifact.py \
  "$pass_dir/refactor-pass-artifact.json" \
  --schema "$schema_path" 2>&1 | tee "$schema_out"
schema_rc=${PIPESTATUS[0]}

ctest_status="PASS"
coverage_status="PASS"
schema_status="PASS"
overall_status="PASS"

if [[ $ctest_rc -ne 0 ]]; then
  ctest_status="FAIL"
  overall_status="FAIL"
fi
if [[ $coverage_rc -ne 0 ]]; then
  coverage_status="FAIL"
  overall_status="FAIL"
fi
if [[ $schema_rc -ne 0 ]]; then
  schema_status="FAIL"
  overall_status="FAIL"
fi

write_status "$overall_status" "$ctest_status" "$coverage_status" "$schema_status" "$ctest_rc" "$coverage_rc" "$schema_rc"

if [[ "$overall_status" == "PASS" ]]; then
  echo "PASS: all gates completed for $pass_dir"
  exit 0
fi

echo "FAIL: one or more gates failed for $pass_dir"
exit 1
