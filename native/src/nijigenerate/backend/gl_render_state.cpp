#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/backend/gl_state_internal.hpp"
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"
#include <math/mat4.hpp>
#include <SDL3/SDL_opengl_glext.h>
#include <cstddef>
#include <stdexcept>

namespace nijigenerate::backend::gl {

void ngGLBackendSetupRenderState(ImDrawData* drawData, float framebufferWidth,
    float framebufferHeight, GLuint vertexArrayObject) {
    const float uiScale = core::dpi::incGetUIScale();
    (void)uiScale;
    using platform::ngGLBackendProcedure;
    auto& state = detail::glBackendState();
    const bool bindSampler = state.glVersion >= 330;
    if (bindSampler && platform::ngGLBackendContextVersion() < 330) {
        throw std::logic_error("Stored GL sampler capability exceeds actual current context");
    }
    const auto enable = ngGLBackendProcedure<decltype(&glEnable)>("glEnable");
    const auto blendEquation = ngGLBackendProcedure<PFNGLBLENDEQUATIONPROC>("glBlendEquation");
    const auto blendFunction = ngGLBackendProcedure<decltype(&glBlendFunc)>("glBlendFunc");
    const auto disable = ngGLBackendProcedure<decltype(&glDisable)>("glDisable");
    const auto viewport = ngGLBackendProcedure<decltype(&glViewport)>("glViewport");
    const auto useProgram = ngGLBackendProcedure<PFNGLUSEPROGRAMPROC>("glUseProgram");
    const auto uniform = ngGLBackendProcedure<PFNGLUNIFORM1IPROC>("glUniform1i");
    const auto uniformMatrix = ngGLBackendProcedure<PFNGLUNIFORMMATRIX4FVPROC>("glUniformMatrix4fv");
    const auto sampler = bindSampler
        ? ngGLBackendProcedure<PFNGLBINDSAMPLERPROC>("glBindSampler") : nullptr;
    const auto vertexArray = ngGLBackendProcedure<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
    const auto buffer = ngGLBackendProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer");
    const auto enableAttribute = ngGLBackendProcedure<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray");
    const auto attribute = ngGLBackendProcedure<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer");
    enable(GL_BLEND);
    blendEquation(GL_FUNC_ADD);
    blendFunction(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    disable(GL_CULL_FACE);
    disable(GL_DEPTH_TEST);
    enable(GL_SCISSOR_TEST);
    viewport(0, 0, compat::ngFloatToInt32(framebufferWidth), compat::ngFloatToInt32(framebufferHeight));
    const float left = drawData->DisplayPos.x;
    const float right = drawData->DisplayPos.x + drawData->DisplaySize.x;
    const float bottom = drawData->DisplayPos.y + drawData->DisplaySize.y;
    const float top = drawData->DisplayPos.y;
    const auto projection = nicxlive::core::math::Mat4::orthographic(left, right, bottom, top, -1.0f, 1.0f);
    useProgram(state.shaderHandle);
    uniform(state.attributeTexture, 0);
    uniformMatrix(state.attributeProjectionMatrix, 1, GL_TRUE, projection[0]);
    if (bindSampler) sampler(0, 0);
    vertexArray(vertexArrayObject);
    buffer(GL_ARRAY_BUFFER, state.vertexBufferHandle);
    buffer(GL_ELEMENT_ARRAY_BUFFER, state.elementsHandle);
    enableAttribute(state.attributeVertexPosition);
    enableAttribute(state.attributeVertexUV);
    enableAttribute(state.attributeVertexColor);
    attribute(state.attributeVertexPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert),
        reinterpret_cast<const void*>(offsetof(ImDrawVert, pos)));
    attribute(state.attributeVertexUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert),
        reinterpret_cast<const void*>(offsetof(ImDrawVert, uv)));
    attribute(state.attributeVertexColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert),
        reinterpret_cast<const void*>(offsetof(ImDrawVert, col)));
}

} // namespace nijigenerate::backend::gl
