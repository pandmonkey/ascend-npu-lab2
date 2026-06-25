#pragma once

#include <cstdint>

// Parameters passed from host to kernel via a single GM workspace buffer.
// Layout matches host struct in host/ubench_host.cpp.
struct UbenchArgs {
    uint32_t mode;        // benchmark mode (see enum in kernel)
    uint32_t chainLen;    // dependency chain length / iteration count
    uint32_t vecLen;      // elements per vector register chunk (d8 count * 8)
    uint32_t numStreams;  // independent streams for throughput
    uint32_t m;           // matmul M
    uint32_t n;           // matmul N
    uint32_t k;           // matmul K
    uint32_t bufType;     // 0=L0A 1=L0B 2=L0C 3=L1 for C4/M3-M5
    uint32_t warmup;      // warmup iterations (not counted)
    uint32_t iters;       // measured iterations
    uint64_t outCycles;   // output: total cycles for measured iters (one core)
    uint64_t outCycles2;  // output: secondary value (e.g. per-op)
    uint64_t reserved;
};
