// Independent OpenGL rendering backend adapter; no fallback no-op renderer.
#include "runtime_gl_backend.hpp"
#include "runtime_gl_runtime.hpp"
#include "runtime_gl_drawable_buffers.hpp"
#include "runtime_gl_part.hpp"
#include "runtime_gl_mask.hpp"
#include "runtime_gl_blend.hpp"
#include "runtime_gl_dynamic_composite.hpp"
#include "runtime_gl_draw_texture.hpp"
#include "runtime_gl_diff_collect.hpp"
#include "runtime_gl_queue.hpp"
#include "debug_gl.hpp"
#include <nijigenerate/core/dbg.hpp>
#include <render/profiler.hpp>
#include <runtime_state.hpp>
#include <math/camera.hpp>
#include <SDL3/SDL_video.h>
#include <nijigenerate/platform/imgui_sdl3_adapter.hpp>
#include <stdexcept>
#include <span>

namespace nijigenerate::backend::runtime_gl {
using nicxlive::core::render::profileScope;
std::unique_ptr<nicxlive::core::RenderCommandEmitter> OpenGLRenderBackend::createCommandEmitter() {
    return std::make_unique<RenderQueue>();
}
void OpenGLRenderBackend::initializeRenderer() {
    oglInitRenderer();
    oglInitDrawableBackend();
    oglInitPartBackendResources();
    oglInitMaskBackend();
}
void OpenGLRenderBackend::resizeViewportTargets(int w, int h) { oglResizeViewport(w, h); }
void OpenGLRenderBackend::dumpViewport(std::vector<std::uint8_t>& data, int w, int h) { oglDumpViewport(data, w, h); }
void OpenGLRenderBackend::dumpViewport(nicxlive::core::common::SharedArray<std::uint8_t>& data, int w, int h) {
    oglDumpViewport(data, w, h);
}
void OpenGLRenderBackend::beginScene() { auto profile = profileScope("BeginScene"); oglBeginScene(); }
void OpenGLRenderBackend::endScene() {
    auto profile = profileScope("EndScene"); oglEndScene();
    nicxlive::core::render::renderProfilerFrameCompleted();
}
void OpenGLRenderBackend::postProcessScene() { auto profile = profileScope("PostProcess"); oglPostProcessScene(); }
void OpenGLRenderBackend::initializeDrawableResources() { oglInitDrawableBackend(); }
void OpenGLRenderBackend::bindDrawableVao() { oglBindDrawableVao(); }
void OpenGLRenderBackend::createDrawableBuffers(Handle& h) { oglCreateDrawableBuffers(h); }
void OpenGLRenderBackend::disposeDrawableBuffers(Handle h) { oglDisposeDrawableBuffers(h); }
void OpenGLRenderBackend::uploadDrawableIndices(Handle h, const std::vector<std::uint16_t>& indices) {
    oglUploadDrawableIndices(h, indices);
}
void OpenGLRenderBackend::uploadDrawableIndices(Handle h,
        nicxlive::core::common::SharedArray<std::uint16_t> indices) {
    oglUploadDrawableIndices(h, indices);
}
void OpenGLRenderBackend::drawDrawableElements(Handle h, std::size_t count) { oglDrawDrawableElements(h, count); }
void OpenGLRenderBackend::uploadSharedVertexBuffer(const Vec2Array& values) { auto profile = profileScope("UploadVertices"); oglUploadSharedVertexBuffer(values); }
void OpenGLRenderBackend::uploadSharedUvBuffer(const Vec2Array& values) { auto profile = profileScope("UploadUV"); oglUploadSharedUvBuffer(values); }
void OpenGLRenderBackend::uploadSharedDeformBuffer(const Vec2Array& values) { auto profile = profileScope("UploadDeformAtlas"); oglUploadSharedDeformBuffer(values); }
bool OpenGLRenderBackend::supportsAdvancedBlend() { return oglSupportsAdvancedBlend(); }
bool OpenGLRenderBackend::supportsAdvancedBlendCoherent() { return oglSupportsAdvancedBlendCoherent(); }
void OpenGLRenderBackend::setAdvancedBlendCoherent(bool enabled) { oglSetAdvancedBlendCoherent(enabled); }
void OpenGLRenderBackend::setLegacyBlendMode(BlendMode mode) { oglSetLegacyBlendMode(mode); }
void OpenGLRenderBackend::setAdvancedBlendEquation(BlendMode mode) { oglSetAdvancedBlendEquation(mode); }
void OpenGLRenderBackend::issueBlendBarrier() { oglIssueBlendBarrier(); }
void OpenGLRenderBackend::initDebugRenderer() { debug_gl::ngOpenGLDebugProvider()->initialize(); }
void OpenGLRenderBackend::setDebugPointSize(float value) { debug_gl::ngOpenGLDebugProvider()->pointSize(value); }
void OpenGLRenderBackend::setDebugLineWidth(float value) { debug_gl::ngOpenGLDebugProvider()->lineWidth(value); }
void OpenGLRenderBackend::uploadDebugBuffer(const std::vector<Vec3>& points, const std::vector<std::uint16_t>& indices) {
    nicxlive::core::math::Vec3Array values;
    values.resize(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) values[i].set(points[i]);
    debug_gl::ngOpenGLDebugProvider()->upload(values, indices);
}
void OpenGLRenderBackend::uploadDebugBuffer(nicxlive::core::math::Vec3Array points,
        nicxlive::core::common::SharedArray<std::uint16_t> indices) {
    // The same actual GL provider consumes these retained Source arrays synchronously.
    const auto provider = debug_gl::ngOpenGLDebugProvider();
    provider->upload(points, std::span<const std::uint16_t>(indices.data(), indices.size()));
}
void OpenGLRenderBackend::setDebugExternalBuffer(Handle vbo, Handle ibo, int count) {
    debug_gl::ngOpenGLDebugProvider()->external(vbo, ibo, count);
}
void OpenGLRenderBackend::drawDebugPoints(const Vec4& color, const Mat4& mvp) {
    debug_gl::ngOpenGLDebugProvider()->points(color, mvp);
}
void OpenGLRenderBackend::drawDebugLines(const Vec4& color, const Mat4& mvp) {
    debug_gl::ngOpenGLDebugProvider()->lines(color, mvp);
}
void OpenGLRenderBackend::drawPartPacket(const PartPacket& packet) {
    auto profile = profileScope("DrawPart"); oglDrawPartPacket(packet);
}
void OpenGLRenderBackend::beginDynamicComposite(Pass& pass) { oglBeginDynamicComposite(pass); }
void OpenGLRenderBackend::endDynamicComposite(Pass& pass) { oglEndDynamicComposite(pass); }
void OpenGLRenderBackend::destroyDynamicComposite(const std::shared_ptr<Surface>& surface) { oglDestroyDynamicComposite(surface); }
void OpenGLRenderBackend::beginMask(bool stencil) { auto profile = profileScope("BeginMask"); oglBeginMask(stencil); }
void OpenGLRenderBackend::applyMask(const MaskApplyPacket& packet) { auto profile = profileScope("ApplyMask"); oglExecuteMaskApplyPacket(packet); }
void OpenGLRenderBackend::beginMaskContent() { auto profile = profileScope("BeginMaskContent"); oglBeginMaskContent(); }
void OpenGLRenderBackend::endMask() { auto profile = profileScope("EndMask"); oglEndMask(); }
void OpenGLRenderBackend::drawTextureAtPart(const Texture& texture, const std::shared_ptr<Part>& part) { oglDrawTextureAtPart(&texture, part); }
void OpenGLRenderBackend::drawTextureAtPosition(const Texture& texture, const Vec2& position, float opacity, const Vec3& color, const Vec3& screen) {
    oglDrawTextureAtPosition(&texture, position, opacity, color, screen);
}
void OpenGLRenderBackend::drawTextureAtRect(const Texture& texture, const Rect& area, const Rect& uv, float opacity, const Vec3& color, const Vec3& screen, void* shader, void* camera) {
    oglDrawTextureAtRect(&texture, area, uv, opacity, color, screen, static_cast<nicxlive::core::Shader*>(shader), static_cast<nicxlive::core::math::Camera*>(camera));
}
void OpenGLRenderBackend::drawTextureAtPart(Texture& texture, const std::shared_ptr<Part>& part) {
    oglDrawTextureAtPart(&texture, part);
}
void OpenGLRenderBackend::drawTextureAtPosition(Texture& texture, const Vec2& position, float opacity,
    const Vec3& color, const Vec3& screen) {
    oglDrawTextureAtPosition(&texture, position, opacity, color, screen);
}
void OpenGLRenderBackend::drawTextureAtRect(Texture& texture, const Rect& area, const Rect& uv, float opacity,
    const Vec3& color, const Vec3& screen, void* shader, void* camera) {
    oglDrawTextureAtRect(&texture, area, uv, opacity, color, screen,
        static_cast<nicxlive::core::Shader*>(shader), static_cast<nicxlive::core::math::Camera*>(camera));
}
OpenGLRenderBackend::Handle OpenGLRenderBackend::framebufferHandle() { return oglGetFramebuffer(); }
void OpenGLRenderBackend::drawTextureAtPart(std::shared_ptr<Texture> texture, std::shared_ptr<Part> part) {
    oglDrawTextureAtPart(texture, part);
}
void OpenGLRenderBackend::drawTextureAtPosition(std::shared_ptr<Texture> texture, Vec2 position, float opacity,
    Vec3 color, Vec3 screen) {
    oglDrawTextureAtPosition(texture, position, opacity, color, screen);
}
void OpenGLRenderBackend::drawTextureAtRect(std::shared_ptr<Texture> texture, Rect area, Rect uv, float opacity,
    Vec3 color, Vec3 screen, std::shared_ptr<nicxlive::core::Shader> shader,
    std::shared_ptr<nicxlive::core::math::Camera> camera) {
    // Keep all real Source owners and copied values alive across the entire GL call.
    oglDrawTextureAtRect(texture, area, uv, opacity, color, screen, shader, camera);
}
void OpenGLRenderBackend::drawTextureAtPart(const Texture* texture, const std::shared_ptr<Part>& part) {
    oglDrawTextureAtPart(texture, part);
}
void OpenGLRenderBackend::drawTextureAtPosition(const Texture* texture, Vec2 position, float opacity, Vec3 color, Vec3 screen) {
    oglDrawTextureAtPosition(texture, position, opacity, color, screen);
}
void OpenGLRenderBackend::drawTextureAtRect(const Texture* texture, Rect area, Rect uv, float opacity, Vec3 color, Vec3 screen,
    nicxlive::core::Shader* shader, nicxlive::core::math::Camera* camera) {
    oglDrawTextureAtRect(texture, area, uv, opacity, color, screen, shader, camera);
}
OpenGLRenderBackend::Handle OpenGLRenderBackend::renderImageHandle() { return oglGetRenderImage(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::mainAlbedoHandle() { return oglGetMainAlbedo(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::mainEmissiveHandle() { return oglGetMainEmissive(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::mainBumpHandle() { return oglGetMainBump(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::compositeImageHandle() { return oglGetCompositeImage(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::compositeFramebufferHandle() { return oglGetCompositeFramebuffer(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::compositeEmissiveHandle() { return oglGetCompositeEmissive(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::compositeBumpHandle() { return oglGetCompositeBump(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::blendFramebufferHandle() { return oglGetBlendFramebuffer(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::blendAlbedoHandle() { return oglGetBlendAlbedo(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::blendEmissiveHandle() { return oglGetBlendEmissive(); }
OpenGLRenderBackend::Handle OpenGLRenderBackend::blendBumpHandle() { return oglGetBlendBump(); }
void OpenGLRenderBackend::addBasicLightingPostProcess() { oglAddBasicLightingPostProcess(); }
void OpenGLRenderBackend::setDifferenceAggregationEnabled(bool value) { oglSetDifferenceAggregationEnabled(value); }
bool OpenGLRenderBackend::isDifferenceAggregationEnabled() { return oglIsDifferenceAggregationEnabled(); }
void OpenGLRenderBackend::setDifferenceAggregationRegion(const Region& region) { oglSetDifferenceAggregationRegion(region); }
OpenGLRenderBackend::Region OpenGLRenderBackend::getDifferenceAggregationRegion() { return oglGetDifferenceAggregationRegion(); }
bool OpenGLRenderBackend::evaluateDifferenceAggregation(Handle texture, int w, int h) { return oglEvaluateDifferenceAggregation(texture, w, h); }
bool OpenGLRenderBackend::fetchDifferenceAggregationResult(Result& result) { return oglFetchDifferenceAggregationResult(result); }
OpenGLRenderBackend::ShaderHandle OpenGLRenderBackend::createShader(
        nicxlive::core::d_runtime::RetainedString vertex, nicxlive::core::d_runtime::RetainedString fragment) {
    auto handle = std::make_shared<GLShaderHandle>();
    oglCreateShaderProgram(handle->shader, vertex.view(), fragment.view());
    return handle;
}
void OpenGLRenderBackend::destroyShader(ShaderHandle shader) { auto handle = requireGLShader(shader); oglDestroyShaderProgram(handle->shader); handle->shader = {}; }
void OpenGLRenderBackend::useShader(ShaderHandle shader) { oglUseShaderProgram(requireGLShader(shader)->shader); }
int OpenGLRenderBackend::getShaderUniformLocation(ShaderHandle shader, nicxlive::core::d_runtime::RetainedString name) {
    return oglShaderGetUniformLocation(requireGLShader(shader)->shader, name.view());
}
void OpenGLRenderBackend::setShaderUniform(ShaderHandle shader, int location, bool value) { requireGLShader(shader); oglSetUniformBool(location, value); }
void OpenGLRenderBackend::setShaderUniform(ShaderHandle shader, int location, int value) { requireGLShader(shader); oglSetUniformInt(location, value); }
void OpenGLRenderBackend::setShaderUniform(ShaderHandle shader, int location, float value) { requireGLShader(shader); oglSetUniformFloat(location, value); }
void OpenGLRenderBackend::setShaderUniform(ShaderHandle shader, int location, Vec2 value) { requireGLShader(shader); oglSetUniformVec2(location, value); }
void OpenGLRenderBackend::setShaderUniform(ShaderHandle shader, int location, Vec3 value) { requireGLShader(shader); oglSetUniformVec3(location, value); }
void OpenGLRenderBackend::setShaderUniform(ShaderHandle shader, int location, Vec4 value) { requireGLShader(shader); oglSetUniformVec4(location, value); }
void OpenGLRenderBackend::setShaderUniform(ShaderHandle shader, int location, Mat4 value) { requireGLShader(shader); oglSetUniformMat4(location, value); }
OpenGLRenderBackend::TextureHandle OpenGLRenderBackend::createTextureHandle() {
    auto handle = std::make_shared<GLTextureHandle>(); oglCreateTextureHandle(handle->id); return handle;
}
void OpenGLRenderBackend::destroyTextureHandle(TextureHandle texture) { auto handle = requireGLTexture(texture); oglDeleteTextureHandle(handle->id); handle->id = 0; }
void OpenGLRenderBackend::uploadTextureData(TextureHandle texture, int w, int h, int inChannels, int outChannels,
    bool stencil, nicxlive::core::SourceTextureBytes data) {
    const auto handle = requireGLTexture(texture);
    oglUploadTextureData(handle->id, w, h, inChannels, outChannels, stencil, data);
}
void OpenGLRenderBackend::updateTextureRegion(TextureHandle texture, int x, int y, int w, int h, int channels,
    nicxlive::core::SourceTextureBytes data) {
    const auto handle = requireGLTexture(texture);
    oglUpdateTextureRegion(handle->id, x, y, w, h, channels, data);
}
void OpenGLRenderBackend::readTextureData(TextureHandle texture, int channels, bool stencil,
    nicxlive::core::SourceTextureBytes data) {
    const auto handle = requireGLTexture(texture);
    oglReadTextureData(handle->id, channels, stencil, data);
}
void OpenGLRenderBackend::bindTextureHandle(TextureHandle texture, unsigned unit) { oglBindTextureHandle(requireGLTexture(texture)->id, unit); }
void OpenGLRenderBackend::bindTextureId(std::uint32_t id, std::uint32_t unit) { oglBindTextureHandle(id, unit); }
void OpenGLRenderBackend::uploadTextureData(TextureHandle texture, int w, int h, int inChannels, int outChannels, bool stencil, const std::vector<std::uint8_t>& data) {
    oglUploadTextureData(requireGLTexture(texture)->id, w, h, inChannels, outChannels, stencil, data);
}
void OpenGLRenderBackend::updateTextureRegion(TextureHandle texture, int x, int y, int w, int h, int channels, const std::vector<std::uint8_t>& data) {
    oglUpdateTextureRegion(requireGLTexture(texture)->id, x, y, w, h, channels, data);
}
void OpenGLRenderBackend::generateTextureMipmap(TextureHandle texture) { oglGenerateTextureMipmap(requireGLTexture(texture)->id); }
void OpenGLRenderBackend::applyTextureFiltering(TextureHandle texture, Filtering value, bool mipmaps) { oglApplyTextureFiltering(requireGLTexture(texture)->id, value, mipmaps); }
void OpenGLRenderBackend::applyTextureWrapping(TextureHandle texture, Wrapping value) { oglApplyTextureWrapping(requireGLTexture(texture)->id, value); }
void OpenGLRenderBackend::applyTextureAnisotropy(TextureHandle texture, float value) { oglApplyTextureAnisotropy(requireGLTexture(texture)->id, value); }
float OpenGLRenderBackend::maxTextureAnisotropy() { return oglMaxTextureAnisotropy(); }
void OpenGLRenderBackend::readTextureData(TextureHandle texture, int channels, bool stencil, std::vector<std::uint8_t>& data) {
    oglReadTextureData(requireGLTexture(texture)->id, channels, stencil, data);
}
std::size_t OpenGLRenderBackend::textureNativeHandle(TextureHandle texture) { return requireGLTexture(texture)->id; }
void OpenGLRenderBackend::destroyTextureNativeHandle(std::size_t texture) { auto id = static_cast<GLId>(texture); if (id) oglDeleteTextureHandle(id); }
unsigned OpenGLRenderBackend::createTexture(const std::vector<std::uint8_t>& data, int w, int h, int channels, bool stencil) {
    GLId id = 0; oglCreateTextureHandle(id); oglUploadTextureData(id, w, h, channels, channels, stencil, data); return id;
}
void OpenGLRenderBackend::updateTexture(unsigned id, const std::vector<std::uint8_t>& data, int w, int h, int channels) {
    oglUpdateTextureRegion(id, 0, 0, w, h, channels, data);
}
void OpenGLRenderBackend::setTextureParams(unsigned id, Filtering filtering, Wrapping wrapping, float anisotropy) {
    oglApplyTextureFiltering(id, filtering); oglApplyTextureWrapping(id, wrapping); oglApplyTextureAnisotropy(id, anisotropy);
}
void OpenGLRenderBackend::readTextureData(unsigned id, int channels, bool stencil, std::vector<std::uint8_t>& data) { oglReadTextureData(id, channels, stencil, data); }
void OpenGLRenderBackend::uploadTextureData(unsigned id, int w, int h, int inChannels, int outChannels, bool stencil, const std::vector<std::uint8_t>& data) {
    oglUploadTextureData(id, w, h, inChannels, outChannels, stencil, data);
}
void OpenGLRenderBackend::generateTextureMipmap(unsigned id) { oglGenerateTextureMipmap(id); }
void OpenGLRenderBackend::disposeTexture(unsigned id) { oglDeleteTextureHandle(id); }
void OpenGLRenderBackend::uploadTextureData(unsigned id, int w, int h, int inChannels, int outChannels,
    bool stencil, nicxlive::core::SourceTextureBytes data) {
    oglUploadTextureData(id, w, h, inChannels, outChannels, stencil, data);
}
void OpenGLRenderBackend::readTextureData(unsigned id, int channels, bool stencil,
    nicxlive::core::SourceTextureBytes data) {
    oglReadTextureData(id, channels, stencil, data);
}
namespace {
// Retain the selected GL runtime until its explicit context-owned shutdown,
// including when another backend temporarily becomes current.
thread_local std::shared_ptr<OpenGLRenderBackend> selected;
thread_local SDL_GLContext selectedContext = nullptr;
std::shared_ptr<OpenGLRenderBackend> createSelectedOpenGLBackend() {
    const auto context = SDL_GL_GetCurrentContext();
    if (!context) throw std::logic_error("GL runtime selection requires a current context");
    if (selected && context != selectedContext)
        throw std::logic_error("Release the existing GL runtime in its owning context before selecting another");
    auto backend = std::make_shared<OpenGLRenderBackend>();
    debug_gl::ngInstallOpenGLDebugProvider();
    nicxlive::core::inSetRenderBackend(backend);
    debug_gl::ngBindOpenGLDebugProvider(backend);
    selected = backend;
    selectedContext = context;
    return backend;
}
std::shared_ptr<nicxlive::core::RenderBackend> createDefaultOpenGLBackend() {
    // Source construction allocates the stateless backend, without creating
    // GPU resources or requiring a context before a resource operation.
    return std::make_shared<OpenGLRenderBackend>();
}
[[maybe_unused]] const bool defaultFactoryRegistered = [] {
    nicxlive::core::ngSetRenderBackendFactory(&createDefaultOpenGLBackend);
    return true;
}();
}
unsigned RenderImageBorrow::textureName() const {
    if (owner_ && (owner_ != selected || context_ != SDL_GL_GetCurrentContext()
        || texture_ != oglGetRenderImage()))
        throw std::logic_error("The render-image borrow expired after context/target ownership changed");
    return texture_;
}
RenderImageBorrow ngBorrowOpenGLRenderImage() {
    RenderImageBorrow result;
    const auto backend = nicxlive::core::tryRenderBackend();
    if (!backend) return result;
    const auto owner = std::dynamic_pointer_cast<OpenGLRenderBackend>(backend);
    if (!owner) {
        // Preserve the canonical current-backend getter's zero-image contract;
        // a nonzero opaque resource from another renderer has no GL mapping.
        if (backend->renderImageHandle())
            throw std::logic_error("The current render image is not an OpenGL texture");
        return result;
    }
    const auto context = SDL_GL_GetCurrentContext();
    if (!context || !selectedContext || owner != selected
        || (context != selectedContext
            && !platform::ngImGuiSDL3SharesOpenGLContext(selectedContext, context)))
        throw std::logic_error("Render-image borrowing requires the owning or shared live GL context");
    result.owner_ = owner;
    result.context_ = context;
    const auto image = owner->renderImageHandle();
    if (owner != selected || context != SDL_GL_GetCurrentContext() || image != oglGetRenderImage())
        throw std::logic_error("The GL image must be the selected runtime's actual albedo target");
    result.texture_ = image;
    return result;
}
std::shared_ptr<OpenGLRenderBackend> ngSelectOpenGLRenderBackend() {
    const auto context = SDL_GL_GetCurrentContext();
    if (!context) throw std::logic_error("GL runtime selection requires a current context");
    nicxlive::core::ngSetRenderBackendFactory(&createDefaultOpenGLBackend);
    if (selected) {
        if (context != selectedContext)
            throw std::logic_error("Release the existing GL runtime in its owning context before selecting another");
        nicxlive::core::inSetRenderBackend(selected);
        debug_gl::ngInstallOpenGLDebugProvider();
        debug_gl::ngBindOpenGLDebugProvider(selected);
        return selected;
    }
    return createSelectedOpenGLBackend();
}
void ngReleaseOpenGLRenderBackend() {
    if (selected) {
        if (!selectedContext || SDL_GL_GetCurrentContext() != selectedContext)
            throw std::logic_error("GL runtime shutdown requires its owning current context");
        if (nicxlive::core::getCurrentRenderBackend() == selected)
            nicxlive::core::inSetRenderBackend({});
        oglReleaseRenderer();
        debug_gl::ngReleaseOpenGLDebugProvider();
    }
    selected.reset();
    selectedContext = nullptr;
}
}

