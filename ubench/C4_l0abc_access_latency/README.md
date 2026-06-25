# C4 L0A/L0B/L0C 访问延迟

## Measurement Target
- Category: Cube Unit
- Unit: cycles
- Priority: normal
- Pattern: buffer_access_chain
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=分别测 L0A、L0B、L0C 读写延迟)

## Design
- **测什么**：L0A/L0B/L0C访问延迟 (cycles)
- **怎么测**：小缓冲区DataCopy读写链作为代理，bufType区分L0A/L0B/L0C
- **为什么**：UB DataCopy是访问L0缓冲区的代理测量
- **运行**：`cd ubench/kernels && bash run.sh C4`

## Notes
分别测 L0A、L0B、L0C 读写延迟
