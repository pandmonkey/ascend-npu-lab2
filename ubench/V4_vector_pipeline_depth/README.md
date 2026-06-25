# V4 向量单元流水线深度

## Measurement Target
- Category: Vector Unit
- Unit: 条
- Priority: normal
- Pattern: latency_vs_independent_chains
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=增加独立链数量直到吞吐不再提升，估算流水线可容纳指令数)

## Design
- **测什么**：向量单元流水线深度 (条)
- **怎么测**：每步发射N条独立加法(N=1..8)，观察吞吐率饱和点
- **为什么**：独立指令可同时处于不同流水阶段，饱和点即深度
- **运行**：`cd ubench/kernels && bash run.sh V4`

## Notes
增加独立链数量直到吞吐不再提升，估算流水线可容纳指令数
