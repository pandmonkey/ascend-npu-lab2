# S1 标量算术指令延迟

## Measurement Target

- Category: Scalar Unit
- Unit: cycles
- Priority: core
- Pattern: dependency_chain

## Current Status

This directory currently contains the first Ascend C build smoke test for the
Lab2 benchmark suite. It builds a minimal custom AICore operator named
`ScalarAddLatency`.

This is not a valid S1 cycle measurement yet. Its job is to verify that the
remote Ascend host can compile our repository-local CANN/Ascend C project.
After this build chain is confirmed, the kernel will be replaced by a true
scalar dependency-chain benchmark.

## Design

- What to measure: S1 will measure scalar add latency in cycles.
- How to measure: the final version should execute a long dependency chain and divide elapsed cycles by the chain length.
- Why this isolates the target component: the dependency chain should prevent instruction-level overlap and expose single-operation latency.

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
bash ../../scripts/run_one.sh S1
```

## Notes

标量加法依赖链；用总周期除以迭代次数估算单条延迟
