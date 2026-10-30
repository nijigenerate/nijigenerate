// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "renderer.hpp"
#include <shader.hpp>
#include "mesheditor/node_stable_sort.hpp"
#include <nijigenerate/platform/gl_backend_procedure.hpp>
#include <nijigenerate/backend/runtime_gl_blend.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nodes/deformable.hpp>
#include <nodes/composite.hpp>
#include <nodes/drawable.hpp>
#include <texture.hpp>
#include <runtime_state.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <bit>
#include <new>
#include <source_location>

#define NG_DEPTH_GL(name, type) ::nijigenerate::platform::ngGLBackendProcedure<type>(name)
namespace nijigenerate::viewport::depth {
namespace gl = backend::runtime_gl;
namespace {
using Vec2 = nicxlive::core::math::Vec2;
using Mat4 = nicxlive::core::math::Mat4;
int glCount(std::size_t size) { return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(size)); }
void restoreEnabled(GLenum capability, GLboolean enabled) { if (enabled) glEnable(capability); else glDisable(capability); }
void normalBlend() { gl::oglSetBlendMode(gl::BlendMode::Normal); }
template<std::size_t V, std::size_t F>
std::shared_ptr<nicxlive::core::Shader> allocateDepthShader(const char (&vertex)[V], const char (&fragment)[F]) {
    return std::allocate_shared<nicxlive::core::Shader>(
        nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::Shader>{},
        compat::RetainedString::fromStaticLiteral(vertex),
        compat::RetainedString::fromStaticLiteral(fragment));
}
constexpr const char vertexSource[] = R"glsl(
#version 330
layout(location = 0) in vec2 vert;
layout(location = 1) in vec2 uv;
out vec2 fragUv;
uniform mat4 mvp;
void main() {
    fragUv = uv;
    gl_Position = mvp * vec4(vert.xy, 0.0, 1.0);
}
)glsl";
constexpr const char fragmentSource[] = R"glsl(
#version 330
in vec2 fragUv;
out vec4 color;
uniform sampler2D tex;
void main() {
    color = texture(tex, fragUv);
}
)glsl";
constexpr const char solidVertexSource[] = R"glsl(
#version 330
layout(location = 0) in vec2 vert;
uniform mat4 mvp;
void main() {
    gl_Position = mvp * vec4(vert.xy, 0.0, 1.0);
}
)glsl";
constexpr const char solidFragmentSource[] = R"glsl(
#version 330
out vec4 color;
uniform vec4 meshColor;
void main() {
    color = meshColor;
}
)glsl";
}
void DepthTextureMeshRenderer::ensureBuffers() {
    if (vao) return;
    NG_DEPTH_GL("glGenVertexArrays", PFNGLGENVERTEXARRAYSPROC)(1, &vao);
    NG_DEPTH_GL("glGenBuffers", PFNGLGENBUFFERSPROC)(1, &vbo);
    NG_DEPTH_GL("glGenBuffers", PFNGLGENBUFFERSPROC)(1, &ibo);
}
void DepthTextureMeshRenderer::ensure() {
    ensureBuffers(); if (shader) return;
    shader = allocateDepthShader(vertexSource, fragmentSource);
    mvpUniform = shader->getUniformLocation(compat::RetainedString::fromStaticLiteral("mvp"));
    textureUniform = shader->getUniformLocation(compat::RetainedString::fromStaticLiteral("tex"));
    loggedEnsure = true;
}
void DepthTextureMeshRenderer::ensureSolid() {
    ensureBuffers(); if (solidShader) return;
    solidShader = allocateDepthShader(solidVertexSource, solidFragmentSource);
    solidMvpUniform = solidShader->getUniformLocation(compat::RetainedString::fromStaticLiteral("mvp"));
    solidColorUniform = solidShader->getUniformLocation(compat::RetainedString::fromStaticLiteral("meshColor"));
}
void DepthTextureMeshRenderer::drawSolid(compat::Slice<Vec2> positions, compat::Slice<std::uint16_t> indices,
    std::shared_ptr<nicxlive::core::math::Camera> camera, nicxlive::core::math::Vec4 color) {
    const auto keepAlive = weak_from_this().lock();
    if (positions.empty() || indices.empty()) { loggedDrawSkip = true; return; }
    ensureSolid();
    const auto depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE), blend = glIsEnabled(GL_BLEND);
    NG_DEPTH_GL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)(vao);
    NG_DEPTH_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ARRAY_BUFFER, vbo);
    NG_DEPTH_GL("glBufferData", PFNGLBUFFERDATAPROC)(GL_ARRAY_BUFFER, positions.size() * sizeof(Vec2), positions.data(), GL_DYNAMIC_DRAW);
    NG_DEPTH_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ELEMENT_ARRAY_BUFFER, ibo);
    NG_DEPTH_GL("glBufferData", PFNGLBUFFERDATAPROC)(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint16_t), indices.data(), GL_DYNAMIC_DRAW);
    glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); normalBlend();
    compat::ngRequireObject(solidShader)->use();
    const auto matrixReceiver=compat::ngRequireObject(solidShader);
    const auto matrixLocation=solidMvpUniform;
    const auto matrix=compat::ngRequireObject(camera)->matrix();
    matrixReceiver->setUniform(matrixLocation,matrix);
    compat::ngRequireObject(solidShader)->setUniform(solidColorUniform,color);
    NG_DEPTH_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)(0);
    NG_DEPTH_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), nullptr);
    glDrawElements(GL_TRIANGLES, glCount(indices.size()), GL_UNSIGNED_SHORT, nullptr);
    const auto error = glGetError();
    if (error != GL_NO_ERROR && !loggedGlError) loggedGlError = true;
    else if (error == GL_NO_ERROR && !loggedDrawSuccess) loggedDrawSuccess = true;
    NG_DEPTH_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(0);
    restoreEnabled(GL_BLEND, blend); restoreEnabled(GL_DEPTH_TEST, depth); restoreEnabled(GL_CULL_FACE, cull);
    NG_DEPTH_GL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)(0);
}
void DepthTextureMeshRenderer::draw(std::shared_ptr<nicxlive::core::Texture> texture, compat::Slice<Vec2> positions,
    compat::Slice<Vec2> uvs, compat::Slice<std::uint16_t> indices, std::shared_ptr<nicxlive::core::math::Camera> camera) {
    const auto keepAlive = weak_from_this().lock();
    if (!texture || positions.empty() || positions.size() != uvs.size() || indices.empty()) { loggedDrawSkip = true; return; }
    ensure(); const auto depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE), blend = glIsEnabled(GL_BLEND);
    compat::Slice<float> data; data.resize(positions.size() * 4);
    for (std::size_t i = 0; i < positions.size(); ++i) {
        data[i * 4] = positions[i].x; data[i * 4 + 1] = positions[i].y; data[i * 4 + 2] = uvs[i].x; data[i * 4 + 3] = uvs[i].y;
    }
    NG_DEPTH_GL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)(vao);
    NG_DEPTH_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ARRAY_BUFFER, vbo);
    NG_DEPTH_GL("glBufferData", PFNGLBUFFERDATAPROC)(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    NG_DEPTH_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ELEMENT_ARRAY_BUFFER, ibo);
    NG_DEPTH_GL("glBufferData", PFNGLBUFFERDATAPROC)(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint16_t), indices.data(), GL_DYNAMIC_DRAW);
    glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); normalBlend();
    compat::ngRequireObject(shader)->use();
    const auto matrixReceiver=compat::ngRequireObject(shader);
    const auto matrixLocation=mvpUniform;
    const auto matrix=compat::ngRequireObject(camera)->matrix();
    matrixReceiver->setUniform(matrixLocation,matrix);
    compat::ngRequireObject(shader)->setUniform(textureUniform,0);
    compat::ngRequireObject(texture)->bind(0);
    NG_DEPTH_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)(0);
    NG_DEPTH_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)(1);
    NG_DEPTH_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    NG_DEPTH_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
        reinterpret_cast<const void*>(sizeof(float) * 2));
    glDrawElements(GL_TRIANGLES, glCount(indices.size()), GL_UNSIGNED_SHORT, nullptr); const auto error = glGetError();
    if (error != GL_NO_ERROR && !loggedGlError) loggedGlError = true;
    else if (error == GL_NO_ERROR && !loggedDrawSuccess) loggedDrawSuccess = true;
    NG_DEPTH_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(0);
    NG_DEPTH_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(1);
    restoreEnabled(GL_BLEND, blend); restoreEnabled(GL_DEPTH_TEST, depth); restoreEnabled(GL_CULL_FACE, cull);
    NG_DEPTH_GL("glBindVertexArray", PFNGLBINDVERTEXARRAYPROC)(0);
}
bool DepthTargetOffscreenTextureRenderer::rebuildTexture(Target target) {
    const auto vertices = compat::ngRequireObject(target)->getVertices().toArray(); if (vertices.empty()) return false;
    minPoint = maxPoint = vertices[0];
    for (std::size_t i = 1; i < vertices.size(); ++i) {
        const auto value = vertices[i];
        minPoint.x = std::min(minPoint.x, value.x); minPoint.y = std::min(minPoint.y, value.y);
        maxPoint.x = std::max(maxPoint.x, value.x); maxPoint.y = std::max(maxPoint.y, value.y);
    }
    sourceSize = {std::max(1.f, maxPoint.x - minPoint.x), std::max(1.f, maxPoint.y - minPoint.y)};
    GLint maxSize=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (!ngDepthTargetOffscreenTextureSize(minPoint, maxPoint, maxSize, textureWidth, textureHeight, texturePixelScale)) return false;
    if (texture && (compat::ngRequireObject(texture)->width() != textureWidth
            || compat::ngRequireObject(texture)->height() != textureHeight)) {
        compat::ngRequireObject(texture)->dispose();
        texture.reset();
    }
    if (!texture) texture = nicxlive::core::Texture::createSource(textureWidth, textureHeight, 4, false, false);
    if (!textureFbo) NG_DEPTH_GL("glGenFramebuffers", PFNGLGENFRAMEBUFFERSPROC)(1, &textureFbo);
    return texture && textureFbo != 0;
}
compat::Slice<DepthTargetOffscreenTextureRenderer::Node> DepthTargetOffscreenTextureRenderer::drawableChildren(Target target) {
    compat::Slice<Node> parts; if (!target) return parts;
    const auto visit = [&](const auto& recurse, Node node) -> void {
        if (compat::ngRequireObject(node)->coverOthers()) {
            const auto children = node->children;
            for (auto child : children) recurse(recurse, child);
        }
        if (auto composite = std::dynamic_pointer_cast<nicxlive::core::nodes::Composite>(node)) {
            if (composite->propagateMeshGroup) parts.append(composite);
        } else if (auto drawable = std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node)) {
            parts.append(drawable);
            const auto children = node->children;
            for (auto child : children) recurse(recurse, child);
        }
    };
    visit(visit, target);
    mesheditor::node::detail::phobosStableNodeSort(parts, [](Node a, Node b) {
        const auto left = compat::ngRequireObject(a)->zSort();
        const auto right = compat::ngRequireObject(b)->zSort();
        return left > right;
    });
    return parts;
}
DepthTargetOffscreenTextureRenderer::~DepthTargetOffscreenTextureRenderer() noexcept(false) { dispose(); }
void DepthTargetOffscreenTextureRenderer::dispose() {
    const auto keepAlive=weak_from_this().lock();
    if (texture) { compat::ngRequireObject(texture)->dispose(); texture.reset(); }
    if (textureFbo) { NG_DEPTH_GL("glDeleteFramebuffers", PFNGLDELETEFRAMEBUFFERSPROC)(1, &textureFbo); textureFbo = 0; }
}
std::shared_ptr<nicxlive::core::Texture> DepthTargetOffscreenTextureRenderer::render(Target target) {
    const auto keepAlive = weak_from_this().lock(); if (!target || !rebuildTexture(target)) return {};
    GLint drawFbo=0, readFbo=0, viewport[4]{}, equationRgb=0, equationAlpha=0;
    GLint srcRgb=0, dstRgb=0, srcAlpha=0, dstAlpha=0, maxBuffers=0;
    const auto nan=std::numeric_limits<GLfloat>::quiet_NaN();
    GLfloat clearColor[4]{nan,nan,nan,nan};
    const auto depth = glIsEnabled(GL_DEPTH_TEST), cull = glIsEnabled(GL_CULL_FACE), blend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
    glGetIntegerv(GL_VIEWPORT, viewport); glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRgb); glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
    glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb); glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor); glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxBuffers);
    compat::Slice<GLenum> buffers; buffers.resize(maxBuffers > 0 ? std::size_t(maxBuffers) : 1);
    for (std::size_t i = 0; i < buffers.size(); ++i) { GLint buffer=0; glGetIntegerv(GLenum(GL_DRAW_BUFFER0 + i), &buffer); buffers[i] = GLenum(buffer); }
    while (buffers.size() > 1 && buffers[buffers.size() - 1] == GL_NONE) buffers.resize(buffers.size() - 1);
    NG_DEPTH_GL("glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC)(GL_FRAMEBUFFER, textureFbo);
    const auto textureId=compat::ngRequireObject(texture)->getTextureId();
    NG_DEPTH_GL("glFramebufferTexture2D", PFNGLFRAMEBUFFERTEXTURE2DPROC)(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
    const GLenum color = GL_COLOR_ATTACHMENT0; NG_DEPTH_GL("glDrawBuffers", PFNGLDRAWBUFFERSPROC)(1, &color);
    if (NG_DEPTH_GL("glCheckFramebufferStatus", PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        NG_DEPTH_GL("glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC)(GL_DRAW_FRAMEBUFFER, GLuint(drawFbo));
        NG_DEPTH_GL("glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC)(GL_READ_FRAMEBUFFER, GLuint(readFbo));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]); return {};
    }
    glViewport(0, 0, textureWidth, textureHeight); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND);
    NG_DEPTH_GL("glBlendEquation", PFNGLBLENDEQUATIONPROC)(GL_FUNC_ADD); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    nicxlive::core::inPushViewport(textureWidth, textureHeight);
    const auto camera = nicxlive::core::inGetCameraHandle();
    compat::ngRequireObject(camera)->scale = texturePixelScale;
    camera->position = {-minPoint.x - sourceSize.x * .5f, -minPoint.y - sourceSize.y * .5f};
    camera->rotation = 0;
    auto transform = std::allocate_shared<Mat4>(nicxlive::core::d_runtime::SourceAllocator<Mat4>{},
        core::math::source_matrix::inverse(target->transform().matrix()));
    target->setOneTimeTransform(transform);
    std::shared_ptr<nicxlive::core::Texture> result;
    compat::ngRunWithScopeExit([&] {
        for (auto part : drawableChildren(target)) compat::ngRequireObject(part)->drawOne();
        result=texture;
    },[&] {
        target->setOneTimeTransform({}); nicxlive::core::inPopViewport();
        NG_DEPTH_GL("glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC)(GL_DRAW_FRAMEBUFFER, GLuint(drawFbo));
        NG_DEPTH_GL("glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC)(GL_READ_FRAMEBUFFER, GLuint(readFbo));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        restoreEnabled(GL_DEPTH_TEST, depth); restoreEnabled(GL_CULL_FACE, cull); restoreEnabled(GL_BLEND, blend);
        NG_DEPTH_GL("glBlendEquationSeparate", PFNGLBLENDEQUATIONSEPARATEPROC)(GLenum(equationRgb), GLenum(equationAlpha));
        NG_DEPTH_GL("glBlendFuncSeparate", PFNGLBLENDFUNCSEPARATEPROC)(GLenum(srcRgb), GLenum(dstRgb), GLenum(srcAlpha), GLenum(dstAlpha));
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        NG_DEPTH_GL("glDrawBuffers", PFNGLDRAWBUFFERSPROC)(glCount(buffers.size()), buffers.data());
    });
    return result;
}
}
#undef NG_DEPTH_GL
