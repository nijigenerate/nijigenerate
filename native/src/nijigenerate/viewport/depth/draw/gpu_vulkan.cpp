#include "gpu_internal.hpp"
#include <nijigenerate/backend/vulkan/compute.hpp>
#include <map>
#include <span>
namespace nijigenerate::viewport::depth::draw {
using namespace gpu_detail;
namespace gpu_detail {
namespace {
struct SampleJob {
    uint32_t layerIndex{}; size_t count{};
    std::shared_ptr<backend::vulkan::ComputeJob> compute;
};
struct ComposeJob {
    uint32_t id{};
    compat::Slice<uint32_t> ids;
    compat::Slice<DepthDrawGpuLayerReadback> readbacks;
    compat::Slice<bool> ready;
};
std::map<uint32_t,SampleJob> sampleJobs;
compat::Slice<ComposeJob> composeJobs;
uint32_t nextSampleId=1,nextComposeId=1;
template<class T> void removeJob(compat::Slice<T>& jobs,size_t index,size_t=0) {
    compat::Slice<T> next;
    for(size_t i=0;i<jobs.size();++i) if(i!=index) next.append(jobs[i]);
    jobs=next;
}
bool cancelSample(uint32_t id) { return sampleJobs.erase(id)>0; }
bool pollSample(uint32_t id,uint64_t timeout,DepthDrawGpuLayerSamplePollResult& result,GpuError& error) {
    result={}; error={}; auto found=sampleJobs.find(id);
    if(found==sampleJobs.end()) { error=GpuError::fromStaticLiteral("DepthDraw GPU layer sample job was not found"); return false; }
    try {
        std::vector<float> values; if(!found->second.compute->poll(values,timeout)) return true;
        auto count=found->second.count;
        result.readback.layerIndex=found->second.layerIndex;
        result.readback.validSamples.resize(count); result.readback.sampleDepths.resize(count);
        for(size_t i=0;i<count;++i) { result.readback.validSamples[i]=values[i]>.5f?1:0; result.readback.sampleDepths[i]=values[count+i]; }
        result.ready=true; sampleJobs.erase(found); return true;
    } catch(const std::exception& e) { error=GpuError(std::string(e.what())); sampleJobs.erase(found); return false; }
}
}
void clearPendingComposeJobs() { sampleJobs.clear(); composeJobs={}; }
}
bool ngSubmitDepthDrawGpuLayerSample(DepthDrawGpuLayerSamplePacket& packet,uint32_t& id,GpuError& error) {
    id=0; error={};
    try {
        validateDepthDrawGpuLayerSamplePacket(packet);
        enforce(ngDepthDrawGpuLayerSampleSupportsConvolution(packet.layer.convolution),
            "DepthDraw GPU layer sampling does not support custom median convolution yet");
        auto upload=ngBuildDepthDrawGpuLayerSampleUpload(packet);
        std::vector<uint32_t> depths(upload.depthPixels.begin(),upload.depthPixels.end());
        std::vector<uint32_t> coverage(upload.normalCoveragePixels.begin(),upload.normalCoveragePixels.end());
        if(coverage.empty()) coverage.resize(4);
        const std::array<std::span<const std::byte>,5> inputs{
            std::as_bytes(std::span(upload.documentPositions.data(),upload.documentPositions.size())),
            std::as_bytes(std::span(upload.layer.data(),upload.layer.size())),
            std::as_bytes(std::span(upload.binding.data(),upload.binding.size())),
            std::as_bytes(std::span(depths)),std::as_bytes(std::span(coverage))};
        auto job=backend::vulkan::ngSubmitCompute(backend::vulkan::ComputeKernel::DepthSample,inputs,uint32_t(packet.documentPositions.size()));
        id=nextSampleId++; sampleJobs.emplace(id,SampleJob{packet.layerIndex,packet.documentPositions.size(),std::move(job)}); return true;
    } catch(const compat::ThrowableException& e) { error=e.msg; return false; }
    catch(const std::exception& e) { error=GpuError(std::string(e.what())); return false; }
}
namespace gpu_detail {
bool submitDepthDrawGpuComposeBackend(DepthDrawGpuComposePacket& packet, std::uint32_t& id, GpuError& error) {
    id = 0; error = {}; ComposeJob job;
    try {
        enforce(ngDepthDrawGpuLayerSampleSupported(),
            [] {
                const auto diagnostic = ngDepthDrawGpuLayerSampleSupportDiagnostic();
                return allocateGpuMemory([&] {
                    return "DepthDraw GPU composition requires Vulkan layer sampling support: " +
                        std::string(diagnostic.view());
                });
            }, 987);
        for (auto layer : packet.layers) enforce(ngDepthDrawGpuLayerSampleSupportsConvolution(layer.convolution),
            "DepthDraw GPU composition does not support custom median convolution yet", 991);
        job.id = nextComposeId++; job.ids.resize(packet.layers.size());
        job.readbacks.resize(packet.layers.size()); job.ready.resize(packet.layers.size());
        const auto layerCount = packet.layers.size();
        for (std::size_t i = 0; i < layerCount; ++i) {
            auto sample = ngBuildDepthDrawGpuLayerSamplePacket(packet, i);
            std::uint32_t sampleId = 0;
            if (!ngSubmitDepthDrawGpuLayerSample(sample, sampleId, error)) {
                for (auto submitted : job.ids) if (submitted) cancelSample(submitted);
                return false;
            }
            job.ids[i] = sampleId;
        }
        composeJobs.append(job); id = job.id; return true;
    } catch (const compat::ThrowableException& e) {
        for (auto submitted : job.ids) if (submitted) cancelSample(submitted);
        error = e.msg; return false;
    }
}
bool pollDepthDrawGpuComposeBackend(std::uint32_t id, const DepthDrawGpuComposePacket& packet,
    DepthDrawGpuDispatchPollResult& r, GpuError& error) {
    r = {}; error = {};
    const auto at = []<class T>(compat::Slice<T>& values, std::size_t index,
        std::size_t line) -> T& {
        if (index >= values.size())
            throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/gpu.d"), line);
        return values[index];
    };
    const auto jobCount = composeJobs.size();
    for (std::size_t i = 0; i < jobCount; ++i) {
        if (at(composeJobs, i, 1030).id != id) continue;
        const auto ids = at(composeJobs, i, 1031).ids;
        const auto cancelRemaining = [&](std::size_t sourceLine) {
            const auto remaining = at(composeJobs, i, sourceLine).ids;
            for (std::size_t j = 0; j < remaining.size(); ++j)
                if (!at(at(composeJobs, i, sourceLine + 1).ready, j, sourceLine + 1))
                    cancelSample(remaining[j]);
            removeJob(composeJobs, i, sourceLine + 5);
        };
        for (std::size_t layer = 0; layer < ids.size(); ++layer) {
            if (at(at(composeJobs, i, 1032).ready, layer, 1032)) continue;
            DepthDrawGpuLayerSamplePollResult polled;
            if (!ngPollDepthDrawGpuLayerSample(ids[layer], polled, error)) {
                cancelRemaining(1035); return false;
            }
            if (!polled.ready) return true;
            if (polled.readback.layerIndex != layer) {
                error = GpuError::fromStaticLiteral("DepthDraw GPU compose layer readback index mismatch");
                cancelRemaining(1046); return false;
            }
            at(at(composeJobs, i, 1054).readbacks, layer, 1054) = polled.readback;
            at(at(composeJobs, i, 1055).ready, layer, 1055) = true;
        }
        r.ready = true;
        r.readback = ngBuildDepthDrawGpuComposeReadback(packet, at(composeJobs, i, 1058).readbacks);
        removeJob(composeJobs, i, 1059); return true;
    }
    error = GpuError::fromStaticLiteral("DepthDraw GPU compose job was not found"); return false;
}
void cancelDepthDrawGpuComposeBackend(std::uint32_t id) {
    const auto jobCount = composeJobs.size();
    for (std::size_t i = 0; i < jobCount; ++i) if (composeJobs[i].id == id) {
        const auto ids = composeJobs[i].ids;
        for (std::size_t j = 0; j < ids.size(); ++j) if (!composeJobs[i].ready[j]) cancelSample(ids[j]);
        removeJob(composeJobs, i); return;
    }
}
}
bool ngPollDepthDrawGpuLayerSample(std::uint32_t id, DepthDrawGpuLayerSamplePollResult& r, GpuError& error) {
    return pollSample(id, 0, r, error);
}
std::size_t ngPendingDepthDrawGpuLayerSampleJobCount() { return sampleJobs.size(); }
}
