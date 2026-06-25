# S1 标量算术指令延迟

## Measurement Target
- Category: Scalar Unit
- Unit: cycles
- Priority: core
- Pattern: dependency_chain
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=标量加法依赖链)

## Design
- **测什么**：单条标量加法指令延迟 (cycles)
- **怎么测**：构造标量加依赖链 `x = x + (x&1) + 1`，链长500~8000，线性拟合斜率
- **为什么**：RAW依赖+非线性扰动防止编译器折叠；从GM加载初值防预计算
- **运行**：`cd ubench/kernels && bash build.sh && bash build_host.sh && bash run.sh S1`

## Notes
标量加法依赖链；用总周期除以迭代次数估算单条延迟
