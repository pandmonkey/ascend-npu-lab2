#!/usr/bin/env python3
"""Create README templates for all benchmark directories."""

from __future__ import annotations

import csv
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "benchmark_manifest.csv"


TEMPLATE = """# {id} {name}

## Measurement Target

- Category: {category}
- Unit: {unit}
- Priority: {priority}
- Pattern: {measurement_pattern}

## Design

- What to measure: TODO
- How to measure: TODO
- Why this isolates the target component: TODO

## Parameters To Sweep

- Problem size: TODO
- Loop count: TODO
- Number of independent dependency chains or streams: TODO
- Warmup count and repeat count: TODO

## Output

Append raw records to:

```text
data/raw/measurements.csv
```

Required fields follow `data/raw/measurements_template.csv`.

## Run

```bash
# TODO: replace with the actual Ascend environment command.
```

## Notes

{notes}
"""


def main() -> int:
    with MANIFEST.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    created = 0
    skipped = 0
    for row in rows:
        out = ROOT / "ubench" / row["ubench_dir"] / "README.md"
        if out.exists():
            skipped += 1
            continue
        out.write_text(TEMPLATE.format(**row), encoding="utf-8")
        created += 1

    print(f"created {created} README templates, skipped {skipped} existing files")
    return 0


if __name__ == "__main__":
    sys.exit(main())

