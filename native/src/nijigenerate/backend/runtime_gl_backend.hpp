#pragma once
#include <render/common.hpp>
#include <math/camera.hpp>
#include <shader.hpp>
#include "runtime_gl_handles.hpp"
#include <SDL3/SDL_video.h>

namespace nijigenerate::backend::runtime_gl {
class OpenGLRenderBackend : public nicxlive::core::RenderBackend {
public:
    using Vec2 = nicxlive::core::math::Vec2;
    using Vec3 = nicxlive::core::math::Vec3;
    using Vec4 = nicxlive::core::math::Vec4;
    using Mat4 = nicxlive::core::math::Mat4;
    using Vec2Array = nicxlive::core::common::Vec2Array;
    using Handle = nicxlive::core::RenderResourceHandle;
    using PartPacket = nicxlive::core::nodes::PartDrawPacket;
    using Surface = nicxlive::core::DynamicCompositeSurface;
    using Pass = nicxlive::core::DynamicCompositePass;
    using Texture = nicxlive::core::Texture;
    using Part = nicxlive::core::nodes::Part;
    using Rect = nicxlive::core::Rect;
    using Filtering = nicxlive::core::Filtering;
    using Wrapping = nicxlive::core::Wrapping;
    using BlendMode = nicxlive::core::nodes::BlendMode;
    using Region = nicxlive::core::DifferenceEvaluationRegion;
    using Result = nicxlive::core::DifferenceEvaluationResult;
    using ShaderHandle = std::shared_ptr<RenderShaderHandle>;
    using TextureHandle = std::shared_ptr<RenderTextureHandle>;
    std::unique_ptr<nicxlive::core::RenderCommandEmitter> createCommandEmitter() override;
    void initializeRenderer() override;
    void resizeViewportTargets(int, int) override;
    void dumpViewport(std::vector<std::uint8_t>&, int, int) override;
    void dumpViewport(nicxlive::core::common::SharedArray<std::uint8_t>&, int, int) override;
    void beginScene() override;
    void endScene() override;
    void postProcessScene() override;
    void initializeDrawableResources() override;
    void bindDrawableVao() override;
    void createDrawableBuffers(Handle&) override;
    void disposeDrawableBuffers(Handle) override;
    void uploadDrawableIndices(Handle, const std::vector<std::uint16_t>&) override;
    void uploadDrawableIndices(Handle, nicxlive::core::common::SharedArray<std::uint16_t>) override;
    void uploadSharedVertexBuffer(const Vec2Array&) override;
    void uploadSharedUvBuffer(const Vec2Array&) override;
    void uploadSharedDeformBuffer(const Vec2Array&) override;
    void drawDrawableElements(Handle, std::size_t) override;
    bool supportsAdvancedBlend() override;
    bool supportsAdvancedBlendCoherent() override;
    void setAdvancedBlendCoherent(bool) override;
    void setLegacyBlendMode(BlendMode) override;
    void setAdvancedBlendEquation(BlendMode) override;
    void issueBlendBarrier() override;
    void initDebugRenderer() override;
    void setDebugPointSize(float) override;
    void setDebugLineWidth(float) override;
    void uploadDebugBuffer(const std::vector<Vec3>&, const std::vector<std::uint16_t>&) override;
    void setDebugExternalBuffer(Handle, Handle, int) override;
    void uploadDebugBuffer(nicxlive::core::math::Vec3Array,
        nicxlive::core::common::SharedArray<std::uint16_t>) override;
    void drawDebugPoints(const Vec4&, const Mat4&) override;
    void drawDebugLines(const Vec4&, const Mat4&) override;
    void drawPartPacket(const PartPacket&) override;
    void beginDynamicComposite(Pass&) override;
    void endDynamicComposite(Pass&) override;
    void destroyDynamicComposite(const std::shared_ptr<Surface>&) override;
    void beginMask(bool) override;
    void applyMask(const MaskApplyPacket&) override;
    void beginMaskContent() override;
    void endMask() override;
    void drawTextureAtPart(const Texture&, const std::shared_ptr<Part>&) override;
    void drawTextureAtPosition(const Texture&, const Vec2&, float, const Vec3&, const Vec3&) override;
    void drawTextureAtRect(const Texture&, const Rect&, const Rect&, float, const Vec3&, const Vec3&, void*, void*) override;
    void drawTextureAtPart(Texture&, const std::shared_ptr<Part>&);
    void drawTextureAtPosition(Texture&, const Vec2&, float, const Vec3&, const Vec3&);
    void drawTextureAtRect(Texture&, const Rect&, const Rect&, float, const Vec3&, const Vec3&, void*, void*);
    void drawTextureAtPart(std::shared_ptr<Texture>, std::shared_ptr<Part>);
    void drawTextureAtPosition(std::shared_ptr<Texture>, Vec2, float, Vec3, Vec3);
    void drawTextureAtRect(std::shared_ptr<Texture>, Rect, Rect, float, Vec3, Vec3,
        std::shared_ptr<nicxlive::core::Shader>, std::shared_ptr<nicxlive::core::math::Camera>);
    void drawTextureAtPart(const Texture*, const std::shared_ptr<Part>&);
    void drawTextureAtPosition(const Texture*, Vec2, float, Vec3, Vec3);
    void drawTextureAtRect(const Texture*, Rect, Rect, float, Vec3, Vec3,
        nicxlive::core::Shader* = nullptr, nicxlive::core::math::Camera* = nullptr);
    Handle framebufferHandle() override;
    Handle renderImageHandle() override;
    Handle mainAlbedoHandle() override;
    Handle mainEmissiveHandle() override;
    Handle mainBumpHandle() override;
    Handle compositeImageHandle() override;
    Handle compositeFramebufferHandle() override;
    Handle compositeEmissiveHandle() override;
    Handle compositeBumpHandle() override;
    Handle blendFramebufferHandle() override;
    Handle blendAlbedoHandle() override;
    Handle blendEmissiveHandle() override;
    Handle blendBumpHandle() override;
    void addBasicLightingPostProcess() override;
    void setDifferenceAggregationEnabled(bool) override;
    bool isDifferenceAggregationEnabled() override;
    void setDifferenceAggregationRegion(const Region&) override;
    Region getDifferenceAggregationRegion() override;
    bool evaluateDifferenceAggregation(Handle, int, int) override;
    bool fetchDifferenceAggregationResult(Result&) override;
    ShaderHandle createShader(nicxlive::core::d_runtime::RetainedString,
        nicxlive::core::d_runtime::RetainedString) override;
    void destroyShader(ShaderHandle) override;
    void useShader(ShaderHandle) override;
    int getShaderUniformLocation(ShaderHandle, nicxlive::core::d_runtime::RetainedString) override;
    void setShaderUniform(ShaderHandle, int, bool) override;
    void setShaderUniform(ShaderHandle, int, int) override;
    void setShaderUniform(ShaderHandle, int, float) override;
    void setShaderUniform(ShaderHandle, int, Vec2) override;
    void setShaderUniform(ShaderHandle, int, Vec3) override;
    void setShaderUniform(ShaderHandle, int, Vec4) override;
    void setShaderUniform(ShaderHandle, int, Mat4) override;
    TextureHandle createTextureHandle();
    void destroyTextureHandle(TextureHandle);
    void bindTextureHandle(TextureHandle, unsigned);
    void bindTextureId(std::uint32_t, std::uint32_t) override;
    void uploadTextureData(TextureHandle, int, int, int, int, bool, const std::vector<std::uint8_t>&);
    void uploadTextureData(TextureHandle, int, int, int, int, bool, nicxlive::core::SourceTextureBytes);
    void updateTextureRegion(TextureHandle, int, int, int, int, int, const std::vector<std::uint8_t>&);
    void updateTextureRegion(TextureHandle, int, int, int, int, int, nicxlive::core::SourceTextureBytes);
    void generateTextureMipmap(TextureHandle);
    void applyTextureFiltering(TextureHandle, Filtering, bool useMipmaps = true);
    void applyTextureWrapping(TextureHandle, Wrapping);
    void applyTextureAnisotropy(TextureHandle, float);
    float maxTextureAnisotropy();
    void readTextureData(TextureHandle, int, bool, std::vector<std::uint8_t>&);
    void readTextureData(TextureHandle, int, bool, nicxlive::core::SourceTextureBytes);
    std::size_t textureNativeHandle(TextureHandle);
    std::size_t textureNativeHandle(std::uint32_t texture) override { return texture; }
    void destroyTextureNativeHandle(std::size_t);
    unsigned createTexture(const std::vector<std::uint8_t>&, int, int, int, bool) override;
    void updateTexture(unsigned, const std::vector<std::uint8_t>&, int, int, int) override;
    void setTextureParams(unsigned, Filtering, Wrapping, float) override;
    void readTextureData(unsigned, int, bool, std::vector<std::uint8_t>&) override;
    void uploadTextureData(unsigned, int, int, int, int, bool, const std::vector<std::uint8_t>&) override;
    void uploadTextureData(unsigned, int, int, int, int, bool, nicxlive::core::SourceTextureBytes);
    void readTextureData(unsigned, int, bool, nicxlive::core::SourceTextureBytes);
    void generateTextureMipmap(unsigned) override;
    void disposeTexture(unsigned) override;
};
// Synchronous borrow of the actual TLS target. The backend is retained through
// consumption; the caller must not switch/destroy contexts or resize targets
// during the borrow. Reacquire after callbacks/resize, never cache across frames.
class RenderImageBorrow {
public:
    unsigned textureName() const;
private:
    std::shared_ptr<OpenGLRenderBackend> owner_;
    SDL_GLContext context_ = nullptr;
    unsigned texture_ = 0;
    friend RenderImageBorrow ngBorrowOpenGLRenderImage();
};
RenderImageBorrow ngBorrowOpenGLRenderImage();

// Explicit runtime selection; call on the GL context owner before renderer init.
std::shared_ptr<OpenGLRenderBackend> ngSelectOpenGLRenderBackend();
void ngReleaseOpenGLRenderBackend();
}
