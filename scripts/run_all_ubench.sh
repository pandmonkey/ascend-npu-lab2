#!/usr/bin/env bash
# Run all 21 microbenchmarks with appropriate parameters.
# Outputs JSON lines to stdout and raw data to data/raw/measurements.csv
set -uo pipefail
cd "$(dirname "$0")/.."
source /usr/local/Ascend/cann-9.0.0/set_env.sh 2>/dev/null || true
export LD_LIBRARY_PATH="/usr/local/Ascend/cann-9.0.0/lib64:/usr/local/Ascend/cann-9.0.0/aarch64-linux/lib64:${LD_LIBRARY_PATH:-}"

KERNELS_DIR="ubench/kernels"
BIN="$KERNELS_DIR/ubench_host"
OUTDIR="data/raw"
JSONL="$OUTDIR/all_benchmarks.jsonl"
mkdir -p "$OUTDIR"

# Device info
DEVICE_ID=${DEVICE_ID:-0}

run() {
    # run <label> <mode> <chain> <extra_args...>
    local label=$1 mode=$2 chain=$3
    shift 3
    local args="--mode $mode --chain $chain --iters 10 --repeat 5 --label $label --device $DEVICE_ID $@"
    echo "[$(date +%H:%M:%S)] $label: $args" >&2
    "$BIN" $args 2>/dev/null || echo "{\"label\":\"$label\",\"error\":true}" 
}

{
# ==================== SCALAR (S1-S3) ====================
# S1: scalar add latency - sweep chain length for linear fit
for cl in 500 1000 2000 4000 8000; do
    run "S1_cl${cl}" 1 $cl "--veclen 64"
done

# S2: scalar add throughput - 8 independent streams
for cl in 500 1000 2000 4000; do
    run "S2_cl${cl}" 2 $cl
done

# S3: scalar memory latency - pointer chasing in GM (large working set >64KB)
for cl in 16000 32000 64000 128000 256000; do
    run "S3_cl${cl}" 3 $cl "--data $((cl * 4 + 4096))"
done

# ==================== VECTOR (V1-V5) ====================
# V1: vector add latency - sweep chain length
for cl in 200 500 1000 2000 4000; do
    run "V1_cl${cl}" 4 $cl "--veclen 64"
done

# V2: vector mul latency - sweep chain length
for cl in 200 500 1000 2000 4000; do
    run "V2_cl${cl}" 5 $cl "--veclen 64"
done

# V3: vector add throughput - sweep chain length
for cl in 200 500 1000 2000 4000; do
    run "V3_cl${cl}" 6 $cl "--veclen 64"
done

# V4: vector pipeline depth - sweep numStreams (1..8)
for ns in 1 2 3 4 5 6 7 8; do
    run "V4_s${ns}" 7 1000 "--veclen 64 --streams $ns"
done

# V5: vector register latency - sweep chain length
for cl in 200 500 1000 2000; do
    run "V5_cl${cl}" 8 $cl "--veclen 64"
done

# ==================== CUBE (C1-C5) ====================
# C1: single tile matmul latency (16x16x16)
for cl in 100 200 500 1000; do
    run "C1_cl${cl}" 9 $cl "--m 16 --n 16 --k 16"
done

# C2: matmul throughput - various sizes (L0A/L0B limited to ~64KB each)
run "C2_16x16x16" 10 1000 "--m 16 --n 16 --k 16"
run "C2_32x32x32" 10 500 "--m 32 --n 32 --k 32"
run "C2_64x64x64" 10 200 "--m 64 --n 64 --k 64"
run "C2_128x128x16" 10 200 "--m 128 --n 128 --k 16"

# C3: cube pipeline depth - sweep chain length (independent tiles)
for cl in 10 50 100 200 500; do
    run "C3_cl${cl}" 11 $cl "--m 16 --n 16 --k 16"
done

# C4: L0A/L0B/L0C access latency
for bt in 0 1 2; do
    for cl in 100 500 1000 2000; do
        run "C4_b${bt}_cl${cl}" 12 $cl "--buf $bt"
    done
done

# C5: matmul scaling - sweep M/N/K (keep within L0A capacity)
for sz in 16 32 48 64 96 128; do
    run "C5_${sz}x${sz}x${sz}" 13 100 "--m $sz --n $sz --k $sz"
done

# ==================== MTE (M1-M8) ====================
# M1: L1 read bandwidth - sweep data size
for b in 4096 16384 65536 131072; do
    run "M1_b${b}" 14 $b "--data 524288"
done

# M2: L1 write bandwidth - sweep data size
for b in 4096 16384 65536 131072; do
    run "M2_b${b}" 15 $b "--data 524288"
done

# M3: L0A read bandwidth (use GM->UB as proxy)
for b in 4096 16384 65536 131072; do
    run "M3_b${b}" 16 $b "--data 524288"
done

# M4: L0B read bandwidth
for b in 4096 16384 65536 131072; do
    run "M4_b${b}" 17 $b "--data 524288"
done

# M5: L0C write bandwidth (use UB->GM as proxy)
for b in 4096 16384 65536 131072; do
    run "M5_b${b}" 18 $b "--data 524288"
done

# M6: HBM memory latency - pointer chasing (large working set >64KB)
for cl in 16000 32000 64000 128000 256000; do
    run "M6_cl${cl}" 19 $cl "--data $((cl * 4 + 4096))"
done

# M7: buffer capacity sweep - find inflection
for b in 1024 2048 4096 8192 16384 32768 65536 131072; do
    run "M7_b${b}" 20 $b "--data 524288"
done

# M8: MTE startup overhead - small data sizes for intercept
for b in 32 64 128 256 512 1024 2048 4096; do
    run "M8_b${b}" 21 $b "--data 8192"
done
} | tee "$JSONL"

echo "=== Done. Results in $JSONL ===" >&2
