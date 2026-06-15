#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/docs/benchmark_manifest.csv"
# shellcheck source=setup_env.sh
source "$ROOT_DIR/scripts/setup_env.sh"

usage() {
  cat <<'EOF'
Usage:
  bash scripts/run_one.sh PARAMETER_ID

Example:
  bash scripts/run_one.sh S1

The script looks up PARAMETER_ID in docs/benchmark_manifest.csv, then runs the
first executable it finds in the corresponding ubench directory:
  run.sh
  build_and_run.sh

Current scaffold directories mostly contain README files, so this will report
"not implemented" until a concrete benchmark is added.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" || $# -ne 1 ]]; then
  usage
  exit 0
fi

PARAMETER_ID="$1"

BENCH_DIR_NAME="$(python3 - "$MANIFEST" "$PARAMETER_ID" <<'PY'
import csv
import sys

manifest, target = sys.argv[1], sys.argv[2]
with open(manifest, newline="", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        if row["id"] == target:
            print(row["ubench_dir"])
            raise SystemExit(0)
raise SystemExit(1)
PY
)" || {
  echo "Unknown parameter id: $PARAMETER_ID" >&2
  exit 2
}

BENCH_DIR="$ROOT_DIR/ubench/$BENCH_DIR_NAME"
if [[ ! -d "$BENCH_DIR" ]]; then
  echo "Benchmark directory does not exist: $BENCH_DIR" >&2
  exit 3
fi

echo "parameter_id=$PARAMETER_ID"
echo "benchmark_dir=$BENCH_DIR_NAME"
echo "host=$(hostname 2>/dev/null || true)"
echo "time=$(date '+%Y-%m-%d %H:%M:%S %Z')"

if [[ -x "$BENCH_DIR/run.sh" ]]; then
  cd "$BENCH_DIR"
  exec ./run.sh
elif [[ -x "$BENCH_DIR/build_and_run.sh" ]]; then
  cd "$BENCH_DIR"
  exec ./build_and_run.sh
else
  echo "not implemented: expected executable run.sh or build_and_run.sh under $BENCH_DIR_NAME" >&2
  exit 10
fi
