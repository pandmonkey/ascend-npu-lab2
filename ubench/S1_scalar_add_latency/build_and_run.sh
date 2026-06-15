#!/usr/bin/env bash
set -euo pipefail

BENCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$BENCH_DIR/../.." && pwd)"

# shellcheck source=../../scripts/setup_env.sh
source "$ROOT_DIR/scripts/setup_env.sh"

BUILD_DIR="$BENCH_DIR/build_out"
rm -rf "$BUILD_DIR"
cmake -S "$BENCH_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "S1 build smoke test passed."
echo "No measurement row was written; cycle-level S1 timing is the next implementation step."

