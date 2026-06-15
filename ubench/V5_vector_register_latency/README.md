# V5 向量寄存器访问延迟

## Measurement Target

- Category: Vector Unit
- Unit: cycles
- Priority: normal
- Pattern: read_write_dependency_chain

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

分别构造读后写、写后读等相关链，区分读写延迟
