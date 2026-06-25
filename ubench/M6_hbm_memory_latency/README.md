# M6 DDR/HBM 访存延迟

## Measurement Target
- Category: MTE
- Unit: cycles
- Priority: normal
- Pattern: pointer_chasing
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=片外内存首字延迟)

## Design
- **测什么**：HBM访存首字延迟 (cycles)
- **怎么测**：GM随机指针链表pointer chasing
- **为什么**：随机访问消除预取，暴露真实HBM延迟
- **运行**：`cd ubench/kernels && bash run.sh M6`

## Notes
片外内存首字延迟；随机 pointer chasing 避免连续带宽掩盖延迟
