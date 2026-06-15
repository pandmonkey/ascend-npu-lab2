# M6 DDR/HBM 访存延迟

## Measurement Target

- Category: MTE
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

片外内存首字延迟；随机 pointer chasing 避免连续带宽掩盖延迟
