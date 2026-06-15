# C2 矩阵乘吞吐率

## Measurement Target

- Category: Cube Unit
- Unit: MAC/s
- Priority: normal
- Pattern: streaming_matmul

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

大规模连续 matmul 使 Cube 饱和；统计 MAC/s 或 TFLOPS
