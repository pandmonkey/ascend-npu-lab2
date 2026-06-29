# Ascend 910B 硬件环境详细报告

> 本文档详细记录 Lab2 实验所用 Ascend 910B 计算卡的硬件规格、微架构参数和软件环境。
> 目的是为外部模型提供足够的上下文，以评估 21 个 microbenchmark 测量值的准确性。

## 1. 芯片标识

| 项目 | 值 | 来源 |
| --- | --- | --- |
| SoC 名称 | `Ascend910_9362` | `aclrtGetSocName()` |
| SocVersion 枚举 | 1 (`ASCEND910B`) | `PlatformAscendC::GetSocVersion()` |
| NPU 架构代号 | A2A3 | `buffer_limits.hpp` 中 `PTO_NPU_ARCH_A2A3` |
| 产品名 | IT22HMDA_4_S | `npu-smi info -t board` |
| 厂商 | Huawei | `npu-smi info -t board` |
| PCI Vendor ID | 0x19E5 | `npu-smi info -t board` |
| PCI Device ID | 0xD803 | `npu-smi info -t board` |
| 芯片数/卡 | 2 (每张卡 2 个 die) | `npu-smi info -t board` |

## 2. 频率与核心

| 项目 | 值 | 来源 |
| --- | --- | --- |
| AI Core 标称频率 | 1650 MHz | `npu-smi info -t common` |
| AI Core 当前频率 | 800 MHz (空闲降频) | `npu-smi info -t common` |
| AI Core 数量 (AIC) | 20 / die | `npu-smi info -t common` + `PlatformAscendC::GetCoreNumAic()` |
| Vector Core 数量 (AIV) | 40 / die | `PlatformAscendC::GetCoreNumAiv()` |
| CoreNum (总计) | 40 | `PlatformAscendC::GetCoreNum()` |
| HBM 频率 | 1600 MHz | `npu-smi info -t memory` |
| 整机 NPU 卡数 | 8 (共 16 个 die) | `npu-smi info -l` |

**注意**：`asc_get_system_cycle()` 的计数频率需要与 AI Core 实际运行频率对应。标称 1650 MHz，但空闲时降频到 800 MHz。运行 benchmark 时频率可能动态变化。

## 3. 片上存储层级

### 3.1 容量（PlatformAscendC API 实测）

| Buffer | 容量 (bytes) | 容量 | 对齐 | 来源 |
| --- | --- | --- | --- | --- |
| L0A | 65,536 | 64 KB | 32B | `GetCoreMemSize(L0_A)` |
| L0B | 65,536 | 64 KB | 32B | `GetCoreMemSize(L0_B)` |
| L0C | 131,072 | 128 KB | 32B | `GetCoreMemSize(L0_C)` |
| L1 (CBUF) | 524,032 | ~512 KB | 32B | `GetCoreMemSize(L1)` |
| UB (UBUF) | 196,352 | ~192 KB | 32B | `GetCoreMemSize(UB)` |
| L2 | 176,160,768 | ~172 MB | - | `GetCoreMemSize(L2)` |
| HBM | 34,359,738,368 | 32 GB | - | `GetCoreMemSize(HBM)` |
| Bias | 2,048 | 2 KB | 32B | `buffer_limits.hpp` |
| FB | 2,048 | 2 KB | - | `GetCoreMemSize(FB)` |

**注意**：
- L2 = 172 MB 是整个 die 共享的（非每核独占），远大于 L1/UB/L0
- L1 实际可用 = 524,032 bytes（比标称 512KB 略多，含管理开销）
- UB 实际可用 = 196,352 bytes（比标称 192KB 略多）

### 3.2 带宽（PlatformAscendC API 实测，per-core）

| Buffer | 带宽 (bytes/cycle) | 换算 (GB/s @1650MHz) |
| --- | --- | --- |
| L2 | 110 | 181.5 |
| HBM | 32 | 52.8 |
| L0A/L0B/L0C/UB/L1 | 0 (API 未返回) | - |

**HBM 聚合带宽估算**：32 bytes/cycle × 20 cores × 1.65 GHz ≈ **1056 GB/s**（单 die）

### 3.3 存储层级关系

```
HBM (32GB, ~1 TB/s 聚合)
  └── L2 Cache (~172 MB, 共享, 110 B/cycle per core)
       └── L1 Buffer (~512 KB, per-core, Cube 用)
            ├── L0A (64 KB, Cube 左矩阵输入)
            ├── L0B (64 KB, Cube 右矩阵输入)
            └── L0C (128 KB, Cube 输出累加器)
       └── UB / Unified Buffer (~192 KB, per-core, Vector 用)
```

### 3.4 TPosition 枚举

```cpp
enum class TPosition : int32_t {
    GM,      // Global Memory (HBM)
    A1,      // L1 Buffer (Cube 输入)
    A2,      // L0A (Cube 左矩阵)
    B1,      // L1 Buffer (Cube 输入)
    B2,      // L0B (Cube 右矩阵)
    C1,      // L1 Buffer (Cube 输出)
    C2,      // L0C (Cube 输出累加器)
    CO1,     // L0C (另一种表示)
    CO2,     // L1 (输出)
    VECIN,   // UB (Vector 输入)
    VECOUT,  // UB (Vector 输出)
    VECCALC, // UB (Vector 计算)
    LCM,     // = VECCALC
};
```

## 4. 计算单元

### 4.1 Scalar Unit
- 标量 ALU，执行整数/浮点标量运算
- 每周期发射 1 条指令（推测）
- 加法延迟 ~1 cycle（实测）

### 4.2 Vector Unit
- 向量 ALU，执行 SIMD 向量运算
- 向量寄存器宽度：`GetVecRegLen()` 返回 0（API 在此 SoC 上未返回有效值）
- 基于 CANN 文档，910B 向量单元每个 cycle 处理 256 bytes（fp32 = 64 元素）
- 向量指令通过 `repeatTime` 参数控制重复次数
- FP32 Add/Mul 延迟 ~8 cycles（实测）

### 4.3 Cube Unit
- 矩阵乘单元，执行 `Mmad` 指令
- 最小 tile：16×16×16（M×N×K），每条 Mmad 做 4096 次 MAC
- `MmadParams` 结构体：`{m, n, k, isBias, fmOffset, enSsparse, enWinogradA, enWinogradB}`
- `LoadData` 从 L1 加载到 L0A/L0B，`Mmad` 从 L0A/L0B 计算到 L0C
- `Fixpipe` 从 L0C 搬运到 L1/UB
- 单次 Mmad(16,16,16) 延迟 ~15 cycles（含 PipeBarrier，实测）

### 4.4 MTE (Memory Transfer Engine)
- 数据搬运引擎，执行 `DataCopy` 在 GM 和 UB 之间搬运
- 管道类型：`PIPE_MTE2`（GM→UB 读）、`PIPE_MTE3`（UB→GM 写）
- `DataCopy` 支持连续和 2D strided 模式

### 4.5 流水线类型

DaVinci 架构有多条独立流水线，通过 `pipe_t` 区分：

| 管道 | 用途 |
| --- | --- |
| PIPE_S | Scalar（标量） |
| PIPE_V | Vector（向量） |
| PIPE_M | Cube/Matmul（矩阵乘） |
| PIPE_MTE2 | MTE 读（GM→片上） |
| PIPE_MTE3 | MTE 写（片上→GM） |
| PIPE_FIX | Fixpipe（L0C→L1/UB） |

`PipeBarrier<PIPE_ALL>()` 等待所有管道完成。

## 5. 软件环境

| 项目 | 值 |
| --- | --- |
| CANN 版本 | 9.0.0 |
| 驱动版本 | 25.5.2 |
| ascendhal 版本 | 7.35.23 |
| Kernel 编译器 | ccec (bisheng clang 15.0.5) |
| Host 编译器 | g++ (aarch64) |
| CMake | 3.22.0 |
| Python | 3.11 |
| OS | openEuler 22.03 SP4 |
| Kernel | 5.10.0-216.0.0.115.oe2203sp4.aarch64 |

## 6. 主机 CPU

| 项目 | 值 |
| --- | --- |
| 架构 | aarch64 |
| CPU 总数 | 640 |
| 每路核心数 | 80 |
| 路数 | 4 |
| 每核线程数 | 2 |
| 厂商 | HiSilicon |
| 最大频率 | 2900 MHz |
| 最小频率 | 400 MHz |
| L1d 缓存 | 20 MiB (320 instances) |
| L2 缓存 | 400 MiB (320 instances) |
| L3 缓存 | 560 MiB (8 instances) |
| 指令集 | SVE, BF16, I8MM, SHA3, SM3, SM4 |

## 7. 拓扑

- 8 张 NPU 卡，每卡 2 个 die，共 16 个 Ascend910 芯片
- 每个 die：20 AIC + 40 AIV，64GB HBM
- PCIe 互联（`npu-smi info -t hccs` 可查 HCCS 带宽）

## 8. 计时与频率换算

### 8.1 `asc_get_system_cycle()`

- 定义在 `c_api/sys_var/sys_var.h`
- 返回 `int64_t`，AI Core 内部周期计数器
- 计数频率 = AI Core 运行频率

### 8.2 频率换算

| 场景 | 频率 | 1 cycle |
| --- | --- | --- |
| 标称（满载） | 1650 MHz | 0.606 ns |
| 空闲（降频） | 800 MHz | 1.25 ns |

**风险**：benchmark 运行时频率可能从 800 MHz 逐渐升到 1650 MHz。短时间 kernel 可能未触发升频，导致 cycle 计数偏高。

### 8.3 带宽换算

```
带宽 (GB/s) = bytes_moved / cycles * freq_GHz
            = bytes_moved / cycles * 1.65
```

## 9. 关键 Ascend C API

### 9.1 计时

```cpp
#include "c_api/sys_var/sys_var.h"
int64_t cycles = asc_get_system_cycle();
```

### 9.2 存储

```cpp
// TBuf 分配 UB 空间
TBuf tBuf;
pipe.InitBuffer(tBuf, size_bytes);
auto tensor = tBuf.Get<float>();  // 或 half, int16_t

// TBuf 指定位置（L0A/L0B/L0C）
TBuf<TPosition::A2> tL0A;
TBuf<TPosition::B2> tL0B;
TBuf<TPosition::CO1> tL0C;

// GlobalTensor
GlobalTensor<float> gm;
gm.SetGlobalBuffer((__gm__ float *)addr);
```

### 9.3 计算

```cpp
Add<float>(dst, src0, src1, count);      // 向量加
Mul<float>(dst, src0, src1, count);      // 向量乘
Max<float>(dst, src0, src1, count);      // 向量取大
Xor<int16_t>(dst, src0, src1, count);    // 向量异或（仅 int16/uint16）
Duplicate<float>(dst, value, count);     // 填充
DataCopy(dst, src, count);               // 数据搬运
LoadData<half>(l0a, l1, params);         // L1→L0A 加载
Mmad<float, half, half>(cL0, aL0, bL0, mmadParams);  // 矩阵乘
PipeBarrier<PIPE_ALL>();                  // 全管道同步
```

### 9.4 类型限制

| 操作 | 支持的类型 |
| --- | --- |
| Add/Mul/Max/Min | float, half, int16_t, int32_t |
| Xor/And/Or | **仅 int16_t / uint16_t** |
| Mmad | half×half→float, int8×int8→int32 |
| DataCopy | 所有类型 |

## 10. 已知测量问题

| 参数 | 测量值 | 疑似实际值 | 问题 |
| --- | --- | --- | --- |
| S3/M6 | ~50 cycles | ~200-400 cycles | L2=172MB，pointer chasing 工作集仅 1MB，可能命中 L2 |
| V4 | 1 | ~4-8 | if 分支影响指令调度；8 个缓冲区可能不够 |
| V5 | 10.9 | ~1-2 | DataCopy 走 MTE 管道，非纯寄存器访问 |
| C2 | 72.9 TMAC/s | ~6-16 TMAC/s | Mmad(16,16,16) 可能不是单条指令 |
| C3 | 500 | ~8-16 | 未找到饱和点 |
| C4 | 10.9 | ~2-5 | UB DataCopy 代理，非直接 L0 访问 |
| M3-M5 | ~3600 GB/s | 未知 | GM↔UB DataCopy 代理 |

## 11. 文件索引

| 文件 | 用途 |
| --- | --- |
| `ubench/kernels/host/query_platform.cpp` | 平台参数查询源码 |
| `ubench/kernels/op_kernel/ubench.cpp` | 统一 kernel（21 模式） |
| `scripts/parse_results.py` | 数据解析（含频率换算） |
| `data/processed/summary.csv` | 21 参数最终结果 |
| `data/raw/all_benchmarks.jsonl` | 原始 JSON 输出 |
