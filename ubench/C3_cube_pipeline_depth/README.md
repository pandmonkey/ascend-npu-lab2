# C3 矩阵乘流水线深度

## Measurement Target
- Category: Cube Unit
- Unit: 条
- Priority: normal
- Pattern: latency_vs_independent_tiles
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=增加独立 tile matmul 数量，观察吞吐饱和点)

## Design
- **测什么**：Cube流水线深度 (条)
- **怎么测**：增加连续Mmad数量(10~500)，观察吞吐饱和点
- **为什么**：独立tile可并行进入流水线
- **运行**：`cd ubench/kernels && bash run.sh C3`

## Notes
增加独立 tile matmul 数量，观察吞吐饱和点
