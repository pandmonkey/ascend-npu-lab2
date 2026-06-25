# Lab2 Ascend NPU Microbenchmark

本目录用于完成"并行与分布式导论 Lab2：昇腾 NPU 微基准测试设计"。

> **状态：已完成。** 21 个参数均已实现、运行、采集数据并生成报告。

## 目录结构

```text
.
├── README.md
├── new-pdc01-intro.pdf
├── 作业要求提取.md
├── docs/
│   ├── benchmark_manifest.csv
│   ├── environment.md              # 实验环境
│   ├── development_log.md           # 开发日志
│   ├── measurement_methodology.md  # 测量方法论
│   ├── preparation_plan.md
│   └── remote_workflow.md
├── ubench/
│   ├── README.md
│   ├── S1_*/ ... M8_*/             # 21 个参数目录（各含 README.md）
│   └── kernels/                    # 统一 kernel 工程
│       ├── op_kernel/ubench.cpp    # Ascend C kernel（21 种模式）
│       ├── host/ubench_host.cpp    # ACL host launcher
│       ├── CMakeLists.txt
│       ├── build.sh / build_host.sh / run.sh
│       └── host/query_platform.cpp # 平台参数查询工具
├── data/
│   ├── raw/
│   │   ├── measurements.csv        # 原始测量数据
│   │   └── all_benchmarks.jsonl    # JSON 格式原始输出
│   └── processed/
│       ├── summary.csv             # 汇总结果
│       └── plots/                  # ASCII 图表（每个参数一个 .txt）
├── scripts/
│   ├── run_all_ubench.sh           # 全量 sweep 运行脚本
│   ├── parse_results.py            # 数据解析 + 线性拟合
│   ├── plot_results.py             # ASCII 图表生成
│   ├── md_to_pdf.py                # Markdown → PDF
│   ├── gen_ubench_readmes.py       # ubench 目录 README 生成
│   ├── validate_project.py
│   └── summarize_results.py
└── report/
    ├── report.md                   # 实验报告源文件
    └── report.pdf                   # 最终报告
```

## 当前可做与不可做

可提前完成：

- 明确 21 个必测参数和评分口径。
- 准备每个 microbenchmark 的独立目录。
- 写清每个参数的设计思路、控制变量、预期输出字段。
- 固定原始数据记录格式。
- 写结果汇总脚本和报告骨架。

必须等拿到昇腾卡后完成：

- 编译并运行真实 kernel 或平台测试程序。
- 采集真实延迟、带宽、吞吐率、流水线深度、容量数据。
- 用真实数据生成统计结果、图表和最终 `report.pdf`。

## 建议执行顺序

1. 确认上机环境：昇腾卡型号、CANN 版本、是否能使用 Ascend C / TIK / TensorOJ、计时 API、提交平台限制。
2. 优先完成 PDF 点名的核心参数：`S1`、`V1`、`V2`、`C1`、`M1`、`M2`。
3. 每完成一个参数，就把原始数据追加到 `data/raw/measurements.csv`。
4. 运行 `python3 scripts/summarize_results.py` 生成 `data/processed/summary.csv`。
5. 把统计结果和图表写入 `report/report.md`，最终导出 `report.pdf`。

## 本地检查

```bash
python3 scripts/init_ubench_readmes.py
python3 scripts/validate_project.py
python3 scripts/summarize_results.py
```

## 远端机器工作流

进入远端：

```bash
ssh pdc-ascend
```

第一次进入后：

```bash
git clone git@github.com:pandmonkey/ascend-npu-lab2.git
cd ascend-npu-lab2
bash scripts/probe_ascend_env.sh
bash scripts/inspect_ascend_templates.sh
```

远端详细流程见 [docs/remote_workflow.md](docs/remote_workflow.md)。
