#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/backend/gl_state_internal.hpp"
#include "nijigenerate/compat/numeric.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"
#include "nijigenerate/backend/runtime_gl_backend.hpp"
#include <runtime_state.hpp>
#include <SDL3/SDL_opengl_glext.h>
#include <bit>
#include <cstdint>

namespace nijigenerate::backend::gl {
void ngGLBackendRenderDrawData(ImDrawData* drawData) {
    const float scale = core::dpi::incGetUIScale();
    const float width = drawData->DisplaySize.x * drawData->FramebufferScale.x * scale;
    const float height = drawData->DisplaySize.y * drawData->FramebufferScale.y * scale;
    if (width <= 0 || height <= 0) return;
    using platform::ngGLBackendProcedure;
    auto& state = detail::glBackendState();
    const auto get = ngGLBackendProcedure<decltype(&glGetIntegerv)>("glGetIntegerv");
    const auto active = ngGLBackendProcedure<PFNGLACTIVETEXTUREPROC>("glActiveTexture");
    const auto isEnabled = ngGLBackendProcedure<decltype(&glIsEnabled)>("glIsEnabled");
    const auto generate = ngGLBackendProcedure<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays");
    const auto upload = ngGLBackendProcedure<PFNGLBUFFERDATAPROC>("glBufferData");
    const auto scissor = ngGLBackendProcedure<decltype(&glScissor)>("glScissor");
    const auto blend = ngGLBackendProcedure<decltype(&glBlendFunc)>("glBlendFunc");
    const auto texture = ngGLBackendProcedure<decltype(&glBindTexture)>("glBindTexture");
    const auto draw = ngGLBackendProcedure<decltype(&glDrawElements)>("glDrawElements");
    const auto destroy = ngGLBackendProcedure<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays");
    const auto program = ngGLBackendProcedure<PFNGLUSEPROGRAMPROC>("glUseProgram");
    const auto vao = ngGLBackendProcedure<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
    const auto buffer = ngGLBackendProcedure<PFNGLBINDBUFFERPROC>("glBindBuffer");
    const auto equation = ngGLBackendProcedure<PFNGLBLENDEQUATIONSEPARATEPROC>("glBlendEquationSeparate");
    const auto blendSeparate = ngGLBackendProcedure<PFNGLBLENDFUNCSEPARATEPROC>("glBlendFuncSeparate");
    const auto enable = ngGLBackendProcedure<decltype(&glEnable)>("glEnable");
    const auto disable = ngGLBackendProcedure<decltype(&glDisable)>("glDisable");
    const auto viewport = ngGLBackendProcedure<decltype(&glViewport)>("glViewport");
    GLint lastActive, lastProgram, lastTexture, lastSampler = 0, lastBuffer, lastVao;
    GLint lastViewport[4], lastScissor[4], srcRgb, dstRgb, srcAlpha, dstAlpha, equationRgb, equationAlpha;
    get(GL_ACTIVE_TEXTURE, &lastActive);
    active(GL_TEXTURE0);
    get(GL_CURRENT_PROGRAM, &lastProgram);
    get(GL_TEXTURE_BINDING_2D, &lastTexture);
    if (state.glVersion >= 330) get(GL_SAMPLER_BINDING, &lastSampler);
    get(GL_ARRAY_BUFFER_BINDING, &lastBuffer);
    get(GL_VERTEX_ARRAY_BINDING, &lastVao);
    get(GL_VIEWPORT, lastViewport);
    get(GL_SCISSOR_BOX, lastScissor);
    get(GL_BLEND_SRC_RGB, &srcRgb); get(GL_BLEND_DST_RGB, &dstRgb);
    get(GL_BLEND_SRC_ALPHA, &srcAlpha); get(GL_BLEND_DST_ALPHA, &dstAlpha);
    get(GL_BLEND_EQUATION_RGB, &equationRgb); get(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
    const auto enabledBlend = isEnabled(GL_BLEND), enabledCull = isEnabled(GL_CULL_FACE);
    const auto enabledDepth = isEnabled(GL_DEPTH_TEST), enabledScissor = isEnabled(GL_SCISSOR_TEST);
    GLuint vertexArray = 0;
    generate(1, &vertexArray);
    ngGLBackendSetupRenderState(drawData, width, height, vertexArray);
    const ImVec2 offset = drawData->DisplayPos;
    const ImVec2 clipScale(drawData->FramebufferScale.x * scale, drawData->FramebufferScale.y * scale);
    for (int index = 0; index < drawData->CmdListsCount; ++index) {
        const auto* list = drawData->CmdLists[index];
        upload(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(list->VtxBuffer.Size) * sizeof(ImDrawVert),
            list->VtxBuffer.Data, GL_STREAM_DRAW);
        upload(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(list->IdxBuffer.Size) * sizeof(ImDrawIdx),
            list->IdxBuffer.Data, GL_STREAM_DRAW);
        for (int commandIndex = 0; commandIndex < list->CmdBuffer.Size; ++commandIndex) {
            const auto* command = &list->CmdBuffer.Data[commandIndex];
            if (command->UserCallback) {
                // Preserve the reset operation through the pinned ImGui API sentinel.
                if (command->UserCallback == ImDrawCallback_ResetRenderState)
                    ngGLBackendSetupRenderState(drawData, width, height, vertexArray);
                else command->UserCallback(list, command);
            } else {
                const ImVec4 clip((command->ClipRect.x - offset.x) * clipScale.x,
                    (command->ClipRect.y - offset.y) * clipScale.y,
                    (command->ClipRect.z - offset.x) * clipScale.x,
                    (command->ClipRect.w - offset.y) * clipScale.y);
                if (clip.x < width && clip.y < height && clip.z >= 0.0f && clip.w >= 0.0f) {
                    scissor(compat::ngFloatToInt32(clip.x), compat::ngFloatToInt32(height - clip.w),
                        compat::ngFloatToInt32(clip.z - clip.x), compat::ngFloatToInt32(clip.w - clip.y));
                    const auto renderImage = runtime_gl::ngBorrowOpenGLRenderImage();
                    const auto textureId = static_cast<GLuint>(command->GetTexID());
                    blend(textureId == renderImage.textureName() ? GL_ONE : GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    texture(GL_TEXTURE_2D, textureId);
                    const auto indices = reinterpret_cast<const void*>(
                        static_cast<std::uintptr_t>(command->IdxOffset) * sizeof(ImDrawIdx));
                    const auto count = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(command->ElemCount));
                    constexpr auto type = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
                    if (state.glVersion >= 320)
                        ngGLBackendProcedure<PFNGLDRAWELEMENTSBASEVERTEXPROC>("glDrawElementsBaseVertex")(
                        GL_TRIANGLES, count, type, indices,
                        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(command->VtxOffset)));
                    else draw(GL_TRIANGLES, count, type, indices);
                }
            }
        }
    }
    destroy(1, &vertexArray);
    program(static_cast<GLuint>(lastProgram)); texture(GL_TEXTURE_2D, static_cast<GLuint>(lastTexture));
    if (state.glVersion >= 330)
        ngGLBackendProcedure<PFNGLBINDSAMPLERPROC>("glBindSampler")(0, static_cast<GLuint>(lastSampler));
    active(static_cast<GLenum>(lastActive)); vao(static_cast<GLuint>(lastVao));
    buffer(GL_ARRAY_BUFFER, static_cast<GLuint>(lastBuffer));
    equation(equationRgb, equationAlpha); blendSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
    (enabledBlend ? enable : disable)(GL_BLEND); (enabledCull ? enable : disable)(GL_CULL_FACE);
    (enabledDepth ? enable : disable)(GL_DEPTH_TEST); (enabledScissor ? enable : disable)(GL_SCISSOR_TEST);
    viewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]);
    scissor(lastScissor[0], lastScissor[1], lastScissor[2], lastScissor[3]);
}
}
