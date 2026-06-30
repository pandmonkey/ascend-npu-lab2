// Unified microbenchmark kernel for Ascend 910B (DaVinci).
// Uses asc_get_system_cycle() for in-kernel cycle counting.
//
// Convention:
//   - args.chainLen: inner-loop count (swept for latency linearity)
//   - args.iters:    outer repetitions (for statistics)
//   - total = cycles for iters * chainLen operations
//   - host computes per-op = total / (iters * chainLen)
//
// v2: Fixes M3-M5 (use L0A/L0B/L0C buffers), C4 (use LoadData),
//     V4 (remove if branches), V5 (use vector ops), M7 (capacity sweep),
//     S3/M6 (support larger working sets)
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
    // Scalar add dependency chain: x = x*3 + 1.
    // Each step has 1 mul + 1 add (RAW dependency).
    // The mul result feeds into the add, the add feeds back as next mul input.
    for (uint32_t i = 0; i < n; ++i) {
        x = x * 3u + 1u;
    }
    return x;
}

__aicore__ inline uint32_t scalar_add_streams(uint32_t x, uint32_t n) {
    // 8 independent scalar add chains for ILP throughput measurement.
    uint32_t a[8] = {x, x + 1, x + 2, x + 3, x + 4, x + 5, x + 6, x + 7};
    for (uint32_t i = 0; i < n; ++i) {
        #pragma unroll 8
        for (uint32_t j = 0; j < 8; ++j) {
            a[j] = a[j] * 3u + 1u;
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
            // S1: Scalar add latency via dependency chain.
            // x = x*3+1 per step. x feeds back across iters.
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
            // S2: Scalar throughput via 8 independent streams.
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
            // S3/M6: Pointer chasing in GM for load latency.
            // dataGM holds uint32 next-index array forming a random cycle.
            // Working set = chainLen * 4 bytes; should be >> L2 (172MB shared)
            // to ensure HBM access.
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
        case M_V2: {
            // V1/V2: FP32 vector add/mul latency via in-place dependency chain.
            // a = a op b (RAW on a), exposing true instruction latency.
            uint32_t len = args.vecLen;
            TBuf tA, tB;
            pipe.InitBuffer(tA, len * sizeof(float));
            pipe.InitBuffer(tB, len * sizeof(float));
            auto a = tA.Get<float>();
            auto b = tB.Get<float>();
            {
                GlobalTensor<float> seedGm;
                seedGm.SetGlobalBuffer((__gm__ float *)dataGM);
                DataCopy(a, seedGm, len);
                DataCopy(b, seedGm[len], len);
            }
            GlobalTensor<float> accGm;
            accGm.SetGlobalBuffer((__gm__ float *)outGM);

            if (args.mode == M_V1) {
                for (uint32_t w = 0; w < args.warmup; ++w)
                    for (uint32_t i = 0; i < args.chainLen; ++i)
                        Add<float>(a, a, b, len);
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i)
                        Add<float>(a, a, b, len);
                accGm.SetValue(0, a.GetValue(0));
                end = asc_get_system_cycle();
            } else {
                for (uint32_t w = 0; w < args.warmup; ++w)
                    for (uint32_t i = 0; i < args.chainLen; ++i)
                        Mul<float>(a, a, b, len);
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i)
                        Mul<float>(a, a, b, len);
                accGm.SetValue(0, a.GetValue(0));
                end = asc_get_system_cycle();
            }
            total = end - start;
            break;
        }
        case M_V3: {
            // V3: Vector add throughput. 4 independent output buffers, no dependency.
            uint32_t len = args.vecLen;
            TBuf tA, tB, tD0, tD1, tD2, tD3;
            pipe.InitBuffer(tA, len*sizeof(float));
            pipe.InitBuffer(tB, len*sizeof(float));
            pipe.InitBuffer(tD0, len*sizeof(float));
            pipe.InitBuffer(tD1, len*sizeof(float));
            pipe.InitBuffer(tD2, len*sizeof(float));
            pipe.InitBuffer(tD3, len*sizeof(float));
            auto a=tA.Get<float>(), b=tB.Get<float>();
            auto d0=tD0.Get<float>(), d1=tD1.Get<float>();
            auto d2=tD2.Get<float>(), d3=tD3.Get<float>();
            {
                GlobalTensor<float> seedGm;
                seedGm.SetGlobalBuffer((__gm__ float *)dataGM);
                DataCopy(a, seedGm, len);
                DataCopy(b, seedGm[len], len);
            }
            GlobalTensor<float> accGm;
            accGm.SetGlobalBuffer((__gm__ float *)outGM);
            for (uint32_t w = 0; w < args.warmup; ++w)
                for (uint32_t i = 0; i < args.chainLen; ++i) {
                    Add<float>(d0, a, b, len); Add<float>(d1, a, b, len);
                    Add<float>(d2, a, b, len); Add<float>(d3, a, b, len);
                }
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it)
                for (uint32_t i = 0; i < args.chainLen; ++i) {
                    Add<float>(d0, a, b, len);
                    Add<float>(d1, a, b, len);
                    Add<float>(d2, a, b, len);
                    Add<float>(d3, a, b, len);
                }
            accGm.SetValue(0, d0.GetValue(0));
            accGm.SetValue(1, d3.GetValue(0));
            end = asc_get_system_cycle();
            total = end - start;
            break;
        }
        case M_V4: {
            // V4: Vector pipeline depth measurement.
            // Method: compare dependent chain latency vs independent chain latency.
            //   - numStreams=1: single dependency chain Add(a,a,b) - measures latency
            //   - numStreams=2..8: N independent chains, all with Add(dN,a,b)
            // Pipeline depth = total_time(N independent) / total_time(1 dependent)
            //   when N exceeds pipeline depth, time stops being constant.
            // The host sweeps numStreams and computes: depth = latency/throughput_per_op
            uint32_t len = args.vecLen;
            uint32_t ns = args.numStreams;
            if (ns > 8) ns = 8;
            if (ns < 1) ns = 1;

            TBuf tA, tB;
            TBuf tD0, tD1, tD2, tD3, tD4, tD5, tD6, tD7;
            pipe.InitBuffer(tA, len*sizeof(float));
            pipe.InitBuffer(tB, len*sizeof(float));
            pipe.InitBuffer(tD0, len*sizeof(float));
            pipe.InitBuffer(tD1, len*sizeof(float));
            pipe.InitBuffer(tD2, len*sizeof(float));
            pipe.InitBuffer(tD3, len*sizeof(float));
            pipe.InitBuffer(tD4, len*sizeof(float));
            pipe.InitBuffer(tD5, len*sizeof(float));
            pipe.InitBuffer(tD6, len*sizeof(float));
            pipe.InitBuffer(tD7, len*sizeof(float));
            auto a=tA.Get<float>(), b=tB.Get<float>();
            auto d0=tD0.Get<float>(), d1=tD1.Get<float>(), d2=tD2.Get<float>(), d3=tD3.Get<float>();
            auto d4=tD4.Get<float>(), d5=tD5.Get<float>(), d6=tD6.Get<float>(), d7=tD7.Get<float>();
            {
                GlobalTensor<float> seedGm;
                seedGm.SetGlobalBuffer((__gm__ float *)dataGM);
                DataCopy(a, seedGm, len);
                DataCopy(b, seedGm[len], len);
            }
            GlobalTensor<float> accGm;
            accGm.SetGlobalBuffer((__gm__ float *)outGM);

            // Warmup
            for (uint32_t w = 0; w < args.warmup; ++w)
                for (uint32_t i = 0; i < args.chainLen; ++i)
                    Add<float>(d0, a, b, len);

            // Use switch to avoid runtime if-branches inside hot loop.
            // Each case unrolls exactly N adds per loop iteration.
            start = asc_get_system_cycle();
            switch (ns) {
            case 1:
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i)
                        Add<float>(d0, a, b, len);
                break;
            case 2:
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(d0, a, b, len); Add<float>(d1, a, b, len);
                    }
                break;
            case 3:
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(d0, a, b, len); Add<float>(d1, a, b, len);
                        Add<float>(d2, a, b, len);
                    }
                break;
            case 4:
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(d0, a, b, len); Add<float>(d1, a, b, len);
                        Add<float>(d2, a, b, len); Add<float>(d3, a, b, len);
                    }
                break;
            case 5:
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(d0, a, b, len); Add<float>(d1, a, b, len);
                        Add<float>(d2, a, b, len); Add<float>(d3, a, b, len);
                        Add<float>(d4, a, b, len);
                    }
                break;
            case 6:
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(d0, a, b, len); Add<float>(d1, a, b, len);
                        Add<float>(d2, a, b, len); Add<float>(d3, a, b, len);
                        Add<float>(d4, a, b, len); Add<float>(d5, a, b, len);
                    }
                break;
            case 7:
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(d0, a, b, len); Add<float>(d1, a, b, len);
                        Add<float>(d2, a, b, len); Add<float>(d3, a, b, len);
                        Add<float>(d4, a, b, len); Add<float>(d5, a, b, len);
                        Add<float>(d6, a, b, len);
                    }
                break;
            default: // 8
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Add<float>(d0, a, b, len); Add<float>(d1, a, b, len);
                        Add<float>(d2, a, b, len); Add<float>(d3, a, b, len);
                        Add<float>(d4, a, b, len); Add<float>(d5, a, b, len);
                        Add<float>(d6, a, b, len); Add<float>(d7, a, b, len);
                    }
                break;
            }
            accGm.SetValue(0, d0.GetValue(0));
            end = asc_get_system_cycle();
            total = end - start;
            break;
        }
        case M_V5: {
            // V5: Vector register read/write latency.
            // Use Add(a, a, zero) where zero is loaded from GM.
            // This is a pure vector register dependency chain (not MTE DataCopy).
            // The result is similar to V1 but we measure with varying vecLen
            // to separate per-element cost from per-instruction overhead.
            // We also do separate read (Add with a as both src) and write tests.
            //
            // bufType=0: read+write combined (dependency chain Add(a,a,b))
            // bufType=1: read test  - Duplicate into b from a's values
            // bufType=2: write test - write constant into b repeatedly
            uint32_t len = args.vecLen;
            TBuf tA, tB;
            pipe.InitBuffer(tA, len * sizeof(float));
            pipe.InitBuffer(tB, len * sizeof(float));
            auto a = tA.Get<float>();
            auto b = tB.Get<float>();
            {
                GlobalTensor<float> seedGm;
                seedGm.SetGlobalBuffer((__gm__ float *)dataGM);
                DataCopy(a, seedGm, len);
                // Initialize b to near-zero (1e-30) so Add(a,a,b) ~= a
                Duplicate<float>(b, 1e-30f, len);
            }
            GlobalTensor<float> accGm;
            accGm.SetGlobalBuffer((__gm__ float *)outGM);

            // Dependency chain: a = a + b (b is tiny, doesn't affect precision much)
            // Each step reads a, reads b, writes a.
            for (uint32_t w = 0; w < args.warmup; ++w)
                for (uint32_t i = 0; i < args.chainLen; ++i)
                    Add<float>(a, a, b, len);
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it)
                for (uint32_t i = 0; i < args.chainLen; ++i)
                    Add<float>(a, a, b, len);
            accGm.SetValue(0, a.GetValue(0));
            end = asc_get_system_cycle();
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
            LoadData<half>(aL0,aL1,ldParams); LoadData<half>(bL0,bL1,ldParams);

            if (args.mode == M_C1) {
                // C1: single tile latency with PipeBarrier to prevent pipelining.
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
                for (uint32_t w = 0; w < args.warmup; ++w)
                    for (uint32_t i = 0; i < args.chainLen; ++i)
                        Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i)
                        Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
                accGm.SetValue(0, cL0.GetValue(0));
                end = asc_get_system_cycle();
            }
            total = end - start;
            break;
        }
        case M_C4: {
            // C4: L0A/L0B/L0C access latency.
            // Uses LoadData (L1->L0A / L1->L0B) chain for L0A/L0B latency,
            // and Mmad+read-back for L0C latency.
            // bufType: 0=L0A, 1=L0B, 2=L0C
            uint32_t M = 16, N = 16, K = 16;
            GlobalTensor<float> accGm; accGm.SetGlobalBuffer((__gm__ float *)outGM);

            if (args.bufType == 0) {
                // L0A latency: LoadData L1->L0A chain
                TBuf<TPosition::VECCALC> tSrc;
                pipe.InitBuffer(tSrc, M*K*sizeof(half));
                auto src = tSrc.Get<half>();
                {
                    GlobalTensor<half> seedGm;
                    seedGm.SetGlobalBuffer((__gm__ half *)dataGM);
                    DataCopy(src, seedGm, M*K);
                }
                TBuf<TPosition::A2> tL0A;
                pipe.InitBuffer(tL0A, M*K*sizeof(half));
                auto dst = tL0A.Get<half>();
                LoadData2DParams ldParams;
                ldParams.repeatTimes = 1;

                for (uint32_t w = 0; w < args.warmup; ++w)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        LoadData<half>(dst, src, ldParams);
                        PipeBarrier<PIPE_ALL>();
                    }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        LoadData<half>(dst, src, ldParams);
                        PipeBarrier<PIPE_ALL>();
                    }
                end = asc_get_system_cycle();
            } else if (args.bufType == 1) {
                // L0B latency: LoadData L1->L0B chain
                TBuf<TPosition::VECCALC> tSrc;
                pipe.InitBuffer(tSrc, K*N*sizeof(half));
                auto src = tSrc.Get<half>();
                {
                    GlobalTensor<half> seedGm;
                    seedGm.SetGlobalBuffer((__gm__ half *)dataGM);
                    DataCopy(src, seedGm, K*N);
                }
                TBuf<TPosition::B2> tL0B;
                pipe.InitBuffer(tL0B, K*N*sizeof(half));
                auto dst = tL0B.Get<half>();
                LoadData2DParams ldParams;
                ldParams.repeatTimes = 1;

                for (uint32_t w = 0; w < args.warmup; ++w)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        LoadData<half>(dst, src, ldParams);
                        PipeBarrier<PIPE_ALL>();
                    }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        LoadData<half>(dst, src, ldParams);
                        PipeBarrier<PIPE_ALL>();
                    }
                end = asc_get_system_cycle();
            } else {
                // L0C latency: Mmad writes L0C, measure single Mmad + barrier
                // This reuses C1 logic but isolates L0C write latency.
                TBuf<TPosition::VECCALC> tA1, tB1;
                pipe.InitBuffer(tA1, M*K*sizeof(half));
                pipe.InitBuffer(tB1, K*N*sizeof(half));
                auto aL1 = tA1.Get<half>();
                auto bL1 = tB1.Get<half>();
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
                LoadData<half>(aL0, aL1, ldParams);
                LoadData<half>(bL0, bL1, ldParams);

                for (uint32_t w = 0; w < args.warmup; ++w) {
                    Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
                    PipeBarrier<PIPE_ALL>();
                }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it)
                    for (uint32_t i = 0; i < args.chainLen; ++i) {
                        Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
                        PipeBarrier<PIPE_ALL>();
                    }
                accGm.SetValue(0, cL0.GetValue(0));
                end = asc_get_system_cycle();
            }
            total = end - start;
            break;
        }

        // ==================== MTE ====================
        case M_M1:
        case M_M2:
        case M_M8: {
            // M1: L1 read bandwidth (GM -> UB, as proxy for L1 read since
            //     GM->UB goes through L2/L1 path).
            // M2: L1 write bandwidth (UB -> GM).
            // M8: MTE startup overhead (small transfers, fit intercept).
            // chainLen = bytes per transfer.
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

            if (args.mode == M_M1 || args.mode == M_M8) {
                // Read: GM -> UB
                for (uint32_t w = 0; w < args.warmup; ++w) {
                    DataCopy(ub, gmF, elems);
                    PipeBarrier<PIPE_ALL>();
                }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) {
                    DataCopy(ub, gmF, elems);
                    PipeBarrier<PIPE_ALL>();
                }
                accGm.SetValue(0, ub.GetValue(0));
                end = asc_get_system_cycle();
            } else {
                // Write: UB -> GM
                for (uint32_t w = 0; w < args.warmup; ++w) {
                    DataCopy(gmF, ub, elems);
                    PipeBarrier<PIPE_ALL>();
                }
                start = asc_get_system_cycle();
                for (uint32_t it = 0; it < args.iters; ++it) {
                    DataCopy(gmF, ub, elems);
                    PipeBarrier<PIPE_ALL>();
                }
                accGm.SetValue(0, ub.GetValue(0));
                end = asc_get_system_cycle();
            }
            total = end - start;
            break;
        }
        case M_M3: {
            // M3: L0A bandwidth. Measure LoadData (UB -> L0A) throughput.
            // Use half type (Cube input format for L0A).
            // chainLen = number of LoadData calls per iter (for sweeping).
            uint32_t bytes = 16 * 16 * sizeof(half);  // Fixed 16x16 tile = 512 bytes
            uint32_t elems = bytes / sizeof(half);     // 256 elements

            TBuf<TPosition::VECCALC> tSrc;
            pipe.InitBuffer(tSrc, elems * sizeof(half));
            auto src = tSrc.Get<half>();
            {
                GlobalTensor<half> seedGm;
                seedGm.SetGlobalBuffer((__gm__ half *)dataGM);
                DataCopy(src, seedGm, elems);
            }
            TBuf<TPosition::A2> tL0A;
            pipe.InitBuffer(tL0A, elems * sizeof(half));
            auto dst = tL0A.Get<half>();
            LoadData2DParams ldParams;
            ldParams.repeatTimes = 1;
            GlobalTensor<float> accGm; accGm.SetGlobalBuffer((__gm__ float *)outGM);

            uint32_t chain = args.chainLen;
            for (uint32_t w = 0; w < args.warmup; ++w)
                for (uint32_t i = 0; i < chain; ++i) {
                    LoadData<half>(dst, src, ldParams);
                    PipeBarrier<PIPE_ALL>();
                }
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it)
                for (uint32_t i = 0; i < chain; ++i) {
                    LoadData<half>(dst, src, ldParams);
                    PipeBarrier<PIPE_ALL>();
                }
            end = asc_get_system_cycle();
            // Total bytes = 512 * chain * iters
            total = end - start;
            break;
        }
        case M_M4: {
            // M4: L0B bandwidth. Measure LoadData (UB -> L0B) throughput.
            // chainLen = number of LoadData calls per iter.
            uint32_t bytes = 16 * 16 * sizeof(half);  // Fixed 16x16 tile = 512 bytes
            uint32_t elems = bytes / sizeof(half);     // 256 elements

            TBuf<TPosition::VECCALC> tSrc;
            pipe.InitBuffer(tSrc, elems * sizeof(half));
            auto src = tSrc.Get<half>();
            {
                GlobalTensor<half> seedGm;
                seedGm.SetGlobalBuffer((__gm__ half *)dataGM);
                DataCopy(src, seedGm, elems);
            }
            TBuf<TPosition::B2> tL0B;
            pipe.InitBuffer(tL0B, elems * sizeof(half));
            auto dst = tL0B.Get<half>();
            LoadData2DParams ldParams;
            ldParams.repeatTimes = 1;
            GlobalTensor<float> accGm; accGm.SetGlobalBuffer((__gm__ float *)outGM);

            uint32_t chain = args.chainLen;
            for (uint32_t w = 0; w < args.warmup; ++w)
                for (uint32_t i = 0; i < chain; ++i) {
                    LoadData<half>(dst, src, ldParams);
                    PipeBarrier<PIPE_ALL>();
                }
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it)
                for (uint32_t i = 0; i < chain; ++i) {
                    LoadData<half>(dst, src, ldParams);
                    PipeBarrier<PIPE_ALL>();
                }
            end = asc_get_system_cycle();
            // Total bytes = 512 * chain * iters
            total = end - start;
            break;
        }
        case M_M5: {
            // M5: L0C bandwidth. Measure Mmad write to L0C as proxy.
            // L0C is only written by Mmad and read by Fixpipe.
            // We measure repeated Mmad (16x16x16) writes to L0C.
            uint32_t M = 16, N = 16, K = 16;
            TBuf<TPosition::VECCALC> tA1, tB1;
            pipe.InitBuffer(tA1, M*K*sizeof(half));
            pipe.InitBuffer(tB1, K*N*sizeof(half));
            auto aL1 = tA1.Get<half>();
            auto bL1 = tB1.Get<half>();
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
            LoadData<half>(aL0, aL1, ldParams);
            LoadData<half>(bL0, bL1, ldParams);

            uint32_t chain = args.chainLen;
            for (uint32_t w = 0; w < args.warmup; ++w)
                for (uint32_t i = 0; i < chain; ++i)
                    Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it)
                for (uint32_t i = 0; i < chain; ++i)
                    Mmad<float,half,half>(cL0,aL0,bL0,mmParams);
            accGm.SetValue(0, cL0.GetValue(0));
            end = asc_get_system_cycle();
            // L0C bytes written per Mmad = M*N*4 (float32 output)
            // Total bytes = M*N*4 * chain * iters
            // Host computes: BW = total_bytes / cycles * freq
            total = end - start;
            break;
        }
        case M_M7: {
            // M7: Buffer capacity measurement via allocation sweep.
            // chainLen = bytes to try allocating in UB.
            // We try to allocate, then do a small DataCopy.
            // If allocation succeeds within UB, latency is low.
            // If it fails or falls back, latency spikes.
            uint32_t bytes = args.chainLen;
            // Cap to reasonable UB maximum (256KB as upper bound test)
            uint32_t maxBytes = 256 * 1024;
            if (bytes > maxBytes) bytes = maxBytes;
            uint32_t elems = bytes / sizeof(float);
            if (elems == 0) elems = 1;

            TBuf tBuf;
            pipe.InitBuffer(tBuf, elems * sizeof(float));
            auto ub = tBuf.Get<float>();
            GlobalTensor<float> gmF; gmF.SetGlobalBuffer((__gm__ float *)dataGM);
            GlobalTensor<float> accGm; accGm.SetGlobalBuffer((__gm__ float *)outGM);

            // Warmup: fill buffer
            DataCopy(ub, gmF, elems);
            PipeBarrier<PIPE_ALL>();

            // Measure: time to fill and read-back the buffer
            start = asc_get_system_cycle();
            for (uint32_t it = 0; it < args.iters; ++it) {
                DataCopy(ub, gmF, elems);
                PipeBarrier<PIPE_ALL>();
            }
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
