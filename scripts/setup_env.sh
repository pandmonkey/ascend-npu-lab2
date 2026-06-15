#!/usr/bin/env bash
# Source this file from benchmark scripts:
#   source scripts/setup_env.sh

if [[ "${LAB2_ASCEND_ENV_READY:-0}" == "1" ]]; then
  return 0 2>/dev/null || exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

DEFAULT_CANN_PATH="/usr/local/Ascend/cann-9.0.0"
CANN_PATH="${ASCEND_CANN_PATH:-$DEFAULT_CANN_PATH}"
SET_ENV="$CANN_PATH/set_env.sh"

if [[ -f "$SET_ENV" ]]; then
  # shellcheck disable=SC1090
  source "$SET_ENV"
  export ASCEND_CANN_PATH="$CANN_PATH"
  export LAB2_ASCEND_ENV_READY=1
else
  echo "[WARN] CANN set_env.sh not found: $SET_ENV" >&2
  echo "[WARN] Set ASCEND_CANN_PATH or run on the Ascend host." >&2
fi

export LAB2_REPO_ROOT="$ROOT_DIR"

