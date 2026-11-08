#include "bone_gpu_async.hpp"
#include "gpu_dispatch_packet.hpp"
#include <nijigenerate/backend/vulkan/compute.hpp>
#include <map>
#include <cstring>
#include <algorithm>
namespace nijigenerate::commands::depth::bone_gpu_async {
namespace vk=backend::vulkan;
namespace {
struct Job { size_t count; std::shared_ptr<vk::ComputeJob> compute; };
std::map<uint32_t,Job> jobs;
uint32_t nextJob=1;
struct Parameters {
    uint32_t vertexCount,boneCount,sourceCount,maxInfluences;
    float influenceRadiusFloor,radiusScale,padding[2]{};
    float targetToRoot[16],rootToTarget[16];
};
static_assert(sizeof(Parameters)==160);
}
compat::Slice<compat::RetainedStringSlice> ngDepthBoneGpuAsyncMissingRequirements() {
    compat::Slice<compat::RetainedStringSlice> missing;
    try { (void)vk::context(); } catch(const std::exception&) {
        missing.append(compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("Vulkan compute context")));
    }
    return missing;
}
bool ngDepthBoneGpuAsyncSupported() {
    return detail::supportHook?detail::supportHook():ngDepthBoneGpuAsyncMissingRequirements().empty();
}
bool ngSubmitDepthBoneGpuAsync(DepthBoneGpuDispatchPacket& packet,uint32_t& id,compat::RetainedStringSlice& error) {
    id=0; error.reset(); if(detail::submitHook) return detail::submitHook(packet,id,error);
    try {
        const auto count=packet.vertices.size();
        if(!count || count>NgDepthBoneGpuAsyncMaxVertices || packet.depths.size()<count ||
            !packet.boneCount || packet.boneCount>NgDepthBoneGpuAsyncMaxBones ||
            !packet.sourceCount || packet.sourceCount>NgDepthBoneGpuAsyncMaxSources ||
            packet.maxInfluences>NgDepthBoneGpuAsyncMaxInfluences ||
            packet.bones.size()<packet.boneCount*NgDepthBoneGpuAsyncBoneStride ||
            packet.sources.size()<packet.sourceCount*NgDepthBoneGpuAsyncSourceStride)
            throw std::invalid_argument("Invalid DepthBone GPU dispatch packet");
        std::vector<float> positions(count*2);
        for(size_t i=0;i<count;++i) { auto v=packet.vertices.at(i); positions[i]=v.x; positions[count+i]=v.y; }
        Parameters params{uint32_t(count),packet.boneCount,packet.sourceCount,
            std::clamp(packet.maxInfluences,1u,NgDepthBoneGpuAsyncMaxInfluences),packet.influenceRadiusFloor,packet.radiusScale};
        for(int row=0;row<4;++row) for(int col=0;col<4;++col) {
            params.targetToRoot[row*4+col]=packet.targetToRoot[row][col];
            params.rootToTarget[row*4+col]=packet.rootToTarget[row][col];
        }
        const std::array<std::span<const std::byte>,5> inputs{
            std::as_bytes(std::span(positions)),std::as_bytes(std::span(packet.depths.data(),packet.depths.size())),
            std::as_bytes(std::span(packet.bones.data(),packet.bones.size())),
            std::as_bytes(std::span(packet.sources.data(),packet.sources.size())),std::as_bytes(std::span(&params,1))};
        auto job=vk::ngSubmitCompute(vk::ComputeKernel::DepthBone,inputs,uint32_t(count));
        id=nextJob++; jobs.emplace(id,Job{count,std::move(job)}); return true;
    } catch(const std::exception& e) { error=compat::RetainedStringSlice(compat::RetainedString(e.what())); return false; }
}
bool ngPollDepthBoneGpuAsync(uint32_t id,NgDepthBoneGpuAsyncResult& result,compat::RetainedStringSlice& error) {
    result={}; error.reset(); if(detail::pollHook) return detail::pollHook(id,result,error);
    auto found=jobs.find(id);
    if(found==jobs.end()) { error=compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral("DepthBone GPU job was not found")); return false; }
    try {
        std::vector<float> output; if(!found->second.compute->poll(output)) return true;
        auto count=found->second.count; result.xs.resize(count); result.ys.resize(count);
        std::copy_n(output.data(),count,result.xs.data()); std::copy_n(output.data()+count,count,result.ys.data());
        result.ready=true; jobs.erase(found); return true;
    } catch(const std::exception& e) { error=compat::RetainedStringSlice(compat::RetainedString(e.what())); jobs.erase(found); return false; }
}
void ngCancelDepthBoneGpuAsync(uint32_t id) { if(detail::cancelHook) detail::cancelHook(id); else jobs.erase(id); }
size_t ngPendingDepthBoneGpuAsyncJobCount() { return jobs.size(); }
}
