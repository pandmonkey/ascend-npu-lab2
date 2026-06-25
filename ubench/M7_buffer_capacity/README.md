# M7 各级 Buffer 容量

## Measurement Target
- Category: MTE
- Unit: KB/MB
- Priority: normal
- Pattern: working_set_sweep
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=扫工作集大小，观察延迟或带宽拐点估算容量)

## Design
- **测什么**：各级Buffer容量 (KB)
- **怎么测**：扫描DataCopy工作集大小1KB~128KB，观察周期/字节比率拐点
- **为什么**：工作集超过缓存容量时延迟跳变
- **运行**：`cd ubench/kernels && bash run.sh M7`

## Notes
扫工作集大小，观察延迟或带宽拐点估算容量
