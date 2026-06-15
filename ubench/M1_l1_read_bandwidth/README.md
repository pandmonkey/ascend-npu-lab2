# M1 L1 Buffer 读取带宽

## Measurement Target

- Category: MTE
- Unit: GB/s
- Priority: core
- Pattern: streaming_bandwidth

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

连续读 L1，逐步增大数据量直到带宽饱和
