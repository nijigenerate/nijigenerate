// Port of the independent opengl/mask.d production renderer.
#include "runtime_gl_mask.hpp"
#include "runtime_gl_part.hpp"
#include "runtime_gl_drawable_buffers.hpp"
#include "runtime_gl_buffer_sync.hpp"
#include "runtime_gl_mask_shaders.hpp"
#include <shader.hpp>
#include <compat/native_access_fault.hpp>
#include <compat/static_error.hpp>
#include <compat/switch_error.hpp>
#include <cstdint>
#include "runtime_gl_geometry_calls.hpp"
namespace nijigenerate::backend::runtime_gl {
#ifndef NICXLIVE_SOURCE_NO_RENDER
namespace {
std::shared_ptr<nicxlive::core::Shader> shader;
int origin{}, matrix{};
bool initialized{};
void attribute(unsigned location, unsigned buffer, std::uintptr_t byteOffset) {
    glEnableVertexAttribArray(location); glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(location, 1, GL_FLOAT, GL_FALSE, 0,
        reinterpret_cast<const void*>(byteOffset));
}
}
void oglInitMaskBackend() {
    if (initialized) return;
    initialized = true;
    shader = std::make_shared<nicxlive::core::Shader>(nicxlive::core::shaderAsset<
        mask_shaders::vertexSource, mask_shaders::fragmentSource>());
    origin = nicxlive::core::d_runtime::ngRequireObject(shader)->getUniformLocation(
        nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("offset"));
    matrix = nicxlive::core::d_runtime::ngRequireObject(shader)->getUniformLocation(
        nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("mvp"));
}
void oglBeginMask(bool useStencil) {
    glEnable(GL_STENCIL_TEST); glClearStencil(useStencil ? 0 : 1); glClear(GL_STENCIL_BUFFER_BIT);
    glStencilMask(0xff); glStencilFunc(GL_ALWAYS, useStencil ? 0 : 1, 0xff); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}
void oglEndMask() { glStencilMask(0xff); glStencilFunc(GL_ALWAYS, 1, 0xff); glDisable(GL_STENCIL_TEST); }
void oglBeginMaskContent() { glStencilFunc(GL_EQUAL, 1, 0xff); glStencilMask(0); }
void oglExecuteMaskPacket(const nicxlive::core::nodes::MaskDrawPacket& packet) {
    oglInitMaskBackend(); if (!packet.indexCount) return;
    oglBindDrawableVao();
    nicxlive::core::d_runtime::ngRequireObject(shader)->use();
    nicxlive::core::d_runtime::ngRequireObject(shader)->setUniform(origin, packet.origin);
    nicxlive::core::d_runtime::ngRequireObject(shader)->setUniform(matrix, packet.mvp);
    if (!packet.vertexCount || !packet.vertexAtlasStride || !packet.deformAtlasStride) return;
    const auto vertex = oglGetSharedVertexBuffer(), deform = oglGetSharedDeformBuffer();
    if (!vertex || !deform) return;
    // All Source offsets are captured before the first attribute GL call.
    // Unsigned arithmetic preserves the ptrdiff_t bit pattern without signed overflow.
    const auto vertexOffset = static_cast<std::uintptr_t>(packet.vertexOffset) * sizeof(float);
    const auto vertexStride = static_cast<std::uintptr_t>(packet.vertexAtlasStride) * sizeof(float);
    const auto vertexLane1 = vertexStride + vertexOffset;
    const auto deformOffset = static_cast<std::uintptr_t>(packet.deformOffset) * sizeof(float);
    const auto deformStride = static_cast<std::uintptr_t>(packet.deformAtlasStride) * sizeof(float);
    const auto deformLane1 = deformStride + deformOffset;
    attribute(0, vertex, vertexOffset);
    attribute(1, vertex, vertexLane1);
    attribute(2, deform, deformOffset);
    attribute(3, deform, deformLane1);
    oglDrawDrawableElements(packet.indexBuffer, packet.indexCount);
    markBufferInUse(vertex); markBufferInUse(deform);
    for (unsigned i = 0; i < 4; ++i) glDisableVertexAttribArray(i);
}
void oglExecuteMaskApplyPacket(const nicxlive::core::RenderBackend::MaskApplyPacket& packet) {
    oglInitMaskBackend(); glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE); glStencilFunc(GL_ALWAYS, packet.isDodge ? 0 : 1, 0xff);
    glStencilMask(0xff);
    using Kind = nicxlive::core::RenderBackend::MaskDrawableKind;
    switch (packet.kind) {
    case Kind::Part: oglExecutePartPacket(packet.partPacket); break;
    case Kind::Mask: oglExecuteMaskPacket(packet.maskPacket); break;
    default:
        throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::SwitchError>(
            "No appropriate switch clause found", "nijilive/core/render/backends/opengl/mask.d", 115);
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}
#else
void oglInitMaskBackend() {}
void oglBeginMask(bool) {}
void oglEndMask() {}
void oglBeginMaskContent() {}
void oglExecuteMaskPacket(const nicxlive::core::nodes::MaskDrawPacket&) {}
void oglExecuteMaskApplyPacket(const nicxlive::core::RenderBackend::MaskApplyPacket&) {}
#endif
}
