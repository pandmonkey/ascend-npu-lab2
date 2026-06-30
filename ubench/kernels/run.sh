#!/usr/bin/env bash
# Run a single benchmark by parameter ID (S1, V2, M3, etc.)
# v2: updated parameters for improved measurements
set -euo pipefail
cd "$(dirname "$0")"
PID="${1:-S1}"
source /usr/local/Ascend/cann-9.0.0/set_env.sh 2>/dev/null || true
export LD_LIBRARY_PATH="/usr/local/Ascend/cann-9.0.0/lib64:/usr/local/Ascend/cann-9.0.0/aarch64-linux/lib64:${LD_LIBRARY_PATH:-}"

echo "=== Running benchmark: $PID ==="

case "$PID" in
    S1) # Scalar add latency - dependency chain sweep
        for cl in 500 1000 2000 4000 8000 16000; do
            ./ubench_host --mode 1 --chain $cl --iters 20 --repeat 10 --label "S1_cl${cl}"
        done;;
    S2) # Scalar throughput - 8 independent streams
        for cl in 500 1000 2000 4000 8000; do
            ./ubench_host --mode 2 --chain $cl --iters 20 --repeat 10 --label "S2_cl${cl}"
        done;;
    S3) # Scalar memory latency - pointer chasing
        for entries in 1000000 4000000 16000000 64000000; do
            ./ubench_host --mode 3 --chain 1000 --data $((entries * 4 + 4096)) --iters 20 --repeat 10 --label "S3_e${entries}"
        done;;
    V1) # FP32 vector add latency
        for cl in 200 500 1000 2000 4000; do
            ./ubench_host --mode 4 --chain $cl --veclen 64 --iters 20 --repeat 10 --label "V1_cl${cl}"
        done;;
    V2) # FP32 vector mul latency
        for cl in 200 500 1000 2000 4000; do
            ./ubench_host --mode 5 --chain $cl --veclen 64 --iters 20 --repeat 10 --label "V2_cl${cl}"
        done;;
    V3) # Vector add throughput
        for cl in 200 500 1000 2000 4000; do
            ./ubench_host --mode 6 --chain $cl --veclen 64 --iters 20 --repeat 10 --label "V3_cl${cl}"
        done;;
    V4) # Vector pipeline depth
        for ns in 1 2 3 4 5 6 7 8; do
            ./ubench_host --mode 7 --chain 1000 --veclen 64 --streams $ns --iters 20 --repeat 10 --label "V4_s${ns}"
        done;;
    V5) # Vector register latency
        for cl in 200 500 1000 2000; do
            ./ubench_host --mode 8 --chain $cl --veclen 64 --iters 20 --repeat 10 --label "V5_cl${cl}_v64"
        done;;
    C1) # Single tile matmul latency
        for cl in 50 100 200 500 1000; do
            ./ubench_host --mode 9 --chain $cl --m 16 --n 16 --k 16 --iters 20 --repeat 10 --label "C1_cl${cl}"
        done;;
    C2) # Matmul throughput
        ./ubench_host --mode 10 --chain 2000 --m 16 --n 16 --k 16 --iters 20 --repeat 10 --label "C2_16x16x16"
        ./ubench_host --mode 10 --chain 1000 --m 32 --n 32 --k 32 --iters 20 --repeat 10 --label "C2_32x32x32"
        ./ubench_host --mode 10 --chain 500 --m 64 --n 64 --k 64 --iters 20 --repeat 10 --label "C2_64x64x64";;
    C3) # Cube pipeline depth
        for cl in 1 2 4 8 16 32 64 128 256 500 1000; do
            ./ubench_host --mode 11 --chain $cl --m 16 --n 16 --k 16 --iters 20 --repeat 10 --label "C3_cl${cl}"
        done;;
    C4) # L0A/L0B/L0C access latency
        for bt in 0 1 2; do
            for cl in 50 100 200 500 1000; do
                ./ubench_host --mode 12 --chain $cl --buf $bt --iters 20 --repeat 10 --label "C4_b${bt}_cl${cl}"
            done
        done;;
    C5) # Matmul scaling
        for sz in 16 32 48 64 96 128; do
            ./ubench_host --mode 13 --chain 100 --m $sz --n $sz --k $sz --iters 20 --repeat 10 --label "C5_${sz}x${sz}x${sz}"
        done;;
    M1) # L1 read bandwidth
        for b in 1024 4096 16384 32768 65536 131072; do
            ./ubench_host --mode 14 --chain $b --data 524288 --iters 20 --repeat 10 --label "M1_b${b}"
        done;;
    M2) # L1 write bandwidth
        for b in 1024 4096 16384 32768 65536 131072; do
            ./ubench_host --mode 15 --chain $b --data 524288 --iters 20 --repeat 10 --label "M2_b${b}"
        done;;
    M3) # L0A bandwidth (LoadData UB->L0A)
        for b in 512 1024 4096 16384 32768 65536; do
            ./ubench_host --mode 16 --chain $b --data 524288 --iters 20 --repeat 10 --label "M3_b${b}"
        done;;
    M4) # L0B bandwidth (LoadData UB->L0B)
        for b in 512 1024 4096 16384 32768 65536; do
            ./ubench_host --mode 17 --chain $b --data 524288 --iters 20 --repeat 10 --label "M4_b${b}"
        done;;
    M5) # L0C bandwidth (via Mmad)
        for cl in 100 200 500 1000 2000; do
            ./ubench_host --mode 18 --chain $cl --data 524288 --iters 20 --repeat 10 --label "M5_cl${cl}"
        done;;
    M6) # HBM memory latency
        for entries in 1000000 4000000 16000000 64000000; do
            ./ubench_host --mode 19 --chain 1000 --data $((entries * 4 + 4096)) --iters 20 --repeat 10 --label "M6_e${entries}"
        done;;
    M7) # Buffer capacity sweep
        for b in 1024 2048 4096 8192 16384 32768 65536 98304 131072 163840 196608 262144; do
            ./ubench_host --mode 20 --chain $b --data 524288 --iters 20 --repeat 10 --label "M7_b${b}"
        done;;
    M8) # MTE startup overhead
        for b in 32 64 128 256 512 1024 2048 4096 8192; do
            ./ubench_host --mode 21 --chain $b --data 16384 --iters 20 --repeat 10 --label "M8_b${b}"
        done;;
    *) echo "Unknown parameter: $PID"; exit 1;;
esac
echo "=== $PID done ==="
