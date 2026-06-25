// Query platform info for buffer sizes, bandwidth, core count.
#include <cstdio>
#include "acl/acl.h"
#include "tiling/platform/platform_ascendc.h"

int main() {
    aclInit(nullptr);
    aclrtSetDevice(0);

    auto *plat = platform_ascendc::PlatformAscendCManager::GetInstance();
    if (!plat) {
        printf("Platform info not available\n");
        aclrtResetDevice(0);
        aclFinalize();
        return 1;
    }

    printf("=== Platform Info ===\n");
    printf("SocVersion: %d\n", (int)plat->GetSocVersion());
    printf("CoreNumAic: %u\n", plat->GetCoreNumAic());
    printf("CoreNumAiv: %u\n", plat->GetCoreNumAiv());
    printf("CoreNum: %u\n", plat->GetCoreNum());
    printf("VecRegLen: %u\n", plat->GetVecRegLen());

    const char* memNames[] = {"L0A", "L0B", "L0C", "L1", "L2", "UB", "HBM", "FB", "BT"};
    platform_ascendc::CoreMemType memTypes[] = {
        platform_ascendc::CoreMemType::L0_A, platform_ascendc::CoreMemType::L0_B,
        platform_ascendc::CoreMemType::L0_C, platform_ascendc::CoreMemType::L1,
        platform_ascendc::CoreMemType::L2, platform_ascendc::CoreMemType::UB,
        platform_ascendc::CoreMemType::HBM, platform_ascendc::CoreMemType::FB,
        platform_ascendc::CoreMemType::BT
    };

    printf("\n=== Memory Sizes & Bandwidth ===\n");
    for (int i = 0; i < 9; i++) {
        uint64_t size = 0, bw = 0;
        plat->GetCoreMemSize(memTypes[i], size);
        plat->GetCoreMemBw(memTypes[i], bw);
        printf("%-4s: size=%lu bytes (%.1f KB), bw=%lu bytes/cycle (%.1f GB/s @1.25GHz)\n",
               memNames[i], size, size / 1024.0, bw, bw * 1.25);
    }

    aclrtResetDevice(0);
    aclFinalize();
    return 0;
}
