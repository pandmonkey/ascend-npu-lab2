# 实验环境

> 本文档记录 Lab2 实验的实际运行环境，由 `scripts/probe_ascend_env.sh` 和 `host/query_platform.cpp` 采集。

## 硬件

| 项目 | 内容 |
| --- | --- |
| 设备型号 | Ascend 910B |
| SoC | Ascend910_9362 |
| NPU 设备节点 | `/dev/davinci[0-15]`（16+ 设备） |
| AI Core 数量 | 20 AIC + 40 AIV（每芯片） |
| AI Core 频率 | 1650 MHz（实测 `npu-smi info -t common`） |
| HBM 容量 | 64 GB |
| HBM 频率 | 1600 MHz |
| OS | openEuler 22.03 SP4 |
| Kernel | 5.10.0-216.0.0.115.oe2203sp4.aarch64 |
| 架构 | aarch64 |

## 软件

| 项目 | 内容 |
| --- | --- |
| CANN 版本 | 9.0.0 |
| 驱动版本 | 25.5.2 |
| CANN 路径 | `/usr/local/Ascend/cann-9.0.0` |
| Kernel 编译器 | `ccec`（bisheng），`/usr/local/Ascend/cann-9.0.0/bin/` |
| Host 编译器 | `g++`（aarch64） |
| CMake | 3.22.0 |
| Python | 3.11 |

## 片上存储容量（PlatformAscendC API 实测）

| Buffer | 容量 (bytes) | 容量 (KB) |
| --- | --- | --- |
| L0A | 65,536 | 64 |
| L0B | 65,536 | 64 |
| L0C | 131,072 | 128 |
| L1 | 524,032 | 512 |
| UB | 196,352 | 192 |
| L2 | 176,160,768 | 172,032 (共享) |
| HBM | 34,359,738,368 | 33,554,432 (32 GB) |

来源：`ubench/kernels/host/query_platform.cpp` → `PlatformAscendC::GetCoreMemSize()`

## 计时方式

- **片上 cycle 计数**：`asc_get_system_cycle()`（`c_api/sys_var/sys_var.h`）
- 频率换算：1 cycle = 1/1.65 ns ≈ 0.606 ns
- 优势：直接在 kernel 内测量，无 host-device 通信干扰

## 构建/运行命令

```bash
# 环境
source /usr/local/Ascend/cann-9.0.0/set_env.sh
export LD_LIBRARY_PATH=/usr/local/Ascend/cann-9.0.0/lib64:/usr/local/Ascend/cann-9.0.0/aarch64-linux/lib64:${LD_LIBRARY_PATH:-}

# 编译 kernel
cd ubench/kernels && bash build.sh

# 编译 host
bash build_host.sh

# 运行单个参数
bash run.sh S1

# 运行全部 21 个参数
cd ../.. && bash scripts/run_all_ubench.sh

# 解析结果
python3 scripts/parse_results.py

# 生成图表
python3 scripts/plot_results.py
```
