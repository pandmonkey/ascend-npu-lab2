# S3 标量访存延迟

## Measurement Target

- Category: Scalar Unit
- Unit: cycles
- Priority: normal
- Pattern: pointer_chasing

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

标量 load/store 依赖链；重点避免编译器优化和预取影响
