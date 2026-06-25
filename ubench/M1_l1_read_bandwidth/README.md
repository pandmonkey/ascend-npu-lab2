# M1 L1 Buffer 读取带宽

## Measurement Target
- Category: MTE
- Unit: GB/s
- Priority: core
- Pattern: streaming_bandwidth
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=连续读 L1，逐步增大数据量直到带宽饱和)

## Design
- **测什么**：L1 Buffer读取带宽 (GB/s)
- **怎么测**：DataCopy GM→UB，数据量4KB~128KB，带宽=bytes×iters/cycles×频率
- **为什么**：连续大块读取使MTE带宽饱和
- **运行**：`cd ubench/kernels && bash run.sh M1`

## Notes
连续读 L1，逐步增大数据量直到带宽饱和
