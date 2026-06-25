# M8 数据搬运启动开销

## Measurement Target
- Category: MTE
- Unit: cycles
- Priority: normal
- Pattern: transfer_size_intercept
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=扫小数据 DMA 传输大小，用线性拟合截距估算启动开销)

## Design
- **测什么**：MTE启动一次DMA的固定开销 (cycles)
- **怎么测**：扫描小数据传输32B~4KB，线性拟合cycles=a+b×bytes，截距a为启动开销
- **为什么**：小数据下固定开销占比大，线性拟合可分离
- **运行**：`cd ubench/kernels && bash run.sh M8`

## Notes
扫小数据 DMA 传输大小，用线性拟合截距估算启动开销
