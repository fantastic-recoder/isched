#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 ]]; then
  echo "Usage: $0 <baseline|post> <output-file> <command...>" >&2
  exit 2
fi

mode="$1"
output_file="$2"
shift 2

if [[ "$mode" != "baseline" && "$mode" != "post" ]]; then
  echo "mode must be 'baseline' or 'post'" >&2
  exit 2
fi

mkdir -p "$(dirname "$output_file")"

{
  echo "# perf capture"
  echo "mode=${mode}"
  echo "timestamp=$(date --iso-8601=seconds)"
  echo "command=$*"

  if command -v perf >/dev/null 2>&1; then
    echo
    echo "## perf stat"
    perf stat -e cycles,instructions,branches,branch-misses "$@" 2>&1
  else
    echo
    echo "## perf stat"
    echo "perf not found on PATH; falling back to /usr/bin/time"
    /usr/bin/time -f 'elapsed_seconds=%e\nuser_seconds=%U\nsys_seconds=%S\nmax_rss_kb=%M' "$@" 2>&1
  fi
} | tee "$output_file"

