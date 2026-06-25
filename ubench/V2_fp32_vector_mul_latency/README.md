# V2 FP32 向量乘法延迟

## Measurement Target
- Category: Vector Unit
- Unit: cycles
- Priority: core
- Pattern: dependency_chain
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=FP32 vector mul 依赖链)

## Design
- **测什么**：FP32向量乘法延迟 (cycles)
- **怎么测**：与V1相同框架，Add替换为Mul
- **为什么**：与V1对称，交叉验证乘加延迟
- **运行**：`cd ubench/kernels && bash run.sh V2`

## Notes
FP32 vector mul 依赖链；与 V1 使用相同计时框架
