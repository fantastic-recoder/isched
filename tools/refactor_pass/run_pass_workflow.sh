#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
  cat >&2 <<'EOF'
Usage: run_pass_workflow.sh <pass-dir> <ctest-regex> <coverage-scope-file> [--schema <schema-path>] [--status-file <path>]

Example:
  tools/refactor_pass/run_pass_workflow.sh \
    specs/008-dod-mech-refactor/artifacts/pass-02 \
    "isched_subscription_broker_tests|test_graphql_subscriptions" \
    specs/008-dod-mech-refactor/artifacts/pass-02/affected-scope.txt \
    --schema specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json
EOF
  exit 2
fi

pass_dir="$1"
ctest_regex="$2"
scope_file="$3"
shift 3

schema_path="specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json"
if [[ "$pass_dir" == *"/009-raise-pass-improvement-ratio/"* ]]; then
  schema_path="specs/009-raise-pass-improvement-ratio/contracts/recovery-pass-artifact.schema.json"
fi
status_file=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --schema)
      if [[ $# -lt 2 ]]; then
        echo "missing value for --schema" >&2
        exit 2
      fi
      schema_path="$2"
      shift 2
      ;;
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
log_file="$pass_dir/run-pass-workflow.log"

log_stage() {
  local stage="$1"
  local result="$2"
  local detail="$3"
  printf '[%s] %s: %s - %s\n' "$(date --iso-8601=seconds)" "$stage" "$result" "$detail" | tee -a "$log_file"
}

require_file() {
  local stage="$1"
  local path="$2"
  if [[ ! -s "$path" ]]; then
    log_stage "$stage" "FAIL" "missing or empty $path"
    exit 1
  fi
}

: >"$log_file"

artifact_path="$pass_dir/recovery-pass-artifact.json"
if [[ ! -f "$artifact_path" ]]; then
  artifact_path="$pass_dir/refactor-pass-artifact.json"
fi

effective_status_file="$status_file"
if [[ -z "$effective_status_file" ]]; then
  effective_status_file="$pass_dir/gate-status.json"
fi

# Stage 1: Analyze hot path evidence must exist.
require_file "AnalyzeHotPaths" "$pass_dir/perf-baseline.txt"
require_file "AnalyzeHotPaths" "$pass_dir/pass-summary.md"
log_stage "AnalyzeHotPaths" "PASS" "baseline and analysis notes present"

# Stage 2: Data layout notes are part of pass-summary for this workflow.
if ! grep -Eiq 'data\s+layout|index' "$pass_dir/pass-summary.md"; then
  log_stage "RedefineDataLayout" "FAIL" "pass-summary.md must describe data layout/index mapping"
  exit 1
fi
log_stage "RedefineDataLayout" "PASS" "data layout/index notes found"

# Stage 3: Logic refactor evidence must be represented in artifact intent.
require_file "RefactorLogic" "$artifact_path"
log_stage "RefactorLogic" "PASS" "artifact intent present"

# Stage 4: Execute tests + coverage + schema gates.
verify_cmd=(tools/refactor_pass/verify_pass_gates.sh "$pass_dir" "$schema_path" "$ctest_regex" "$scope_file")
verify_cmd+=(--status-file "$effective_status_file")

set +e
"${verify_cmd[@]}" | tee -a "$log_file"
verify_rc=${PIPESTATUS[0]}
set -e
if [[ $verify_rc -ne 0 ]]; then
  log_stage "ExpandTestsAndVerify" "FAIL" "verification gates failed"
  exit 1
fi
log_stage "ExpandTestsAndVerify" "PASS" "all verification gates passed"

# Stage 5: Post-pass docs/perf closure must exist after verification.
require_file "VerifyAndDocument" "$pass_dir/perf-post.txt"
require_file "VerifyAndDocument" "$pass_dir/performance-summary.md"
require_file "VerifyAndDocument" "$pass_dir/schema-validation.txt"
require_file "VerifyAndDocument" "$effective_status_file"
log_stage "VerifyAndDocument" "PASS" "post-pass docs and schema evidence present"

echo "PASS: ordered workflow completed for $pass_dir" | tee -a "$log_file"

