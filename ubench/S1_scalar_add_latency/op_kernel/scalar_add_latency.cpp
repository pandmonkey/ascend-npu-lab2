#include "kernel_operator.h"

#include "scalar_add_latency_tiling.h"
#include "tiling_key_scalar_add_latency.h"

using namespace AscendC;

constexpr int32_t BUFFER_NUM = 2;
constexpr int32_t TILE_LENGTH = 1024;

template <class DT_X>
class KernelScalarAddLatency {
public:
    __aicore__ inline KernelScalarAddLatency() {}

    __aicore__ inline void Init(GM_ADDR x, GM_ADDR y, uint32_t length, uint32_t iterations) {
        uint32_t blockNum = GetBlockNum();
        uint32_t blockIdx = GetBlockIdx();
        uint32_t coreLength = length / blockNum;
        uint32_t coreRemain = length % blockNum;

        uint32_t startOffset;
        if (blockIdx < coreRemain) {
            coreLength += 1;
            startOffset = blockIdx * coreLength;
        } else {
            startOffset = blockIdx * coreLength + coreRemain;
        }

        this->tileNum = coreLength / TILE_LENGTH;
        this->tailLength = coreLength - this->tileNum * TILE_LENGTH;
        this->iterations = iterations;

        xGm.SetGlobalBuffer((__gm__ DT_X *)x + startOffset, coreLength);
        yGm.SetGlobalBuffer((__gm__ DT_X *)y + startOffset, coreLength);

        uint32_t tileBytes = TILE_LENGTH * sizeof(DT_X);
        pipe.InitBuffer(inQueueX, BUFFER_NUM, tileBytes);
        pipe.InitBuffer(outQueueY, BUFFER_NUM, tileBytes);
    }

    __aicore__ inline void Process() {
        for (int32_t i = 0; i < tileNum; ++i) {
            CopyIn(i, TILE_LENGTH);
            Compute(TILE_LENGTH);
            CopyOut(i, TILE_LENGTH);
        }
        if (tailLength > 0) {
            CopyIn(tileNum, tailLength);
            Compute(tailLength);
            CopyOut(tileNum, tailLength);
        }
    }

private:
    __aicore__ inline void CopyIn(int32_t progress, uint32_t len) {
        LocalTensor<DT_X> xLocal = inQueueX.AllocTensor<DT_X>();
        DataCopyExtParams copyParams;
        copyParams.blockCount = 1;
        copyParams.blockLen = len * sizeof(DT_X);
        copyParams.srcStride = 0;
        copyParams.dstStride = 0;
        copyParams.rsv = 0;
        DataCopyPadExtParams<DT_X> padParams;
        padParams.isPad = false;
        padParams.leftPadding = 0;
        padParams.rightPadding = 0;
        padParams.paddingValue = 0;
        DataCopyPad(xLocal, xGm[progress * TILE_LENGTH], copyParams, padParams);
        inQueueX.EnQue(xLocal);
    }

    __aicore__ inline void Compute(uint32_t len) {
        LocalTensor<DT_X> xLocal = inQueueX.DeQue<DT_X>();
        LocalTensor<DT_X> yLocal = outQueueY.AllocTensor<DT_X>();
        DT_X one = static_cast<DT_X>(1.0f);
        Adds(yLocal, xLocal, one, len);
        outQueueY.EnQue(yLocal);
        inQueueX.FreeTensor(xLocal);
    }

    __aicore__ inline void CopyOut(int32_t progress, uint32_t len) {
        LocalTensor<DT_X> yLocal = outQueueY.DeQue<DT_X>();
        DataCopyExtParams copyParams;
        copyParams.blockCount = 1;
        copyParams.blockLen = len * sizeof(DT_X);
        copyParams.srcStride = 0;
        copyParams.dstStride = 0;
        copyParams.rsv = 0;
        DataCopyPad(yGm[progress * TILE_LENGTH], yLocal, copyParams);
        outQueueY.FreeTensor(yLocal);
    }

private:
    TPipe pipe;
    TQue<QuePosition::VECIN, BUFFER_NUM> inQueueX;
    TQue<QuePosition::VECOUT, BUFFER_NUM> outQueueY;
    GlobalTensor<DT_X> xGm;
    GlobalTensor<DT_X> yGm;
    int32_t tileNum;
    uint32_t tailLength;
    uint32_t iterations;
};

template <typename DT_X>
__global__ __aicore__ void scalar_add_latency(GM_ADDR x, GM_ADDR y, GM_ADDR workspace, GM_ADDR tiling) {
    REGISTER_TILING_DEFAULT(ScalarAddLatencyTilingData);
    GET_TILING_DATA_WITH_STRUCT(ScalarAddLatencyTilingData, tilingData, tiling);
    KernelScalarAddLatency<DT_X> op;
    op.Init(x, y, tilingData.length, tilingData.iterations);
    op.Process();
}

