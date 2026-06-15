#!/usr/bin/env python3
"""Validate the Lab2 project skeleton before and after Ascend runs."""

from __future__ import annotations

import csv
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "benchmark_manifest.csv"
RAW_TEMPLATE = ROOT / "data" / "raw" / "measurements_template.csv"

EXPECTED_RAW_COLUMNS = [
    "parameter_id",
    "run_id",
    "benchmark_dir",
    "device_model",
    "device_id",
    "aicore_id",
    "cann_version",
    "driver_version",
    "variant",
    "problem_size",
    "loop_count",
    "warmup_count",
    "repeat_index",
    "elapsed_cycles",
    "elapsed_us",
    "bytes_moved",
    "operations",
    "measured_value",
    "unit",
    "notes",
]


def fail(message: str) -> None:
    print(f"[FAIL] {message}")
    raise SystemExit(1)


def ok(message: str) -> None:
    print(f"[OK] {message}")


def read_manifest() -> list[dict[str, str]]:
    if not MANIFEST.exists():
        fail(f"missing manifest: {MANIFEST}")
    with MANIFEST.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if len(rows) != 21:
        fail(f"manifest should contain 21 parameters, got {len(rows)}")
    ids = [row["id"] for row in rows]
    if len(ids) != len(set(ids)):
        fail("manifest contains duplicated parameter ids")
    ok("manifest has 21 unique parameters")
    return rows


def validate_dirs(rows: list[dict[str, str]]) -> None:
    missing = []
    for row in rows:
        path = ROOT / "ubench" / row["ubench_dir"]
        if not path.is_dir():
            missing.append(str(path.relative_to(ROOT)))
    if missing:
        fail("missing ubench directories: " + ", ".join(missing))
    ok("all ubench directories exist")


def validate_raw_template() -> None:
    if not RAW_TEMPLATE.exists():
        fail(f"missing raw data template: {RAW_TEMPLATE}")
    with RAW_TEMPLATE.open(newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        header = next(reader, [])
    if header != EXPECTED_RAW_COLUMNS:
        fail("raw data template header mismatch")
    ok("raw data template header is valid")


def main() -> int:
    rows = read_manifest()
    validate_dirs(rows)
    validate_raw_template()
    ok("project skeleton is ready")
    return 0


if __name__ == "__main__":
    sys.exit(main())

