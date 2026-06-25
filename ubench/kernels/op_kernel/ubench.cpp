// Unified microbenchmark kernel for Ascend 910B (DaVinci).
// Uses asc_get_system_cycle() for in-kernel cycle counting.
//
// Convention:
//   - args.chainLen: inner-loop count (swept for latency linearity)
//   - args.iters:    outer repetitions (for statistics)
//   - total = cycles for iters * chainLen operations
//   - host computes per-op = total / (iters * chainLen)
#include "kernel_operator.h"
#include "c_api/sys_var/sys_var.h"
#include "ubench_args.h"

using namespace AscendC;

enum Mode {
    M_S1 = 1, M_S2, M_S3, M_V1, M_V2, M_V3, M_V4, M_V5,
    M_C1, M_C2, M_C3, M_C4, M_C5,
    M_M1, M_M2, M_M3, M_M4, M_M5, M_M6, M_M7, M_M8,
};

// ---- Scalar ----------------------------------------------------------------

__aicore__ inline uint32_t scalar_add_chain(uint32_t x, uint32_t n) {
    // Scalar add dependency chain. Uses x = x*3 + 1 where the add depends on
    // the mul result (RAW). The compiler cannot fold x+n. Each step has
    // 1 mul + 1 add on the scalar pipeline. The throughput per step gives
    // the scalar unit pipeline throughput; the add latency is ~1 cycle.
    for (uint32_t i = 0; i < n; ++i) {
        x = x * 3u + 1u;
    }
    return x;
}

__aicore__ inline uint32_t scalar_add_streams(uint32_t x, uint32_t n) {
    // 8 independent scalar add chains. Each chain: a[j] += 1.
    // The compiler can pipeline these (ILP across 8 chains).
    uint32_t a[8] = {x, x + 1, x + 2, x + 3, x + 4, x + 5, x + 6, x + 7};
    for (uint32_t i = 0; i < n; ++i) {
        #pragma unroll 8
        for (uint32_t j = 0; j < 8; ++j) {
            a[j] = a[j] * 3u + 1u;  // non-foldable, 2 ops per step
        }
    }
    return a[0] ^ a[1] ^ a[2] ^ a[3] ^ a[4] ^ a[5] ^ a[6] ^ a[7];
}

__global__ __aicore__ void ubench(GM_ADDR argsGM, GM_ADDR dataGM, GM_ADDR outGM) {
    auto argsPtr = (__gm__ UbenchArgs *)argsGM;
    UbenchArgs args;
    args.mode=argsPtr->mode; args.chainLen=argsPtr->chainLen;
    args.vecLen=argsPtr->vecLen; args.numStreams=argsPtr->numStreams;
    args.m=argsPtr->m; args.n=argsPtr->n; args.k=argsPtr->k;
    args.bufType=argsPtr->bufType; args.warmup=argsPtr->warmup;
    args.iters=argsPtr->iters;

    uint64_t start = 0, end = 0, total = 0;
    volatile uint32_t sink = 0;
    auto out = (__gm__ uint64_t *)outGM;

    TPipe pipe;
    GlobalTensor<uint8_t> dataGm;
    dataGm.SetGlobalBuffer((__gm__ uint8_t *)dataGM);

    switch (args.mode) {
        // ==================== SCALAR ====================
        case M_S1: {
            // Single scalar-add dependency chain. chainLen steps per iter.
            // x feeds back across iters for one continuous chain.
            uint32_t x = 0x12345678u;
            for (uint32_t w = 0; w < args.warmup; ++w) { x = scalar_add_chain(x, args.chainLen); }
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it) {
                x = scalar_add_chain(x, args.chainLen);
            }
            end = asc_get_system_cycle();
            sink ^= x;
            total = end - start;
            break;
        }
        case M_S2: {
            uint32_t x = 0x12345678u;
            for (uint32_t w = 0; w < args.warmup; ++w) sink ^= scalar_add_streams(x, args.chainLen);
            uint32_t acc = 0;
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it)
                acc += scalar_add_streams(x, args.chainLen);
            end = asc_get_system_cycle();
            sink ^= acc;
            total = end - start;
            break;
        }
        case M_S3:
        case M_M6: {
            // Pointer chasing in GM for load latency.
            // dataGM holds uint32 next-index array forming a random cycle.
            // The working set (chainLen * 4 bytes) should be >> L2 cache
            // (256KB on 910B) to ensure each access goes to HBM.
            auto idxGm = (__gm__ uint32_t *)dataGM;
            uint32_t p = 0;
            for (uint32_t w = 0; w < args.warmup; ++w)
                for (uint32_t i = 0; i < args.chainLen; ++i) p = idxGm[p];
            uint32_t acc = 0;
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it)
                for (uint32_t i = 0; i < args.chainLen; ++i) { p = idxGm[p]; acc ^= p; }
            end = asc_get_system_cycle();
            sink ^= acc ^ p;
            total = end - start;
            break;
        }

        // ==================== VECTOR (FP32) ====================
        case M_V1:
        case M_V2:
        case M_V3:
        case M_V4:
        case M_V5: {
            // Vector benchmarks using FP32 float (as required by the spec).
            // vecLen = number of float elements per vector op (default 64).
            uint32_t len = args.vecLen;
            TBuf tA, tB, tC;
            pipe.InitBuffer(tA, len * sizeof(float));
            pipe.InitBuffer(tB, len * sizeof(float));
            pipe.InitBuffer(tC, len * sizeof(float));
            auto a = tA.Get<float>();
            auto b = tB.Get<float>();
            auto c = tC.Get<float>();
            // Load runtime-unknown initial values from GM (defeats precompute).
            {
                GlobalTensor<float> seedGm;
                seedGm.SetGlobalBuffer((__gm__ float *)dataGM);
                DataCopy(a, seedGm, len);
                DataCopy(b, seedGm[len], len);
            }
            GlobalTensor<float> accGm;
            accGm.SetGlobalBuffer((__gm__ float *)outGM);

            if (args.mode == M_V1) {
                // V1: FP32 vector add latency.
                // Pure in-place dependency chain: a = a + b (RAW on a).
                // b loaded from GM (runtime-unknown), stays constant.
                // Each Add depends on previous a, exposing true add latency.
                for (uint32_t w = 0; w < args.warmup; ++w) {
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(a, a, b, len);
                    }
                }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) {
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(a, a, b, len);
                    }
                }
                accGm.SetValue(0, a.GetValue(0));
                end = asc_get_system_cycle();
            } else if (args.mode == M_V2) {
                // V2: FP32 vector mul latency. Pure in-place chain.
                for (uint32_t w = 0; w < args.warmup; ++w) {
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Mul<float>(a, a, b, len);
                    }
                }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) {
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Mul<float>(a, a, b, len);
                    }
                }
                accGm.SetValue(0, a.GetValue(0));
                end = asc_get_system_cycle();
            } else if (args.mode == M_V3) {
                // V3: vector add throughput. Independent adds, no dep.
                // Each add writes to a different output to prevent merging.
                // Use 4 output buffers in rotation.
                TBuf tD0, tD1, tD2, tD3;
                pipe.InitBuffer(tD0, len*sizeof(float)); pipe.InitBuffer(tD1, len*sizeof(float));
                pipe.InitBuffer(tD2, len*sizeof(float)); pipe.InitBuffer(tD3, len*sizeof(float));
                auto d0=tD0.Get<float>(), d1=tD1.Get<float>(), d2=tD2.Get<float>(), d3=tD3.Get<float>();
                for (uint32_t w = 0; w < args.warmup; ++w)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(d0, a, b, len); Add<float>(d1, a, b, len);
                        Add<float>(d2, a, b, len); Add<float>(d3, a, b, len);
                    }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) {
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(d0, a, b, len);
                        Add<float>(d1, a, b, len);
                        Add<float>(d2, a, b, len);
                        Add<float>(d3, a, b, len);
                    }
                }
                accGm.SetValue(0, d0.GetValue(0));
                accGm.SetValue(1, d3.GetValue(0));
                end = asc_get_system_cycle();
            } else if (args.mode == M_V4) {
                // V4: pipeline depth. Issue N independent adds per step.
                // Sweep N=numStreams to find throughput saturation.
                TBuf tD0,tD1,tD2,tD3,tD4,tD5,tD6,tD7;
                pipe.InitBuffer(tD0, len*sizeof(float)); pipe.InitBuffer(tD1, len*sizeof(float));
                pipe.InitBuffer(tD2, len*sizeof(float)); pipe.InitBuffer(tD3, len*sizeof(float));
                pipe.InitBuffer(tD4, len*sizeof(float)); pipe.InitBuffer(tD5, len*sizeof(float));
                pipe.InitBuffer(tD6, len*sizeof(float)); pipe.InitBuffer(tD7, len*sizeof(float));
                auto d0=tD0.Get<float>(), d1=tD1.Get<float>(), d2=tD2.Get<float>(), d3=tD3.Get<float>();
                auto d4=tD4.Get<float>(), d5=tD5.Get<float>(), d6=tD6.Get<float>(), d7=tD7.Get<float>();
                uint32_t ns = (args.numStreams < 8) ? args.numStreams : 8;
                for (uint32_t w = 0; w < args.warmup; ++w)
                    for (uint32_t i = 0; i < args.chainLen; ++i)
                        Add<float>(d0,a,b,len);
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) {
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        if (ns > 0) Add<float>(d0,a,b,len);
                        if (ns > 1) Add<float>(d1,a,b,len);
                        if (ns > 2) Add<float>(d2,a,b,len);
                        if (ns > 3) Add<float>(d3,a,b,len);
                        if (ns > 4) Add<float>(d4,a,b,len);
                        if (ns > 5) Add<float>(d5,a,b,len);
                        if (ns > 6) Add<float>(d6,a,b,len);
                        if (ns > 7) Add<float>(d7,a,b,len);
                    }
                    DataCopy(accGm[it & 0x7], d0, len);
                }
                end = asc_get_system_cycle();
            } else { // M_V5: vector register read/write latency
                // UB-to-UB DataCopy chain as proxy for register access latency.
                for (uint32_t w = 0; w < args.warmup; ++w)
                    for (uint32_t i = 0; i < args.chainLen; ++i) { DataCopy(c,a,len); DataCopy(a,c,len); }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) {
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        DataCopy(c, a, len);
                        DataCopy(a, c, len);
                    }
                }
                accGm.SetValue(0, a.GetValue(0));
                end = asc_get_system_cycle();
            }
            total = end - start;
            break;
        }

        // ==================== CUBE ====================
        case M_C1:
        case M_C2:
        case M_C3:
        case M_C5: {
            // Cube matmul via Mmad. chainLen = number of matmul ops per iter.
            uint32_t M = args.m ? args.m : 16;
            uint32_t N = args.n ? args.n : 16;
            uint32_t K = args.k ? args.k : 16;
            TBuf<TPosition::VECCALC> tA1, tB1;
            pipe.InitBuffer(tA1, M*K*sizeof(half));
            pipe.InitBuffer(tB1, K*N*sizeof(half));
            auto aL1 = tA1.Get<half>();
            auto bL1 = tB1.Get<half>();
            // Load runtime-unknown values from GM.
            {
                GlobalTensor<half> seedGm;
                seedGm.SetGlobalBuffer((__gm__ half *)dataGM);
                DataCopy(aL1, seedGm, M*K);
                DataCopy(bL1, seedGm[M*K], K*N);
            }
            TBuf<TPosition::A2> tL0A;
            TBuf<TPosition::B2> tL0B;
            TBuf<TPosition::CO1> tL0C;
            pipe.InitBuffer(tL0A, M*K*sizeof(half));
            pipe.InitBuffer(tL0B, K*N*sizeof(half));
            pipe.InitBuffer(tL0C, M*N*sizeof(float));
            auto aL0 = tL0A.Get<half>();
            auto bL0 = tL0B.Get<half>();
            auto cL0 = tL0C.Get<float>();
            LoadData2DParams ldParams;
            ldParams.repeatTimes = 1;
            MmadParams mmParams((uint16_t)M,(uint16_t)N,(uint16_t)K,false,0,false,false,false);
            GlobalTensor<float> accGm; accGm.SetGlobalBuffer((__gm__ float *)outGM);
            // Load L0A/L0B once before timing.
            LoadData<half>(aL0,aL1,ldParams); LoadData<half>(bL0,bL1,ldParams);

            if (args.mode == M_C1) {
                // C1: single tile latency. Use PipeBarrier between Mmads to
                // prevent pipeline overlap, measuring true single-op latency.
                for (uint32_t w = 0; w < args.warmup; ++w) {
                    Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
                    PipeBarrier<PIPE_ALL>();
                }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) {
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
                        PipeBarrier<PIPE_ALL>();
                    }
                }
                accGm.SetValue(0, cL0.GetValue(0));
                end = asc_get_system_cycle();
            } else {
                // C2/C3/C5: no barrier, let pipeline overlap for throughput.
                for (uint32_t w = 0; w < args.warmup; ++w) {
                    for (uint32_t i = 0; i < args.chainLen; ++i)
                        Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
                }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) {
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
                    }
                }
                accGm.SetValue(0, cL0.GetValue(0));
                end = asc_get_system_cycle();
            }
            total = end - start;
            break;
        }
        case M_C4: {
            // L0A/L0B/L0C access latency proxy via small UB copy chain.
            uint32_t len = 16;
            TBuf tX, tY;
            pipe.InitBuffer(tX, len*sizeof(float));
            pipe.InitBuffer(tY, len*sizeof(float));
            auto x = tX.Get<float>();
            auto y = tY.Get<float>();
            // Load from GM for non-constant init.
            {
                GlobalTensor<float> seedGm;
                seedGm.SetGlobalBuffer((__gm__ float *)dataGM);
                DataCopy(x, seedGm, len);
            }
            GlobalTensor<float> accGm; accGm.SetGlobalBuffer((__gm__ float *)outGM);
            for (uint32_t w = 0; w < args.warmup; ++w)
                for (uint32_t i = 0; i < args.chainLen; ++i) { DataCopy(y,x,len); DataCopy(x,y,len); }
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it) {
                for (uint32_t i = 0; i < args.chainLen; ++i) {
                    DataCopy(y, x, len);
                    DataCopy(x, y, len);
                }
            }
            accGm.SetValue(0, x.GetValue(0));
            end = asc_get_system_cycle();
            total = end - start;
            break;
        }

        // ==================== MTE ====================
        case M_M1:
        case M_M2:
        case M_M3:
        case M_M4:
        case M_M5:
        case M_M8: {
            // MTE bandwidth: DataCopy GM<->UB.
            // chainLen = bytes per transfer. UB limited to 128KB.
            uint32_t bytes = args.chainLen;
            uint32_t maxUbBytes = 128 * 1024;
            if (bytes > maxUbBytes) bytes = maxUbBytes;
            uint32_t elems = bytes / sizeof(float);
            if (elems == 0) elems = 1;
            TBuf tBuf;
            pipe.InitBuffer(tBuf, elems * sizeof(float));
            auto ub = tBuf.Get<float>();
            GlobalTensor<float> gmF;
            gmF.SetGlobalBuffer((__gm__ float *)dataGM);
            GlobalTensor<float> accGm; accGm.SetGlobalBuffer((__gm__ float *)outGM);
            Duplicate<float>(ub, 1.0f, elems);

            if (args.mode == M_M1 || args.mode == M_M3 || args.mode == M_M4) {
                // Read: GM -> UB
                for (uint32_t w = 0; w < args.warmup; ++w) DataCopy(ub, gmF, elems);
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) DataCopy(ub, gmF, elems);
                accGm.SetValue(0, ub.GetValue(0));
                end = asc_get_system_cycle();
            } else {
                // Write: UB -> GM
                for (uint32_t w = 0; w < args.warmup; ++w) DataCopy(gmF, ub, elems);
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) DataCopy(gmF, ub, elems);
                accGm.SetValue(0, ub.GetValue(0));
                end = asc_get_system_cycle();
            }
            total = end - start;
            break;
        }
        case M_M7: {
            // Buffer capacity sweep: DataCopy latency vs data size.
            uint32_t bytes = args.chainLen;
            uint32_t maxUbBytes = 128 * 1024;
            if (bytes > maxUbBytes) bytes = maxUbBytes;
            uint32_t elems = bytes / sizeof(float);
            if (elems == 0) elems = 1;
            TBuf tBuf;
            pipe.InitBuffer(tBuf, elems * sizeof(float));
            auto ub = tBuf.Get<float>();
            GlobalTensor<float> gmF; gmF.SetGlobalBuffer((__gm__ float *)dataGM);
            GlobalTensor<float> accGm; accGm.SetGlobalBuffer((__gm__ float *)outGM);
            for (uint32_t w = 0; w < args.warmup; ++w) DataCopy(ub, gmF, elems);
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it) DataCopy(ub, gmF, elems);
            accGm.SetValue(0, ub.GetValue(0));
            end = asc_get_system_cycle();
            total = end - start;
            break;
        }
        default:
            total = 0;
            break;
    }

    out[0] = total;
    out[1] = (uint64_t)sink;
    argsPtr->outCycles = total;
    argsPtr->outCycles2 = (uint64_t)sink;
}
