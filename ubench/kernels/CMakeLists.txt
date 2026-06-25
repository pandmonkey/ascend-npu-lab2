cmake_minimum_required(VERSION 3.16.0)
project(ubench_kernels)

set(SOC_VERSION "ascend910b2" CACHE STRING "Ascend SoC version")
set(ASCEND_CANN_PACKAGE_PATH "/usr/local/Ascend/cann-9.0.0" CACHE STRING "CANN path")
set(RUN_MODE "npu")
set(ASCEND_KERNEL_LAUNCH_ONLY ON)

list(APPEND CMAKE_MODULE_PATH
    ${ASCEND_CANN_PACKAGE_PATH}/aarch64-linux/tikcpp/ascendc_kernel_cmake)
include(ascendc)

ascendc_library(ubench_kernels STATIC
    op_kernel/ubench.cpp
)
