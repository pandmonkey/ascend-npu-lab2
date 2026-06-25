#!/usr/bin/env bash
# Run a single benchmark by parameter ID (S1, V2, M3, etc.)
set -euo pipefail
cd "$(dirname "$0")"
PID="${1:-S1}"
source /usr/local/Ascend/cann-9.0.0/set_env.sh 2>/dev/null || true
export LD_LIBRARY_PATH="/usr/local/Ascend/cann-9.0.0/lib64:/usr/local/Ascend/cann-9.0.0/aarch64-linux/lib64:${LD_LIBRARY_PATH:-}"
./ubench_host --help 2>/dev/null || true
echo "Run: $PID"
case "$PID" in
    S1) ./ubench_host --mode 1 --chain 2000 --iters 10 --repeat 5 --label S1;;
    S2) ./ubench_host --mode 2 --chain 2000 --iters 10 --repeat 5 --label S2;;
    S3) ./ubench_host --mode 3 --chain 4000 --iters 10 --repeat 5 --data 65536 --label S3;;
    V1) ./ubench_host --mode 4 --chain 1000 --veclen 64 --iters 10 --repeat 5 --label V1;;
    V2) ./ubench_host --mode 5 --chain 1000 --veclen 64 --iters 10 --repeat 5 --label V2;;
    V3) ./ubench_host --mode 6 --chain 1000 --veclen 64 --iters 10 --repeat 5 --label V3;;
    V4) ./ubench_host --mode 7 --chain 1000 --veclen 64 --streams 4 --iters 10 --repeat 5 --label V4;;
    V5) ./ubench_host --mode 8 --chain 1000 --veclen 64 --iters 10 --repeat 5 --label V5;;
    C1) ./ubench_host --mode 9 --chain 100 --m 16 --n 16 --k 16 --iters 10 --repeat 5 --label C1;;
    C2) ./ubench_host --mode 10 --chain 100 --m 128 --n 128 --k 128 --iters 10 --repeat 5 --label C2;;
    C3) ./ubench_host --mode 11 --chain 100 --m 16 --n 16 --k 16 --iters 10 --repeat 5 --label C3;;
    C4) ./ubench_host --mode 12 --chain 1000 --buf 0 --iters 10 --repeat 5 --label C4;;
    C5) ./ubench_host --mode 13 --chain 100 --m 64 --n 64 --k 64 --iters 10 --repeat 5 --label C5;;
    M1) ./ubench_host --mode 14 --chain 65536 --data 524288 --iters 10 --repeat 5 --label M1;;
    M2) ./ubench_host --mode 15 --chain 65536 --data 524288 --iters 10 --repeat 5 --label M2;;
    M3) ./ubench_host --mode 16 --chain 65536 --data 524288 --iters 10 --repeat 5 --label M3;;
    M4) ./ubench_host --mode 17 --chain 65536 --data 524288 --iters 10 --repeat 5 --label M4;;
    M5) ./ubench_host --mode 18 --chain 65536 --data 524288 --iters 10 --repeat 5 --label M5;;
    M6) ./ubench_host --mode 19 --chain 4000 --data 65536 --iters 10 --repeat 5 --label M6;;
    M7) ./ubench_host --mode 20 --chain 32768 --data 524288 --iters 10 --repeat 5 --label M7;;
    M8) ./ubench_host --mode 21 --chain 256 --data 8192 --iters 10 --repeat 5 --label M8;;
    *) echo "Unknown: $PID"; exit 1;;
esac
