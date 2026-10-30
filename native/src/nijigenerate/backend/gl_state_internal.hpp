#pragma once

#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/platform/gl_backend_policy.hpp"
#include <array>
#include <cstdint>

namespace nijigenerate::backend::gl::detail {

// Canonical thread-local state for the backend source module.
// Numeric GL identifiers do not create or own resources by being stored.
struct GLBackendState {
    GLuint glVersion = 0;
    std::array<char, 32> glslVersionString = platform::ngGLBackendInitialGLSLVersion();
    GLuint fontTexture = 0;
    GLuint shaderHandle = 0;
    GLuint vertexShaderHandle = 0;
    GLuint fragmentShaderHandle = 0;
    GLint attributeTexture = 0;
    GLint attributeProjectionMatrix = 0;
    GLuint attributeVertexPosition = 0;
    GLuint attributeVertexUV = 0;
    GLuint attributeVertexColor = 0;
    std::uint32_t vertexBufferHandle = 0;
    std::uint32_t elementsHandle = 0;
};

GLBackendState& glBackendState();

} // namespace nijigenerate::backend::gl::detail
