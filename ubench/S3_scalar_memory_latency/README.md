# S3 标量访存延迟

## Measurement Target
- Category: Scalar Unit
- Unit: cycles
- Priority: normal
- Pattern: pointer_chasing
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=标量 load/store 依赖链)

## Design
- **测什么**：标量访存延迟 (cycles)
- **怎么测**：GM中随机指针链表pointer chasing，线性拟合斜率
- **为什么**：随机访问消除预取，RAW依赖暴露真实延迟
- **运行**：`cd ubench/kernels && bash run.sh S3`

## Notes
标量 load/store 依赖链；重点避免编译器优化和预取影响
