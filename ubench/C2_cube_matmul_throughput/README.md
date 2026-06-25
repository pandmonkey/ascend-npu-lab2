# C2 矩阵乘吞吐率

## Measurement Target
- Category: Cube Unit
- Unit: MAC/s
- Priority: normal
- Pattern: streaming_matmul
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=大规模连续 matmul 使 Cube 饱和)

## Design
- **测什么**：矩阵乘吞吐率 (MAC/s)
- **怎么测**：大规模连续Mmad(16³~256³)，MAC/s=M×N×K×chain/cycles×频率
- **为什么**：大规模使Cube单元饱和
- **运行**：`cd ubench/kernels && bash run.sh C2`

## Notes
大规模连续 matmul 使 Cube 饱和；统计 MAC/s 或 TFLOPS
