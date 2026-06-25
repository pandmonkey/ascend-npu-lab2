# 开发日志

记录 Lab2 microbenchmark 套件从骨架到可运行的完整开发过程。

## 时间线

### 阶段 0：环境探测

- 确认运行环境即为 Ascend 910B 机器（非远程）
- SoC: Ascend910_9362，CANN 9.0.0，驱动 25.5.2
- 16+ NPU 设备节点 `/dev/davinci[0-15]`
- `npu-smi info` 确认：AI Core 频率 1650 MHz，20 AIC + 40 AIV，HBM 64GB
- 编译器：`ccec`（bisheng），位于 `/usr/local/Ascend/cann-9.0.0/bin/`
- CMake 模块：`ascendc.cmake` 位于 `tikcpp/ascendc_kernel_cmake/`

### 阶段 1：工具链搭建

**目标**：找到从 Ascend C kernel 源码到可执行 host 程序的完整编译路径。

**尝试过的路径：**
1. ~~ACL NN 自定义算子框架（op_host + op_kernel）~~ → 太重，每个 benchmark 需要完整的 tiling 框架
2. ~~`aclrtBinaryLoadFromFile` + 手写 kernel binary~~ → 二进制打包格式不透明
3. ✅ `ascendc_library` + `ASCEND_KERNEL_LAUNCH_ONLY=ON` → 生成静态库 `libubench_kernels.a`，内含 `aclrtlaunch_ubench()` host launch 函数

**最终方案：**
- Kernel: `ubench/kernels/op_kernel/ubench.cpp`，统一 21 种模式的 `__global__ __aicore__` 函数
- Host: `ubench/kernels/host/ubench_host.cpp`，ACL runtime 分配 GM、传参、launch、读回结果
- 构建: `build.sh`（CMake + ccec 编译 kernel）+ `build_host.sh`（g++ 链接静态库 + ACL）

**关键发现：**
- `asc_get_system_cycle()`（`c_api/sys_var/sys_var.h`）提供片上 cycle 计数，避免 host-device 通信干扰
- 生成的 `host_stub.cpp` 自动注册 kernel 二进制并暴露 `aclrtlaunch_ubench()` C 接口
- `aclrtFree()` 只接受 1 个参数（不是 3 个）
- `aclrtSynchronizeStream()`（不是 `aclrtSyncStream`）

### 阶段 2：编译器优化对抗（核心难点）

这是整个项目最大的技术挑战。Ascend C 编译器（bisheng/clang）极其激进地优化循环。

**遇到的问题和解决：**

| 问题 | 现象 | 原因 | 解决 |
| --- | --- | --- | --- |
| 标量加法链被折叠 | `x += 1` 循环 → 0 cycles | 编译器折叠为 `x += n` | 改用 `x = x*3 + 1`（mul+add，非线性） |
| 标量跨迭代ILP | S1 每add仅0.09 cycles | 10次迭代独立，编译器流水化 | x 跨迭代反馈（`x = chain(x, n)` 而非 `acc += chain(x, n)`） |
| 向量加法链被消除 | V1 不随chain增长 | DataCopy(a,c) 被死代码消除 | 改用 in-place `Add(a, a, b)`，无 DataCopy |
| 向量结果被预计算 | V1 恒定~54 cycles | 输入用 Duplicate 常量初始化 | 从 GM DataCopy 加载运行时未知初值 |
| Max 扰动混入延迟 | V1 = 35.5→14.5 cycles | Max 也在 V 管道，增加关键路径 | 移除 Max，用纯 `Add(a, a, b)` RAW 链 |
| Cube Mmad 被流水化 | C1 = 0.92 cycles | 连续 Mmad 全重叠 | 每次 Mmad 后 `PipeBarrier<PIPE_ALL>()` |
| 向量吞吐被消除 | V3 = 0.13 ops/cycle | 相同输入的重复 Add 被合并 | 4 个独立输出缓冲区轮转 |

**最终 V1/V2 方案：**
```cpp
// 纯 in-place RAW 依赖链，无额外指令混入
for (uint32_t i = 0; i < chainLen; ++i) {
    Add<float>(a, a, b, len);  // a = a + b, RAW on a
}
```
- V1 = 8.2 cycles，V2 = 8.5 cycles（Mul 替换 Add）

### 阶段 3：平台参数获取

编写 `host/query_platform.cpp`，通过 `PlatformAscendC` API 获取 ground truth：

```
L0A: 64 KB    L0B: 64 KB    L0C: 128 KB
L1:  512 KB   UB:  192 KB   L2:  172 MB (共享)
AICore: 20 AIC + 40 AIV
AI Core Freq: 1650 MHz (npu-smi)
HBM: 64GB, 1600 MHz
```

这些值用于：
- M7（buffer 容量）的直接报告
- 验证 C2（Cube 吞吐率）量级
- 校正 SoC 频率从 1.25GHz → 1.65GHz

### 阶段 4：数据采集与分析

**脚本：**
- `scripts/run_all_ubench.sh`：21 个参数 × 多组配置 sweep，JSON 输出
- `scripts/parse_results.py`：JSON → measurements.csv + summary.csv，线性拟合
- `scripts/plot_results.py`：ASCII 图表（无 matplotlib 依赖）

**统计方法：**
- 延迟类：5 个 chain 长度 → 最小二乘线性拟合 → 斜率 = 单条指令延迟
- 吞吐类：线性拟合斜率 → ops/cycle
- 带宽类：最大数据量下的 bytes×iters/cycles×freq
- pointer chasing（S3/M6）：仅用 chain≥16000 的数据点（避开 L2 cache）

### 阶段 5：已知问题与未解决项

| 参数 | 问题 | 影响 |
| --- | --- | --- |
| S3/M6 | HBM 延迟 ~50 cycles，疑似仍命中 L2（L2=172MB） | 可能 D 级 |
| V4 | 流水线深度=1，但实际可能 >1（if 分支影响调度） | 可能 D 级 |
| V5 | 用 UB DataCopy 代理寄存器延迟，不准 | 可能 D 级 |
| C2 | 72.9 TMAC/s 单核，可能偏高（Mmad(16,16,16) 可能不是单指令） | 可能 C/D 级 |
| C3 | 未找到饱和点，报 500 | 可能 D 级 |
| C4 | 用 UB DataCopy 代理 L0A/L0B/L0C 延迟 | 可能 D 级 |
| M3-M5 | 用 GM↔UB DataCopy 代理 L0A/L0B/L0C 带宽 | 可能 D 级 |

### 阶段 6：报告与打包

- `report/report.md` → `scripts/md_to_pdf.py`（ps2pdf）→ `report/report.pdf`
- 21 个 ubench 目录的 README 由 `scripts/gen_ubench_readmes.py` 生成
- 打包：`lab2_2300013126_赵烁然.zip`

## 关键文件索引

| 文件 | 用途 |
| --- | --- |
| `ubench/kernels/op_kernel/ubench.cpp` | 统一 kernel，21 种模式 |
| `ubench/kernels/op_kernel/ubench_args.h` | kernel-host 参数结构体 |
| `ubench/kernels/host/ubench_host.cpp` | ACL host launcher |
| `ubench/kernels/host/query_platform.cpp` | 平台参数查询工具 |
| `ubench/kernels/CMakeLists.txt` | kernel 编译配置 |
| `ubench/kernels/build.sh` | kernel 构建脚本 |
| `ubench/kernels/build_host.sh` | host 构建脚本 |
| `ubench/kernels/run.sh` | 单参数运行入口 |
| `scripts/run_all_ubench.sh` | 全量 sweep 运行脚本 |
| `scripts/parse_results.py` | 数据解析 + 线性拟合 |
| `scripts/plot_results.py` | ASCII 图表生成 |
| `scripts/md_to_pdf.py` | Markdown → PDF 转换 |
| `scripts/gen_ubench_readmes.py` | ubench 目录 README 生成 |
