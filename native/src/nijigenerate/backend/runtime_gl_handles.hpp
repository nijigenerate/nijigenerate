#pragma once
#include "runtime_gl_shader.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <render/backend_handles.hpp>
#include "runtime_gl_texture.hpp"
#include <memory>
#include <string>

namespace nijigenerate::backend::runtime_gl {
// The original backend handle hierarchy is polymorphic and retains object identity.
using RenderBackendHandle = nicxlive::core::RenderBackendHandle;
using RenderShaderHandle = nicxlive::core::RenderShaderHandle;
using RenderTextureHandle = nicxlive::core::RenderTextureHandle;
struct GLShaderHandle : RenderShaderHandle { ShaderProgramHandle shader; };
struct GLTextureHandle : RenderTextureHandle { GLId id = 0; };
inline std::shared_ptr<GLShaderHandle> requireGLShader(const std::shared_ptr<RenderShaderHandle>& handle) {
    auto result = std::dynamic_pointer_cast<GLShaderHandle>(handle);
    if (!result) {
        // enforce copies its lazy message only on the failing path.
        throw compat::ThrowableException(
            compat::RetainedString(std::string("Shader handle is not backed by OpenGL")),
            compat::RetainedString::fromStaticLiteral("nijilive/core/render/backends/opengl/handles.d"), 18);
    }
    return result;
}
inline std::shared_ptr<GLTextureHandle> requireGLTexture(const std::shared_ptr<RenderTextureHandle>& handle) {
    auto result = std::dynamic_pointer_cast<GLTextureHandle>(handle);
    if (!result) {
        throw compat::ThrowableException(
            compat::RetainedString(std::string("Texture handle is not backed by OpenGL")),
            compat::RetainedString::fromStaticLiteral("nijilive/core/render/backends/opengl/handles.d"), 24);
    }
    return result;
}
}
