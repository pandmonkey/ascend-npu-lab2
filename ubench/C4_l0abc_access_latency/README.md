# C4 L0A/L0B/L0C 访问延迟

## Measurement Target

- Category: Cube Unit
- Unit: cycles
- Priority: normal
- Pattern: buffer_access_chain

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

分别测 L0A、L0B、L0C 读写延迟
