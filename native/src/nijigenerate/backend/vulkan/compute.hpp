#pragma once
#include "context.hpp"
#include <array>
#include <span>
namespace nijigenerate::backend::vulkan {
enum class ComputeKernel { DepthBone, DepthSample };
class ComputeJob {
    Context* owner_{};
    std::array<Buffer,6> buffers_{};
    VkShaderModule shader_{};
    VkDescriptorSetLayout sets_{};
    VkDescriptorPool pool_{};
    VkPipelineLayout layout_{};
    VkPipeline pipeline_{};
    VkCommandPool commands_{};
    VkFence fence_{};
    uint32_t count_{};
    bool submitted_ = false;
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
