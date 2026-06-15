#!/usr/bin/env python3
"""Summarize raw Lab2 microbenchmark measurements.

Input:
  data/raw/measurements.csv, if it exists.
  Otherwise the script falls back to measurements_template.csv and emits an
  empty summary with only the header.

Output:
  data/processed/summary.csv
"""

from __future__ import annotations

import csv
from collections import defaultdict
from pathlib import Path
import statistics
import sys


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "data" / "raw" / "measurements.csv"
RAW_TEMPLATE = ROOT / "data" / "raw" / "measurements_template.csv"
OUT = ROOT / "data" / "processed" / "summary.csv"

SUMMARY_COLUMNS = [
    "parameter_id",
    "unit",
    "count",
    "mean",
    "stdev",
    "min",
    "max",
    "benchmark_dirs",
    "variants",
]


def parse_float(value: str) -> float | None:
    value = value.strip()
    if not value:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def load_rows() -> list[dict[str, str]]:
    source = RAW if RAW.exists() else RAW_TEMPLATE
    with source.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def summarize(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    groups: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        parameter_id = row.get("parameter_id", "").strip()
        unit = row.get("unit", "").strip()
        value = parse_float(row.get("measured_value", ""))
        if not parameter_id or value is None:
            continue
        groups[(parameter_id, unit)].append(row)

    summary_rows = []
    for (parameter_id, unit), items in sorted(groups.items()):
        values = [float(row["measured_value"]) for row in items]
        dirs = sorted({row.get("benchmark_dir", "").strip() for row in items if row.get("benchmark_dir", "").strip()})
        variants = sorted({row.get("variant", "").strip() for row in items if row.get("variant", "").strip()})
        summary_rows.append(
            {
                "parameter_id": parameter_id,
                "unit": unit,
                "count": str(len(values)),
                "mean": f"{statistics.mean(values):.6g}",
                "stdev": f"{statistics.stdev(values):.6g}" if len(values) > 1 else "0",
                "min": f"{min(values):.6g}",
                "max": f"{max(values):.6g}",
                "benchmark_dirs": ";".join(dirs),
                "variants": ";".join(variants),
            }
        )
    return summary_rows


def write_summary(rows: list[dict[str, str]]) -> None:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=SUMMARY_COLUMNS)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    rows = load_rows()
    summary_rows = summarize(rows)
    write_summary(summary_rows)
    print(f"wrote {OUT} with {len(summary_rows)} summarized parameter/unit groups")
    return 0


if __name__ == "__main__":
    sys.exit(main())

