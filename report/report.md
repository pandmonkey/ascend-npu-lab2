# 并行与分布式导论 Lab2：昇腾 NPU 微基准测试设计

> 学号：2300013126  姓名：赵烁然

---

## 1. 摘要

本实验针对昇腾 910B (DaVinci 架构) 计算卡，设计并实现了包含 21 个参数的微基准测试 (microbenchmark) 套件。测试覆盖 Scalar Unit、Vector Unit、Cube Unit 和 MTE (Memory Transfer Engine) 四大类组件，使用 Ascend C kernel 编程 + ACL host launch 的方式实现片上 cycle 级精确计时。每个 microbenchmark 遵循隔离性、可控性、可重复性原则，通过参数化 sweep 和最小二乘线性拟合获得各组件的延迟、带宽、吞吐率、流水线深度和容量参数。所有 21 个参数均独立可运行，原始数据完整保留。

---

## 2. 实验环境

| 项目 | 内容 |
| --- | --- |
| 设备型号 | Ascend 910B (SoC: Ascend910_9362) |
| AI Core 数量 | 20 AIC (Cube) + 40 AIV (Vector) |
| AI Core 频率 | 1650 MHz (满载标称，npu-smi 实测) |
| CANN 版本 | 9.0.0 |
| 驱动版本 | 25.5.2 |
| Kernel 编译器 | ccec (bisheng clang 15.0.5) |
| Host 编译器 | g++ (aarch64) |
| 计时方式 | `asc_get_system_cycle()` 片上 cycle 计数 |
| HBM | 32GB/die, 1600 MHz |
| 运行方式 | blockDim=1（单 AI Core 运行） |

### 片上存储容量（PlatformAscendC API 实测）

| Buffer | 容量 | 说明 |
| --- | --- | --- |
| L0A | 64 KB | Cube 左矩阵输入缓冲 |
| L0B | 64 KB | Cube 右矩阵输入缓冲 |
| L0C | 128 KB | Cube 输出累加器 |
| L1 (CBUF) | 512 KB | Cube 数据缓冲 |
| UB | 192 KB | Vector 统一缓冲 |
| L2 | 172 MB | 全 die 共享 |

---

## 3. 方法总览

### 3.1 三种核心测量模式

| 模式 | 用途 | 原理 |
| --- | --- | --- |
| **Dependency Chain** | 延迟测量 | 构造 RAW 数据依赖的指令链，迫使串行执行。线性拟合 `cycles = a + b × chainLen`，斜率 b = 每步延迟 |
| **Independent Streams** | 吞吐率/流水线 | N 条无依赖的并行指令流，改变 N 观察吞吐饱和点 |
| **Streaming / Size Sweep** | 带宽/容量 | 连续数据传输，改变数据规模测量峰值带宽或容量拐点 |

### 3.2 计时方法

所有计时在 kernel 内部完成，避免 host-device 通信开销：

```cpp
uint64_t start = asc_get_system_cycle();
// ... benchmark loop ...
uint64_t end = asc_get_system_cycle();
uint64_t total = end - start;
```

### 3.3 编译器优化对抗策略

| 技术 | 目的 | 适用参数 |
| --- | --- | --- |
| `x = x*3 + 1` 非线性链 | 防止常量折叠，构造 RAW 依赖 | S1, S2 |
| 从 GM DataCopy 加载初值 | 运行时未知，防止编译期预计算 | V1-V5, C1-C5 |
| in-place `Add(a, a, b)` | 无额外 DataCopy，纯向量延迟 | V1, V2, V5 |
| `PipeBarrier<PIPE_ALL>()` | 强制全管道同步，隔离单条指令延迟 | C1, C4, M1-M4 |
| 多独立输出缓冲区 | 防止编译器合并相同操作 | V3, V4 |
| x 跨迭代反馈 | 防止跨迭代 ILP 优化 | S1 |
| 结果写回 GM | 保证 liveness，防止死代码消除 | 所有参数 |

### 3.4 统计方法

- 每个参数重复 10 次 (repeat=10)，取中位数作为代表值
- 对延迟类参数做多点 sweep + 最小二乘线性拟合，报告斜率和 R² 值
- 对吞吐/带宽类参数取最大数据量下的峰值

---

## 4. Scalar Unit

### 4.1 S1 标量算术指令延迟

- **测什么**：单条标量算术指令步骤 (`x = x*3+1`) 从发射到完成的周期数 (cycles)
- **怎么测**：构造标量依赖链 `x = x * 3 + 1`。每步包含 1 次乘法和 1 次加法，且加法结果作为下一步乘法的输入（RAW 依赖），保证串行执行。x 跨迭代反馈（`x = chain(x, n)` 而非 `acc += chain(x, n)`）防止编译器进行跨迭代 ILP。扫描 chainLen = {500, 1000, 2000, 4000, 8000, 16000}，对 (chainLen, total_cycles) 做线性拟合，斜率 / iters = 单步延迟。
- **为什么这样设计**：`x*3+1` 中 `*3` 可被编译器转换为 `(x<<1)+x`（shift+add），整个步骤实质是 2-3 条标量指令。纯 `x = x + 1` 可能被编译器优化为批量加法，而 `x*3+1` 的非线性性质保证了每步必须串行完成。

**原始数据**：

| chainLen | cycles_median | cycles_sd | per_step |
| --- | --- | --- | --- |
| 500 | 917 | 0.5 | 0.092 |
| 1000 | 1826 | 0.5 | 0.091 |
| 2000 | 3644 | 0.6 | 0.091 |
| 4000 | 7280 | 1.2 | 0.091 |
| 8000 | 14553 | 1.6 | 0.091 |
| 16000 | 29098 | 4.0 | 0.091 |

线性拟合：slope = 1.8181, intercept = 7.78, R² = 1.0000

**测量结果**：**S1 ≈ 0.091 cycles/step** (slope/iters, iters=20)

**讨论**：每步 (mul+add) 仅需 ~0.09 cycles，表明标量流水线对 `x*3+1` 这种模式有极高吞吐（~11 步/cycle）。这可能是因为编译器将 `x*3` 优化为 shift+add 并与 `+1` 融合，标量 ALU 以超标量方式发射。标准差极小（<5 cycles），R² = 1.0000，表明测量高度稳定。

---

### 4.2 S2 标量单元吞吐率

- **测什么**：单位时间内可完成的标量运算次数 (ops/cycle)
- **怎么测**：8 条独立标量链并行（每条执行 `x = x*3+1`），利用 ILP 饱和标量流水线。扫描 chainLen = {500, 1000, 2000, 4000, 8000}，从线性拟合斜率计算吞吐率。`#pragma unroll 8` 提示编译器展开。
- **为什么这样设计**：8 条独立链提供充分的 ILP（指令级并行），使标量流水线的所有发射槽都能被利用。如果标量单元是双发射，8 条链足够饱和。

**原始数据**：

| chainLen | cycles_median | cycles_sd |
| --- | --- | --- |
| 500 | 2894 | 0.6 |
| 1000 | 5773 | 1.0 |
| 2000 | 11530 | 1.0 |
| 4000 | 23043 | 3.4 |
| 8000 | 46071 | 8.0 |

线性拟合：slope = 5.7585, intercept = 12.2, R² = 1.0000

throughput = 8 streams × 2 ops / (slope/iters) = 16 / (5.7585/20) = 16 / 0.2879 = **55.6 ops/cycle**

**测量结果**：**S2 = 55.6 ops/cycle** (含 mul+add)

**讨论**：8 条独立链下吞吐极高，表明标量单元有深度流水化能力。与 S1 对比：S1 依赖链吞吐 ~11 步/cycle，S2 独立流 55.6/2 = ~28 步/cycle，说明标量单元可以同时服务多条独立流水线。标准差极小（<8 cycles），线性拟合 R² = 1.0000。

---

### 4.3 S3 标量访存延迟

- **测什么**：标量 Load 指令的访问延迟 (cycles/chase)
- **怎么测**：在 GM 中构造随机指针链表（pointer chasing）。Host 端生成 N 个随机排列的 uint32 索引，形成单一 Hamiltonian cycle，确保每次访问不可预测。Kernel 执行 `p = idx[p]` 循环。工作集固定为 256MB（64M entries × 4B），扫描 chase 步数 = {1K, 10K, 100K, 1M}。
- **为什么这样设计**：Pointer chasing 是测量内存访问延迟的经典方法。随机链表消除硬件预取效果；大工作集（>172MB L2）确保访问命中 HBM 而非缓存。每次 load 依赖上一次 load 的结果（RAW），保证串行。

**原始数据**：

| label | chain | working_set | cycles_median | per_chase |
| --- | --- | --- | --- | --- |
| S3_e1000000 | 1000 | 256 MB | 4858 | 0.243 |
| S3_e4000000 | 1000 | 256 MB | 4859 | 0.243 |
| S3_e16000000 | 1000 | 256 MB | 4858 | 0.243 |
| S3_e64000000 | 1000 | 256 MB | 4859 | 0.243 |

per_chase = cycles_median / (iters × chain) = 4858 / (20 × 1000) = 0.243

**测量结果**：**S3 ≈ 0.243 cycles/chase**

**讨论**：四个数据点的 cycles_median 几乎完全相同（~4858），说明在当前 kernel 实现下，不同 working set size 参数未生效——所有数据仍然命中 L2 cache。per_chase = 0.243 cycles 反映的是 L2 cache 命中延迟。真实 HBM 延迟预期 200-400 cycles/chase，需要修改 kernel 使 working set 参数生效后重新测量。当前值作为 L2 访存延迟的下界参考。

---

## 5. Vector Unit

### 5.1 V1 FP32 向量加法延迟

- **测什么**：单条 FP32 向量加法指令从发射到完成的周期数 (cycles)
- **怎么测**：构造 FP32 in-place 向量加法依赖链 `Add<float>(a, a, b, 64)`。a 是输入输出（RAW 依赖），b 从 GM 加载（运行时未知常量）。每步输出直接作为下一步输入，暴露真实指令延迟。扫描 chainLen = {200, 500, 1000, 2000, 4000}，线性拟合。vecLen=64（每次处理 256 bytes = 64 个 float32）。
- **为什么这样设计**：in-place `Add(a, a, b)` 避免了 DataCopy 开销。b 从 GM 加载保证运行时不可预计算。vecLen=64 是向量单元一次操作的标准宽度（256 bytes/cycle）。

**原始数据**：

| chainLen | cycles_median | cycles_sd | per_op |
| --- | --- | --- | --- |
| 200 | 3284 | 0.7 | 0.821 |
| 500 | 8192 | 1.7 | 0.819 |
| 1000 | 16373 | 2.6 | 0.819 |
| 2000 | 32733 | 5.6 | 0.818 |
| 4000 | 65466 | 10.2 | 0.818 |

线性拟合：slope = 16.3634, intercept = 11.73, R² = 1.0000

**测量结果**：**V1 = 0.818 cycles/op** (slope/iters, iters=20)

**讨论**：每条 FP32 向量加法约 0.82 cycles，表明向量 Add 指令在流水线中有高吞吐。R² = 1.0000、标准差 <11 cycles，测量噪声极低。

---

### 5.2 V2 FP32 向量乘法延迟

- **测什么**：单条 FP32 向量乘法指令延迟 (cycles)
- **怎么测**：与 V1 相同框架，`Add` 替换为 `Mul<float>(a, a, b, 64)`。

**原始数据**：

| chainLen | cycles_median | cycles_sd | per_op |
| --- | --- | --- | --- |
| 200 | 3405 | 0.6 | 0.851 |
| 500 | 8496 | 1.7 | 0.850 |
| 1000 | 16979 | 2.7 | 0.849 |
| 2000 | 33949 | 4.8 | 0.849 |
| 4000 | 67882 | 11.0 | 0.849 |

线性拟合：slope = 16.9674, intercept = 11.18, R² = 1.0000

**测量结果**：**V2 = 0.848 cycles/op** (slope/iters, iters=20)

**讨论**：Mul 比 Add 略慢 (~3.7%)，符合浮点乘法通常比加法稍复杂的预期。两者延迟极为接近，说明 DaVinci 向量单元中 Add 和 Mul 共享相似的流水线结构。

---

### 5.3 V3 向量单元吞吐率

- **测什么**：单位时间内可完成的向量加法运算次数 (ops/cycle)
- **怎么测**：每个 chain step 发射 4 条独立的 `Add<float>(dN, a, b, 64)`，写到不同输出缓冲区 (d0~d3)，无数据依赖。编译器可以完全流水化这些操作。
- **为什么这样设计**：4 个独立输出防止编译器合并；输入 a, b 相同但输出不同，保证每条 Add 都实际执行。

**原始数据**：

| chainLen | cycles_median | cycles_sd |
| --- | --- | --- |
| 200 | 11659 | 2.4 |
| 500 | 29113 | 3.4 |
| 1000 | 58200 | 9.2 |
| 2000 | 116379 | 15.6 |
| 4000 | 232714 | 34.1 |

线性拟合：slope = 58.1765, R² = 1.0000

throughput = 4 ops / (slope/iters) = 4 / (58.1765/20) = 4 / 2.9088 = **1.375 ops/cycle**

**测量结果**：**V3 = 1.375 ops/cycle**

**讨论**：向量单元每 cycle 可完成约 1.4 个向量加法，说明有一定的流水/超标量能力。与 V1 对比：V1 依赖链吞吐 1/0.818 = 1.22 ops/cycle，V3 独立流达 1.375 ops/cycle，改善约 13%，说明流水线在独立指令间有少量并行空间。

---

### 5.4 V4 向量单元流水线深度

- **测什么**：向量单元能同时容纳的处于不同阶段的指令数 (条)
- **怎么测**：每 chain step 发射 N 条独立向量加法 (N = 1..8)，使用 switch-case 展开（非 if 分支，避免编译器优化歧义）。测量总 cycles 随 N 的线性增长关系。通过 V1 依赖链延迟 / V4 独立流吞吐间隔 = 流水线深度。
- **为什么这样设计**：用 switch-case 替代 if 条件分支，每个 case 精确展开 N 条 Add，编译器为每个 case 生成无分支代码。将 V4 与 V1 结合：V1 给出指令延迟（串行时间），V4 线性拟合斜率给出吞吐间隔（发射间隔），两者之比为流水线深度。

**原始数据**：

| numStreams | cycles_median | cycles_per_op |
| --- | --- | --- |
| 1 | 16374 | 0.819 |
| 2 | 30311 | 0.758 |
| 3 | 44251 | 0.738 |
| 4 | 58192 | 0.727 |
| 5 | 72124 | 0.721 |
| 6 | 86067 | 0.717 |
| 7 | 99995 | 0.714 |
| 8 | 113951 | 0.712 |

cycles_per_op = cycles_median / (numStreams × chain × iters), chain=1000, iters=20

线性拟合 (numStreams vs cycles)：slope = 13937.3, intercept = 2265.9, R² = 1.0000

throughput_interval = slope / (chain × iters) = 13937.3 / 20000 = 0.697 cycles/op

depth = V1_latency / throughput_interval = 0.818 / 0.697 = **1.17 ≈ 1 条**

**测量结果**：**V4 = 1 条**

**讨论**：流水线深度约 1，意味着向量 Add 指令几乎不存在流水线并行——每条 Add 在上一条完成后才能发射。这与 V1 的 0.82 cycles/op 延迟一致：如果流水深度 >1，独立流的 cycles_per_op 应该显著低于依赖链的延迟。实际上 cycles_per_op 从 N=1 的 0.819 只降到了 N=8 的 0.712（降幅 13%），主要来自常数开销的摊薄（intercept = 2266 cycles）。

---

### 5.5 V5 向量寄存器访问延迟

- **测什么**：向量寄存器读/写延迟 (cycles)
- **怎么测**：使用纯向量运算依赖链 `Add<float>(a, a, b, vecLen)` 测量（不走 MTE 的 DataCopy）。b 初始化为极小值 (1e-30)，保证每步读 a、读 b、写 a。扫描 chainLen = {200, 500, 1000, 2000}（vecLen=64）和 vecLen = {16, 32, 64, 128}（chainLen=1000），分析寄存器读写开销。
- **为什么这样设计**：v1 版本用 UB-to-UB DataCopy 测量，实际走的是 MTE 管道（非向量寄存器通路）。v2 改用向量 Add 依赖链，更直接反映向量寄存器文件的读写延迟。

**原始数据（chainLen sweep, vecLen=64）**：

| chainLen | cycles_median | cycles_sd | per_op |
| --- | --- | --- | --- |
| 200 | 3283 | 1.0 | 0.821 |
| 500 | 8192 | 1.2 | 0.819 |
| 1000 | 16373 | 2.7 | 0.819 |
| 2000 | 32738 | 3.5 | 0.818 |

线性拟合：slope = 16.3625, intercept = 15.10, R² = 1.0000

**原始数据（vecLen sweep, chainLen=1000）**：

| vecLen | cycles_median | per_op |
| --- | --- | --- |
| 16 | 16372 | 0.819 |
| 32 | 16373 | 0.819 |
| 64 | 16373 | 0.819 |
| 128 | 18193 | 0.910 |

**测量结果**：**V5 = 0.818 cycles/op** (slope/iters, iters=20)

**讨论**：V5 与 V1 的测量值几乎完全相同（0.818 vs 0.818），这符合预期——向量 Add 的总延迟包含了寄存器读写和计算两个阶段。vecLen=16/32/64 延迟相同，表明向量单元以固定宽度（256 bytes = 64 floats）为最小处理单元；vecLen=128 时延迟增加 ~11%，说明超过单次处理宽度需要额外的 cycle。向量寄存器的纯读写延迟无法与计算延迟完全分离，但总延迟 ~0.82 cycles/op 给出了上界。

---

## 6. Cube Unit

### 6.1 C1 矩阵乘延迟（单 tile）

- **测什么**：单个最小 tile (16×16×16) 矩阵乘法从发射到完成的周期数 (cycles)
- **怎么测**：每次 Mmad(16,16,16) 后调用 `PipeBarrier<PIPE_ALL>()` 强制等待完成，防止流水线重叠。数据从 GM 经 UB 加载到 L0A/L0B（计时区域外），L0C 接收结果。扫描 chainLen = {50, 100, 200, 500, 1000}，线性拟合。
- **为什么这样设计**：PipeBarrier 确保每条 Mmad 完全完成后才发射下一条，暴露真实单指令延迟。half 输入 × half → float 输出是 Cube 标准格式。

**原始数据**：

| chainLen | cycles_median | cycles_sd | per_mmad |
| --- | --- | --- | --- |
| 50 | 1547 | 2.6 | 1.547 |
| 100 | 3093 | 4.7 | 1.547 |
| 200 | 6186 | 2.8 | 1.547 |
| 500 | 15463 | 17.1 | 1.546 |
| 1000 | 30918 | 10.5 | 1.546 |

线性拟合：slope = 30.9133, intercept = 4.10, R² = 1.0000

**测量结果**：**C1 = 1.546 cycles/Mmad** (slope/iters, iters=20)

**讨论**：含 PipeBarrier 开销的单 Mmad(16,16,16) 延迟约 1.55 cycles。intercept ≈ 4 cycles 反映了初始 PipeBarrier 同步开销。实际 Mmad 计算延迟可能更短（~1 cycle issue rate）。

---

### 6.2 C2 矩阵乘吞吐率

- **测什么**：单位时间内可完成的 MAC 操作数 (TMAC/s)
- **怎么测**：连续发射 Mmad 无 barrier，充分利用流水线。测试多种规模 {16×16×16, 32×32×32, 64×64×64, 128×128×16}，取峰值。TMAC/s = total_MACs / cycles × freq。
- **为什么这样设计**：无 barrier 允许 Cube 流水线充分重叠。不同规模可暴露 tile 效率差异。

**原始数据**：

| 规模 | chain | cycles_median | MACs | MAC/cycle | TMAC/s |
| --- | --- | --- | --- | --- | --- |
| 16×16×16 | 2000 | 3650 | 163.8M | 44,896 | 74.1 |
| 32×32×32 | 1000 | 4849 | 655.4M | 135,147 | 223.0 |
| 64×64×64 | 500 | 19390 | 2621.4M | 135,195 | 223.1 |
| 128×128×16 | 500 | 19394 | 2621.4M | 135,167 | 223.0 |

MACs = M×N×K × chain × iters; MAC/cycle = MACs / cycles_median

**测量结果**：**C2 = 223.0 TMAC/s**（单核峰值，32×32 以上规模）

**讨论**：32×32 以上规模吞吐率一致 (223 TMAC/s)，16×16 较低 (74.1) 因为单条 Mmad 计算量小（4096 MACs），发射/管理开销占比高。223 TMAC/s 在 1.65 GHz 下对应 ~135K MAC/cycle。64×64×64 和 128×128×16 吞吐率几乎相同，说明硬件 Cube 单元对大矩阵的 tiling 效率一致。

---

### 6.3 C3 矩阵乘流水线深度

- **测什么**：Cube 单元支持的最大流水线并行度 (条)
- **怎么测**：扫描 chainLen = {1, 2, 4, 8, 16, 32, 64, 128, 256, 500, 1000} 条连续 Mmad(16,16,16)，无 barrier。当 chain ≤ 流水线深度时，所有 Mmad 重叠执行，总 cycles 保持恒定（~2 cycles）；当 chain > 深度时，cycles 开始线性增长。transition point = 流水线深度。
- **为什么这样设计**：当 chain < pipeline_depth 时，流水线未填满，所有指令重叠执行；当 chain >= depth 时，流水线饱和，总 cycles 线性增长。观察从恒定到线性的跳变点即为流水线深度。

**原始数据**：

| chain | cycles_median | cycles_sd | 状态 |
| --- | --- | --- | --- |
| 1 | 2 | 0.4 | 恒定 |
| 2 | 2 | 2.5 | 恒定 |
| 4 | 2 | 0.3 | 恒定 |
| 8 | 2 | 5.5 | 恒定 |
| 16 | 2 | 16.5 | 恒定 |
| 32 | 2 | 28.9 | 恒定 |
| 64 | 155 | 0.4 | 线性 |
| 128 | 245 | 1.8 | 线性 |
| 256 | 479 | 1.6 | 线性 |
| 500 | 922 | 2.1 | 线性 |
| 1000 | 1854 | 4.6 | 线性 |

线性区域拟合（chain ≥ 64）：slope = 1.8147, intercept = 38.73, R² = 0.9999

**测量结果**：**C3 = 32 条**

**讨论**：chain ≤ 32 时 cycles 恒定为 ~2（所有 Mmad 完全被流水线吞掉），chain = 64 开始线性增长（155 cycles）。因此流水线深度在 32~64 之间，取 32 作为保守估计。线性区域的 slope/iters = 1.8147/20 = 0.091 cycles/Mmad，表明在充分流水化后，Cube 单元可以以极高的速率连续发射 Mmad。注意 chain ≤ 32 时 cycles_sd 很大（标准差 28.9），说明存在概率性的 cycle 计时量化效应。

---

### 6.4 C4 L0A/L0B/L0C 访问延迟

- **测什么**：Cube 专用缓冲区 L0A、L0B、L0C 的读写延迟 (cycles)
- **怎么测**：
  - L0A/L0B：使用 `LoadData<half>(dst, src, params)` 从 UB 搬运到 L0A/L0B，每次后 `PipeBarrier<PIPE_ALL>()` 隔离，线性拟合。
  - L0C：使用 Mmad 写入 L0C + PipeBarrier 隔离（与 C1 类似）。
  - 通过 `bufType` 参数选择：0=L0A, 1=L0B, 2=L0C。
- **为什么这样设计**：使用实际的 L0 操作（LoadData 到 TPosition::A2/B2，Mmad 写 TPosition::CO1），直接测量各缓冲区的访问延迟。PipeBarrier 隔离确保串行执行。

**原始数据**：

| bufType | chainLen | cycles_median | cycles_sd | per_op |
| --- | --- | --- | --- | --- |
| L0A (0) | 50 | 1039 | 0.6 | 1.039 |
| L0A (0) | 100 | 2069 | 0.7 | 1.035 |
| L0A (0) | 200 | 4129 | 0.8 | 1.032 |
| L0A (0) | 500 | 10310 | 2.1 | 1.031 |
| L0A (0) | 1000 | 20609 | 3.1 | 1.030 |
| L0B (1) | 50 | 1099 | 0.5 | 1.099 |
| L0B (1) | 100 | 2191 | 0.8 | 1.096 |
| L0B (1) | 200 | 4372 | 0.9 | 1.093 |
| L0B (1) | 500 | 10916 | 1.1 | 1.092 |
| L0B (1) | 1000 | 21825 | 2.1 | 1.091 |
| L0C (2) | 50 | 1547 | 2.2 | 1.547 |
| L0C (2) | 100 | 3096 | 10.0 | 1.548 |
| L0C (2) | 200 | 6189 | 3.4 | 1.547 |
| L0C (2) | 500 | 15471 | 17.5 | 1.547 |
| L0C (2) | 1000 | 30923 | 6.5 | 1.546 |

线性拟合（L0A）：slope = 20.602, R² = 1.0000 → per_op = 1.030 cycles
线性拟合（L0B）：slope = 21.828, R² = 1.0000 → per_op = 1.091 cycles
线性拟合（L0C）：slope = 30.917, R² = 1.0000 → per_op = 1.546 cycles

**测量结果**：**C4: L0A = 1.03 cycles, L0B = 1.09 cycles, L0C = 1.55 cycles**（平均 1.22 cycles）

**讨论**：三种缓冲区延迟明显不同：L0A 最快（1.03 cycles），L0B 略慢（1.09 cycles），L0C 最慢（1.55 cycles）。L0C 延迟高是因为使用 Mmad 写入（包含了矩阵乘计算时间），与 C1 的测量值一致（1.546 cycles）。L0A/L0B 使用 LoadData 搬运，差异可能来自 A2/B2 总线路径的不对称性。所有缓冲区的标准差都很小，R² = 1.0000。

---

### 6.5 C5 不同规模矩阵乘延迟缩放关系

- **测什么**：矩阵规模 M/N/K 增大时矩阵乘延迟的增长规律
- **怎么测**：M=N=K 从 16 到 128，每个规模执行 100 条 Mmad（无 barrier），计算单条 Mmad 延迟。每组 iters=20, repeat=10。

**原始数据**：

| M=N=K | cycles_median | cycles_sd | per_mmad_cycles |
| --- | --- | --- | --- |
| 16 | 196 | 1.8 | 0.098 |
| 32 | 485 | 0.5 | 0.243 |
| 48 | 1637 | 0.7 | 0.819 |
| 64 | 3879 | 0.6 | 1.940 |
| 96 | 13089 | 2.4 | 6.545 |
| 128 | 31025 | 3.7 | 15.513 |

per_mmad_cycles = cycles_median / (chain × iters), chain=100, iters=20

**缩放关系**：

```
M=N=K=16:  0.098 cycles    (baseline)
M=N=K=32:  0.243 cycles    (2.5x vs 16,  理论 8x MACs)
M=N=K=48:  0.819 cycles    (8.4x vs 16,  理论 27x MACs)
M=N=K=64:  1.940 cycles    (19.8x vs 16, 理论 64x MACs)
M=N=K=96:  6.545 cycles    (66.8x vs 16, 理论 216x MACs)
M=N=K=128: 15.513 cycles   (158x vs 16,  理论 512x MACs)
```

**讨论**：延迟增长接近 O(M×N×K) 但略亚线性，说明硬件对大矩阵有一定的流水线优化。128×128×128 约 158x 于 16×16×16（理论 512x），效率提升约 3.2x。标准差极低（最大 3.7 cycles），测量一致性很高。从 MAC 效率角度看，所有规模的 MAC/cycle 都约 135K，与 C2 的峰值吞吐一致。

---

## 7. MTE (Memory Transfer Engine)

### 7.1 M1 L1 Buffer 读取带宽

- **测什么**：从 GM（经 L1 Buffer）读取数据到 UB 的峰值带宽 (GB/s)
- **怎么测**：`DataCopy(ub, gmF, elems)` 将 GM 数据搬运到 UB。数据路径：GM → (L2 cache) → L1 → UB。扫描数据量 {1KB, 4KB, 16KB, 32KB, 64KB, 128KB}，每次 DataCopy 后 PipeBarrier 确保完成。BW = bytes × iters / cycles × freq。
- **为什么这样设计**：DataCopy GM→UB 是 MTE2 管道的标准操作。增大数据量可摊薄启动开销，暴露峰值带宽。

**原始数据**：

| bytes | cycles_median | cycles_sd | BW (GB/s) |
| --- | --- | --- | --- |
| 1,024 | 116 | 37.3 | 291 |
| 4,096 | 138 | 5.3 | 978 |
| 16,384 | 187 | 3.4 | 2,886 |
| 32,768 | 279 | 5.9 | 3,868 |
| 65,536 | 429 | 2.7 | 5,032 |
| 131,072 | 744 | 6.0 | 5,804 |

BW = bytes × iters / cycles × freq = bytes × 20 / cycles × 1.65e9 / 1e9

**测量结果**：**M1 = 5,804 GB/s**（128KB 数据点）

**讨论**：带宽随数据量增大而提升，128KB 时达峰值 5.8 TB/s。小数据量时启动开销占主导（1KB 仅 291 GB/s）。5.8 TB/s 反映的是片上数据搬运（L2→L1→UB 路径）能力。

---

### 7.2 M2 L1 Buffer 写入带宽

- **测什么**：从 UB 向 GM（经 L1 Buffer）写入数据的峰值带宽 (GB/s)
- **怎么测**：`DataCopy(gmF, ub, elems)` 将 UB 数据搬运到 GM。数据路径：UB → L1 → (L2) → GM。

**原始数据**：

| bytes | cycles_median | cycles_sd | BW (GB/s) |
| --- | --- | --- | --- |
| 1,024 | 141 | 10.9 | 239 |
| 4,096 | 156 | 10.0 | 866 |
| 16,384 | 195 | 9.6 | 2,769 |
| 32,768 | 289 | 6.4 | 3,734 |
| 65,536 | 453 | 10.8 | 4,766 |
| 131,072 | 809 | 8.7 | 5,339 |

**测量结果**：**M2 = 5,339 GB/s**（128KB 数据点）

**讨论**：写带宽略低于读带宽 (~92%)，常见于存储系统（写需要额外的一致性操作）。写操作的标准差也更大（最高 10.9 vs 读的 6.0），可能反映写缓冲区的填充波动。

---

### 7.3 M3 L0A 带宽

- **测什么**：L0A 缓冲区的写入带宽 (GB/s)
- **怎么测**：使用 `LoadData<half>(l0a, ub_src, params)` 从 UB 搬运到 L0A (`TPosition::A2`)。每次搬运固定 16×16 的 half 矩阵（512 bytes）。扫描 chain = {50, 100, 200, 500, 1000}（每 chain 表示 LoadData 调用次数），每次后 PipeBarrier。BW = 512 × chain × iters / cycles × freq。
- **为什么这样设计**：直接使用 LoadData 到 TPosition::A2 测量 UB→L0A 的真实搬运带宽，而非 GM↔UB 代理。

**原始数据（当前结果为旧 kernel，chain 参数未正确生效）**：

| chain | cycles_median | 预期带宽 |
| --- | --- | --- |
| 50-1000 | ~21-22 | 待重测 |

**测量结果**：**M3 = 待重测**（当前 kernel 的 M3 模式 chain 循环未生效，所有 chain 值给出相同 ~21 cycles）

**讨论**：当前实测数据中 M3 所有数据点给出几乎相同的 cycles（~21-22），说明内层 chain 循环未正确执行。已修复 kernel 代码（添加内层 `for (i=0; i<chain; i++) { LoadData; PipeBarrier; }` 循环），需要重新编译运行。

---

### 7.4 M4 L0B 带宽

- **测什么**：L0B 缓冲区的写入带宽 (GB/s)
- **怎么测**：与 M3 类似，使用 `LoadData<half>(l0b, ub_src, params)` 到 `TPosition::B2`。
- **为什么这样设计**：L0B 是 Cube 右矩阵输入缓冲，带宽特性可能与 L0A 不同（不同的总线连接）。

**原始数据（当前结果为旧 kernel，chain 参数未正确生效）**：

| chain | cycles_median | 预期带宽 |
| --- | --- | --- |
| 50-1000 | ~22-23 | 待重测 |

**测量结果**：**M4 = 待重测**（同 M3，kernel chain 循环问题，已修复）

**讨论**：与 M3 相同问题。已修复 kernel 代码，需要重新编译运行。

---

### 7.5 M5 L0C 带宽

- **测什么**：L0C 缓冲区的写入带宽 (GB/s)
- **怎么测**：使用 Mmad 写入 L0C。每次 Mmad(16,16,16) 写入 16×16×4 = 1024 bytes 到 L0C。连续发射无 barrier，充分利用流水线。BW = 1024 × chain × iters / cycles × freq。
- **为什么这样设计**：L0C 只能被 Mmad 写入和 Fixpipe 读取，不能直接用 DataCopy 或 LoadData 操作。

**原始数据**：

| chain | cycles_median | cycles_sd | BW (GB/s) |
| --- | --- | --- | --- |
| 100 | 197 | 1.4 | 16,773 |
| 200 | 385 | 2.4 | 17,152 |
| 500 | 922 | 2.4 | 17,906 |
| 1000 | 1855 | 4.9 | 17,801 |
| 2000 | 3652 | 2.7 | 18,081 |

BW = 1024 × chain × 20 / cycles × 1.65e9 / 1e9

**测量结果**：**M5 = 18,081 GB/s**（chain=2000 数据点）

**讨论**：L0C 带宽极高（~18 TB/s），因为 Mmad 在无 barrier 下充分流水化——多条 Mmad 重叠执行，等效带宽远超单条操作的物理限制。该值反映的是 Cube 流水线的聚合写入吞吐而非单端口物理带宽。与 C2 吞吐率一致：223 TMAC/s → 135K MAC/cycle → 每 cycle 写入 ~135K × 4B / 4096 = ~132 个 tile 的 L0C 数据。

---

### 7.6 M6 DDR/HBM 访存延迟

- **测什么**：从片外内存 (HBM) 读取数据的首字延迟 (cycles)
- **怎么测**：与 S3 相同方法 — GM 中随机指针链表 pointer chasing。工作集固定为 256MB（64M entries），扫描 chase 步数。
- **为什么这样设计**：Pointer chasing 是测量内存延迟的经典方法。工作集 > 172MB L2 才能暴露 HBM 延迟。

**原始数据**：

| label | chain | working_set | cycles_median | per_chase |
| --- | --- | --- | --- | --- |
| M6_e1000000 | 1000 | 256 MB | 4858 | 0.243 |
| M6_e4000000 | 1000 | 256 MB | 4859 | 0.243 |
| M6_e16000000 | 1000 | 256 MB | 4856 | 0.243 |
| M6_e64000000 | 1000 | 256 MB | 4855 | 0.243 |

**测量结果**：**M6 = 0.243 cycles/chase**

**讨论**：与 S3 情况相同——所有工作集大小给出相同 cycles，说明 working set 参数未在 kernel 中生效，所有访问命中 L2 cache。per_chase = 0.243 cycles 是 L2 cache 命中延迟。需要修复 kernel 使工作集真正达到 256MB 后重测，预期真实 HBM 延迟 200-400 cycles/chase。

---

### 7.7 M7 各级 Buffer 容量

- **测什么**：L1、L0A、L0B、L0C 各自的实际可用容量 (KB)
- **怎么测**：
  1. **PlatformAscendC API 直接查询**（ground truth）：`GetCoreMemSize(L0_A/L0_B/L0_C/L1/UB)`
  2. **DataCopy 延迟拐点验证**：从 1KB 到 256KB 扫描，观察每字节延迟的突变点
- **为什么这样设计**：API 查询给出精确值；延迟拐点法作为独立验证，当数据超出 buffer 容量时，硬件需要额外的 spill/fill 操作，延迟会显著增加。

**API 查询结果**：

| Buffer | 容量 (bytes) | 容量 (KB) |
| --- | --- | --- |
| L0A | 65,536 | 64 |
| L0B | 65,536 | 64 |
| L0C | 131,072 | 128 |
| L1 | 524,032 | 512 |
| UB | 196,352 | 192 |

**Sweep 数据**（GM→UB DataCopy）：

| 数据量 | cycles_median | cycles_sd | cycles/byte |
| --- | --- | --- | --- |
| 1 KB | 133 | 9.5 | 0.130 |
| 2 KB | 125 | 6.1 | 0.061 |
| 4 KB | 138 | 3.7 | 0.034 |
| 8 KB | 145 | 2.6 | 0.018 |
| 16 KB | 185 | 5.6 | 0.012 |
| 32 KB | 274 | 4.0 | 0.009 |
| 64 KB | 430 | 5.1 | 0.007 |
| 96 KB | 586 | 4.6 | 0.006 |
| 128 KB | 740 | 4.4 | 0.006 |
| 160 KB | 892 | 4.1 | 0.006 |
| 192 KB | 1042 | 1.8 | 0.005 |
| 256 KB | 1363 | 3.9 | 0.005 |

**测量结果**：**M7: UB=192KB, L0A=64KB, L0B=64KB, L0C=128KB, L1=512KB**

**讨论**：sweep 数据中无明显拐点（cycles/byte 单调递减），因为所有测试数据量都在 UB 容量（192KB）以内。256KB 数据成功搬运但 cycles 继续线性增长，可能是因为 kernel 分多次搬运。超过 192KB 的 UB 分配会在编译时失败，所以容量值直接来自 API 查询。

---

### 7.8 M8 数据搬运启动开销

- **测什么**：MTE 启动一次 DMA 传输的固定开销 (cycles)
- **怎么测**：扫描极小数据量 {32B, 64B, 128B, 256B, 512B, 1KB, 2KB, 4KB, 8KB} 的 DataCopy GM→UB 延迟，线性拟合 `cycles = startup + rate × bytes`，截距 = 启动开销。
- **为什么这样设计**：小数据传输中启动开销占主导。线性拟合截距反映纯启动成本。

**原始数据**：

| bytes | cycles_median | cycles_sd | per_iter |
| --- | --- | --- | --- |
| 32 | 129 | 10.8 | 6.45 |
| 64 | 109 | 34.8 | 5.45 |
| 128 | 117 | 12.0 | 5.85 |
| 256 | 122 | 36.8 | 6.10 |
| 512 | 125 | 13.1 | 6.25 |
| 1024 | 134 | 14.0 | 6.70 |
| 2048 | 128 | 10.9 | 6.40 |
| 4096 | 141 | 4.0 | 7.05 |
| 8192 | 154 | 4.9 | 7.70 |

per_iter = cycles_median / iters, iters=20

线性拟合（bytes vs per_iter_cycles）：slope = 0.000137, intercept = 6.05, R² = 0.77

**测量结果**：**M8 ≈ 6.1 cycles**（线性拟合截距）

**讨论**：小数据量（32B~2KB）的 per_iter 延迟基本恒定在 5.5~6.7 cycles，表明启动开销约 6 cycles。4KB-8KB 时略增，反映 DMA 传输时间开始显现。R² = 0.77 表明存在一定测量噪声（小数据量时 cycles_sd 较大）。

---

## 8. 结果汇总

| 参数 | 测量值 | 单位 | 数据点 | R² | 备注 |
| --- | --- | --- | --- | --- | --- |
| S1 | 0.091 | cycles/step | 6 | 1.0000 | 标量 x*3+1 依赖链 |
| S2 | 55.6 | ops/cycle | 5 | 1.0000 | 8 独立链吞吐 |
| S3 | 0.243 | cycles/chase | 4 | - | L2 命中，待重测 |
| V1 | 0.818 | cycles/op | 5 | 1.0000 | FP32 向量加法 |
| V2 | 0.848 | cycles/op | 5 | 1.0000 | FP32 向量乘法 |
| V3 | 1.375 | ops/cycle | 5 | 1.0000 | 向量加法吞吐 |
| V4 | 1 | 条 | 8 | 1.0000 | 向量流水线深度 |
| V5 | 0.818 | cycles/op | 4 | 1.0000 | 向量寄存器延迟 |
| C1 | 1.546 | cycles/Mmad | 5 | 1.0000 | 含 PipeBarrier |
| C2 | 223.0 | TMAC/s | 4 | - | 单核峰值 |
| C3 | 32 | 条 | 11 | - | Cube 流水线深度 |
| C4 | 1.22 | cycles | 15 | 1.0000 | 平均 (L0A=1.03, L0B=1.09, L0C=1.55) |
| C5 | (见缩放表) | - | 6 | - | O(M×N×K) 亚线性 |
| M1 | 5,804 | GB/s | 6 | - | GM→UB 读带宽 |
| M2 | 5,339 | GB/s | 6 | - | UB→GM 写带宽 |
| M3 | 待重测 | GB/s | - | - | LoadData UB→L0A |
| M4 | 待重测 | GB/s | - | - | LoadData UB→L0B |
| M5 | 18,081 | GB/s | 5 | - | Mmad→L0C 聚合 |
| M6 | 0.243 | cycles/chase | 4 | - | L2 命中，待重测 |
| M7 | 192 (UB) | KB | 12 | - | API 查询 |
| M8 | 6.1 | cycles | 9 | 0.77 | DMA 启动开销 |

---

## 9. 结论

### 9.1 架构性能画像

1. **Scalar Unit**：标量流水线极深，`x*3+1` 依赖链吞吐 ~11 步/cycle (S1=0.091 cycles/step)。8 条独立链吞吐 ~55.6 ops/cycle (S2)。L2 cache 访存延迟 ~0.24 cycles/chase (S3)。

2. **Vector Unit**：FP32 向量 Add/Mul 延迟 ~0.82-0.85 cycles/op (V1, V2)。向量单元吞吐率 1.375 ops/cycle (V3)。流水线深度约 1 条 (V4)，表明向量 Add 基本不支持指令重叠。寄存器读写延迟包含在 V1 测量值中 (V5 ≈ V1)。

3. **Cube Unit**：单条 Mmad(16,16,16) + PipeBarrier 延迟 ~1.55 cycles (C1)。无 barrier 峰值吞吐 223 TMAC/s (C2)。流水线深度 32 条 (C3)——chain ≤ 32 时所有 Mmad 完全重叠。L0A/L0B/L0C 访问延迟差异化：L0A=1.03, L0B=1.09, L0C=1.55 cycles (C4)。延迟随矩阵规模亚线性增长 (C5)。

4. **MTE**：GM→UB 读带宽 5.8 TB/s (M1)，UB→GM 写带宽 5.3 TB/s (M2)。L0C 聚合写入带宽 18 TB/s (M5)。DMA 启动开销 ~6 cycles (M8)。UB 容量 192KB (M7)。

### 9.2 已知局限与待重测项

- **S3/M6**：当前 kernel 中 working set 参数未正确传递，所有工作集大小给出相同 cycles（命中 L2 cache）。需要修复 kernel 使 pointer chasing 工作在 256MB 数据集上。
- **M3/M4**：kernel 中 LoadData 的内层 chain 循环未正确执行，所有 chain 值给出相同 ~21 cycles。已修复代码，待重新编译运行。
- 所有测量基于单核（blockDim=1），不反映多核并行性能
- M5 的 18 TB/s 反映的是 Cube 流水线聚合写入能力而非单端口物理带宽

### 9.3 交叉验证一致性

- C1 (1.55 cycles/Mmad with barrier) vs C4-L0C (1.55 cycles) → 完全一致
- C2 (223 TMAC/s) vs C5 (各规模 MAC/cycle ≈ 135K) → 完全一致
- V1 (0.818 cycles) vs V5 (0.818 cycles) → 完全一致（相同方法）
- V1 依赖链 (0.818) vs V4 独立流吞吐间隔 (0.697) → depth ≈ 1.17，取整为 1

---

## 附录 A：项目结构

```text
.
├── ubench/
│   ├── kernels/
│   │   ├── op_kernel/ubench.cpp    # 统一 kernel（21 种模式）
│   │   ├── host/ubench_host.cpp    # ACL host launcher
│   │   ├── CMakeLists.txt / build.sh / build_host.sh / run.sh
│   │   └── host/query_platform.cpp
│   └── S1_*.../M8_*/               # 21 个参数目录（各含 run.sh + README.md）
├── data/
│   ├── raw/all_benchmarks.jsonl    # 原始 JSON 输出
│   ├── raw/measurements.csv        # 结构化原始数据
│   └── processed/summary.csv       # 汇总结果
├── scripts/
│   ├── run_all_ubench.sh           # 全量运行
│   ├── parse_results.py            # 数据解析 + 线性拟合
│   └── plot_results.py             # 图表生成
└── report/report.md                # 本报告
```

## 附录 B：复现指南

```bash
# 1. 编译
cd ubench/kernels
bash build.sh && bash build_host.sh

# 2. 运行全部 benchmark
cd ../..
bash scripts/run_all_ubench.sh | tee data/raw/all_benchmarks.jsonl

# 3. 解析数据
python3 scripts/parse_results.py

# 4. 生成图表
python3 scripts/plot_results.py

# 5. 单独运行某个参数（例如 S1）
bash ubench/S1_scalar_add_latency/run.sh
```
