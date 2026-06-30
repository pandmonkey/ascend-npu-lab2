#!/usr/bin/env bash
# Run S2 benchmark independently.
# Delegates to the unified kernel in ubench/kernels/.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNELS_DIR="$(cd "$SCRIPT_DIR/../kernels" && pwd)"

# Build if needed
if [[ ! -f "$KERNELS_DIR/ubench_host" ]]; then
    echo "Building kernel..."
    bash "$KERNELS_DIR/build.sh"
    bash "$KERNELS_DIR/build_host.sh"
fi

echo "=== S2: S2 scalar add throughput ==="
cd "$KERNELS_DIR"
bash run.sh S2
