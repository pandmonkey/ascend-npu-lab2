# M8 数据搬运启动开销

## Measurement Target

- Category: MTE
- Unit: cycles
- Priority: normal
- Pattern: transfer_size_intercept

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

扫小数据 DMA 传输大小，用线性拟合截距估算启动开销
