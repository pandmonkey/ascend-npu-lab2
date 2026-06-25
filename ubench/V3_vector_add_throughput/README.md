# V3 向量单元吞吐率

## Measurement Target
- Category: Vector Unit
- Unit: ops/cycle
- Priority: normal
- Pattern: independent_streams
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=多条独立向量加法流)

## Design
- **测什么**：向量单元吞吐率 (ops/cycle)
- **怎么测**：连续无依赖向量加 `c=a+b`，吞吐率=chainLen/cycles
- **为什么**：无跨迭代依赖，全速流水
- **运行**：`cd ubench/kernels && bash run.sh V3`

## Notes
多条独立向量加法流；按 ops/cycle 统计
