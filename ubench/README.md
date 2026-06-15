# Microbenchmark Directory Map

每个子目录对应一个必测参数。后续拿到昇腾环境后，每个目录至少应补齐：

- `README.md`：说明测什么、怎么测、为什么这样测、如何运行。
- 源代码：按实际环境选择 Ascend C、TIK、CANN 自定义算子、TensorOJ 模板或 Python wrapper。
- 构建脚本：例如 `build.sh`、`CMakeLists.txt` 或课程平台要求的配置文件。
- 运行脚本：例如 `run.sh`，输出可追加到 `data/raw/measurements.csv` 的记录。

## 参数目录

| 参数 | 目录 | 优先级 |
| --- | --- | --- |
| S1 | `S1_scalar_add_latency` | core |
| S2 | `S2_scalar_add_throughput` | normal |
| S3 | `S3_scalar_memory_latency` | normal |
| V1 | `V1_fp32_vector_add_latency` | core |
| V2 | `V2_fp32_vector_mul_latency` | core |
| V3 | `V3_vector_add_throughput` | normal |
| V4 | `V4_vector_pipeline_depth` | normal |
| V5 | `V5_vector_register_latency` | normal |
| C1 | `C1_cube_tile_matmul_latency` | core |
| C2 | `C2_cube_matmul_throughput` | normal |
| C3 | `C3_cube_pipeline_depth` | normal |
| C4 | `C4_l0abc_access_latency` | normal |
| C5 | `C5_matmul_scaling` | normal |
| M1 | `M1_l1_read_bandwidth` | core |
| M2 | `M2_l1_write_bandwidth` | core |
| M3 | `M3_l0a_bandwidth` | normal |
| M4 | `M4_l0b_bandwidth` | normal |
| M5 | `M5_l0c_bandwidth` | normal |
| M6 | `M6_hbm_memory_latency` | normal |
| M7 | `M7_buffer_capacity` | normal |
| M8 | `M8_mte_startup_overhead` | normal |

