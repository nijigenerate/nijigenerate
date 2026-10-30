#pragma once
#include "gpu_program.hpp"
#include "gpu_job_resources.hpp"
#include "../gpu_dispatch_packet.hpp"
#include <algorithm>
#include <span>
#include <utility>

namespace nijigenerate::commands::depth::bone_gpu_async::detail {
template<class F, class... Args> inline decltype(auto) gpuCall(const char* name, Args&&... args) {
    return depthBoneGlProcedure<F>(name)(std::forward<Args>(args)...);
}
template<class F> struct GpuScopeExit {
    F restore;
    ~GpuScopeExit() { restore(); }
};
inline compat::Slice<float> packVertexLanes(DepthBoneGpuDispatchPacket& packet) {
    const auto count = packet.vertices.size();
    compat::Slice<float> result;
    result.resize(count * 2, std::numeric_limits<float>::quiet_NaN());
    for (std::size_t i = 0; i < count; ++i) {
        const auto vertex = packet.vertices.at(i);
        result[i] = vertex.x;
        result[i + count] = vertex.y;
    }
    return result;
}
inline void validatePacket(DepthBoneGpuDispatchPacket& packet) {
    const auto enforce = []<std::size_t N>(bool value, const char (&message)[N], std::size_t line) {
        if (!value) throw DepthBoneGpuError(message, line);
    };
    enforce(packet.vertices.size() > 0, "DepthBone GPU packet has no vertices", 523);
    enforce(packet.vertices.size() <= NgDepthBoneGpuAsyncMaxVertices, "DepthBone GPU packet exceeds maximum vertex count", 524);
    enforce(packet.depths.size() >= packet.vertices.size(), "DepthBone GPU depth buffer is too short", 525);
    enforce(packet.boneCount > 0, "DepthBone GPU packet has no bones", 526);
    enforce(packet.sourceCount > 0, "DepthBone GPU packet has no sources", 527);
    enforce(packet.boneCount <= NgDepthBoneGpuAsyncMaxBones, "DepthBone GPU packet exceeds maximum bone count", 528);
    enforce(packet.sourceCount <= NgDepthBoneGpuAsyncMaxSources, "DepthBone GPU packet exceeds maximum source count", 529);
    enforce(packet.maxInfluences <= NgDepthBoneGpuAsyncMaxInfluences, "DepthBone GPU packet exceeds maximum influence count", 530);
    enforce(packet.bones.size() >= packet.boneCount * NgDepthBoneGpuAsyncBoneStride, "DepthBone GPU bone buffer is too short", 531);
    enforce(packet.sources.size() >= packet.sourceCount * NgDepthBoneGpuAsyncSourceStride, "DepthBone GPU source buffer is too short", 532);
}
} // namespace nijigenerate::commands::depth::bone_gpu_async::detail

namespace nijigenerate::commands::depth::bone_gpu_async {
inline bool ngSubmitDepthBoneGpuAsync(DepthBoneGpuDispatchPacket& packet, std::uint32_t& jobId,
    compat::RetainedStringSlice& error) {
    using namespace detail;
    jobId = 0;
    error.reset();
    if (submitHook) return submitHook(packet, jobId, error);
    PendingDepthBoneGpuJob job;
    bool queued = false;
    try {
        if (!ngDepthBoneGpuAsyncSupported()) throw DepthBoneGpuError(
            "DepthBone GPU async deformation requires transform feedback and sync support", 542);
        validatePacket(packet);
        ensureProgram();
        GLint program{}, vao{}, arrayBuffer{}, texture0{}, texture1{}, activeTexture{}, textureBuffer{}, feedbackBuffer{};
        const GLboolean discard = gpuCall<decltype(&glIsEnabled)>("glIsEnabled", GL_RASTERIZER_DISCARD);
        gpuCall<decltype(&glGetIntegerv)>("glGetIntegerv", GL_CURRENT_PROGRAM, &program);
        gpuCall<decltype(&glGetIntegerv)>("glGetIntegerv", GL_VERTEX_ARRAY_BINDING, &vao);
        gpuCall<decltype(&glGetIntegerv)>("glGetIntegerv", GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        gpuCall<decltype(&glGetIntegerv)>("glGetIntegerv", NG_GL_TEXTURE_BUFFER_BINDING, &textureBuffer);
        gpuCall<decltype(&glGetIntegerv)>("glGetIntegerv", GL_ACTIVE_TEXTURE, &activeTexture);
        gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", GL_TEXTURE0);
        gpuCall<decltype(&glGetIntegerv)>("glGetIntegerv", GL_TEXTURE_BINDING_BUFFER, &texture0);
        gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", GL_TEXTURE1);
        gpuCall<decltype(&glGetIntegerv)>("glGetIntegerv", GL_TEXTURE_BINDING_BUFFER, &texture1);
        gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", static_cast<GLenum>(activeTexture));
        gpuCall<decltype(&glGetIntegerv)>("glGetIntegerv", GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &feedbackBuffer);
        const auto feedback0 = captureTransformFeedbackBinding(0);
        const auto feedback1 = captureTransformFeedbackBinding(1);
        // Installed after all capture calls, exactly where D scope(exit) begins.
        GpuScopeExit restore{[&] {
            restoreTransformFeedbackBinding(0, feedback0);
            restoreTransformFeedbackBinding(1, feedback1);
            gpuCall<PFNGLBINDBUFFERPROC>("glBindBuffer", GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<GLuint>(feedbackBuffer));
            gpuCall<PFNGLBINDBUFFERPROC>("glBindBuffer", GL_TEXTURE_BUFFER, static_cast<GLuint>(textureBuffer));
            gpuCall<PFNGLBINDBUFFERPROC>("glBindBuffer", GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
            gpuCall<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray", static_cast<GLuint>(vao));
            gpuCall<PFNGLUSEPROGRAMPROC>("glUseProgram", static_cast<GLuint>(program));
            gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", GL_TEXTURE0);
            gpuCall<decltype(&glBindTexture)>("glBindTexture", GL_TEXTURE_BUFFER, static_cast<GLuint>(texture0));
            gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", GL_TEXTURE1);
            gpuCall<decltype(&glBindTexture)>("glBindTexture", GL_TEXTURE_BUFFER, static_cast<GLuint>(texture1));
            gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", static_cast<GLenum>(activeTexture));
            if (discard == GL_TRUE) gpuCall<decltype(&glEnable)>("glEnable", GL_RASTERIZER_DISCARD);
            else gpuCall<decltype(&glDisable)>("glDisable", GL_RASTERIZER_DISCARD);
        }};
        job.id = nextJobId++;
        job.count = packet.vertices.size();
        gpuCall<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays", 1, &job.vertexArray);
        gpuCall<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray", job.vertexArray);
        const auto lanes = packVertexLanes(packet);
        job.verticesBuffer = createBuffer(GL_ARRAY_BUFFER, std::as_bytes(std::span(lanes.data(), lanes.size())));
        job.depthsBuffer = createBuffer(GL_ARRAY_BUFFER, std::as_bytes(std::span(packet.depths.data(), packet.depths.size())));
        createTextureBuffer(job.bonesBuffer, job.bonesTexture, {packet.bones.data(), packet.bones.size()});
        createTextureBuffer(job.sourcesBuffer, job.sourcesTexture, {packet.sources.data(), packet.sources.size()});
        const auto count = packet.vertices.size();
        const auto laneBytes = static_cast<GLsizeiptr>(count * sizeof(float));
        gpuCall<PFNGLGENBUFFERSPROC>("glGenBuffers", 1, &job.outputBuffer);
        gpuCall<PFNGLBINDBUFFERPROC>("glBindBuffer", GL_ARRAY_BUFFER, job.outputBuffer);
        gpuCall<PFNGLBUFFERDATAPROC>("glBufferData", GL_ARRAY_BUFFER, laneBytes * 2, nullptr, GL_STREAM_READ);
        gpuCall<PFNGLUSEPROGRAMPROC>("glUseProgram", depthBoneProgram);
        gpuCall<PFNGLUNIFORM1UIPROC>("glUniform1ui", uVertexCount, static_cast<GLuint>(packet.vertices.size()));
        gpuCall<PFNGLUNIFORM1UIPROC>("glUniform1ui", uBoneCount, packet.boneCount);
        gpuCall<PFNGLUNIFORM1UIPROC>("glUniform1ui", uSourceCount, packet.sourceCount);
        gpuCall<PFNGLUNIFORM1UIPROC>("glUniform1ui", uMaxInfluences,
            std::min(packet.maxInfluences == 0 ? 1u : packet.maxInfluences, NgDepthBoneGpuAsyncMaxInfluences));
        gpuCall<PFNGLUNIFORM1FPROC>("glUniform1f", uInfluenceRadiusFloor, packet.influenceRadiusFloor);
        gpuCall<PFNGLUNIFORM1FPROC>("glUniform1f", uRadiusScale, packet.radiusScale);
        gpuCall<PFNGLUNIFORMMATRIX4FVPROC>("glUniformMatrix4fv", uTargetToRoot, 1, GL_TRUE, &packet.targetToRoot[0][0]);
        gpuCall<PFNGLUNIFORMMATRIX4FVPROC>("glUniformMatrix4fv", uRootToTarget, 1, GL_TRUE, &packet.rootToTarget[0][0]);
        gpuCall<PFNGLUNIFORM1IPROC>("glUniform1i", uBones, 0);
        gpuCall<PFNGLUNIFORM1IPROC>("glUniform1i", uSources, 1);
        gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", GL_TEXTURE0);
        gpuCall<decltype(&glBindTexture)>("glBindTexture", GL_TEXTURE_BUFFER, job.bonesTexture);
        gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", GL_TEXTURE1);
        gpuCall<decltype(&glBindTexture)>("glBindTexture", GL_TEXTURE_BUFFER, job.sourcesTexture);
        gpuCall<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray", 0);
        gpuCall<PFNGLBINDBUFFERPROC>("glBindBuffer", GL_ARRAY_BUFFER, job.verticesBuffer);
        gpuCall<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer", 0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
        gpuCall<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray", 1);
        gpuCall<PFNGLBINDBUFFERPROC>("glBindBuffer", GL_ARRAY_BUFFER, job.verticesBuffer);
        gpuCall<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer", 1, 1, GL_FLOAT, GL_FALSE, 0,
            reinterpret_cast<const void*>(static_cast<GLintptr>(packet.vertices.size() * sizeof(float))));
        gpuCall<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray", 2);
        gpuCall<PFNGLBINDBUFFERPROC>("glBindBuffer", GL_ARRAY_BUFFER, job.depthsBuffer);
        gpuCall<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer", 2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
        gpuCall<PFNGLBINDBUFFERRANGEPROC>("glBindBufferRange", GL_TRANSFORM_FEEDBACK_BUFFER, 0, job.outputBuffer, 0, laneBytes);
        gpuCall<PFNGLBINDBUFFERRANGEPROC>("glBindBufferRange", GL_TRANSFORM_FEEDBACK_BUFFER, 1, job.outputBuffer, laneBytes, laneBytes);
        gpuCall<decltype(&glEnable)>("glEnable", GL_RASTERIZER_DISCARD);
        gpuCall<PFNGLBEGINTRANSFORMFEEDBACKPROC>("glBeginTransformFeedback", GL_POINTS);
        gpuCall<decltype(&glDrawArrays)>("glDrawArrays", GL_POINTS, 0, static_cast<GLsizei>(count));
        gpuCall<PFNGLENDTRANSFORMFEEDBACKPROC>("glEndTransformFeedback");
        if (discard == GL_FALSE) gpuCall<decltype(&glDisable)>("glDisable", GL_RASTERIZER_DISCARD);
        job.fence = gpuCall<PFNGLFENCESYNCPROC>("glFenceSync", GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        gpuCall<decltype(&glFlush)>("glFlush");
        gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", GL_TEXTURE1);
        gpuCall<decltype(&glBindTexture)>("glBindTexture", GL_TEXTURE_BUFFER, 0);
        gpuCall<PFNGLACTIVETEXTUREPROC>("glActiveTexture", GL_TEXTURE0);
        gpuCall<decltype(&glBindTexture)>("glBindTexture", GL_TEXTURE_BUFFER, 0);
        gpuCall<PFNGLDISABLEVERTEXATTRIBARRAYPROC>("glDisableVertexAttribArray", 0);
        gpuCall<PFNGLDISABLEVERTEXATTRIBARRAYPROC>("glDisableVertexAttribArray", 1);
        gpuCall<PFNGLDISABLEVERTEXATTRIBARRAYPROC>("glDisableVertexAttribArray", 2);
        pendingJobs.append(job);
        queued = true;
        jobId = job.id;
        return true;
    } catch (const compat::ThrowableException& failure) {
        if (!queued) deleteJobResources(job);
        error = compat::RetainedStringSlice(failure.msg);
        return false;
    }
}
} // namespace nijigenerate::commands::depth::bone_gpu_async
