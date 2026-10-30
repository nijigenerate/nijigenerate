#pragma once

#include "gpu_indexed_binding_operations.hpp"
#include "gpu_render_state.hpp"
#include "gpu_vertex_source.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::commands::depth::bone_gpu_async::detail {

template<class Allocate> decltype(auto) allocateGpuPrimitiveMemory(Allocate&& allocate) {
    try {
        return std::forward<Allocate>(allocate)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline compat::RetainedString copyGpuEnforceMessage(std::string_view message) {
    if (message.empty()) return compat::RetainedString::fromStaticLiteral("Enforcement failed");
    try {
        return compat::RetainedString(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"),394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"),394);
    }
}

// Preserve the full D Exception.msg, including embedded NULs in driver logs.
class DepthBoneGpuError : public compat::ThrowableException {
public:
    template<std::size_t N> DepthBoneGpuError(const char (&message)[N], std::size_t sourceLine)
        : ThrowableException(copyGpuEnforceMessage(std::string_view(message,N-1)),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone_gpu_async.d"),
            sourceLine) {}
    explicit DepthBoneGpuError(std::string message, std::size_t sourceLine)
        : ThrowableException(copyGpuEnforceMessage(message),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone_gpu_async.d"),
            sourceLine) {}
};

inline void checkShader(GLuint shader, std::string_view label) {
    GLint status{};
    depthBoneGlProcedure<PFNGLGETSHADERIVPROC>("glGetShaderiv")(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) return;
    GLint logLength{};
    depthBoneGlProcedure<PFNGLGETSHADERIVPROC>("glGetShaderiv")(shader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log;
    if (logLength > 1) {
        // Growing a D char array initializes each byte with char.init (0xff).
        allocateGpuPrimitiveMemory([&] {
            log.resize(static_cast<std::size_t>(logLength), static_cast<char>(0xff));
        });
        depthBoneGlProcedure<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog")(
            shader, logLength, nullptr, log.data());
    }
    throw DepthBoneGpuError(allocateGpuPrimitiveMemory([&] {
        return std::string(label) + " compile failed: " + log;
    }),425);
}

inline void checkProgram(GLuint program) {
    GLint status{};
    depthBoneGlProcedure<PFNGLGETPROGRAMIVPROC>("glGetProgramiv")(program, GL_LINK_STATUS, &status);
    if (status == GL_TRUE) return;
    GLint logLength{};
    depthBoneGlProcedure<PFNGLGETPROGRAMIVPROC>("glGetProgramiv")(program, GL_INFO_LOG_LENGTH, &logLength);
    std::string log;
    if (logLength > 1) {
        allocateGpuPrimitiveMemory([&] {
            log.resize(static_cast<std::size_t>(logLength), static_cast<char>(0xff));
        });
        depthBoneGlProcedure<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog")(
            program, logLength, nullptr, log.data());
    }
    throw DepthBoneGpuError(allocateGpuPrimitiveMemory([&] {
        return "DepthBone async transform feedback shader link failed: " + log;
    }),439);
}

inline void ensureProgram() {
    if (depthBoneProgram != 0) return;
    const auto shader = depthBoneGlProcedure<PFNGLCREATESHADERPROC>("glCreateShader")(GL_VERTEX_SHADER);
    const GLchar* src = VertexSourceBytes;
    depthBoneGlProcedure<PFNGLSHADERSOURCEPROC>("glShaderSource")(shader, 1, &src, nullptr);
    depthBoneGlProcedure<PFNGLCOMPILESHADERPROC>("glCompileShader")(shader);
    checkShader(shader, "DepthBone async transform feedback shader");

    depthBoneProgram = depthBoneGlProcedure<PFNGLCREATEPROGRAMPROC>("glCreateProgram")();
    depthBoneGlProcedure<PFNGLATTACHSHADERPROC>("glAttachShader")(depthBoneProgram, shader);
    const GLchar* varyings[]{"outDeformX", "outDeformY"};
    depthBoneGlProcedure<PFNGLTRANSFORMFEEDBACKVARYINGSPROC>("glTransformFeedbackVaryings")(
        depthBoneProgram, 2, varyings, GL_SEPARATE_ATTRIBS);
    depthBoneGlProcedure<PFNGLLINKPROGRAMPROC>("glLinkProgram")(depthBoneProgram);
    depthBoneGlProcedure<PFNGLDETACHSHADERPROC>("glDetachShader")(depthBoneProgram, shader);
    depthBoneGlProcedure<PFNGLDELETESHADERPROC>("glDeleteShader")(shader);
    checkProgram(depthBoneProgram);

    const auto uniform = depthBoneGlProcedure<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation");
    uVertexCount = uniform(depthBoneProgram, "vertexCount");
    uBoneCount = uniform(depthBoneProgram, "boneCount");
    uSourceCount = uniform(depthBoneProgram, "sourceCount");
    uMaxInfluences = uniform(depthBoneProgram, "maxInfluences");
    uInfluenceRadiusFloor = uniform(depthBoneProgram, "influenceRadiusFloor");
    uRadiusScale = uniform(depthBoneProgram, "radiusScale");
    uTargetToRoot = uniform(depthBoneProgram, "targetToRoot");
    uRootToTarget = uniform(depthBoneProgram, "rootToTarget");
    uBones = uniform(depthBoneProgram, "bones");
    uSources = uniform(depthBoneProgram, "sources");
}

} // namespace nijigenerate::commands::depth::bone_gpu_async::detail
