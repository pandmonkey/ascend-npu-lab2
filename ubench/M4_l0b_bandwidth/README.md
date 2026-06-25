# M4 L0B 带宽

## Measurement Target
- Category: MTE
- Unit: GB/s
- Priority: normal
- Pattern: streaming_bandwidth
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=L0B 读写带宽)

## Design
- **测什么**：L0B缓冲区读写带宽 (GB/s)
- **怎么测**：GM→UB DataCopy作为L0B带宽代理
- **运行**：`cd ubench/kernels && bash run.sh M4`

## Notes
L0B 读写带宽；需要明确方向
