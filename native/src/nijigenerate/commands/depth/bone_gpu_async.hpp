#pragma once

#include <cstdint>
#include <cstddef>
#include "gpu_async_result.hpp"
// Include gpu_dispatch_packet.hpp when constructing the runtime-backed packet.
#include "detail/gpu_simple_hook_types.hpp"
#include "detail/gpu_simple_hook_slots.hpp"

namespace nijigenerate::commands::depth::bone_gpu_async {

inline constexpr std::uint32_t NgDepthBoneGpuAsyncMaxBones = 64u;
inline constexpr std::uint32_t NgDepthBoneGpuAsyncMaxSources = 128u;
inline constexpr std::uint32_t NgDepthBoneGpuAsyncMaxInfluences = 16u;
inline constexpr std::uint32_t NgDepthBoneGpuAsyncMaxVertices = 1'000'000u;
inline constexpr std::uint32_t NgDepthBoneGpuAsyncBoneStride = 24u;
inline constexpr std::uint32_t NgDepthBoneGpuAsyncSourceStride = 28u;

// Both source backend branches expose the same public operations.
bool ngDepthBoneGpuAsyncSupported();
compat::Slice<compat::RetainedStringSlice> ngDepthBoneGpuAsyncMissingRequirements();
bool ngSubmitDepthBoneGpuAsync(DepthBoneGpuDispatchPacket& packet, std::uint32_t& jobId,
    compat::RetainedStringSlice& error);
bool ngPollDepthBoneGpuAsync(std::uint32_t jobId, NgDepthBoneGpuAsyncResult& result,
    compat::RetainedStringSlice& error);
void ngCancelDepthBoneGpuAsync(std::uint32_t jobId);
std::size_t ngPendingDepthBoneGpuAsyncJobCount();

inline void ngSetDepthBoneGpuAsyncTestHooks(NgDepthBoneGpuAsyncSupportHook support,
    NgDepthBoneGpuAsyncSubmitHook submit, NgDepthBoneGpuAsyncPollHook poll,
    NgDepthBoneGpuAsyncCancelHook cancel = nullptr) {
    detail::supportHook = support;
    detail::submitHook = submit;
    detail::pollHook = poll;
    detail::cancelHook = cancel;
}

inline void ngClearDepthBoneGpuAsyncTestHooks() {
    detail::supportHook = nullptr;
    detail::submitHook = nullptr;
    detail::pollHook = nullptr;
    detail::cancelHook = nullptr;
}

} // namespace nijigenerate::commands::depth::bone_gpu_async

#if defined(NIJIGENERATE_GPU_NONRENDER) && defined(NIJIGENERATE_GPU_RENDER)
#error "Select exactly one depth GPU backend per target"
#elif defined(NIJIGENERATE_GPU_NONRENDER)
#include "detail/gpu_nonrender_operations.hpp"
#elif defined(NIJIGENERATE_GPU_RENDER)
#include "detail/gpu_render_operations.hpp"
#endif
