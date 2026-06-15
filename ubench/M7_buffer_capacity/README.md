# M7 各级 Buffer 容量

## Measurement Target

- Category: MTE
- Unit: KB/MB
- Priority: normal
- Pattern: working_set_sweep

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

扫工作集大小，观察延迟或带宽拐点估算容量
