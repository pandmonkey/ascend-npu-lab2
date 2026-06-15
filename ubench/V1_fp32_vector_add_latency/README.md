# V1 FP32 向量加法延迟

## Measurement Target

- Category: Vector Unit
- Unit: cycles
- Priority: core
- Pattern: dependency_chain

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

FP32 vector add 依赖链；总周期除以向量指令次数
