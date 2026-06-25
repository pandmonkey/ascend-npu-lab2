# C5 不同规模矩阵乘延迟缩放关系

## Measurement Target
- Category: Cube Unit
- Unit: -
- Priority: normal
- Pattern: size_sweep
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=扫 M/N/K 规模并画延迟折线图)

## Design
- **测什么**：矩阵规模M/N/K增大时延迟增长规律
- **怎么测**：扫描M=N=K=16~256，画延迟折线图
- **为什么**：揭示Cube计算量与延迟的缩放关系
- **运行**：`cd ubench/kernels && bash run.sh C5`

## Notes
扫 M/N/K 规模并画延迟折线图
