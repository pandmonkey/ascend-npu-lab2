# C3 矩阵乘流水线深度

## Measurement Target

- Category: Cube Unit
- Unit: 条
- Priority: normal
- Pattern: latency_vs_independent_tiles

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

增加独立 tile matmul 数量，观察吞吐饱和点
