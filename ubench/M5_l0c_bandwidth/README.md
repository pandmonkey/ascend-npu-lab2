# M5 L0C 带宽

## Measurement Target
- Category: MTE
- Unit: GB/s
- Priority: normal
- Pattern: streaming_bandwidth
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=L0C 读写带宽)

## Design
- **测什么**：L0C缓冲区读写带宽 (GB/s)
- **怎么测**：UB→GM DataCopy作为L0C带宽代理
- **运行**：`cd ubench/kernels && bash run.sh M5`

## Notes
L0C 读写带宽；需要明确方向
