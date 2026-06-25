#!/usr/bin/env python3
"""Generate ASCII/text plots from benchmark data (no matplotlib needed)."""
import json, csv, re, math
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).resolve().parents[1]
JSONL = ROOT / "data" / "raw" / "all_benchmarks.jsonl"
PLOT_DIR = ROOT / "data" / "processed" / "plots"
PLOT_DIR.mkdir(parents=True, exist_ok=True)

def load_jsonl():
    records = []
    with open(JSONL) as f:
        for line in f:
            try:
                r = json.loads(line.strip())
                if not r.get('error'):
                    records.append(r)
            except:
                pass
    return records

def extract_pid(label):
    m = re.match(r'^([SVC]\d+|M\d+)', label)
    return m.group(1) if m else None

def ascii_plot(pid, title, x_label, y_label, points):
    """Generate an ASCII chart and save as .txt"""
    if not points:
        return
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    
    lines = []
    lines.append(f"{'='*60}")
    lines.append(f"  {pid}: {title}")
    lines.append(f"  X: {x_label}  Y: {y_label}")
    lines.append(f"{'='*60}")
    
    x_min, x_max = min(xs), max(xs)
    y_min, y_max = min(ys), max(ys)
    if y_max == y_min:
        y_max = y_min + 1
    
    chart_w = 50
    chart_h = 15
    
    grid = [[' ' for _ in range(chart_w)] for _ in range(chart_h)]
    
    for x, y in points:
        col = int((x - x_min) / (x_max - x_min + 1e-9) * (chart_w - 1)) if x_max > x_min else 0
        row = int((y_max - y) / (y_max - y_min + 1e-9) * (chart_h - 1)) if y_max > y_min else 0
        col = max(0, min(chart_w-1, col))
        row = max(0, min(chart_h-1, row))
        grid[row][col] = '*'
    
    # Connect points with -
    for i in range(len(points)-1):
        x1, y1 = points[i]
        x2, y2 = points[i+1]
        col1 = int((x1 - x_min) / (x_max - x_min + 1e-9) * (chart_w - 1)) if x_max > x_min else 0
        col2 = int((x2 - x_min) / (x_max - x_min + 1e-9) * (chart_w - 1)) if x_max > x_min else 0
        row1 = int((y_max - y1) / (y_max - y_min + 1e-9) * (chart_h - 1)) if y_max > y_min else 0
        row2 = int((y_max - y2) / (y_max - y_min + 1e-9) * (chart_h - 1)) if y_max > y_min else 0
        for c in range(min(col1,col2)+1, max(col1,col2)):
            if grid[row1][c] == ' ':
                grid[row1][c] = '-'
    
    for row in range(chart_h):
        y_val = y_max - (row / (chart_h-1)) * (y_max - y_min) if chart_h > 1 else y_max
        lines.append(f"{y_val:>10.1f} | {''.join(grid[row])}")
    
    lines.append(f"{'':>11} +{'-'*chart_w}")
    lines.append(f"{'':>11} {x_min:<25}{x_max:>25}")
    
    # Data table
    lines.append(f"\nData points:")
    lines.append(f"  {'X':>10} {'Y':>10}")
    for x, y in points:
        lines.append(f"  {x:>10.1f} {y:>10.1f}")
    
    # Linear fit
    if len(points) >= 2:
        n = len(points)
        sx = sum(xs); sy = sum(ys)
        sxx = sum(x*x for x in xs); sxy = sum(x*y for x,y in points)
        denom = n*sxx - sx*sx
        if denom != 0:
            slope = (n*sxy - sx*sy) / denom
            intercept = (sy - slope*sx) / n
            lines.append(f"\nLinear fit: y = {slope:.4f} * x + {intercept:.4f}")
            lines.append(f"  slope (per-op latency) = {slope:.4f}")
    
    content = '\n'.join(lines)
    with open(PLOT_DIR / f'{pid}.txt', 'w') as f:
        f.write(content)
    print(f"  saved {pid}.txt")

def main():
    records = load_jsonl()
    print(f"Loaded {len(records)} records")
    
    # Latency sweeps
    for pid, title in [
        ('S1', 'Scalar Add Latency'), ('S2', 'Scalar Add Throughput'),
        ('S3', 'Scalar Memory Latency'),
        ('V1', 'Vector Add Latency'), ('V2', 'Vector Mul Latency'),
        ('V3', 'Vector Add Throughput'), ('V5', 'Vector Register Latency'),
        ('C1', 'Cube Tile Matmul Latency'), ('C4', 'L0A/L0B/L0C Access Latency'),
        ('M6', 'HBM Memory Latency'),
    ]:
        recs = [r for r in records if extract_pid(r.get('label','')) == pid and r.get('cycles_median',0) > 0]
        if recs:
            recs.sort(key=lambda r: r.get('chain', 0))
            points = [(r.get('chain',0), r.get('cycles_median',0)) for r in recs]
            ascii_plot(pid, title, 'Chain Length', 'Cycles', points)
    
    # Bandwidth sweeps
    for pid, title in [
        ('M1', 'L1 Read Bandwidth'), ('M2', 'L1 Write Bandwidth'),
        ('M3', 'L0A Bandwidth'), ('M4', 'L0B Bandwidth'),
        ('M5', 'L0C Bandwidth'), ('M7', 'Buffer Capacity Sweep'),
        ('M8', 'MTE Startup Overhead'),
    ]:
        recs = [r for r in records if extract_pid(r.get('label','')) == pid and r.get('cycles_median',0) > 0]
        if recs:
            recs.sort(key=lambda r: r.get('chain', 0))
            SOC_FREQ = 1.25e9
            points = [(r.get('chain',0)/1024, r.get('chain',0)*r.get('iters',10)/r.get('cycles_median',1)*SOC_FREQ/1e9) for r in recs]
            ascii_plot(pid, title, 'Data Size (KB)', 'Bandwidth (GB/s)', points)
    
    # V4 pipeline depth
    recs = [r for r in records if extract_pid(r.get('label','')) == 'V4' and r.get('cycles_median',0) > 0]
    if recs:
        recs.sort(key=lambda r: r.get('streams', 1))
        points = [(r.get('streams',1), r.get('streams',1)*r.get('chain',1000)/r.get('cycles_median',1)) for r in recs]
        ascii_plot('V4', 'Vector Pipeline Depth', 'Num Streams', 'Throughput (ops/cycle)', points)
    
    # C5 scaling
    recs = [r for r in records if extract_pid(r.get('label','')) == 'C5' and r.get('cycles_median',0) > 0]
    if recs:
        recs.sort(key=lambda r: r.get('m', 16))
        points = [(r.get('m',16), r.get('cycles_median',0)) for r in recs]
        ascii_plot('C5', 'Matmul Latency vs Size', 'M=N=K', 'Cycles', points)
    
    print(f"\nAll plots saved to {PLOT_DIR}")

if __name__ == '__main__':
    main()
