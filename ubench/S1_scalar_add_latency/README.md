# S1 标量算术指令延迟

## Measurement Target

- Category: Scalar Unit
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

标量加法依赖链；用总周期除以迭代次数估算单条延迟
