#pragma once
// Declarations of existing regression.d15304/15334/15603/15623 only.
#include <nijigenerate/commands/depth/bone_gpu_async.hpp>
#include <nijigenerate/commands/depth/gpu_dispatch_packet.hpp>
namespace nijigenerate_tests::regression::detail {
bool fakeDepthBoneGpuSubmit(
    nijigenerate::commands::depth::bone_gpu_async::DepthBoneGpuDispatchPacket& packet,
    std::uint32_t& jobId, nijigenerate::compat::RetainedStringSlice& error);
bool fakeDepthBoneGpuPoll(std::uint32_t jobId,
    nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult& result,
    nijigenerate::compat::RetainedStringSlice& error);
bool fakeDepthBoneSourceDepthSubmit(
    nijigenerate::commands::depth::bone_gpu_async::DepthBoneGpuDispatchPacket& packet,
    std::uint32_t& jobId, nijigenerate::compat::RetainedStringSlice& error);
bool fakeDepthBoneTransformPoll(std::uint32_t jobId,
    nijigenerate::commands::depth::bone_gpu_async::NgDepthBoneGpuAsyncResult& result,
    nijigenerate::compat::RetainedStringSlice& error);
}
