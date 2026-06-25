# V5 向量寄存器访问延迟

## Measurement Target
- Category: Vector Unit
- Unit: cycles
- Priority: normal
- Pattern: read_write_dependency_chain
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode=分别构造读后写、写后读等相关链，区分读写延迟)

## Design
- **测什么**：向量寄存器读/写延迟 (cycles)
- **怎么测**：UB到UB DataCopy链 `c=a; a=c`，每对含一次读写
- **为什么**：DataCopy经过向量寄存器，近似寄存器访问延迟
- **运行**：`cd ubench/kernels && bash run.sh V5`

## Notes
分别构造读后写、写后读等相关链，区分读写延迟
