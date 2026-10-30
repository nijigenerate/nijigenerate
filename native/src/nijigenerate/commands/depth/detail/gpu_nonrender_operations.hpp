#pragma once

#include "gpu_simple_hook_slots.hpp"
#include "../gpu_async_result.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <cstddef>
#include <cstdint>

// Include only in the non-rendering backend; the rendering backend has different bodies.
namespace nijigenerate::commands::depth::bone_gpu_async {

inline bool ngDepthBoneGpuAsyncSupported() {
    if (detail::supportHook != nullptr) return detail::supportHook();
    return false;
}

inline compat::Slice<compat::RetainedStringSlice> ngDepthBoneGpuAsyncMissingRequirements() {
    static const compat::RetainedStringSlice requirement(
        compat::RetainedString::fromStaticLiteral("rendering backend"));
    return {requirement};
}

inline bool ngSubmitDepthBoneGpuAsync(DepthBoneGpuDispatchPacket& packet, std::uint32_t& jobId,
    compat::RetainedStringSlice& error) {
    jobId = 0;
    error.reset();
    error = compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(
        "DepthBone GPU async deformation requires the rendering backend"));
    if (detail::submitHook != nullptr) {
        // D function-pointer out parameters are initialized again on hook entry.
        jobId = 0;
        error.reset();
        return detail::submitHook(packet, jobId, error);
    }
    return false;
}

inline bool ngPollDepthBoneGpuAsync(std::uint32_t jobId, NgDepthBoneGpuAsyncResult& result,
    compat::RetainedStringSlice& error) {
    result = NgDepthBoneGpuAsyncResult{};
    error.reset();
    error = compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(
        "DepthBone GPU async deformation requires the rendering backend"));
    if (detail::pollHook != nullptr) {
        result = NgDepthBoneGpuAsyncResult{};
        error.reset();
        return detail::pollHook(jobId, result, error);
    }
    return false;
}

inline void ngCancelDepthBoneGpuAsync(std::uint32_t jobId) {
    if (detail::cancelHook != nullptr) detail::cancelHook(jobId);
}

inline std::size_t ngPendingDepthBoneGpuAsyncJobCount() {
    return 0;
}

} // namespace nijigenerate::commands::depth::bone_gpu_async
