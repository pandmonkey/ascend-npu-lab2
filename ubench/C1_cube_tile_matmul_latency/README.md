# C1 矩阵乘延迟（单 tile）

## Measurement Target

- Category: Cube Unit
- Unit: cycles
- Priority: core
- Pattern: single_tile_latency

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

固定最小 tile，例如 16x16；测一次 tile matmul 发射到完成周期
