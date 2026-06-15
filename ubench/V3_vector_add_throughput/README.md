# V3 向量单元吞吐率

## Measurement Target

- Category: Vector Unit
- Unit: ops/cycle
- Priority: normal
- Pattern: independent_streams

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

多条独立向量加法流；按 ops/cycle 统计
