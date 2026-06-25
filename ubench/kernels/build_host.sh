#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
source /usr/local/Ascend/cann-9.0.0/set_env.sh
CANN=/usr/local/Ascend/cann-9.0.0/aarch64-linux
g++ -std=c++17 -O2 -o ubench_host host/ubench_host.cpp \
  -I"$CANN/include" \
  build/lib/libubench_kernels.a \
  -L"$CANN/lib64" -lascendcl -lruntime -lascendc_runtime \
  -lregister -ltiling_api -lplatform -lerror_manager -lprofapi \
  -lge_common_base -lunified_dlog -lmmpa -lascend_dump -lc_sec \
  -ldl -lpthread
echo "host build done: $(pwd)/ubench_host"
