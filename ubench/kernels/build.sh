#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
source /usr/local/Ascend/cann-9.0.0/set_env.sh
rm -rf build
cmake -S . -B build \
  -DSOC_VERSION=ascend910b2 \
  -DASCEND_CANN_PACKAGE_PATH=/usr/local/Ascend/cann-9.0.0 \
  -DRUN_MODE=npu \
  -DASCEND_KERNEL_LAUNCH_ONLY=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
echo "build done"
ls -la build/lib
