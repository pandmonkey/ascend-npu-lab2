# V1 FP32 向量加法延迟

## Measurement Target
- Category: Vector Unit
- Unit: cycles
- Priority: core
- Pattern: dependency_chain
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=FP32 vector add 依赖链)

## Design
- **测什么**：FP32向量加法延迟 (cycles)
- **怎么测**：int16向量加依赖链 `c=a+b; a=c; b^=c`，XOR扰动非线性，链长200~4000
- **为什么**：向量ALU对int16/FP32共享流水线；XOR不在加法关键路径
- **运行**：`cd ubench/kernels && bash run.sh V1`

## Notes
FP32 vector add 依赖链；总周期除以向量指令次数
