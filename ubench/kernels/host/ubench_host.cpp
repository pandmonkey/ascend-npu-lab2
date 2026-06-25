// Host launcher for the unified ubench kernel.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "acl/acl.h"

struct UbenchArgs {
    uint32_t mode;
    uint32_t chainLen;
    uint32_t vecLen;
    uint32_t numStreams;
    uint32_t m;
    uint32_t n;
    uint32_t k;
    uint32_t bufType;
    uint32_t warmup;
    uint32_t iters;
    uint64_t outCycles;
    uint64_t outCycles2;
    uint64_t reserved;
};

extern "C" uint32_t aclrtlaunch_ubench(uint32_t numBlocks, void* stream,
                                       void* argsGM, void* dataGM, void* outGM);

static const int MODE_S1=1, MODE_S2=2, MODE_S3=3, MODE_V1=4, MODE_V2=5, MODE_V3=6,
    MODE_V4=7, MODE_V5=8, MODE_C1=9, MODE_C2=10, MODE_C3=11, MODE_C4=12, MODE_C5=13,
    MODE_M1=14, MODE_M2=15, MODE_M3=16, MODE_M4=17, MODE_M5=18, MODE_M6=19,
    MODE_M7=20, MODE_M8=21;

struct RunCfg {
    int mode;
    uint32_t chainLen=1000;
    uint32_t vecLen=64;
    uint32_t numStreams=1;
    uint32_t m=0,n=0,k=0;
    uint32_t bufType=0;
    uint32_t warmup=5;
    uint32_t iters=20;
    uint32_t dataBytes=1<<20;  // GM data buffer size
};

// Run one measurement; returns cycles for the measured iters.
static uint64_t run_once(const RunCfg &c, aclrtStream stream,
                         void* argsGM, void* dataGM, void* outGM) {
    UbenchArgs a{};
    a.mode=c.mode; a.chainLen=c.chainLen; a.vecLen=c.vecLen;
    a.numStreams=c.numStreams; a.m=c.m; a.n=c.n; a.k=c.k;
    a.bufType=c.bufType; a.warmup=c.warmup; a.iters=c.iters;
    aclrtMemcpy(argsGM, sizeof(a), &a, sizeof(a), ACL_MEMCPY_HOST_TO_DEVICE);
    aclrtSynchronizeStream(stream);
    aclrtlaunch_ubench(1, stream, argsGM, dataGM, outGM);
    aclrtSynchronizeStream(stream);
    uint64_t out[4]={0,0,0,0};
    aclrtMemcpy(out, sizeof(out), outGM, sizeof(out), ACL_MEMCPY_DEVICE_TO_HOST);
    return out[0];
}

int main(int argc, char** argv) {
    RunCfg c;
    std::string label;
    int deviceId=0;
    // Args: --mode <n> --chain <n> --veclen <n> --streams <n> --m <n> --n <n> --k <n>
    //       --buf <n> --warmup <n> --iters <n> --data <bytes> --label <s> --repeat <n> --device <n>
    int repeat=10;
    for (int i=1;i<argc;i++){
        std::string a=argv[i];
        auto next=[&]()->std::string{return (i+1<argc)?argv[++i]:"";};
        if(a=="--mode")c.mode=std::stoi(next());
        else if(a=="--chain")c.chainLen=std::stoul(next());
        else if(a=="--veclen")c.vecLen=std::stoul(next());
        else if(a=="--streams")c.numStreams=std::stoul(next());
        else if(a=="--m")c.m=std::stoul(next());
        else if(a=="--n")c.n=std::stoul(next());
        else if(a=="--k")c.k=std::stoul(next());
        else if(a=="--buf")c.bufType=std::stoul(next());
        else if(a=="--warmup")c.warmup=std::stoul(next());
        else if(a=="--iters")c.iters=std::stoul(next());
        else if(a=="--data")c.dataBytes=std::stoul(next());
        else if(a=="--label")label=next();
        else if(a=="--repeat")repeat=std::stoi(next());
        else if(a=="--device")deviceId=std::stoi(next());
    }
    if(c.mode==0){fprintf(stderr,"usage: %s --mode <1..21> ...\n",argv[0]);return 1;}

    aclInit(nullptr);
    aclrtSetDevice(deviceId);
    aclrtStream stream; aclrtCreateStream(&stream);

    void *argsGM=nullptr,*dataGM=nullptr,*outGM=nullptr;
    aclrtMalloc(&argsGM, 4096, ACL_MEM_MALLOC_HUGE_FIRST);
    size_t allocBytes = c.dataBytes;
    if(c.mode==MODE_S3||c.mode==MODE_M6){
        // pointer-chasing array of uint32 indices, sized to chainLen + some.
        allocBytes = std::max((size_t)allocBytes, (size_t)(c.chainLen*4+4096));
    }
    aclrtMalloc(&dataGM, allocBytes, ACL_MEM_MALLOC_HUGE_FIRST);
    aclrtMalloc(&outGM, 4096, ACL_MEM_MALLOC_HUGE_FIRST);

    // For S3/M6 pointer chasing, build a permutation chain in dataGM.
    if(c.mode==MODE_S3||c.mode==MODE_M6){
        uint32_t n = c.chainLen;
        std::vector<uint32_t> idx(n);
        for(uint32_t i=0;i<n;i++)idx[i]=i;
        // shuffle deterministically to defeat prefetch
        unsigned int seed=12345;
        for(uint32_t i=n-1;i>0;i--){uint32_t r=rand_r(&seed)%(i+1);std::swap(idx[i],idx[r]);}
        // make it a single cycle
        std::vector<uint32_t> pos(n);
        for(uint32_t i=0;i<n;i++)pos[idx[i]]=i;
        std::vector<uint32_t> chain(n);
        uint32_t cur=idx[0];
        for(uint32_t i=0;i<n;i++){uint32_t nx=idx[(pos[cur]+1)%n];chain[cur]=nx;cur=idx[(pos[cur]+1)%n];}
        aclrtMemcpy(dataGM, n*sizeof(uint32_t), chain.data(), n*sizeof(uint32_t), ACL_MEMCPY_HOST_TO_DEVICE);
    }

    // Warmup run
    run_once(c, stream, argsGM, dataGM, outGM);

    std::vector<uint64_t> cyc;
    for(int r=0;r<repeat;r++){
        uint64_t v=run_once(c, stream, argsGM, dataGM, outGM);
        cyc.push_back(v);
    }
    std::sort(cyc.begin(),cyc.end());
    uint64_t med = cyc[cyc.size()/2];
    double mean=0;for(auto v:cyc)mean+=v;mean/=cyc.size();
    double var=0;for(auto v:cyc)var+=(v-mean)*(v-mean);var/=cyc.size();
    double sd=std::sqrt(var);

    // Output JSON line
    printf("{\"label\":\"%s\",\"mode\":%d,\"chain\":%u,\"veclen\":%u,\"streams\":%u,"
           "\"m\":%u,\"n\":%u,\"k\":%u,\"buf\":%u,\"iters\":%u,\"repeat\":%d,"
           "\"cycles_median\":%llu,\"cycles_mean\":%.1f,\"cycles_sd\":%.1f,"
           "\"cycles_min\":%llu,\"cycles_max\":%llu}\n",
           label.c_str(),c.mode,c.chainLen,c.vecLen,c.numStreams,c.m,c.n,c.k,c.bufType,
           c.iters,repeat,(unsigned long long)med,mean,sd,
           (unsigned long long)cyc.front(),(unsigned long long)cyc.back());

    aclrtFree(argsGM);
    aclrtFree(dataGM);
    aclrtFree(outGM);
    aclrtDestroyStream(stream);
    aclrtResetDevice(deviceId);
    aclFinalize();
    return 0;
}
