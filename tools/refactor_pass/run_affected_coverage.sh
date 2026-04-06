#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "Usage: $0 <scope-file> <output-file> <gcovr-html-or-xml-report-path> [gcovr-extra-args...]" >&2
  exit 2
fi

scope_file="$1"
output_file="$2"
report_path="$3"
shift 3

if [[ ! -f "$scope_file" ]]; then
  echo "Scope file not found: $scope_file" >&2
  exit 2
fi

if ! command -v gcovr >/dev/null 2>&1; then
  echo "gcovr is required but not installed" | tee "$output_file"
  exit 1
fi

mapfile -t scope_filters < <(grep -vE '^\s*(#|$)' "$scope_file")
if [[ ${#scope_filters[@]} -eq 0 ]]; then
  echo "No scope filters found in $scope_file" | tee "$output_file"
  exit 2
fi

# Align branch gating to meaningful decision logic by excluding compiler-generated
# and exception-only branch edges that are not part of pass behavior acceptance.
gcovr_cmd=(
  gcovr
  --root .
  --txt
  --txt-metric branch
  --fail-under-line 80
  --fail-under-branch 80
  --exclude-unreachable-branches
  --exclude-throw-branches
  --gcov-ignore-errors all
  --print-summary
)
for f in "${scope_filters[@]}"; do
  gcovr_cmd+=(--filter "$f")
done
if [[ -n "$report_path" ]]; then
  gcovr_cmd+=(--html-details "$report_path")
fi
if [[ $# -gt 0 ]]; then
  gcovr_cmd+=("$@")
fi

mkdir -p "$(dirname "$output_file")"
{
  echo "# affected scope coverage gate"
  echo "timestamp=$(date --iso-8601=seconds)"
  echo "scope_file=$scope_file"
  echo "report_path=$report_path"
  echo "filters="
  printf '  %s\n' "${scope_filters[@]}"
  echo
  "${gcovr_cmd[@]}"
} 2>&1 | tee "$output_file"

