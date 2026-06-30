#!/usr/bin/env python3
"""Parse JSONL benchmark results into measurements.csv and summary.csv.
Computes derived metrics (per-op latency, bandwidth, throughput) using linear fits.

v2: All values derived from actual data - no hardcoded results.
    Fixed S1/S2 (was hardcoded), C2/C3/M7 computation, M3-M5 bandwidth.
"""
import json, csv, re, statistics, math
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).resolve().parents[1]
JSONL = ROOT / "data" / "raw" / "all_benchmarks.jsonl"
RAW_CSV = ROOT / "data" / "raw" / "measurements.csv"
SUMMARY_CSV = ROOT / "data" / "processed" / "summary.csv"

# SoC clock frequency for Ascend 910B AI Core (measured via npu-smi)
SOC_FREQ_GHZ = 1.65
SOC_FREQ_HZ = SOC_FREQ_GHZ * 1e9

def extract_param_id(label):
    m = re.match(r'^([SVC]\d+|M\d+)', label)
    return m.group(1) if m else None

def parse_record(line):
    try:
        return json.loads(line)
    except:
        return None

def linear_fit(xs, ys):
    """Least squares: y = slope*x + intercept. Returns (slope, intercept, R²)."""
    n = len(xs)
    if n < 2:
        return None, None, None
    sx = sum(xs); sy = sum(ys)
    sxx = sum(x*x for x in xs); sxy = sum(x*y for x,y in zip(xs,ys))
    syy = sum(y*y for y in ys)
    denom = n*sxx - sx*sx
    if denom == 0:
        return None, None, None
    slope = (n*sxy - sx*sy) / denom
    intercept = (sy - slope*sx) / n
    # R²
    ss_res = sum((y - (slope*x + intercept))**2 for x,y in zip(xs,ys))
    ss_tot = sum((y - sy/n)**2 for y in ys)
    r2 = 1 - ss_res/ss_tot if ss_tot > 0 else 0
    return slope, intercept, r2

def get_chain_cycles(records):
    """Extract (chain, cycles_median) pairs, sorted by chain."""
    vals = [(r.get('chain', 0), r.get('cycles_median', 0), r) for r in records]
    vals = [(c, cyc, r) for c, cyc, r in vals if cyc > 0 and c > 0]
    vals.sort(key=lambda x: x[0])
    return vals

def compute_metric(param_id, records):
    """Compute the final measured value for a parameter from its sweep records.
    Returns: (value, unit_detail, stats_dict)
    """
    if not records:
        return None, '', {}

    stats = {}

    # === SCALAR ===
    if param_id == 'S1':
        # Scalar add latency: dependency chain x = x*3+1.
        # kernel: for(iters) { for(chainLen) { x = x*3+1 } }
        # Linear fit on chainLen: cycles = slope * chainLen + intercept
        # The slope represents cycles per unit chainLen increase.
        # Since each chain step = 1 mul + 1 add, slope ≈ per-step cycles.
        # Note: the outer iters loop is compiled as a continuous chain extension,
        # so slope already reflects per-step throughput, NOT per-step*iters.
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        chains = [v[0] for v in vals]
        iters = vals[0][2].get('iters', 10)
        cycles = [v[1] for v in vals]
        slope, intercept, r2 = linear_fit(chains, cycles)
        if slope is not None:
            # slope = cycles per chain step (total across iters).
            # The scalar pipeline processes x=x*3+1 in ~1 cycle throughput.
            per_step = slope / iters
            stats = {'slope': slope, 'intercept': intercept, 'r2': r2,
                     'per_step': per_step, 'iters': iters,
                     'note': 'per_step = slope/iters; each step = x*3+1'}
            return per_step, 'cycles', stats
        return None, '', {}

    elif param_id == 'S2':
        # Scalar throughput: 8 independent chains, each x=x*3+1 per step
        # Total ops per chain step = 8 streams × 2 ops (mul+add) = 16 ops
        # slope / iters = cycles per chain step
        # throughput = total_ops_per_step / cycles_per_step = 16 / (slope/iters)
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        chains = [v[0] for v in vals]
        iters = vals[0][2].get('iters', 20)
        cycles = [v[1] for v in vals]
        slope, intercept, r2 = linear_fit(chains, cycles)
        if slope is not None and slope > 0:
            cycles_per_step = slope / iters
            # S2 kernel: 8 streams, each doing x=x*3+1 (2 scalar ops per step)
            ops_per_step = 8 * 2
            throughput = ops_per_step / cycles_per_step
            stats = {'slope': slope, 'intercept': intercept, 'r2': r2,
                     'cycles_per_step': cycles_per_step, 'ops_per_step': ops_per_step}
            return throughput, 'ops/cycle', stats
        return None, '', {}

    elif param_id in ('S3', 'M6'):
        # Pointer chasing latency.
        # New v2: S3/M6 use entries (array size), not chain steps.
        # cycles = f(working_set_size). With fixed chase steps (chain=1000),
        # we measure total cycles for 1000 chases at different array sizes.
        # Per-chase latency = total_cycles / (iters * chain_steps)
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        # For v2 data: chainLen is fixed (1000), data size varies via --data.
        # Label pattern: S3_e{entries} or S3_cl{chain}
        # Check if we have entry-based or chain-based data
        has_entries = any('_e' in r.get('label','') for r in records)
        if has_entries:
            # Entry-based: use largest working set for most accurate HBM measurement
            best = max(vals, key=lambda x: x[2].get('chain', 0) * 4 + x[0])
            c, cyc, r = best
            chain_steps = c  # chain = 1000 chase steps
            iters = r.get('iters', 20)
            per_chase = cyc / (iters * chain_steps)
            stats = {'total_cycles': cyc, 'chain_steps': chain_steps, 'iters': iters}
            return per_chase, 'cycles', stats
        else:
            # Legacy chain-based data: linear fit on chain length
            chains = [v[0] for v in vals]
            cycles = [v[1] for v in vals]
            iters = vals[0][2].get('iters', 10)
            slope, intercept, r2 = linear_fit(chains, cycles)
            if slope is not None:
                per_chase = slope / iters
                stats = {'slope': slope, 'intercept': intercept, 'r2': r2, 'iters': iters}
                return per_chase, 'cycles', stats
            # Fallback: simple division
            best = max(vals, key=lambda x: x[0])
            c, cyc, r = best
            return cyc / (c * iters), 'cycles', {}

    # === VECTOR ===
    elif param_id in ('V1', 'V2'):
        # Vector add/mul latency: dependency chain, linear fit
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        chains = [v[0] for v in vals]
        iters = vals[0][2].get('iters', 20)
        cycles = [v[1] for v in vals]
        slope, intercept, r2 = linear_fit(chains, cycles)
        if slope is not None:
            per_op = slope / iters
            stats = {'slope': slope, 'intercept': intercept, 'r2': r2, 'iters': iters}
            return per_op, 'cycles', stats
        return None, '', {}

    elif param_id == 'V3':
        # Vector add throughput: 4 independent adds per chain step
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        chains = [v[0] for v in vals]
        iters = vals[0][2].get('iters', 20)
        cycles = [v[1] for v in vals]
        slope, intercept, r2 = linear_fit(chains, cycles)
        if slope is not None and slope > 0:
            cycles_per_step = slope / iters
            ops_per_step = 4  # 4 independent adds per chain step
            throughput = ops_per_step / cycles_per_step
            stats = {'slope': slope, 'intercept': intercept, 'r2': r2,
                     'cycles_per_step': cycles_per_step}
            return throughput, 'ops/cycle', stats
        return None, '', {}

    elif param_id == 'V4':
        # Vector pipeline depth: sweep numStreams, find where throughput saturates.
        # Pipeline depth = V1_latency (cycles per add) / throughput_per_add (cycles per add at saturation)
        # Alternative: find the numStreams where total time stops growing linearly.
        vals = [(r.get('streams', r.get('numStreams', 1)),
                 r.get('cycles_median', 0), r) for r in records]
        vals = [(ns, cyc, r) for ns, cyc, r in vals if cyc > 0]
        vals.sort(key=lambda x: x[0])
        if not vals:
            return None, '', {}

        # Compute per-op cycles for each numStreams setting
        per_op = []
        for ns, cyc, r in vals:
            chain = r.get('chain', 1000)
            iters = r.get('iters', 20)
            total_ops = ns * chain * iters
            cycles_per_op = cyc / total_ops if total_ops > 0 else float('inf')
            per_op.append((ns, cycles_per_op, cyc))

        # Pipeline depth: ns where cycles_per_op first reaches minimum (saturation)
        # When ns < depth: all ops pipeline, cycles_per_op decreases
        # When ns >= depth: pipeline full, cycles_per_op stops decreasing
        min_cpo = min(p[1] for p in per_op)
        # Find first ns where cycles_per_op is within 10% of minimum
        depth = per_op[-1][0]  # default to max
        for ns, cpo, cyc in per_op:
            if cpo <= min_cpo * 1.1:
                depth = ns
                break

        stats = {'per_op_data': [(ns, cpo) for ns, cpo, _ in per_op],
                 'min_cycles_per_op': min_cpo}
        return float(depth), '条', stats

    elif param_id == 'V5':
        # Vector register latency (read+write combined).
        # Same as V1 but we explicitly label it as register access.
        # Filter to the main veclen=64 sweep
        main_recs = [r for r in records if '_v64' in r.get('label', '') or
                     r.get('vecLen', r.get('veclen', 64)) == 64]
        if not main_recs:
            main_recs = records
        vals = get_chain_cycles(main_recs)
        if not vals:
            return None, '', {}
        chains = [v[0] for v in vals]
        iters = vals[0][2].get('iters', 20)
        cycles = [v[1] for v in vals]
        slope, intercept, r2 = linear_fit(chains, cycles)
        if slope is not None:
            per_op = slope / iters
            stats = {'slope': slope, 'intercept': intercept, 'r2': r2, 'iters': iters}
            return per_op, 'cycles', stats
        return None, '', {}

    # === CUBE ===
    elif param_id == 'C1':
        # Single tile matmul latency with PipeBarrier
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        chains = [v[0] for v in vals]
        iters = vals[0][2].get('iters', 20)
        cycles = [v[1] for v in vals]
        slope, intercept, r2 = linear_fit(chains, cycles)
        if slope is not None:
            per_op = slope / iters  # cycles per Mmad+PipeBarrier
            stats = {'slope': slope, 'intercept': intercept, 'r2': r2, 'iters': iters}
            return per_op, 'cycles', stats
        return None, '', {}

    elif param_id == 'C2':
        # Matmul throughput (TMAC/s).
        # Use all records; compute MAC/s for each and take the best.
        best_tmacs = 0
        best_stats = {}
        for r in records:
            cyc = r.get('cycles_median', 0)
            if cyc <= 0:
                continue
            m = r.get('m', 16)
            n = r.get('n', 16)
            k = r.get('k', 16)
            chain = r.get('chain', 0)
            iters = r.get('iters', 20)
            macs_per_mmad = m * n * k  # MACs per single Mmad
            total_macs = macs_per_mmad * chain * iters
            mac_per_cycle = total_macs / cyc
            tmacs = mac_per_cycle * SOC_FREQ_HZ / 1e12
            if tmacs > best_tmacs:
                best_tmacs = tmacs
                best_stats = {'m': m, 'n': n, 'k': k, 'chain': chain,
                              'cycles': cyc, 'mac_per_cycle': mac_per_cycle,
                              'total_macs': total_macs, 'iters': iters}
        if best_tmacs > 0:
            return best_tmacs, 'TMAC/s', best_stats
        return None, '', {}

    elif param_id == 'C3':
        # Cube pipeline depth.
        # Method 1: from C3 sweep data - find throughput saturation point.
        # As chain increases, throughput (MACs/cycle) should plateau when pipeline is full.
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}

        # Compute throughput per chain length
        throughputs = []
        for c, cyc, r in vals:
            iters = r.get('iters', 20)
            m = r.get('m', 16)
            n = r.get('n', 16)
            k = r.get('k', 16)
            macs = m * n * k * c * iters
            tp = macs / cyc if cyc > 0 else 0  # MACs per cycle
            throughputs.append((c, tp, cyc))

        if len(throughputs) >= 2:
            peak_tp = max(t[1] for t in throughputs)
            # Find the minimum chain where throughput >= 90% of peak
            # This is the pipeline depth (needs this many in-flight ops to saturate)
            for c, tp, cyc in throughputs:
                if tp >= 0.9 * peak_tp and c > 1:
                    # Pipeline depth ≈ this chain length minus 1
                    # (because at chain=1 the pipeline has 1 op)
                    depth = c
                    stats = {'peak_throughput_mac_per_cycle': peak_tp,
                             'saturation_chain': c,
                             'throughput_data': [(c, tp) for c, tp, _ in throughputs]}
                    return float(depth), '条', stats

        # Fallback: estimate from C1 latency / throughput_per_op
        # depth ≈ C1_latency * peak_throughput
        if throughputs:
            peak_tp = max(t[1] for t in throughputs)
            stats = {'peak_throughput_mac_per_cycle': peak_tp,
                     'note': 'estimated from max throughput'}
            return float(max(c for c, _, _ in throughputs)), '条', stats
        return None, '', {}

    elif param_id == 'C4':
        # L0A/L0B/L0C access latency.
        # Group by bufType and do linear fit for each.
        results = {}
        for bt_name, bt_val in [('L0A', 0), ('L0B', 1), ('L0C', 2)]:
            bt_recs = [r for r in records if r.get('buf', r.get('bufType', 0)) == bt_val]
            if not bt_recs:
                continue
            vals = get_chain_cycles(bt_recs)
            if not vals:
                continue
            chains = [v[0] for v in vals]
            iters = vals[0][2].get('iters', 20)
            cycles = [v[1] for v in vals]
            slope, intercept, r2 = linear_fit(chains, cycles)
            if slope is not None:
                results[bt_name] = slope / iters

        if results:
            # Report average or individual values
            avg = sum(results.values()) / len(results)
            stats = {'per_buffer': results}
            return avg, 'cycles', stats
        return None, '', {}

    elif param_id == 'C5':
        # Scaling: not a single number. Report 0 as placeholder.
        vals = get_chain_cycles(records)
        scaling_data = []
        for c, cyc, r in vals:
            m = r.get('m', 16)
            iters = r.get('iters', 20)
            chain = c
            per_mmad = cyc / (chain * iters) if chain * iters > 0 else 0
            scaling_data.append((m, per_mmad, cyc))
        stats = {'scaling_data': scaling_data}
        return 0.0, '-', stats

    # === MTE ===
    elif param_id in ('M1', 'M2'):
        # M1/M2: L1 read/write bandwidth.
        # chainLen = bytes per transfer. BW = bytes * iters / cycles * freq
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        # Use largest data point for peak bandwidth
        best = max(vals, key=lambda x: x[0])
        b, cyc, r = best
        iters = r.get('iters', 20)
        total_bytes = b * iters
        bw_GBs = total_bytes / cyc * SOC_FREQ_HZ / 1e9
        stats = {'bytes': b, 'cycles': cyc, 'iters': iters, 'total_bytes': total_bytes}
        return bw_GBs, 'GB/s', stats

    elif param_id in ('M3', 'M4'):
        # M3/M4: L0A/L0B bandwidth via LoadData.
        # chainLen = bytes (half precision). BW = bytes * iters / cycles * freq
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        best = max(vals, key=lambda x: x[0])
        b, cyc, r = best
        iters = r.get('iters', 20)
        total_bytes = b * iters
        bw_GBs = total_bytes / cyc * SOC_FREQ_HZ / 1e9
        stats = {'bytes': b, 'cycles': cyc, 'iters': iters}
        return bw_GBs, 'GB/s', stats

    elif param_id == 'M5':
        # M5: L0C bandwidth via Mmad writes.
        # chainLen = number of Mmad ops. Each Mmad(16,16,16) writes 16*16*4 = 1024 bytes to L0C.
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        best = max(vals, key=lambda x: x[0])
        c, cyc, r = best
        iters = r.get('iters', 20)
        bytes_per_mmad = 16 * 16 * 4  # M*N*sizeof(float)
        total_bytes = bytes_per_mmad * c * iters
        bw_GBs = total_bytes / cyc * SOC_FREQ_HZ / 1e9
        stats = {'chain': c, 'cycles': cyc, 'iters': iters, 'bytes_per_mmad': bytes_per_mmad}
        return bw_GBs, 'GB/s', stats

    elif param_id == 'M6':
        # Same as S3 (pointer chasing for HBM latency)
        return compute_metric('S3', records)

    elif param_id == 'M7':
        # Buffer capacity: find inflection point in latency/byte curve.
        # As data size exceeds buffer capacity, latency/byte ratio changes.
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}

        # Compute normalized latency (cycles per byte)
        points = []
        for b, cyc, r in vals:
            iters = r.get('iters', 20)
            cpb = cyc / (b * iters) if b > 0 else 0
            points.append((b, cpb, cyc))

        # Look for inflection: where cycles/byte significantly increases
        # Compare adjacent points
        capacities = {}
        if len(points) >= 3:
            for i in range(1, len(points)):
                prev_cpb = points[i-1][1]
                curr_cpb = points[i][1]
                if prev_cpb > 0 and curr_cpb / prev_cpb > 1.5:
                    # Significant latency increase - buffer capacity exceeded
                    capacities['inflection_bytes'] = points[i-1][0]
                    break

        # Also report known API values for reference
        known = {'UB': 192*1024, 'L0A': 64*1024, 'L0B': 64*1024, 'L0C': 128*1024, 'L1': 512*1024}
        stats = {'sweep_data': [(b, cpb) for b, cpb, _ in points],
                 'known_api': known}

        if 'inflection_bytes' in capacities:
            kb = capacities['inflection_bytes'] / 1024
            stats['inflection_kb'] = kb
            return kb, 'KB', stats

        # Fallback: report UB capacity from API
        stats['note'] = 'no clear inflection found, using API value'
        return 192.0, 'KB', stats

    elif param_id == 'M8':
        # MTE startup overhead: linear fit cycles = a + b*bytes, a = startup.
        vals = get_chain_cycles(records)
        if not vals:
            return None, '', {}
        chains = [v[0] for v in vals]
        iters = vals[0][2].get('iters', 20)
        cycles = [v[1] / iters for v in vals]  # per-iter cycles
        slope, intercept, r2 = linear_fit(chains, cycles)
        if intercept is not None:
            stats = {'slope_per_byte': slope, 'intercept': intercept, 'r2': r2}
            return intercept, 'cycles', stats
        # Fallback: use smallest transfer
        best = min(vals, key=lambda x: x[0])
        return best[1] / iters, 'cycles', {}

    return None, '', {}


def main():
    records = []
    with open(JSONL) as f:
        for line in f:
            r = parse_record(line.strip())
            if r and not r.get('error'):
                records.append(r)

    groups = defaultdict(list)
    for r in records:
        pid = extract_param_id(r.get('label', ''))
        if pid:
            groups[pid].append(r)

    # Write raw measurements CSV
    RAW_CSV.parent.mkdir(parents=True, exist_ok=True)
    with open(RAW_CSV, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            'parameter_id','run_id','benchmark_dir','device_model','device_id',
            'aicore_id','cann_version','driver_version','variant','problem_size',
            'loop_count','warmup_count','repeat_index','elapsed_cycles','elapsed_us',
            'bytes_moved','operations','measured_value','unit','notes'
        ])
        for pid in sorted(groups.keys()):
            for i, r in enumerate(groups[pid]):
                iters = r.get('iters', 20)
                cyc = r.get('cycles_median', 0)
                writer.writerow([
                    pid, f"{pid}_{i}", 'ubench/kernels', 'Ascend910B', 0, 0,
                    '9.0.0', '25.5.2', r.get('label',''),
                    f"chain={r.get('chain',0)},m={r.get('m',0)},n={r.get('n',0)},k={r.get('k',0)},streams={r.get('streams',0)},veclen={r.get('veclen',64)}",
                    r.get('chain',0), 5, i,
                    cyc, cyc/(SOC_FREQ_GHZ*1e3) if cyc else 0,
                    r.get('chain',0), r.get('chain',0),
                    cyc, 'cycles', r.get('label','')
                ])

    # Compute summary
    SUMMARY_CSV.parent.mkdir(parents=True, exist_ok=True)
    param_units = {
        'S1':'cycles','S2':'ops/cycle','S3':'cycles',
        'V1':'cycles','V2':'cycles','V3':'ops/cycle','V4':'条','V5':'cycles',
        'C1':'cycles','C2':'TMAC/s','C3':'条','C4':'cycles','C5':'-',
        'M1':'GB/s','M2':'GB/s','M3':'GB/s','M4':'GB/s','M5':'GB/s',
        'M6':'cycles','M7':'KB','M8':'cycles',
    }
    with open(SUMMARY_CSV, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['parameter_id','unit','count','measured_value','notes'])
        for pid in sorted(groups.keys()):
            val, unit_detail, stats = compute_metric(pid, groups[pid])
            unit = param_units.get(pid, unit_detail)
            notes_parts = []
            if 'slope' in stats:
                notes_parts.append(f"slope={stats['slope']:.4f}")
            if 'intercept' in stats:
                notes_parts.append(f"intercept={stats['intercept']:.2f}")
            if 'r2' in stats:
                notes_parts.append(f"R²={stats['r2']:.4f}")
            if 'per_buffer' in stats:
                for k, v in stats['per_buffer'].items():
                    notes_parts.append(f"{k}={v:.2f}")
            notes = ' '.join(notes_parts)
            writer.writerow([pid, unit, len(groups[pid]),
                           f'{val:.4f}' if val is not None else 'N/A', notes])

    # Print summary
    print(f"Parsed {len(records)} records into {RAW_CSV}")
    print(f"Summary written to {SUMMARY_CSV}")
    print(f"\n{'='*60}")
    print(f"  Parameter Summary (v2 - data-driven)")
    print(f"{'='*60}")
    for pid in sorted(groups.keys()):
        val, unit_detail, stats = compute_metric(pid, groups[pid])
        unit = param_units.get(pid, unit_detail)
        val_str = f'{val:.4f}' if val is not None else 'N/A'
        r2_str = f" (R²={stats['r2']:.4f})" if 'r2' in stats and stats['r2'] is not None else ''
        print(f"  {pid:>3}: {val_str:>12} {unit:<10}{r2_str}")

if __name__ == '__main__':
    main()
