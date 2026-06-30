#!/usr/bin/env bash
# Run all 21 microbenchmarks with appropriate parameters.
# Outputs JSON lines to stdout and raw data to data/raw/all_benchmarks.jsonl
# v2: improved sweep parameters, larger S3/M6 working sets, proper M3/M4/M5
set -uo pipefail
cd "$(dirname "$0")/.."
source /usr/local/Ascend/cann-9.0.0/set_env.sh 2>/dev/null || true
export LD_LIBRARY_PATH="/usr/local/Ascend/cann-9.0.0/lib64:/usr/local/Ascend/cann-9.0.0/aarch64-linux/lib64:${LD_LIBRARY_PATH:-}"

KERNELS_DIR="ubench/kernels"
BIN="$KERNELS_DIR/ubench_host"
OUTDIR="data/raw"
JSONL="$OUTDIR/all_benchmarks.jsonl"
mkdir -p "$OUTDIR"

DEVICE_ID=${DEVICE_ID:-0}

run() {
    local label=$1 mode=$2 chain=$3
    shift 3
    local args="--mode $mode --chain $chain --iters 20 --repeat 10 --label $label --device $DEVICE_ID $@"
    echo "[$(date +%H:%M:%S)] $label: $args" >&2
    "$BIN" $args 2>/dev/null || echo "{\"label\":\"$label\",\"error\":true}"
}

{
# ==================== SCALAR (S1-S3) ====================
# S1: scalar add latency - dependency chain sweep
for cl in 500 1000 2000 4000 8000 16000; do
    run "S1_cl${cl}" 1 $cl
done

# S2: scalar add throughput - 8 independent streams
for cl in 500 1000 2000 4000 8000; do
    run "S2_cl${cl}" 2 $cl
done

# S3: scalar memory latency - pointer chasing
# Working set must be >> 172MB L2. Use chain=64M entries = 256MB.
# But limit each chase to avoid timeout; use fewer chase steps with large arrays.
for entries in 1000000 4000000 16000000 64000000; do
    run "S3_e${entries}" 3 1000 "--data $((entries * 4 + 4096))"
done

# ==================== VECTOR (V1-V5) ====================
# V1: vector add latency - dependency chain sweep
for cl in 200 500 1000 2000 4000; do
    run "V1_cl${cl}" 4 $cl "--veclen 64"
done

# V2: vector mul latency - dependency chain sweep
for cl in 200 500 1000 2000 4000; do
    run "V2_cl${cl}" 5 $cl "--veclen 64"
done

# V3: vector add throughput - 4 independent adds per step
for cl in 200 500 1000 2000 4000; do
    run "V3_cl${cl}" 6 $cl "--veclen 64"
done

# V4: vector pipeline depth - sweep independent streams (1..8)
for ns in 1 2 3 4 5 6 7 8; do
    run "V4_s${ns}" 7 1000 "--veclen 64 --streams $ns"
done

# V5: vector register latency - sweep vecLen for per-element analysis
for cl in 200 500 1000 2000; do
    run "V5_cl${cl}_v64" 8 $cl "--veclen 64"
done
# Also sweep vecLen with fixed chainLen
for vl in 16 32 64 128; do
    run "V5_cl1000_v${vl}" 8 1000 "--veclen $vl"
done

# ==================== CUBE (C1-C5) ====================
# C1: single tile matmul latency (16x16x16), with PipeBarrier
for cl in 50 100 200 500 1000; do
    run "C1_cl${cl}" 9 $cl "--m 16 --n 16 --k 16"
done

# C2: matmul throughput - various sizes, no barrier
run "C2_16x16x16" 10 2000 "--m 16 --n 16 --k 16"
run "C2_32x32x32" 10 1000 "--m 32 --n 32 --k 32"
run "C2_64x64x64" 10 500 "--m 64 --n 64 --k 64"
run "C2_128x128x16" 10 500 "--m 128 --n 128 --k 16"

# C3: cube pipeline depth - sweep chain length to find throughput saturation
for cl in 1 2 4 8 16 32 64 128 256 500 1000; do
    run "C3_cl${cl}" 11 $cl "--m 16 --n 16 --k 16"
done

# C4: L0A/L0B/L0C access latency
# bufType=0: L0A via LoadData, bufType=1: L0B via LoadData, bufType=2: L0C via Mmad
for bt in 0 1 2; do
    for cl in 50 100 200 500 1000; do
        run "C4_b${bt}_cl${cl}" 12 $cl "--buf $bt"
    done
done

# C5: matmul scaling - sweep M/N/K
for sz in 16 32 48 64 96 128; do
    run "C5_${sz}x${sz}x${sz}" 13 100 "--m $sz --n $sz --k $sz"
done

# ==================== MTE (M1-M8) ====================
# M1: L1 read bandwidth - GM->UB, sweep data size
for b in 1024 4096 16384 32768 65536 131072; do
    run "M1_b${b}" 14 $b "--data 524288"
done

# M2: L1 write bandwidth - UB->GM, sweep data size
for b in 1024 4096 16384 32768 65536 131072; do
    run "M2_b${b}" 15 $b "--data 524288"
done

# M3: L0A bandwidth - LoadData UB->L0A (half precision)
for b in 512 1024 4096 16384 32768 65536; do
    run "M3_b${b}" 16 $b "--data 524288"
done

# M4: L0B bandwidth - LoadData UB->L0B (half precision)
for b in 512 1024 4096 16384 32768 65536; do
    run "M4_b${b}" 17 $b "--data 524288"
done

# M5: L0C bandwidth - Mmad writes to L0C, sweep chain
for cl in 100 200 500 1000 2000; do
    run "M5_cl${cl}" 18 $cl "--data 524288"
done

# M6: HBM memory latency - pointer chasing with large working set
# Need working set >> 172MB L2.
for entries in 1000000 4000000 16000000 64000000; do
    run "M6_e${entries}" 19 1000 "--data $((entries * 4 + 4096))"
done

# M7: buffer capacity sweep
for b in 1024 2048 4096 8192 16384 32768 65536 98304 131072 163840 196608 262144; do
    run "M7_b${b}" 20 $b "--data 524288"
done

# M8: MTE startup overhead - small data sizes for intercept
for b in 32 64 128 256 512 1024 2048 4096 8192; do
    run "M8_b${b}" 21 $b "--data 16384"
done
} | tee "$JSONL"

echo "=== Done. Results in $JSONL ===" >&2
