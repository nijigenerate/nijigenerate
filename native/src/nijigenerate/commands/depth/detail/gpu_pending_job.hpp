#pragma once

#include <SDL3/SDL_opengl.h>
#include <cstddef>
#include <cstdint>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::depth::bone_gpu_async::detail {

struct PendingDepthBoneGpuJob {
    std::uint32_t id{};
    std::size_t count{};
    GLuint verticesBuffer{};
    GLuint depthsBuffer{};
    GLuint bonesBuffer{};
    GLuint sourcesBuffer{};
    GLuint outputBuffer{};
    GLuint bonesTexture{};
    GLuint sourcesTexture{};
    GLuint vertexArray{};
    GLsync fence{};
};

} // namespace nijigenerate::commands::depth::bone_gpu_async::detail

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::commands::depth::bone_gpu_async::detail::PendingDepthBoneGpuJob> {
    inline static constexpr bool isNativeObject=false;
    inline static constexpr char qualifiedArgument[]=
        "(nijigenerate.commands.depth.bone_gpu_async.PendingDepthBoneGpuJob)";
    inline static constexpr char unqualifiedArgument[]="(PendingDepthBoneGpuJob)";
    inline static constexpr char qualifiedElement[]=
        "nijigenerate.commands.depth.bone_gpu_async.PendingDepthBoneGpuJob";
    inline static constexpr char unqualifiedElement[]="PendingDepthBoneGpuJob";
};
} // namespace nicxlive::core::d_runtime
