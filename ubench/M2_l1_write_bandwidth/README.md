# M2 L1 Buffer 写入带宽

## Measurement Target
- Category: MTE
- Unit: GB/s
- Priority: core
- Pattern: streaming_bandwidth
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=连续写 L1，逐步增大数据量直到带宽饱和)

## Design
- **测什么**：L1 Buffer写入带宽 (GB/s)
- **怎么测**：DataCopy UB→GM，数据量4KB~128KB
- **为什么**：连续大块写入使MTE带宽饱和
- **运行**：`cd ubench/kernels && bash run.sh M2`

## Notes
连续写 L1，逐步增大数据量直到带宽饱和
