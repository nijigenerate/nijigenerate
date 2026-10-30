#pragma once

#include <cstdint>
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::commands::depth::bone_gpu_async {

using NgDepthBoneGpuAsyncSupportHook = bool (*)();
struct DepthBoneGpuDispatchPacket;
struct NgDepthBoneGpuAsyncResult;
using NgDepthBoneGpuAsyncSubmitHook = bool (*)(DepthBoneGpuDispatchPacket& packet,
    std::uint32_t& jobId, compat::RetainedStringSlice& error);
using NgDepthBoneGpuAsyncPollHook = bool (*)(std::uint32_t jobId,
    NgDepthBoneGpuAsyncResult& result, compat::RetainedStringSlice& error);
using NgDepthBoneGpuAsyncCancelHook = void (*)(std::uint32_t jobId);

} // namespace nijigenerate::commands::depth::bone_gpu_async
