#!/usr/bin/env bash
set -euo pipefail

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
artifact_path="$pass_dir/recovery-pass-artifact.json"
if [[ ! -f "$artifact_path" ]]; then
  artifact_path="$pass_dir/refactor-pass-artifact.json"
fi
mkdir -p "$(dirname "$status_file")"

artifact_line_coverage=0
artifact_branch_coverage=0
artifact_tests_green=false
artifact_validation=true
artifact_docs_spec=false
artifact_docs_updates_count=0

write_status() {
  local overall="$1"
  local ctest_status="$2"
  local coverage_status="$3"
  local schema_status="$4"
  local docs_status="$5"
  local ctest_rc="$6"
  local coverage_rc="$7"
  local schema_rc="$8"
  cat >"$status_file" <<EOF
{
  "passDir": "$pass_dir",
  "artifactPath": "$artifact_path",
  "overallStatus": "$overall",
  "ctestStatus": "$ctest_status",
  "coverageStatus": "$coverage_status",
  "schemaStatus": "$schema_status",
  "docsSpecStatus": "$docs_status",
  "ctestExitCode": $ctest_rc,
  "coverageExitCode": $coverage_rc,
  "schemaExitCode": $schema_rc,
  "lineCoveragePct": $artifact_line_coverage,
  "branchCoveragePct": $artifact_branch_coverage,
  "testsGreen": $artifact_tests_green,
  "artifactValidation": $artifact_validation,
  "docsSpecUpdated": $artifact_docs_spec,
  "docsSpecUpdatesCount": $artifact_docs_updates_count,
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

set +e
readarray -t gate_fields < <(python3 - "$artifact_path" <<'PY'
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
if not path.exists():
    raise SystemExit(2)

data = json.loads(path.read_text(encoding="utf-8"))
gates = data.get("gates")
artifacts = data.get("artifacts")
if not isinstance(gates, dict) or not isinstance(artifacts, dict):
    raise SystemExit(3)

docs_updates = artifacts.get("docsSpecUpdates", [])
if not isinstance(docs_updates, list):
    docs_updates = []

print(float(gates.get("affectedLineCoveragePct", 0.0)))
print(float(gates.get("affectedBranchCoveragePct", 0.0)))
print("true" if bool(gates.get("testsGreen", False)) else "false")
print("true" if bool(gates.get("artifactValidation", False)) else "false")
print("true" if bool(gates.get("docsSpecUpdated", False)) else "false")
print(len(docs_updates))
PY
)
artifact_parse_output_rc=$?
set -e

if [[ $artifact_parse_output_rc -ne 0 ]]; then
  echo "failed to parse gate fields from $artifact_path" | tee "$schema_out"
  write_status "FAIL" "FAIL" "FAIL" "FAIL" "FAIL" 1 1 1
  exit 1
fi


artifact_line_coverage="${gate_fields[0]}"
artifact_branch_coverage="${gate_fields[1]}"
artifact_tests_green="${gate_fields[2]}"
artifact_validation="${gate_fields[3]}"
artifact_docs_spec="${gate_fields[4]}"
artifact_docs_updates_count="${gate_fields[5]}"

set +e
(
  cd cmake-build-debug
  ctest --output-on-failure -R "$ctest_regex"
) 2>&1 | tee "$ctest_out"
ctest_rc=${PIPESTATUS[0]}

tools/refactor_pass/run_affected_coverage.sh "$scope_file" "$coverage_out" "$pass_dir/coverage.html"
coverage_rc=$?
python3 tools/refactor_pass/validate_pass_artifact.py \
  "$artifact_path" \
  --schema "$schema_path" 2>&1 | tee "$schema_out"
schema_rc=${PIPESTATUS[0]}
set -e

ctest_status="PASS"
coverage_status="PASS"
schema_status="PASS"
docs_status="PASS"
overall_status="PASS"

if [[ $ctest_rc -ne 0 || "$artifact_tests_green" != "true" ]]; then
  ctest_status="FAIL"
  overall_status="FAIL"
fi
line_gate_ok=0
branch_gate_ok=0
if awk -v v="$artifact_line_coverage" 'BEGIN { exit !(v+0 >= 80) }'; then
  line_gate_ok=1
fi
if awk -v v="$artifact_branch_coverage" 'BEGIN { exit !(v+0 >= 80) }'; then
  branch_gate_ok=1
fi

if [[ $coverage_rc -ne 0 || $line_gate_ok -ne 1 || $branch_gate_ok -ne 1 ]]; then
  coverage_status="FAIL"
  overall_status="FAIL"
fi
if [[ $schema_rc -ne 0 || "$artifact_validation" != "true" ]]; then
  schema_status="FAIL"
  overall_status="FAIL"
fi
if [[ "$artifact_docs_spec" != "true" || "$artifact_docs_updates_count" -lt 1 ]]; then
  docs_status="FAIL"
  overall_status="FAIL"
fi

write_status "$overall_status" "$ctest_status" "$coverage_status" "$schema_status" "$docs_status" "$ctest_rc" "$coverage_rc" "$schema_rc"

if [[ "$overall_status" == "PASS" ]]; then
  echo "PASS: all gates completed for $pass_dir"
  exit 0
fi

echo "FAIL: one or more gates failed for $pass_dir"
exit 1
