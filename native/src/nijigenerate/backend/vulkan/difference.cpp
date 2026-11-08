// Copyright (c) 2020 Inochi2D Project; (c) 2024 nijigenerate Project. BSD-2-Clause.
#include "difference.hpp"
#include "vulkan_shaders.hpp"
#include <cstring>
#include <algorithm>
namespace nijigenerate::backend::vulkan {
DifferenceReduction ngReduceDifference(Image& source,nicxlive::core::DifferenceEvaluationRegion region) {
    auto& c=context(); DifferenceReduction result;
    result.sourceWidth=region.width; result.sourceHeight=region.height;
    std::array<Buffer,2> buffers{};
    VkShaderModule shader{}; VkDescriptorSetLayout setLayout{}; VkDescriptorPool pool{};
    VkPipelineLayout layout{}; VkPipeline pipeline{};
    auto release=[&] {
        if(pipeline) vkDestroyPipeline(c.device,pipeline,nullptr);
        if(layout) vkDestroyPipelineLayout(c.device,layout,nullptr);
        if(shader) vkDestroyShaderModule(c.device,shader,nullptr);
        if(pool) vkDestroyDescriptorPool(c.device,pool,nullptr);
        if(setLayout) vkDestroyDescriptorSetLayout(c.device,setLayout,nullptr);
        for(auto& b:buffers) c.destroy(b);
    };
    try {
        auto size=size_t(region.width)*region.height*4*sizeof(float);
        for(auto& b:buffers) c.ensureBuffer(b,size,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        VkDescriptorSetLayoutBinding bindings[]{
            {0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
        VkDescriptorSetLayoutCreateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; set.bindingCount=3; set.pBindings=bindings;
        check(vkCreateDescriptorSetLayout(c.device,&set,nullptr,&setLayout));
        VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO}; poolInfo.maxSets=2; poolInfo.poolSizeCount=2; poolInfo.pPoolSizes=sizes;
        check(vkCreateDescriptorPool(c.device,&poolInfo,nullptr,&pool));
        VkDescriptorSetLayout layouts[]{setLayout,setLayout}; VkDescriptorSet sets[2]{};
        VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO}; alloc.descriptorPool=pool; alloc.descriptorSetCount=2; alloc.pSetLayouts=layouts;
        check(vkAllocateDescriptorSets(c.device,&alloc,sets));
        VkDescriptorImageInfo image{c.sampler,source.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        for(uint32_t i=0;i<2;++i) {
            VkDescriptorBufferInfo bufferInfos[]{{buffers[1-i].handle,0,size},{buffers[i].handle,0,size}};
            VkWriteDescriptorSet writes[3]{};
            for(uint32_t b=0;b<3;++b) {
                writes[b]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}; writes[b].dstSet=sets[i]; writes[b].dstBinding=b;
                writes[b].descriptorCount=1; writes[b].descriptorType=b?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                if(b) writes[b].pBufferInfo=&bufferInfos[b-1]; else writes[b].pImageInfo=&image;
            }
            vkUpdateDescriptorSets(c.device,3,writes,0,nullptr);
        }
        VkShaderModuleCreateInfo module{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO}; module.codeSize=sizeof(difference_comp); module.pCode=difference_comp;
        check(vkCreateShaderModule(c.device,&module,nullptr,&shader));
        VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,32};
        VkPipelineLayoutCreateInfo pipelineLayout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; pipelineLayout.setLayoutCount=1; pipelineLayout.pSetLayouts=&setLayout;
        pipelineLayout.pushConstantRangeCount=1; pipelineLayout.pPushConstantRanges=&push; check(vkCreatePipelineLayout(c.device,&pipelineLayout,nullptr,&layout));
        VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO}; info.layout=layout;
        info.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT; info.stage.module=shader; info.stage.pName="main";
        check(vkCreateComputePipelines(c.device,VK_NULL_HANDLE,1,&info,nullptr,&pipeline));
        int last=0;
        c.immediate([&](VkCommandBuffer command) {
            c.transition(command,source,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            int w=region.width,h=region.height,stage=0;
            do {
                int nw=(w>16 || h>16)?(w+1)/2:w,nh=(w>16 || h>16)?(h+1)/2:h;
                int32_t params[]{w,h,nw,nh,region.x,region.y,stage,int32_t(source.height)};
                vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
                vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&sets[stage%2],0,nullptr);
                vkCmdPushConstants(command,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,32,params);
                vkCmdDispatch(command,(nw+7)/8,(nh+7)/8,1);
                VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
                last=stage%2; ++stage; w=nw; h=nh;
            } while(w>16 || h>16);
            result.width=w; result.height=h;
        });
        result.data.resize(size_t(result.width)*result.height*4);
        std::memcpy(result.data.data(),buffers[last].mapped,result.data.size()*sizeof(float));
    } catch(...) { release(); throw; }
    release(); return result;
}
}
