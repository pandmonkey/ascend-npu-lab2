# 测量方法论

> 本文档记录 21 个参数的测量方法、设计思路、已知问题和误差分析。

## 总体方法

### 三种核心模式

| 模式 | 用途 | 方法 |
| --- | --- | --- |
| Dependency Chain | 测延迟 | RAW 数据依赖链，线性拟合 `cycles = a + b×chain`，斜率 b = 单条指令延迟 |
| Independent Streams | 测吞吐/流水线 | N 条独立指令流，改变 N 观察吞吐饱和 |
| Streaming / Size Sweep | 测带宽/容量 | 连续 DataCopy，改变数据量找峰值或拐点 |

### 计时

所有计时在 kernel 内部用 `asc_get_system_cycle()`：
```cpp
uint64_t start = asc_get_system_cycle();
// ... benchmark loop ...
uint64_t end = asc_get_system_cycle();
uint64_t total = end - start;
```

### 编译器优化对抗

| 技术 | 应用参数 |
| --- | --- |
| `x = x*3 + 1` 非线性链 | S1, S2 |
| 从 GM DataCopy 加载初值 | V1-V5, C1, C4 |
| in-place `Add(a, a, b)` 无 DataCopy | V1, V2 |
| `PipeBarrier<PIPE_ALL>()` | C1 |
| 4 个独立输出缓冲区轮转 | V3 |
| x 跨迭代反馈 | S1 |
| 写回 GM 保证 liveness | 所有参数 |

## 逐参数说明

### S1 标量加法延迟

- **链**：`x = x * 3 + 1`（mul+add，非线性，不可折叠）
- **反馈**：x 跨迭代连续（`x = chain(x, n)` 而非 `acc += chain(x, n)`）
- **拟合**：slope = cycles/chain-step，每 step 含 1 mul + 1 add
- **结果**：S1 = 1.0 cycles（标量单元 1 cycle 发射 1 条指令）
- **已知问题**：mul 和 add 在标量流水线中重叠，无法分离纯 add 延迟

### S2 标量吞吐率

- **方法**：8 条独立 `x = x*3 + 1` 链并行
- **结果**：S2 = 2.0 ops/cycle（双发射）
- **已知问题**：8 条链的 ILP 可能使测量值高于单链吞吐

### S3 标量访存延迟

- **方法**：GM 随机指针链表 pointer chasing
- **工作集**：chain≥16000（>64KB，避开 L1/L2 cache）
- **结果**：S3 = 49.9 cycles
- **已知问题**：910B L2=172MB（共享），chain=256000 时工作集仅 1MB，可能仍命中 L2

### V1/V2 FP32 向量加/乘延迟

- **链**：`Add<float>(a, a, b, len)`（in-place RAW）
- **无 Max 扰动**：早期版本用 `Max(b, b, a)` 防折叠，但 Max 在 V 管道混入延迟（35.5→14.5 cycles）。最终移除 Max，纯 Add 链
- **结果**：V1 = 8.2 cycles, V2 = 8.5 cycles
- **vecLen 无关**：vecLen=16/32/64 结果一致（硬件向量单元一次处理）

### V3 向量吞吐率

- **方法**：4 个独立输出缓冲区，`Add(d0,a,b), Add(d1,a,b), ...` 无依赖
- **结果**：V3 = 1.38 ops/cycle
- **已知问题**：编译器可能对相同输入的 Add 做指令合并

### V4 向量流水线深度

- **方法**：每步发射 N 条独立 Add（N=1..8），测吞吐饱和
- **结果**：V4 = 1（N=1 时已达峰值）
- **已知问题**：if 分支可能影响指令调度；实际深度可能 >1

### V5 向量寄存器延迟

- **方法**：UB→UB DataCopy 链 `c=a; a=c`
- **结果**：V5 = 10.9 cycles
- **已知问题**：DataCopy 走 MTE 管道，不完全等于纯寄存器访问

### C1 单 tile 矩阵乘延迟

- **方法**：Mmad(16,16,16) + `PipeBarrier<PIPE_ALL>()` 每次
- **结果**：C1 = 15.5 cycles（含 PipeBarrier 开销 ~5-10 cycles）
- **已知问题**：PipeBarrier 本身有开销，实际 Mmad 延迟可能更低

### C2 矩阵乘吞吐率

- **方法**：连续 Mmad(16,16,16) 无 barrier
- **结果**：C2 = 72.9 TMAC/s（单核）
- **已知问题**：Mmad(16,16,16) 可能不是单条硬件指令，内部可能被拆分

### C3 Cube 流水线深度

- **方法**：增加连续 Mmad 数量观察吞吐饱和
- **结果**：C3 = 500（未饱和）
- **已知问题**：L0C 累加不释放可能影响后续发射；实际深度可能 ~11（从 C2 推算）

### C4 L0A/L0B/L0C 访问延迟

- **方法**：小缓冲区 UB DataCopy 读写链代理
- **结果**：C4 = 10.9 cycles
- **已知问题**：UB DataCopy 是代理测量，非直接访问 L0A/L0B/L0C

### C5 矩阵乘规模缩放

- **方法**：扫描 M=N=K = 16~128
- **结果**：延迟随 O(M×N×K) 线性增长

### M1/M2 L1 读写带宽

- **方法**：DataCopy GM→UB / UB→GM，数据量 4KB~128KB
- **结果**：M1 = 3598 GB/s, M2 = 3390 GB/s
- **已知问题**：测的是 GM↔UB 全路径，非纯 L1 带宽

### M3-M5 L0A/L0B/L0C 带宽

- **方法**：用 M1/M2 的 DataCopy 作为代理
- **结果**：M3~M5 ≈ 3300-3640 GB/s
- **已知问题**：代理测量，非直接访问 L0 缓冲区

### M6 HBM 访存延迟

- **方法**：随机指针链表，工作集 >64KB
- **结果**：M6 = 50.2 cycles
- **已知问题**：同 S3，可能仍命中 L2

### M7 Buffer 容量

- **方法**：PlatformAscendC API 直接查询 + DataCopy 延迟拐点验证
- **结果**：UB = 192 KB, L1 = 512 KB, L0A = 64 KB, L0B = 64 KB, L0C = 128 KB
- **准确度**：高（来自平台 API ground truth）

### M8 MTE 启动开销

- **方法**：小数据传输线性拟合截距
- **结果**：M8 = 40.7 cycles
- **已知问题**：极小数据量时 cycle 量化误差大

## 误差来源汇总

1. **SoC 频率**：1650 MHz（npu-smi 实测），但 `asc_get_system_cycle()` 的计数频率可能不完全等于 AI Core 频率
2. **L2 cache**：910B L2 = 172MB（共享），pointer chasing 工作集可能未完全超出
3. **编译器优化**：尽管用了多种对抗技术，编译器仍可能部分优化循环
4. **代理测量**：C4/M3-M5/V5 使用 DataCopy 代理，非直接访问目标硬件
5. **PipeBarrier 开销**：C1 测量中 PipeBarrier 混入约 5-10 cycles
6. **单核运行**：所有测试在 blockDim=1 下运行，不反映多核并行性能
7. **系统负载**：多用户共享 NPU 服务器，可能存在干扰
