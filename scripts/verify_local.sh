#!/usr/bin/env bash
# Local verification script for Lab2 project.
# Run this BEFORE submitting to ensure completeness.
# Does NOT require an Ascend NPU — only checks structure, scripts, and data.
set -uo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

PASS=0
FAIL=0
WARN=0

pass() { echo "[PASS] $1"; ((PASS++)); }
fail() { echo "[FAIL] $1"; ((FAIL++)); }
warn() { echo "[WARN] $1"; ((WARN++)); }

echo "=================================================="
echo "  Lab2 昇腾 NPU 微基准测试 — 本地验证"
echo "  $(date)"
echo "=================================================="
echo

# ---------- 1. Project structure ----------
echo "--- 1. Project Structure ---"

ALL_PIDS="S1 S2 S3 V1 V2 V3 V4 V5 C1 C2 C3 C4 C5 M1 M2 M3 M4 M5 M6 M7 M8"
DIRS=(
    S1_scalar_add_latency
    S2_scalar_add_throughput
    S3_scalar_memory_latency
    V1_fp32_vector_add_latency
    V2_fp32_vector_mul_latency
    V3_vector_add_throughput
    V4_vector_pipeline_depth
    V5_vector_register_latency
    C1_cube_tile_matmul_latency
    C2_cube_matmul_throughput
    C3_cube_pipeline_depth
    C4_l0abc_access_latency
    C5_matmul_scaling
    M1_l1_read_bandwidth
    M2_l1_write_bandwidth
    M3_l0a_bandwidth
    M4_l0b_bandwidth
    M5_l0c_bandwidth
    M6_hbm_memory_latency
    M7_buffer_capacity
    M8_mte_startup_overhead
)

dir_ok=0
for d in "${DIRS[@]}"; do
    dir="ubench/$d"
    if [ -d "$dir" ]; then
        ((dir_ok++))
    else
        fail "Missing directory: $dir"
    fi
done
if [ "$dir_ok" -eq 21 ]; then
    pass "All 21 ubench directories exist"
fi

# ---------- 2. Independent run.sh for each parameter ----------
echo
echo "--- 2. Independent run.sh Scripts ---"

runsh_ok=0
for d in "${DIRS[@]}"; do
    f="ubench/$d/run.sh"
    if [ -f "$f" ] && [ -x "$f" ] 2>/dev/null || head -1 "$f" 2>/dev/null | grep -q bash; then
        ((runsh_ok++))
    elif [ -f "$f" ]; then
        warn "run.sh exists but may not be executable: $f"
        ((runsh_ok++))
    else
        fail "Missing run.sh: $f"
    fi
done
if [ "$runsh_ok" -eq 21 ]; then
    pass "All 21 parameter directories have run.sh"
fi

# ---------- 3. Core kernel and host files ----------
echo
echo "--- 3. Core Build Files ---"

for f in \
    ubench/kernels/op_kernel/ubench.cpp \
    ubench/kernels/host/ubench_host.cpp \
    ubench/kernels/CMakeLists.txt \
    ubench/kernels/build.sh \
    ubench/kernels/build_host.sh \
    ubench/kernels/run.sh; do
    if [ -f "$f" ]; then
        pass "Found $f"
    else
        fail "Missing $f"
    fi
done

# ---------- 4. Scripts ----------
echo
echo "--- 4. Scripts ---"

for f in \
    scripts/run_all_ubench.sh \
    scripts/parse_results.py \
    scripts/plot_results.py; do
    if [ -f "$f" ]; then
        pass "Found $f"
    else
        fail "Missing $f"
    fi
done

# ---------- 5. Report ----------
echo
echo "--- 5. Report Completeness ---"

REPORT="report/report.md"
if [ ! -f "$REPORT" ]; then
    fail "Missing report: $REPORT"
else
    pass "Report file exists"

    # Check each parameter is covered in the report
    report_ok=0
    for pid in $ALL_PIDS; do
        if grep -q "$pid" "$REPORT"; then
            ((report_ok++))
        else
            fail "Report missing parameter: $pid"
        fi
    done
    if [ "$report_ok" -eq 21 ]; then
        pass "Report covers all 21 parameters"
    fi

    # Check for key sections
    for section in "摘要" "实验环境" "方法总览" "Scalar Unit" "Vector Unit" "Cube Unit" "MTE" "结果汇总" "结论"; do
        if grep -q "$section" "$REPORT"; then
            pass "Report has section: $section"
        else
            warn "Report may be missing section: $section"
        fi
    done

    # Check that each parameter has data tables (look for | --- | patterns near each PID)
    tables=0
    for pid in $ALL_PIDS; do
        if grep -A 20 "### .*$pid" "$REPORT" | grep -q "|.*|.*|"; then
            ((tables++))
        fi
    done
    if [ "$tables" -ge 18 ]; then
        pass "Most parameters have data tables ($tables/21)"
    else
        warn "Only $tables/21 parameters have data tables"
    fi

    # Count total lines
    lines=$(wc -l < "$REPORT")
    if [ "$lines" -ge 400 ]; then
        pass "Report has $lines lines (>= 400)"
    else
        warn "Report only has $lines lines (expected >= 400)"
    fi
fi

# ---------- 6. Data files ----------
echo
echo "--- 6. Data Files ---"

JSONL="data/raw/all_benchmarks.jsonl"
if [ ! -f "$JSONL" ]; then
    warn "No raw data file: $JSONL (need to run benchmarks first)"
else
    records=$(wc -l < "$JSONL")
    pass "JSONL has $records records"

    # Check all 21 parameters have data
    data_pids=0
    for pid in $ALL_PIDS; do
        if grep -q "\"${pid}_" "$JSONL" || grep -q "\"label\":\"${pid}" "$JSONL"; then
            ((data_pids++))
        else
            warn "JSONL missing data for: $pid"
        fi
    done
    if [ "$data_pids" -eq 21 ]; then
        pass "JSONL covers all 21 parameters"
    else
        warn "JSONL covers $data_pids/21 parameters"
    fi
fi

SUMMARY="data/processed/summary.csv"
if [ ! -f "$SUMMARY" ]; then
    warn "No summary file: $SUMMARY (run parse_results.py first)"
else
    summary_pids=$(tail -n +2 "$SUMMARY" | cut -d, -f1 | sort -u | wc -l | tr -d ' ')
    if [ "$summary_pids" -eq 21 ]; then
        pass "Summary.csv has all 21 parameters"
    else
        warn "Summary.csv has $summary_pids/21 parameters"
    fi

    # Check for obviously wrong values
    while IFS=, read -r pid unit count value notes; do
        [ "$pid" = "parameter_id" ] && continue
        # Check value is non-zero (except C5 which is a scaling relationship)
        if [ "$pid" != "C5" ] && echo "$value" | grep -q "^0\.0*$"; then
            warn "$pid has zero measured value"
        fi
        # Check for unreasonably large values (e.g., M5 with old data)
        if echo "$value" | awk '{if ($1 > 1e6 && "'$pid'" != "C2") exit 0; else exit 1}'; then
            warn "$pid has very large value ($value $unit) — may need re-measurement"
        fi
    done < "$SUMMARY"
fi

# ---------- 7. Parse results dry-run ----------
echo
echo "--- 7. Parse Results Validation ---"

if [ -f "$JSONL" ] && command -v python3 &>/dev/null; then
    output=$(python3 scripts/parse_results.py 2>&1)
    if [ $? -eq 0 ]; then
        pass "parse_results.py runs without errors"
    else
        fail "parse_results.py failed: $(echo "$output" | tail -3)"
    fi
else
    warn "Cannot run parse_results.py (missing python3 or JSONL data)"
fi

# ---------- 8. Kernel source sanity checks ----------
echo
echo "--- 8. Kernel Source Checks ---"

KERNEL="ubench/kernels/op_kernel/ubench.cpp"
if [ -f "$KERNEL" ]; then
    # Check all 21 modes exist
    modes_found=$(grep -c "case M_" "$KERNEL" 2>/dev/null || echo 0)
    if [ "$modes_found" -ge 21 ]; then
        pass "Kernel has $modes_found mode cases (>= 21)"
    else
        warn "Kernel only has $modes_found mode cases (expected >= 21)"
    fi

    # Check timing uses asc_get_system_cycle
    if grep -q "asc_get_system_cycle" "$KERNEL"; then
        pass "Kernel uses asc_get_system_cycle() for timing"
    else
        fail "Kernel does not use asc_get_system_cycle()"
    fi

    # Check TPosition usage for L0A/L0B/L0C
    for pos in "TPosition::A2" "TPosition::B2" "TPosition::CO1"; do
        if grep -q "$pos" "$KERNEL"; then
            pass "Kernel uses $pos"
        else
            warn "Kernel does not use $pos (may affect M3/M4/M5/C4 accuracy)"
        fi
    done

    # Check PipeBarrier usage
    if grep -q "PipeBarrier" "$KERNEL"; then
        pass "Kernel uses PipeBarrier for synchronization"
    else
        warn "Kernel missing PipeBarrier — timing may be inaccurate"
    fi
fi

# ---------- 9. Manifest check ----------
echo
echo "--- 9. Manifest ---"

MANIFEST="docs/benchmark_manifest.csv"
if [ -f "$MANIFEST" ]; then
    manifest_count=$(tail -n +2 "$MANIFEST" | grep -c .)
    if [ "$manifest_count" -eq 21 ]; then
        pass "Manifest has 21 parameters"
    else
        warn "Manifest has $manifest_count parameters (expected 21)"
    fi
else
    warn "Missing manifest: $MANIFEST"
fi

# ---------- Summary ----------
echo
echo "=================================================="
echo "  SUMMARY: $PASS passed, $FAIL failed, $WARN warnings"
echo "=================================================="

if [ "$FAIL" -gt 0 ]; then
    echo "  Some checks FAILED — fix before submitting."
    exit 1
elif [ "$WARN" -gt 3 ]; then
    echo "  Multiple warnings — review before submitting."
    exit 0
else
    echo "  Project looks good!"
    exit 0
fi
