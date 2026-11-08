#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=""
OUTPUT_JSON=""
ITERATIONS=20

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --output)
      OUTPUT_JSON="$2"
      shift 2
      ;;
    --iterations)
      ITERATIONS="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$BUILD_DIR" || -z "$OUTPUT_JSON" ]]; then
  echo "Usage: $0 --build-dir <cmake-build-dir> --output <json-path> [--iterations N]" >&2
  exit 2
fi

TEST_BIN_DIR="$BUILD_DIR/src/test/cpp/isched"
PLUGIN_TESTS="$TEST_BIN_DIR/isched_plugin_system_tests"

if [[ ! -x "$PLUGIN_TESTS" ]]; then
  echo "Missing test binary: $PLUGIN_TESTS" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUTPUT_JSON")"

run_samples() {
  local filter="$1"
  local samples=()

  for ((i = 1; i <= ITERATIONS; ++i)); do
    local start_ns end_ns elapsed_ms
    start_ns=$(date +%s%N)
    "$PLUGIN_TESTS" "$filter" --verbosity quiet > /dev/null
    end_ns=$(date +%s%N)
    elapsed_ms=$(((end_ns - start_ns) / 1000000))
    samples+=("$elapsed_ms")
  done

  printf '%s\n' "${samples[@]}"
}

DISPATCH_FILTER='[dispatch][load][plugin][us1]'
GRAPHQL_FILTER='[fr021][plugin][schema][us2]'
RESTORE_FILTER='[plugin][restore][us4]'

DISPATCH_SAMPLES=$(run_samples "$DISPATCH_FILTER")
GRAPHQL_SAMPLES=$(run_samples "$GRAPHQL_FILTER")
RESTORE_SAMPLES=$(run_samples "$RESTORE_FILTER")

python3 - <<'PY' "$OUTPUT_JSON" "$BUILD_DIR" "$ITERATIONS" "$DISPATCH_FILTER" "$GRAPHQL_FILTER" "$RESTORE_FILTER" "$DISPATCH_SAMPLES" "$GRAPHQL_SAMPLES" "$RESTORE_SAMPLES"
import datetime
import json
import math
import sys

out_path, build_dir, iterations, dispatch_filter, graphql_filter, restore_filter, dispatch_raw, graphql_raw, restore_raw = sys.argv[1:]


def parse_samples(raw: str):
    return [int(v) for v in raw.splitlines() if v.strip()]


def metric_payload(name: str, test_filter: str, samples):
    ordered = sorted(samples)
    idx = max(0, math.ceil(0.95 * len(ordered)) - 1)
    return {
        "name": name,
        "filter": test_filter,
        "samplesMs": samples,
        "p95Ms": ordered[idx],
        "meanMs": round(sum(samples) / len(samples), 2),
        "minMs": min(samples),
        "maxMs": max(samples),
    }

payload = {
    "generatedAtUtc": datetime.datetime.now(datetime.timezone.utc).isoformat().replace("+00:00", "Z"),
    "buildDir": build_dir,
    "iterations": int(iterations),
    "scenarios": {
        "dispatch_request": metric_payload("dispatch_request", dispatch_filter, parse_samples(dispatch_raw)),
        "graphql_resolver_path": metric_payload("graphql_resolver_path", graphql_filter, parse_samples(graphql_raw)),
        "restore_persisted_plugins": metric_payload("restore_persisted_plugins", restore_filter, parse_samples(restore_raw)),
    },
}

with open(out_path, "w", encoding="utf-8") as f:
    json.dump(payload, f, indent=2)
    f.write("\n")

print(json.dumps(payload, indent=2))
PY

