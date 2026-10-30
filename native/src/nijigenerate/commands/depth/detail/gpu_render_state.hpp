#pragma once

#include <SDL3/SDL_opengl.h>
#include <cstdint>
#include <nijigenerate/compat/slice.hpp>
#include "gpu_pending_job.hpp"

namespace nijigenerate::commands::depth::bone_gpu_async::detail {

inline constexpr GLenum NG_GL_TEXTURE_BUFFER_BINDING = 0x8C2A;

inline constinit GLuint depthBoneProgram{};
inline constinit GLint uVertexCount = -1;
inline constinit GLint uBoneCount = -1;
inline constinit GLint uSourceCount = -1;
inline constinit GLint uMaxInfluences = -1;
inline constinit GLint uInfluenceRadiusFloor = -1;
inline constinit GLint uRadiusScale = -1;
inline constinit GLint uTargetToRoot = -1;
inline constinit GLint uRootToTarget = -1;
inline constinit GLint uBones = -1;
inline constinit GLint uSources = -1;
inline constinit std::uint32_t nextJobId = 1;
inline compat::Slice<PendingDepthBoneGpuJob> pendingJobs;

} // namespace nijigenerate::commands::depth::bone_gpu_async::detail
