#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=setup_env.sh
source "$ROOT_DIR/scripts/setup_env.sh"

CANN_PATH="${ASCEND_CANN_PATH:-/usr/local/Ascend/cann-9.0.0}"

section() {
  printf '\n===== %s =====\n' "$1"
}

section "CANN Path"
echo "$CANN_PATH"

section "Key Tools"
for c in atc msprof ccec gcc g++ cmake make ninja python3; do
  printf '%-8s ' "$c"
  command -v "$c" || true
done

section "Ascend C Core Files"
for p in \
  "$CANN_PATH/aarch64-linux/asc/include/kernel_operator.h" \
  "$CANN_PATH/aarch64-linux/tikcpp/ascendc_kernel_cmake/ascendc.cmake" \
  "$CANN_PATH/aarch64-linux/bin/ascendc_pack_kernel" \
  "$CANN_PATH/aarch64-linux/lib64/libascendc_runtime.a" \
  "$CANN_PATH/aarch64-linux/lib64/libascendcl.so"; do
  if [[ -e "$p" ]]; then
    ls -l "$p"
  else
    echo "missing: $p"
  fi
done

section "Operator Project Templates"
find "$CANN_PATH/tools/op_project_templates" "$CANN_PATH/tools/new_op_project_template" \
  -maxdepth 4 -type f 2>/dev/null | sort | sed -n '1,160p'

section "Template CMakeLists"
for p in \
  "$CANN_PATH/tools/op_project_templates/op_project_tmpl/CMakeLists.txt" \
  "$CANN_PATH/tools/new_op_project_template/custom_op/CMakeLists.txt"; do
  if [[ -f "$p" ]]; then
    echo "--- $p"
    sed -n '1,120p' "$p"
  fi
done

section "Template build.sh"
for p in \
  "$CANN_PATH/tools/op_project_templates/op_project_tmpl/build.sh" \
  "$CANN_PATH/tools/new_op_project_template/custom_op/build.sh"; do
  if [[ -f "$p" ]]; then
    echo "--- $p"
    sed -n '1,160p' "$p"
  fi
done

