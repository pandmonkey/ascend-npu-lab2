# M3 L0A 带宽

## Measurement Target
- Category: MTE
- Unit: GB/s
- Priority: normal
- Pattern: streaming_bandwidth
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=L0A 读写带宽)

## Design
- **测什么**：L0A缓冲区读写带宽 (GB/s)
- **怎么测**：GM→UB DataCopy作为L0A带宽代理
- **为什么**：L0A带宽通过DataCopy路径间接测量
- **运行**：`cd ubench/kernels && bash run.sh M3`

## Notes
L0A 读写带宽；需要明确方向
