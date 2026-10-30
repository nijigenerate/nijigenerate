// Complete independent opengl/draw_texture.d selected OpenGL source, BSD-2-Clause.
#include "runtime_gl_draw_texture.hpp"
#include "runtime_gl_texture.hpp"
#include "runtime_gl_part.hpp"
#include "runtime_gl_drawable_buffers.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <runtime_state.hpp>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <array>
#include <cstddef>

#define NG_QUAD_GL(name, type) ::nijigenerate::platform::ngGLBackendProcedure<type>(name)
namespace nijigenerate::backend::runtime_gl {
using nicxlive::core::d_runtime::RetainedString;
namespace {
// Original __gshared storage: these three names belong to the shared render context.
GLuint quadVertexBuffer = 0;
GLuint quadUvBuffer = 0;
GLuint quadElementBuffer = 0;
void ensureQuadBuffers() {
    if (quadVertexBuffer == 0) NG_QUAD_GL("glGenBuffers", PFNGLGENBUFFERSPROC)(1, &quadVertexBuffer);
    if (quadUvBuffer == 0) NG_QUAD_GL("glGenBuffers", PFNGLGENBUFFERSPROC)(1, &quadUvBuffer);
    if (quadElementBuffer == 0) NG_QUAD_GL("glGenBuffers", PFNGLGENBUFFERSPROC)(1, &quadElementBuffer);
}
void drawQuad(const std::array<float, 8>& vertices, const std::array<float, 8>& uvs) {
    constexpr std::ptrdiff_t laneBytes = 4 * sizeof(float);
    // Resolve each actual source call after all preceding GL writes have occurred.
    NG_QUAD_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)(0);
    NG_QUAD_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ARRAY_BUFFER, quadVertexBuffer);
    NG_QUAD_GL("glBufferData", PFNGLBUFFERDATAPROC)(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    NG_QUAD_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    NG_QUAD_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)(1);
    NG_QUAD_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ARRAY_BUFFER, quadVertexBuffer);
    NG_QUAD_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)(1, 1, GL_FLOAT, GL_FALSE, 0,
        reinterpret_cast<const void*>(laneBytes));
    NG_QUAD_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)(2);
    NG_QUAD_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ARRAY_BUFFER, quadUvBuffer);
    NG_QUAD_GL("glBufferData", PFNGLBUFFERDATAPROC)(GL_ARRAY_BUFFER,
        uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);
    NG_QUAD_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)(2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    NG_QUAD_GL("glEnableVertexAttribArray", PFNGLENABLEVERTEXATTRIBARRAYPROC)(3);
    NG_QUAD_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ARRAY_BUFFER, quadUvBuffer);
    NG_QUAD_GL("glVertexAttribPointer", PFNGLVERTEXATTRIBPOINTERPROC)(3, 1, GL_FLOAT, GL_FALSE, 0,
        reinterpret_cast<const void*>(laneBytes));
    NG_QUAD_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(4);
    NG_QUAD_GL("glVertexAttrib1f", PFNGLVERTEXATTRIB1FPROC)(4, 0);
    NG_QUAD_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(5);
    NG_QUAD_GL("glVertexAttrib1f", PFNGLVERTEXATTRIB1FPROC)(5, 0);
    NG_QUAD_GL("glBindBuffer", PFNGLBINDBUFFERPROC)(GL_ELEMENT_ARRAY_BUFFER, quadElementBuffer);
    const GLushort indices[6] = {0, 1, 2, 2, 1, 3};
    NG_QUAD_GL("glBufferData", PFNGLBUFFERDATAPROC)(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    NG_QUAD_GL("glDrawElements", void(APIENTRY*)(GLenum, GLsizei, GLenum, const void*))(
        GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    NG_QUAD_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(0);
    NG_QUAD_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(1);
    NG_QUAD_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(2);
    NG_QUAD_GL("glDisableVertexAttribArray", PFNGLDISABLEVERTEXATTRIBARRAYPROC)(3);
}
void bindDrawTexture(const nicxlive::core::Texture* texture) {
    // Preserve the existing const-only native compatibility API.
    if (texture->backendId() != 0) oglBindTextureHandle(texture->backendId(), 0);
}
void bindDrawTexture(nicxlive::core::Texture* texture) { texture->bind(); }
template<typename Texture>
void drawTextureAtPart(Texture* texture,
    std::shared_ptr<nicxlive::core::nodes::Part> part) {
    if (!texture || !part) return;
    const float halfWidth = texture->width() / 2;
    const float halfHeight = texture->height() / 2;
    oglBindDrawableVao();
    ensureQuadBuffers();
    const auto model = part->immediateModelMatrix();
    const auto space = part->currentRenderSpace();
    compat::ngRequireObject(partShader)->use();
    compat::ngRequireObject(partShader)->setUniform(mvp, space.matrix * model);
    using nicxlive::core::nodes::Drawable;
    compat::ngRequireObject(partShader)->setUniform(offset, part->Drawable::getMesh().origin);
    compat::ngRequireObject(partShader)->setUniform(gopacity, part->opacity);
    compat::ngRequireObject(partShader)->setUniform(gMultColor, part->tint);
    compat::ngRequireObject(partShader)->setUniform(gScreenColor, part->screenTint);
    bindDrawTexture(texture);
    drawQuad({-halfWidth, halfWidth, -halfWidth, halfWidth, -halfHeight, -halfHeight, halfHeight, halfHeight},
        {0, 1, 0, 1, 0, 0, 1, 1});
}
template<typename Texture>
void drawTextureAtPosition(Texture* texture,
    nicxlive::core::math::Vec2 position, float opacity,
    nicxlive::core::math::Vec3 color, nicxlive::core::math::Vec3 screenColor) {
    if (!texture) return;
    const float width = static_cast<float>(texture->width());
    const float height = static_cast<float>(texture->height());
    oglDrawTextureAtRect(texture, {position.x - width * 0.5f, position.y - height * 0.5f, width, height},
        {0, 0, 1, 1}, opacity, color, screenColor);
}
template<typename Texture>
void drawTextureAtRect(Texture* texture,
    nicxlive::core::Rect area, nicxlive::core::Rect uvs, float opacity,
    nicxlive::core::math::Vec3 color, nicxlive::core::math::Vec3 screenColor,
    nicxlive::core::Shader* suppliedShader, nicxlive::core::math::Camera* camera) {
    if (!texture) return;
    oglBindDrawableVao();
    ensureQuadBuffers();
    const auto shader = suppliedShader ? suppliedShader->shared_from_this() : partShader;
    const auto defaultCamera = camera ? std::shared_ptr<nicxlive::core::math::Camera>{}
        : nicxlive::core::inGetCameraHandle();
    if (!camera) camera = defaultCamera.get();
    compat::ngRequireObject(shader)->use();
    const auto matrixLocation = shader->getUniformLocation(RetainedString::fromStaticLiteral("mvp"));
    const auto matrix = compat::ngRequireObject(camera)->matrix()
        * nicxlive::core::math::Mat4::scale({1, 1, 1});
    shader->setUniform(matrixLocation, matrix);
    const auto offsetLocation = shader->getUniformLocation(RetainedString::fromStaticLiteral("offset"));
    if (offsetLocation != -1) shader->setUniform(offsetLocation, nicxlive::core::math::Vec2{0, 0});
    const auto opacityLocation = shader->getUniformLocation(RetainedString::fromStaticLiteral("opacity"));
    shader->setUniform(opacityLocation, opacity);
    const auto colorLocation = shader->getUniformLocation(RetainedString::fromStaticLiteral("multColor"));
    shader->setUniform(colorLocation, color);
    const auto screenLocation = shader->getUniformLocation(RetainedString::fromStaticLiteral("screenColor"));
    shader->setUniform(screenLocation, screenColor);
    bindDrawTexture(texture);
    drawQuad({area.x, area.x + area.w, area.x, area.x + area.w,
        area.y, area.y, area.y + area.h, area.y + area.h},
        {uvs.x, uvs.w, uvs.x, uvs.w, uvs.y, uvs.y, uvs.h, uvs.h});
}
}
void oglDrawTextureAtPart(const nicxlive::core::Texture* texture,
    const std::shared_ptr<nicxlive::core::nodes::Part>& part) { drawTextureAtPart(texture, part); }
void oglDrawTextureAtPart(nicxlive::core::Texture* texture,
    const std::shared_ptr<nicxlive::core::nodes::Part>& part) { drawTextureAtPart(texture, part); }
void oglDrawTextureAtPosition(const nicxlive::core::Texture* texture, nicxlive::core::math::Vec2 position,
    float opacity, nicxlive::core::math::Vec3 color, nicxlive::core::math::Vec3 screen) {
    drawTextureAtPosition(texture, position, opacity, color, screen);
}
void oglDrawTextureAtPosition(nicxlive::core::Texture* texture, nicxlive::core::math::Vec2 position,
    float opacity, nicxlive::core::math::Vec3 color, nicxlive::core::math::Vec3 screen) {
    drawTextureAtPosition(texture, position, opacity, color, screen);
}
void oglDrawTextureAtRect(const nicxlive::core::Texture* texture, nicxlive::core::Rect area,
    nicxlive::core::Rect uvs, float opacity, nicxlive::core::math::Vec3 color,
    nicxlive::core::math::Vec3 screen, nicxlive::core::Shader* shader, nicxlive::core::math::Camera* camera) {
    drawTextureAtRect(texture, area, uvs, opacity, color, screen, shader, camera);
}
void oglDrawTextureAtRect(nicxlive::core::Texture* texture, nicxlive::core::Rect area,
    nicxlive::core::Rect uvs, float opacity, nicxlive::core::math::Vec3 color,
    nicxlive::core::math::Vec3 screen, nicxlive::core::Shader* shader, nicxlive::core::math::Camera* camera) {
    drawTextureAtRect(texture, area, uvs, opacity, color, screen, shader, camera);
}

// Source class arguments remain owned throughout the same actual private GL bodies.
void oglDrawTextureAtPart(std::shared_ptr<nicxlive::core::Texture> texture,
    std::shared_ptr<nicxlive::core::nodes::Part> part) {
    drawTextureAtPart(texture.get(), part);
}
void oglDrawTextureAtPosition(std::shared_ptr<nicxlive::core::Texture> texture,
    nicxlive::core::math::Vec2 position, float opacity,
    nicxlive::core::math::Vec3 color, nicxlive::core::math::Vec3 screen) {
    drawTextureAtPosition(texture.get(), position, opacity, color, screen);
}
void oglDrawTextureAtRect(std::shared_ptr<nicxlive::core::Texture> texture, nicxlive::core::Rect area,
    nicxlive::core::Rect uvs, float opacity, nicxlive::core::math::Vec3 color,
    nicxlive::core::math::Vec3 screen, std::shared_ptr<nicxlive::core::Shader> shader,
    std::shared_ptr<nicxlive::core::math::Camera> camera) {
    drawTextureAtRect(texture.get(), area, uvs, opacity, color, screen, shader.get(), camera.get());
}
}
#undef NG_QUAD_GL
