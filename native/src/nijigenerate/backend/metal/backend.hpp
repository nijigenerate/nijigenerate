#pragma once
#include "context.hpp"
#include <array>
#include <core/puppet.hpp>
#include <core/render/common.hpp>
#include <map>
#include <memory>
#include <span>

namespace nijigenerate::backend::metal {
inline nicxlive::core::math::Vec3 ngSceneAmbientLight{1,1,1};
class Backend final : public nicxlive::core::RenderBackend {
  struct Impl;
  std::unique_ptr<Impl> p;

public:
  using Handle = nicxlive::core::RenderResourceHandle;
  using TextureHandle = std::shared_ptr<nicxlive::core::RenderTextureHandle>;
  using Bytes = nicxlive::core::SourceTextureBytes;
  using Vec2 = nicxlive::core::math::Vec2;
  using Vec3 = nicxlive::core::math::Vec3;
  using Vec4 = nicxlive::core::math::Vec4;
  using Mat4 = nicxlive::core::math::Mat4;
  std::unique_ptr<nicxlive::core::RenderCommandEmitter> createCommandEmitter() override;
  TextureHandle createTextureHandle() override;
  void destroyTextureHandle(TextureHandle) override;
  void bindTextureHandle(TextureHandle, unsigned) override;
  void bindTextureId(uint32_t, uint32_t) override;
  void uploadTextureData(TextureHandle, int, int, int, int, bool, Bytes) override;
  void updateTextureRegion(TextureHandle, int, int, int, int, int, Bytes) override;
  void generateTextureMipmap(TextureHandle) override;
  void applyTextureFiltering(TextureHandle, nicxlive::core::Filtering, bool) override;
  void applyTextureWrapping(TextureHandle, nicxlive::core::Wrapping) override;
  void applyTextureAnisotropy(TextureHandle, float) override;
  float maxTextureAnisotropy() override;
  void readTextureData(TextureHandle, int, bool, Bytes) override;
  std::size_t textureNativeHandle(TextureHandle) override;
  std::size_t textureNativeHandle(uint32_t) override;
  void uploadTextureData(uint32_t, int, int, int, int, bool, Bytes) override;
  void uploadTextureData(uint32_t, int, int, int, int, bool, const std::vector<uint8_t>&) override;
  void readTextureData(uint32_t, int, bool, Bytes) override;
  void readTextureData(uint32_t, int, bool, std::vector<uint8_t>&) override;
  void generateTextureMipmap(uint32_t) override;
  void dumpViewport(nicxlive::core::common::SharedArray<uint8_t>&, int, int) override;
  void uploadDrawableIndices(Handle, nicxlive::core::common::SharedArray<uint16_t>) override;
  bool supportsAdvancedBlend() override;
  bool supportsAdvancedBlendCoherent() override;
  void setAdvancedBlendCoherent(bool) override;
  void setLegacyBlendMode(nicxlive::core::nodes::BlendMode) override;
  void setAdvancedBlendEquation(nicxlive::core::nodes::BlendMode) override;
  void issueBlendBarrier() override;
  void setDifferenceAggregationEnabled(bool) override;
  bool isDifferenceAggregationEnabled() override;
  void setDifferenceAggregationRegion(const nicxlive::core::DifferenceEvaluationRegion&) override;
  nicxlive::core::DifferenceEvaluationRegion getDifferenceAggregationRegion() override;
  bool evaluateDifferenceAggregation(Handle,int,int) override;
  bool fetchDifferenceAggregationResult(nicxlive::core::DifferenceEvaluationResult&) override;
  bool hasTexture(uint32_t) const;
  void flush();
  void drawTextureAtPart(const nicxlive::core::Texture&,const std::shared_ptr<nicxlive::core::nodes::Part>&) override;
  void drawTextureAtPosition(const nicxlive::core::Texture&,const Vec2&,float,const Vec3&,const Vec3&) override;
  void drawTextureAtRect(const nicxlive::core::Texture&,const nicxlive::core::Rect&,const nicxlive::core::Rect&,
      float,const Vec3&,const Vec3&,void*,void*) override;
  void drawMesh(uint32_t texture, std::span<const Vec3> positions,
      std::span<const Vec2> uvs, std::span<const uint32_t> indices, const Mat4& matrix,
      Vec4 tint = {1,1,1,1}, Vec3 screen = {}, bool depth = false, nicxlive::core::nodes::BlendMode blend = nicxlive::core::nodes::BlendMode::Normal);
  void pushTarget(uint32_t texture, Vec4 clearColor = {}, bool clear = true);
  void popTarget();
  void initDebugRenderer() override;
  void setDebugPointSize(float) override;
  void setDebugLineWidth(float) override;
  void uploadDebugBuffer(const std::vector<Vec3>&, const std::vector<uint16_t>&) override;
  void uploadDebugBuffer(nicxlive::core::math::Vec3Array,
      nicxlive::core::common::SharedArray<uint16_t>) override;
  void drawDebugPoints(const Vec4&, const Mat4&) override;
  void drawDebugLines(const Vec4&, const Mat4&) override;
  Backend();
  ~Backend() override;
  void shutdown();
  void initializeRenderer() override;
  void createDrawableBuffers(nicxlive::core::RenderResourceHandle &) override;
  void disposeDrawableBuffers(nicxlive::core::RenderResourceHandle) override;
  void uploadDrawableIndices(nicxlive::core::RenderResourceHandle, const std::vector<uint16_t> &) override;
  uint32_t createTexture(const std::vector<uint8_t> &, int, int, int,
                         bool) override;
  void updateTexture(uint32_t, const std::vector<uint8_t> &, int, int,
                     int) override;
  void setTextureParams(uint32_t, nicxlive::core::Filtering,
                        nicxlive::core::Wrapping, float) override;
  void disposeTexture(uint32_t) override;
  std::vector<uint8_t> readTexture(uint32_t);
  ImTextureID uiTexture(uint32_t);
  void drawPuppet(nicxlive::core::Puppet &);
  void drawPartPacket(const nicxlive::core::nodes::PartDrawPacket &) override;
  void beginMask(bool) override;
  void applyMask(const MaskApplyPacket &) override;
  void beginMaskContent() override;
  void endMask() override;
  void
  beginDynamicComposite(nicxlive::core::DynamicCompositePass &) override;
  void
  endDynamicComposite(nicxlive::core::DynamicCompositePass &) override;
  void destroyDynamicComposite(
      const std::shared_ptr<nicxlive::core::DynamicCompositeSurface> &)
      override;
  void resizeViewportTargets(int, int) override;
  void beginScene() override;
  void endScene() override;
  void postProcessScene() override;
  void addBasicLightingPostProcess() override;
  void dumpViewport(std::vector<uint8_t> &, int, int) override;
  nicxlive::core::RenderResourceHandle renderImageHandle() override;
  nicxlive::core::RenderResourceHandle mainAlbedoHandle() override;
  nicxlive::core::RenderResourceHandle mainEmissiveHandle() override;
  nicxlive::core::RenderResourceHandle mainBumpHandle() override;
  void render();
  Image &sceneImage();
  ImTextureID overlayBlurTexture();
  void drawSprites(uint32_t, const float *, size_t,
                   const nicxlive::core::nodes::Mat4 &);
};
Backend &backend();
ImTextureID uiTexture(uint32_t);
} // namespace nijigenerate::backend::metal
