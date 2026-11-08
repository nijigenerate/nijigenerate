#pragma once
#include "context.hpp"
#include <array>
#include <span>
namespace nijigenerate::backend::metal {
enum class ComputeKernel { DepthBone, DepthSample };
class ComputeJob {
    struct Impl;
    std::unique_ptr<Impl> p;
public:
    ComputeJob(ComputeKernel,const std::array<std::span<const std::byte>,5>&,uint32_t);
    ~ComputeJob();
    ComputeJob(const ComputeJob&)=delete;
    ComputeJob& operator=(const ComputeJob&)=delete;
    bool poll(std::vector<float>&,uint64_t timeout=0);
    void release();
};
std::shared_ptr<ComputeJob> ngSubmitCompute(ComputeKernel,const std::array<std::span<const std::byte>,5>&,uint32_t);
void ngShutdownComputeJobs();
}
