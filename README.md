# Lab2 Ascend NPU Microbenchmark

本目录用于完成“并行与分布式导论 Lab2：昇腾 NPU 微基准测试设计”。

当前阶段先完成不依赖昇腾卡的准备工作：作业要求提取、工程结构、参数清单、数据记录格式、报告骨架和脚本。真正的测量结果必须在昇腾 910B 或课程指定等价环境上采集。

## 目录结构

```text
.
├── README.md
├── new-pdc01-intro.pdf
├── 作业要求提取.md
├── docs/
│   ├── benchmark_manifest.csv
│   ├── preparation_plan.md
│   └── remote_workflow.md
├── ubench/
│   ├── README.md
│   ├── S1_scalar_add_latency/
│   ├── ...
│   └── M8_mte_startup_overhead/
├── data/
│   ├── raw/
│   │   └── measurements_template.csv
│   └── processed/
├── scripts/
│   ├── init_ubench_readmes.py
│   ├── probe_ascend_env.sh
│   ├── run_all.sh
│   ├── run_one.sh
│   ├── summarize_results.py
│   └── validate_project.py
└── report/
    └── report.md
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
```

远端详细流程见 [docs/remote_workflow.md](docs/remote_workflow.md)。
