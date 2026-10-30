#pragma once

#include "gpu_pending_job.hpp"
#include "gpu_indexed_binding_operations.hpp"
#include <array>
#include <span>

namespace nijigenerate::commands::depth::bone_gpu_async::detail {

inline GLuint createBuffer(GLenum target, std::span<const std::byte> bytes, GLenum usage = GL_DYNAMIC_DRAW) {
    GLuint buffer{};
    depthBoneGlProcedure<PFNGLGENBUFFERSPROC>("glGenBuffers")(1, &buffer);
    depthBoneGlProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer")(target, buffer);
    depthBoneGlProcedure<PFNGLBUFFERDATAPROC>("glBufferData")(
        target, static_cast<GLsizeiptr>(bytes.size()), bytes.data(), usage);
    return buffer;
}

inline void createTextureBuffer(GLuint& buffer, GLuint& texture, std::span<const float> data) {
    buffer = createBuffer(GL_TEXTURE_BUFFER, std::as_bytes(data));
    depthBoneGlProcedure<decltype(&glGenTextures)>("glGenTextures")(1, &texture);
    depthBoneGlProcedure<decltype(&glBindTexture)>("glBindTexture")(GL_TEXTURE_BUFFER, texture);
    depthBoneGlProcedure<PFNGLTEXBUFFERPROC>("glTexBuffer")(GL_TEXTURE_BUFFER, GL_R32F, buffer);
    depthBoneGlProcedure<decltype(&glBindTexture)>("glBindTexture")(GL_TEXTURE_BUFFER, 0);
}

inline void deleteJobResources(PendingDepthBoneGpuJob& job) {
    if (job.fence != nullptr) {
        if (const auto deleteSync = depthBoneGlProcedure<PFNGLDELETESYNCPROC>("glDeleteSync")) {
            deleteSync(job.fence);
            job.fence = nullptr;
        }
    }
    const std::array<GLuint, 5> buffers{
        job.verticesBuffer, job.depthsBuffer, job.bonesBuffer, job.sourcesBuffer, job.outputBuffer};
    bool hasBuffer = false;
    for (const auto buffer : buffers) hasBuffer = hasBuffer || buffer != 0;
    if (hasBuffer) {
        if (const auto deleteBuffers = depthBoneGlProcedure<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers")) {
            deleteBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());
        }
    }
    const std::array<GLuint, 2> textures{job.bonesTexture, job.sourcesTexture};
    bool hasTexture = false;
    for (const auto texture : textures) hasTexture = hasTexture || texture != 0;
    if (hasTexture) {
        if (const auto deleteTextures = depthBoneGlProcedure<decltype(&glDeleteTextures)>("glDeleteTextures")) {
            deleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
        }
    }
    if (job.vertexArray != 0) {
        if (const auto deleteVertexArrays =
            depthBoneGlProcedure<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays")) {
            deleteVertexArrays(1, &job.vertexArray);
        }
    }
    job = PendingDepthBoneGpuJob{};
}

} // namespace nijigenerate::commands::depth::bone_gpu_async::detail
