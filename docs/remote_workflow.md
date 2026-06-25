# Remote Ascend Workflow

> **更新：本实验实际在 Ascend 910B 机器上直接运行（非远程 SSH）。** 以下为实际工作流。

## 环境

当前运行环境即为 Ascend 910B 机器：

- OS：openEuler 22.03 SP4, aarch64
- CANN：9.0.0（`/usr/local/Ascend/cann-9.0.0`）
- 设备：Ascend910_9362，16+ NPU
- 用户：`u2300013126`

## 第一次进入机器

```bash
git clone git@github.com:pandmonkey/ascend-npu-lab2.git
cd ascend-npu-lab2
bash scripts/probe_ascend_env.sh
bash scripts/inspect_ascend_templates.sh
```

如果远端没有配置 GitHub SSH key，可以先用 HTTPS：

```bash
git clone https://github.com/pandmonkey/ascend-npu-lab2.git
```

`probe_ascend_env.sh` 会生成：

```text
docs/env_snapshots/env_snapshot_YYYYmmdd_HHMMSS.txt
```

这个文件默认被 `.gitignore` 忽略，避免把主机名、内网 IP、用户名、设备状态直接提交到 GitHub。需要分析环境时，把文件内容贴回来即可。

## 日常更新代码

```bash
cd ~/ascend-npu-lab2
git pull
```

## CANN 环境

脚本默认加载：

```bash
/usr/local/Ascend/cann-9.0.0/set_env.sh
```

如果课程机器切换到其他 CANN 版本，可以用环境变量覆盖：

```bash
export ASCEND_CANN_PATH=/usr/local/Ascend/cann-8.5.0
bash scripts/probe_ascend_env.sh
```

当前远端已确认存在：

- `/usr/local/Ascend/cann-9.0.0/aarch64-linux/asc/include/kernel_operator.h`
- `/usr/local/Ascend/cann-9.0.0/aarch64-linux/tikcpp/ascendc_kernel_cmake/ascendc.cmake`
- `/usr/local/Ascend/cann-9.0.0/tools/op_project_templates/op_project_tmpl/build.sh`
- `/usr/local/Ascend/cann-9.0.0/tools/new_op_project_template/custom_op/build.sh`

`which ascendc` 找不到是正常的；Ascend C 不是一个叫 `ascendc` 的单独命令，主要通过 `ccec`、CMake 模块、头文件和工程模板使用。

## 运行单个参数

```bash
bash scripts/run_one.sh S1
```

目前 `ubench/` 目录还只是骨架。后续某个参数补齐 `run.sh` 或 `build_and_run.sh` 后，就可以通过统一入口运行。

## 运行核心参数

```bash
bash scripts/run_all.sh --core
```

核心参数来自 PDF 建议：

- `S1`
- `V1`
- `V2`
- `C1`
- `M1`
- `M2`

## 运行全部参数

```bash
bash scripts/run_all.sh
```

## 数据文件

真实测量结果应写入：

```text
data/raw/measurements.csv
```

字段格式参考：

```text
data/raw/measurements_template.csv
```

汇总结果由脚本生成：

```bash
python3 scripts/summarize_results.py
```

输出：

```text
data/processed/summary.csv
```

## 推回结果

确认不包含敏感环境快照后：

```bash
git status --short
git add data/raw/measurements.csv data/processed/summary.csv ubench report docs scripts README.md
git commit -m "Add benchmark results"
git push
```

不要直接提交 `docs/env_snapshots/`，除非你确认里面没有敏感信息。
