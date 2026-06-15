# V2 FP32 向量乘法延迟

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

FP32 vector mul 依赖链；与 V1 使用相同计时框架
