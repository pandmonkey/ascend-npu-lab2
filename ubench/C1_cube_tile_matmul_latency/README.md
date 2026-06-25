# C1 矩阵乘延迟（单 tile）

## Measurement Target
- Category: Cube Unit
- Unit: cycles
- Priority: core
- Pattern: single_tile_latency
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=固定最小 tile，例如 16x16)

## Design
- **测什么**：单tile(16×16×16)矩阵乘延迟 (cycles)
- **怎么测**：L0A/L0B中放half矩阵，Mmad计算到L0C，chainLen=100~1000线性拟合
- **为什么**：16×16×16是Cube最小粒度，LoadData仅在初始化不进入计时
- **运行**：`cd ubench/kernels && bash run.sh C1`

## Notes
固定最小 tile，例如 16x16；测一次 tile matmul 发射到完成周期
