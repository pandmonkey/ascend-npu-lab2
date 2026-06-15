# C5 不同规模矩阵乘延迟缩放关系

## Measurement Target

- Category: Cube Unit
- Unit: -
- Priority: normal
- Pattern: size_sweep

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

扫 M/N/K 规模并画延迟折线图
