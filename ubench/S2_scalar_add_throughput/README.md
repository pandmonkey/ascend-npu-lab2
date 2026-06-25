# S2 标量单元吞吐率

## Measurement Target
- Category: Scalar Unit
- Unit: ops/cycle
- Priority: normal
- Pattern: independent_streams
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=多条独立标量加法流)

## Design
- **测什么**：标量单元吞吐率 (ops/cycle)
- **怎么测**：8条独立标量加法链并行，总ops=chainLen×8，吞吐率=ops/cycles
- **为什么**：独立链暴露ILP和流水线能力
- **运行**：`cd ubench/kernels && bash run.sh S2`

## Notes
多条独立标量加法流；改变并行链数量观察吞吐饱和
