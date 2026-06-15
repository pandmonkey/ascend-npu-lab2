#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/docs/benchmark_manifest.csv"
LOG_DIR="$ROOT_DIR/data/processed/run_logs"
mkdir -p "$LOG_DIR"
# shellcheck source=setup_env.sh
source "$ROOT_DIR/scripts/setup_env.sh"

ONLY_CORE=0
if [[ "${1:-}" == "--core" ]]; then
  ONLY_CORE=1
fi

PARAM_FILE="$(mktemp "${TMPDIR:-/tmp}/lab2_params.XXXXXX")"
trap 'rm -f "$PARAM_FILE"' EXIT

python3 - "$MANIFEST" "$ONLY_CORE" > "$PARAM_FILE" <<'PY'
import csv
import sys

manifest, only_core = sys.argv[1], sys.argv[2] == "1"
with open(manifest, newline="", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        if only_core and row["priority"] != "core":
            continue
        print(row["id"])
PY

FAILED=0
while IFS= read -r parameter_id; do
  [[ -n "$parameter_id" ]] || continue
  echo "===== Running $parameter_id ====="
  log_file="$LOG_DIR/${parameter_id}_$(date +%Y%m%d_%H%M%S).log"
  if bash "$ROOT_DIR/scripts/run_one.sh" "$parameter_id" 2>&1 | tee "$log_file"; then
    echo "===== $parameter_id: OK ====="
  else
    status=${PIPESTATUS[0]}
    echo "===== $parameter_id: FAILED exit=$status ====="
    FAILED=$((FAILED + 1))
  fi
done < "$PARAM_FILE"

python3 "$ROOT_DIR/scripts/summarize_results.py"

if [[ "$FAILED" -ne 0 ]]; then
  echo "$FAILED benchmark(s) failed or are not implemented" >&2
  exit 1
fi

echo "all requested benchmarks completed"
