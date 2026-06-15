#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/docs/env_snapshots"
mkdir -p "$OUT_DIR"

TS="$(date +%Y%m%d_%H%M%S)"
OUT_FILE="$OUT_DIR/env_snapshot_${TS}.txt"

section() {
  printf '\n===== %s =====\n' "$1"
}

run_cmd() {
  local label="$1"
  shift
  section "$label"
  printf '$'
  printf ' %q' "$@"
  printf '\n'
  "$@" 2>&1 || printf '[command failed: exit=%s]\n' "$?"
}

run_shell() {
  local label="$1"
  local cmd="$2"
  section "$label"
  printf '$ %s\n' "$cmd"
  bash -lc "$cmd" 2>&1 || printf '[command failed: exit=%s]\n' "$?"
}

{
  section "Snapshot Metadata"
  echo "timestamp_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
  echo "timestamp_local=$(date '+%Y-%m-%d %H:%M:%S %Z')"
  echo "repo_root=$ROOT_DIR"
  echo "user=$(id -un 2>/dev/null || true)"
  echo "uid_gid=$(id 2>/dev/null || true)"
  echo "hostname=$(hostname 2>/dev/null || true)"

  run_cmd "Kernel" uname -a
  run_shell "OS Release" 'cat /etc/os-release 2>/dev/null || lsb_release -a 2>/dev/null || true'
  run_shell "CPU" 'lscpu 2>/dev/null || cat /proc/cpuinfo | head -120'
  run_shell "Memory" 'free -h 2>/dev/null || cat /proc/meminfo | head -40'
  run_shell "Load And Users" 'uptime; who 2>/dev/null || true'
  run_shell "Disk" 'df -h . "$HOME" /tmp 2>/dev/null | sed -n "1,20p"'

  run_shell "PATH" 'printf "%s\n" "$PATH" | tr ":" "\n"'
  run_shell "Ascend Environment Variables" 'env | sort | grep -Ei "ASCEND|CANN|DDK|NPU|ATC|TOOLKIT|LD_LIBRARY_PATH|PYTHONPATH" || true'
  run_shell "Ascend Directories" 'ls -ld /usr/local/Ascend* /usr/local/Ascend/* /opt/Ascend* /opt/ascend* 2>/dev/null || true'
  run_shell "Ascend Setenv Candidates" 'find /usr/local/Ascend /opt -maxdepth 5 \( -name set_env.sh -o -name ascend_toolkit_install.info -o -name version.info \) 2>/dev/null | sort || true'

  run_shell "NPU Device Nodes" 'ls -l /dev/davinci* /dev/davinci_manager /dev/hisi_hdc /dev/devmm_svm 2>/dev/null || true'
  run_shell "NPU SMI" 'command -v npu-smi; npu-smi info 2>/dev/null || true'
  run_shell "NPU SMI Detail" 'for i in 0 1 2 3 4 5 6 7; do npu-smi info -i "$i" -t board 2>/dev/null || true; npu-smi info -i "$i" -t usages 2>/dev/null || true; done'

  run_shell "Compiler And Build Tools" 'for c in gcc g++ clang clang++ cmake make ninja python python3 pip pip3 atc msprof aclprof ascendc ccec; do printf "%-10s " "$c"; command -v "$c" || true; done'
  run_shell "Tool Versions" 'python3 --version 2>&1 || true; gcc --version 2>&1 | head -1 || true; g++ --version 2>&1 | head -1 || true; cmake --version 2>&1 | head -1 || true; atc --version 2>&1 || true; msprof --version 2>&1 || true'
  run_shell "Python Packages" 'python3 - <<'"'"'PY'"'"'
import importlib.util
for name in ["numpy", "pandas", "matplotlib", "acl", "torch"]:
    spec = importlib.util.find_spec(name)
    print(f"{name}: {\"yes\" if spec else \"no\"}")
PY'

  run_shell "Git State" 'git rev-parse --show-toplevel 2>/dev/null; git status --short --branch 2>/dev/null || true; git log --oneline -1 2>/dev/null || true'
} > "$OUT_FILE"

printf 'Wrote full environment snapshot:\n  %s\n\n' "$OUT_FILE"
printf 'Quick summary:\n'
printf '  OS: '
grep -m1 '^PRETTY_NAME=' /etc/os-release 2>/dev/null | cut -d= -f2- | tr -d '"' || true
printf '  Kernel/arch: '
uname -rmo 2>/dev/null || true
printf '  NPU device nodes: '
find /dev -maxdepth 1 -name 'davinci[0-9]*' 2>/dev/null | wc -l | tr -d ' '
printf '\n'
printf '  npu-smi: '
command -v npu-smi 2>/dev/null || printf 'not found'
printf '\n'
printf '  CANN dirs: '
find /usr/local/Ascend -maxdepth 1 -type d -name 'cann-*' 2>/dev/null | xargs -n1 basename 2>/dev/null | tr '\n' ' '
printf '\n'
printf '  Build tools: '
for c in gcc g++ cmake make ninja python3 atc msprof; do
  if command -v "$c" >/dev/null 2>&1; then
    printf '%s ' "$c"
  fi
done
printf '\n\n'
printf 'Paste the snapshot file content only when detailed diagnosis is needed.\n'
