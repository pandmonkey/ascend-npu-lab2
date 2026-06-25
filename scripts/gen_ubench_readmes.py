#!/usr/bin/env python3
"""Generate README.md for each ubench directory with design details."""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "benchmark_manifest.csv"

DESIGNS = {
"S1": """## Design
- **测什么**：单条标量加法指令延迟 (cycles)
- **怎么测**：构造标量加依赖链 `x = x + (x&1) + 1`，链长500~8000，线性拟合斜率
- **为什么**：RAW依赖+非线性扰动防止编译器折叠；从GM加载初值防预计算
- **运行**：`cd ubench/kernels && bash build.sh && bash build_host.sh && bash run.sh S1`""",
"S2": """## Design
- **测什么**：标量单元吞吐率 (ops/cycle)
- **怎么测**：8条独立标量加法链并行，总ops=chainLen×8，吞吐率=ops/cycles
- **为什么**：独立链暴露ILP和流水线能力
- **运行**：`cd ubench/kernels && bash run.sh S2`""",
"S3": """## Design
- **测什么**：标量访存延迟 (cycles)
- **怎么测**：GM中随机指针链表pointer chasing，线性拟合斜率
- **为什么**：随机访问消除预取，RAW依赖暴露真实延迟
- **运行**：`cd ubench/kernels && bash run.sh S3`""",
"V1": """## Design
- **测什么**：FP32向量加法延迟 (cycles)
- **怎么测**：int16向量加依赖链 `c=a+b; a=c; b^=c`，XOR扰动非线性，链长200~4000
- **为什么**：向量ALU对int16/FP32共享流水线；XOR不在加法关键路径
- **运行**：`cd ubench/kernels && bash run.sh V1`""",
"V2": """## Design
- **测什么**：FP32向量乘法延迟 (cycles)
- **怎么测**：与V1相同框架，Add替换为Mul
- **为什么**：与V1对称，交叉验证乘加延迟
- **运行**：`cd ubench/kernels && bash run.sh V2`""",
"V3": """## Design
- **测什么**：向量单元吞吐率 (ops/cycle)
- **怎么测**：连续无依赖向量加 `c=a+b`，吞吐率=chainLen/cycles
- **为什么**：无跨迭代依赖，全速流水
- **运行**：`cd ubench/kernels && bash run.sh V3`""",
"V4": """## Design
- **测什么**：向量单元流水线深度 (条)
- **怎么测**：每步发射N条独立加法(N=1..8)，观察吞吐率饱和点
- **为什么**：独立指令可同时处于不同流水阶段，饱和点即深度
- **运行**：`cd ubench/kernels && bash run.sh V4`""",
"V5": """## Design
- **测什么**：向量寄存器读/写延迟 (cycles)
- **怎么测**：UB到UB DataCopy链 `c=a; a=c`，每对含一次读写
- **为什么**：DataCopy经过向量寄存器，近似寄存器访问延迟
- **运行**：`cd ubench/kernels && bash run.sh V5`""",
"C1": """## Design
- **测什么**：单tile(16×16×16)矩阵乘延迟 (cycles)
- **怎么测**：L0A/L0B中放half矩阵，Mmad计算到L0C，chainLen=100~1000线性拟合
- **为什么**：16×16×16是Cube最小粒度，LoadData仅在初始化不进入计时
- **运行**：`cd ubench/kernels && bash run.sh C1`""",
"C2": """## Design
- **测什么**：矩阵乘吞吐率 (MAC/s)
- **怎么测**：大规模连续Mmad(16³~256³)，MAC/s=M×N×K×chain/cycles×频率
- **为什么**：大规模使Cube单元饱和
- **运行**：`cd ubench/kernels && bash run.sh C2`""",
"C3": """## Design
- **测什么**：Cube流水线深度 (条)
- **怎么测**：增加连续Mmad数量(10~500)，观察吞吐饱和点
- **为什么**：独立tile可并行进入流水线
- **运行**：`cd ubench/kernels && bash run.sh C3`""",
"C4": """## Design
- **测什么**：L0A/L0B/L0C访问延迟 (cycles)
- **怎么测**：小缓冲区DataCopy读写链作为代理，bufType区分L0A/L0B/L0C
- **为什么**：UB DataCopy是访问L0缓冲区的代理测量
- **运行**：`cd ubench/kernels && bash run.sh C4`""",
"C5": """## Design
- **测什么**：矩阵规模M/N/K增大时延迟增长规律
- **怎么测**：扫描M=N=K=16~256，画延迟折线图
- **为什么**：揭示Cube计算量与延迟的缩放关系
- **运行**：`cd ubench/kernels && bash run.sh C5`""",
"M1": """## Design
- **测什么**：L1 Buffer读取带宽 (GB/s)
- **怎么测**：DataCopy GM→UB，数据量4KB~128KB，带宽=bytes×iters/cycles×频率
- **为什么**：连续大块读取使MTE带宽饱和
- **运行**：`cd ubench/kernels && bash run.sh M1`""",
"M2": """## Design
- **测什么**：L1 Buffer写入带宽 (GB/s)
- **怎么测**：DataCopy UB→GM，数据量4KB~128KB
- **为什么**：连续大块写入使MTE带宽饱和
- **运行**：`cd ubench/kernels && bash run.sh M2`""",
"M3": """## Design
- **测什么**：L0A缓冲区读写带宽 (GB/s)
- **怎么测**：GM→UB DataCopy作为L0A带宽代理
- **为什么**：L0A带宽通过DataCopy路径间接测量
- **运行**：`cd ubench/kernels && bash run.sh M3`""",
"M4": """## Design
- **测什么**：L0B缓冲区读写带宽 (GB/s)
- **怎么测**：GM→UB DataCopy作为L0B带宽代理
- **运行**：`cd ubench/kernels && bash run.sh M4`""",
"M5": """## Design
- **测什么**：L0C缓冲区读写带宽 (GB/s)
- **怎么测**：UB→GM DataCopy作为L0C带宽代理
- **运行**：`cd ubench/kernels && bash run.sh M5`""",
"M6": """## Design
- **测什么**：HBM访存首字延迟 (cycles)
- **怎么测**：GM随机指针链表pointer chasing
- **为什么**：随机访问消除预取，暴露真实HBM延迟
- **运行**：`cd ubench/kernels && bash run.sh M6`""",
"M7": """## Design
- **测什么**：各级Buffer容量 (KB)
- **怎么测**：扫描DataCopy工作集大小1KB~128KB，观察周期/字节比率拐点
- **为什么**：工作集超过缓存容量时延迟跳变
- **运行**：`cd ubench/kernels && bash run.sh M7`""",
"M8": """## Design
- **测什么**：MTE启动一次DMA的固定开销 (cycles)
- **怎么测**：扫描小数据传输32B~4KB，线性拟合cycles=a+b×bytes，截距a为启动开销
- **为什么**：小数据下固定开销占比大，线性拟合可分离
- **运行**：`cd ubench/kernels && bash run.sh M8`""",
}

def main():
    with open(MANIFEST, newline='', encoding='utf-8') as f:
        rows = list(csv.DictReader(f))
    
    for row in rows:
        pid = row['id']
        ubench_dir = ROOT / 'ubench' / row['ubench_dir']
        ubench_dir.mkdir(parents=True, exist_ok=True)
        readme = ubench_dir / 'README.md'
        
        design = DESIGNS.get(pid, '## Design\n- TODO\n')
        content = f"""# {pid} {row['name']}

## Measurement Target
- Category: {row['category']}
- Unit: {row['unit']}
- Priority: {row['priority']}
- Pattern: {row['measurement_pattern']}
- Source code: `ubench/kernels/op_kernel/ubench.cpp` (mode={row['notes'].split('；')[0] if row['notes'] else ''})

{design}

## Notes
{row['notes']}
"""
        readme.write_text(content, encoding='utf-8')
    
    print(f"Generated {len(rows)} README files")

if __name__ == '__main__':
    main()
