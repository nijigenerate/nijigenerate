#pragma once

#include "gpu_simple_hook_slots.hpp"
#include "gpu_job_resources.hpp"
#include "gpu_render_state.hpp"
#include "gpu_program.hpp"
#include "../gpu_async_result.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <SDL3/SDL_video.h>
#include <utility>
#include <limits>

namespace nijigenerate::commands::depth::bone_gpu_async {

inline compat::Slice<compat::RetainedStringSlice> ngDepthBoneGpuAsyncMissingRequirements() {
    compat::Slice<compat::RetainedStringSlice> missing;
    const auto check = [&]<std::size_t N>(const char (&name)[N]) {
        if (SDL_GL_GetProcAddress(name) == nullptr) {
            missing.append(compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(name)));
        }
    };
    check("glCreateShader");
    check("glTransformFeedbackVaryings");
    check("glBeginTransformFeedback");
    check("glEndTransformFeedback");
    check("glBindBufferRange");
    check("glBindBufferBase");
    check("glGetIntegeri_v");
    check("glGetInteger64i_v");
    check("glGetBufferSubData");
    check("glGenVertexArrays");
    check("glBindVertexArray");
    check("glDeleteVertexArrays");
    check("glTexBuffer");
    check("glFenceSync");
    check("glClientWaitSync");
    check("glDeleteSync");
    return missing;
}

inline bool ngDepthBoneGpuAsyncSupported() {
    if (detail::supportHook != nullptr) return detail::supportHook();
    return ngDepthBoneGpuAsyncMissingRequirements().empty();
}

namespace detail {

inline void removePendingJob(std::size_t index) {
    // Concatenation replaces the array; it must not shift the retained old array.
    const auto left = pendingJobs.slice(0, index);
    const auto right = pendingJobs.slice(index + 1, pendingJobs.size());
    const auto count = left.size() + right.size();
    if (!count) { pendingJobs = {}; return; }
    compat::Slice<PendingDepthBoneGpuJob> remaining;
    remaining.resize(count);
    std::size_t destination=0;
    for (const auto& job : left) remaining[destination++]=job;
    for (const auto& job : right) remaining[destination++]=job;
    pendingJobs=remaining;
}

inline bool pollDepthBoneGpuAsync(std::uint32_t jobId, std::uint64_t timeoutNanoseconds,
    NgDepthBoneGpuAsyncResult& result, compat::RetainedStringSlice& error) {
    result = NgDepthBoneGpuAsyncResult{};
    error.reset();
    if (pollHook != nullptr) {
        result = NgDepthBoneGpuAsyncResult{};
        error.reset();
        return pollHook(jobId, result, error);
    }
    for (std::size_t i = 0; i < pendingJobs.size(); ++i) {
        if (pendingJobs[i].id != jobId) continue;
        const auto flags = static_cast<GLbitfield>(timeoutNanoseconds > 0 ? GL_SYNC_FLUSH_COMMANDS_BIT : 0);
        const auto status = depthBoneGlProcedure<PFNGLCLIENTWAITSYNCPROC>("glClientWaitSync")(
            pendingJobs[i].fence, flags, timeoutNanoseconds);
        if (status == GL_TIMEOUT_EXPIRED) return true;
        if (status == GL_WAIT_FAILED) {
            error = compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(
                "DepthBone GPU fence wait failed"));
            deleteJobResources(pendingJobs[i]);
            removePendingJob(i);
            return false;
        }

        result.ready = true;
        result.xs.resize(pendingJobs[i].count, std::numeric_limits<float>::quiet_NaN());
        result.ys.resize(pendingJobs[i].count, std::numeric_limits<float>::quiet_NaN());
        const auto byteCount = static_cast<GLsizeiptr>(pendingJobs[i].count * sizeof(float));
        const auto yOffset = static_cast<GLintptr>(pendingJobs[i].count * sizeof(float));
        GLint previousArrayBuffer{};
        depthBoneGlProcedure<decltype(&glGetIntegerv)>("glGetIntegerv")(
            GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
        depthBoneGlProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer")(GL_ARRAY_BUFFER, pendingJobs[i].outputBuffer);
        depthBoneGlProcedure<PFNGLGETBUFFERSUBDATAPROC>("glGetBufferSubData")(
            GL_ARRAY_BUFFER, 0, byteCount, result.xs.data());
        depthBoneGlProcedure<PFNGLGETBUFFERSUBDATAPROC>("glGetBufferSubData")(
            GL_ARRAY_BUFFER, yOffset, byteCount, result.ys.data());
        depthBoneGlProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer")(
            GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
        deleteJobResources(pendingJobs[i]);
        removePendingJob(i);
        return true;
    }
    error = compat::RetainedStringSlice(compat::RetainedString::fromStaticLiteral(
        "DepthBone GPU async job was not found"));
    return false;
}

} // namespace detail

inline bool ngPollDepthBoneGpuAsync(std::uint32_t jobId, NgDepthBoneGpuAsyncResult& result,
    compat::RetainedStringSlice& error) {
    return detail::pollDepthBoneGpuAsync(jobId, 0, result, error);
}

inline void ngCancelDepthBoneGpuAsync(std::uint32_t jobId) {
    if (detail::cancelHook != nullptr) {
        detail::cancelHook(jobId);
        return;
    }
    for (std::size_t i = 0; i < detail::pendingJobs.size(); ++i) {
        if (detail::pendingJobs[i].id != jobId) continue;
        detail::deleteJobResources(detail::pendingJobs[i]);
        // D concatenation creates a replacement array, after resource cleanup.
        // Retained descriptors of the old array still observe the cleared job.
        detail::removePendingJob(i);
        return;
    }
}

inline std::size_t ngPendingDepthBoneGpuAsyncJobCount() {
    return detail::pendingJobs.size();
}

} // namespace nijigenerate::commands::depth::bone_gpu_async

#include "gpu_submit.hpp"
