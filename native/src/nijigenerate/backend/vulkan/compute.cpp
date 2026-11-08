#include "compute.hpp"
#include "vulkan_shaders.hpp"
#include <cstring>
#include <stdexcept>
namespace nijigenerate::backend::vulkan {
namespace { std::vector<std::weak_ptr<ComputeJob>> jobs; }
ComputeJob::ComputeJob(ComputeKernel kernel,const std::array<std::span<const std::byte>,5>& input,uint32_t count)
    : owner_(&context()),count_(count) {
    if(!count) throw std::invalid_argument("Compute dispatch has no elements");
    auto& c=*owner_;
    try {
        VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(c.physical,&properties);
        if((count+63)/64>properties.limits.maxComputeWorkGroupCount[0]) throw std::length_error("Compute dispatch exceeds device limits");
        std::array<VkDescriptorSetLayoutBinding,6> bindings{};
        for(uint32_t i=0;i<6;++i) {
            auto bytes=i==5?size_t(count)*2*sizeof(float):std::max<size_t>(4,input[i].size());
            if(bytes>properties.limits.maxStorageBufferRange) throw std::length_error("Compute buffer exceeds device limits");
            c.ensureBuffer(buffers_[i],bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            std::memset(buffers_[i].mapped,0,bytes);
            if(i<5 && !input[i].empty()) std::memcpy(buffers_[i].mapped,input[i].data(),input[i].size());
            bindings[i]={i,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
        }
        VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setInfo.bindingCount=6; setInfo.pBindings=bindings.data(); check(vkCreateDescriptorSetLayout(c.device,&setInfo,nullptr,&sets_));
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,6};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; pool.maxSets=1; pool.poolSizeCount=1; pool.pPoolSizes=&size;
        check(vkCreateDescriptorPool(c.device,&pool,nullptr,&pool_));
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; allocate.descriptorPool=pool_; allocate.descriptorSetCount=1; allocate.pSetLayouts=&sets_;
        VkDescriptorSet set{}; check(vkAllocateDescriptorSets(c.device,&allocate,&set));
        std::array<VkDescriptorBufferInfo,6> infos{}; std::array<VkWriteDescriptorSet,6> writes{};
        for(uint32_t i=0;i<6;++i) {
            infos[i]={buffers_[i].handle,0,i==5?size_t(count)*2*sizeof(float):std::max<size_t>(4,input[i].size())};
            writes[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; writes[i].dstSet=set; writes[i].dstBinding=i;
            writes[i].descriptorCount=1; writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[i].pBufferInfo=&infos[i];
        }
        vkUpdateDescriptorSets(c.device,6,writes.data(),0,nullptr);
        VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        if(kernel==ComputeKernel::DepthBone) { shader.codeSize=sizeof(depth_bone_comp); shader.pCode=depth_bone_comp; }
        else { shader.codeSize=sizeof(depth_sample_comp); shader.pCode=depth_sample_comp; }
        check(vkCreateShaderModule(c.device,&shader,nullptr,&shader_));
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,4};
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; layout.setLayoutCount=1; layout.pSetLayouts=&sets_;
        layout.pushConstantRangeCount=1; layout.pPushConstantRanges=&range; check(vkCreatePipelineLayout(c.device,&layout,nullptr,&layout_));
        VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO}; pipeline.layout=layout_;
        pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; pipeline.stage.module=shader_; pipeline.stage.pName="main";
        check(vkCreateComputePipelines(c.device,VK_NULL_HANDLE,1,&pipeline,nullptr,&pipeline_));
        VkCommandPoolCreateInfo commandPool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; commandPool.queueFamilyIndex=c.family; commandPool.flags=VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        check(vkCreateCommandPool(c.device,&commandPool,nullptr,&commands_));
        VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; commandInfo.commandPool=commands_; commandInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; commandInfo.commandBufferCount=1;
        VkCommandBuffer command{}; check(vkAllocateCommandBuffers(c.device,&commandInfo,&command));
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; check(vkBeginCommandBuffer(command,&begin));
        vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_);
        vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout_,0,1,&set,0,nullptr);
        vkCmdPushConstants(command,layout_,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&count);
        vkCmdDispatch(command,(count+63)/64,1,1);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        check(vkEndCommandBuffer(command));
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; check(vkCreateFence(c.device,&fence,nullptr,&fence_));
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&command;
        check(vkQueueSubmit(c.queue,1,&submit,fence_));
        submitted_=true;
    } catch(...) { release(); throw; }
}
ComputeJob::~ComputeJob() { release(); }
void ComputeJob::release() {
    if(!owner_) return; auto& c=*owner_;
    if(submitted_) vkWaitForFences(c.device,1,&fence_,VK_TRUE,UINT64_MAX);
    if(fence_) vkDestroyFence(c.device,fence_,nullptr);
    if(commands_) vkDestroyCommandPool(c.device,commands_,nullptr);
    if(pipeline_) vkDestroyPipeline(c.device,pipeline_,nullptr);
    if(layout_) vkDestroyPipelineLayout(c.device,layout_,nullptr);
    if(shader_) vkDestroyShaderModule(c.device,shader_,nullptr);
    if(pool_) vkDestroyDescriptorPool(c.device,pool_,nullptr);
    if(sets_) vkDestroyDescriptorSetLayout(c.device,sets_,nullptr);
    for(auto& b:buffers_) c.destroy(b);
    owner_=nullptr;
}
bool ComputeJob::poll(std::vector<float>& values,uint64_t timeout) {
    if(!owner_) throw std::logic_error("Compute job has been released");
    auto result=vkWaitForFences(owner_->device,1,&fence_,VK_TRUE,timeout);
    if(result==VK_TIMEOUT) return false; check(result);
    values.resize(size_t(count_)*2); std::memcpy(values.data(),buffers_[5].mapped,values.size()*sizeof(float)); return true;
}
std::shared_ptr<ComputeJob> ngSubmitCompute(ComputeKernel kernel,const std::array<std::span<const std::byte>,5>& inputs,uint32_t count) {
    auto job=std::make_shared<ComputeJob>(kernel,inputs,count);
    std::erase_if(jobs,[](const auto& weak) { return weak.expired(); }); jobs.push_back(job); return job;
}
void ngShutdownComputeJobs() { for(auto weak:jobs) if(auto job=weak.lock()) job->release(); jobs.clear(); }
}
