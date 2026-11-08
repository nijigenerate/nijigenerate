#include "compute.hpp"
#include "compute_internal.hpp"
#include "metal_shaders.hpp"
#include <cstring>
#include <map>
#include <mutex>
namespace nijigenerate::backend::metal {
namespace {
std::vector<std::weak_ptr<ComputeJob>> jobs;
std::mutex pipelineMutex;
}
id<MTLComputePipelineState> ngComputePipeline(const char* name,const char* source) {
    // Jobs may outlive the calling command, but the renderer owns the device.
    std::lock_guard lock(pipelineMutex);
    auto& c=context();
    auto key=[NSString stringWithUTF8String:name];
    if(auto existing=c.p->computePipelines[key]) return existing;
    NSError* error=nil;
    auto options=[MTLCompileOptions new]; options.languageVersion=MTLLanguageVersion2_3; options.fastMathEnabled=NO;
    auto library=[c.p->device newLibraryWithSource:[NSString stringWithUTF8String:source] options:options error:&error];
    if(!library) throw std::runtime_error(error.localizedDescription.UTF8String);
    auto pipeline=[c.p->device newComputePipelineStateWithFunction:[library newFunctionWithName:@"main0"] error:&error];
    if(!pipeline) throw std::runtime_error(error.localizedDescription.UTF8String);
    c.p->computePipelines[key]=pipeline; return pipeline;
}
struct ComputeJob::Impl {
    std::array<id<MTLBuffer>,6> buffers;
    id<MTLCommandBuffer> command;
    dispatch_semaphore_t completed=dispatch_semaphore_create(0);
    uint32_t count{};
};
ComputeJob::ComputeJob(ComputeKernel kernel,const std::array<std::span<const std::byte>,5>& inputs,uint32_t count)
    : p(std::make_unique<Impl>()) {
    @autoreleasepool {
        if(!count) throw std::invalid_argument("Compute dispatch has no elements");
        auto& c=context(); p->count=count;
        for(unsigned i=0;i<6;++i) {
            const auto size=i==5?size_t(count)*2*sizeof(float):std::max<size_t>(4,inputs[i].size());
            if(size>c.p->device.maxBufferLength) throw std::length_error("Compute buffer exceeds Metal device limits");
            p->buffers[i]=[c.p->device newBufferWithLength:size options:MTLResourceStorageModeShared];
            if(!p->buffers[i]) throw std::runtime_error("Metal compute allocation failed");
            std::memset(p->buffers[i].contents,0,size);
            if(i<5 && !inputs[i].empty()) std::memcpy(p->buffers[i].contents,inputs[i].data(),inputs[i].size());
        }
        auto pipeline=kernel==ComputeKernel::DepthBone?ngComputePipeline("depth_bone",depth_bone_source):ngComputePipeline("depth_sample",depth_sample_source);
        p->command=[c.p->queue commandBuffer]; p->command.label=@"Depth compute";
        auto encoder=[p->command computeCommandEncoder]; [encoder setComputePipelineState:pipeline];
        for(unsigned i=0;i<6;++i) [encoder setBuffer:p->buffers[i] offset:0 atIndex:i];
        [encoder setBytes:&count length:sizeof(count) atIndex:6];
        [encoder dispatchThreadgroups:MTLSizeMake((count+63)/64,1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
        [encoder endEncoding];
        auto completed=p->completed;
        [p->command addCompletedHandler:^(id<MTLCommandBuffer>) { dispatch_semaphore_signal(completed); }];
        [p->command commit];
    }
}
ComputeJob::~ComputeJob() { release(); }
void ComputeJob::release() {
    if(!p) return;
    if(p->command) [p->command waitUntilCompleted];
    p.reset();
}
bool ComputeJob::poll(std::vector<float>& values,uint64_t timeout) {
    if(!p) throw std::logic_error("Compute job has been released");
    if(p->command.status<MTLCommandBufferStatusCompleted) {
        if(!timeout) return false;
        if(timeout==UINT64_MAX) [p->command waitUntilCompleted];
        else if(dispatch_semaphore_wait(p->completed,dispatch_time(DISPATCH_TIME_NOW,int64_t(std::min<uint64_t>(timeout,INT64_MAX))))) return false;
    }
    checkCommand(p->command);
    values.resize(size_t(p->count)*2);
    std::memcpy(values.data(),p->buffers[5].contents,values.size()*sizeof(float)); return true;
}
std::shared_ptr<ComputeJob> ngSubmitCompute(ComputeKernel kernel,const std::array<std::span<const std::byte>,5>& inputs,uint32_t count) {
    auto job=std::make_shared<ComputeJob>(kernel,inputs,count);
    std::erase_if(jobs,[](const auto& weak) { return weak.expired(); }); jobs.push_back(job); return job;
}
void ngShutdownComputeJobs() { for(auto weak:jobs) if(auto job=weak.lock()) job->release(); jobs.clear(); }
}
