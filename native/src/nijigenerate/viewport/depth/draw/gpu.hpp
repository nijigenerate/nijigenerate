#pragma once
#include "session.hpp"
#include "composer.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/viewport/depth/common/targetview.hpp>
#include <limits>
namespace nijigenerate::viewport::depth::draw {
using GpuError = compat::RetainedString;
inline constexpr std::uint32_t DepthDrawGpuLayerStride = 24u;
inline constexpr std::uint32_t DepthDrawGpuBindingStride = 8u;
inline constexpr std::uint32_t DepthDrawGpuDocumentPositionStride = 2u;
inline constexpr std::uint32_t DepthDrawGpuRgbaPixelStride = 4u;
inline constexpr std::uint32_t DepthDrawGpuMaxVertices = 1000000u;
inline constexpr std::uint32_t DepthDrawGpuMaxLayers = 1024u;
inline constexpr auto DepthDrawGpuMaxCustomRadius = io::depthimage::DepthImageMaxCustomRadius;
enum class DepthDrawGpuLayerField : std::int32_t {
    Width = 0,
    Height = 1,
    DepthPixelOffset = 2,
    NormalCoverageOffset = 3,
    Flags = 4,
    BoundsLeft = 5,
    BoundsTop = 6,
    BoundsWidth = 7,
    BoundsHeight = 8,
    Opacity = 9,
    XyOffsetX = 10,
    XyOffsetY = 11,
    XyScaleX = 12,
    XyScaleY = 13,
    ZOffset = 14,
    ZScale = 15,
    BackDepth = 16,
    FrontDepth = 17,
    AlphaThreshold = 18,
    Channel = 19,
    Convolution = 20,
    CustomRadius = 21,
    SampleDepthScale = 22,
};
enum class DepthDrawGpuBindingField : std::int32_t {
    LayerIndex = 0,
    MergePolicy = 1,
    Flags = 2,
    CoverageThreshold = 3,
};
struct DepthDrawGpuLayerPacket {
    DepthDrawString layerId{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t depthPixelOffset{};
    std::uint32_t normalCoverageOffset{};
    std::uint32_t flags{};
    float boundsLeft = std::numeric_limits<float>::quiet_NaN();
    float boundsTop = std::numeric_limits<float>::quiet_NaN();
    float boundsWidth = std::numeric_limits<float>::quiet_NaN();
    float boundsHeight = std::numeric_limits<float>::quiet_NaN();
    float opacity = std::numeric_limits<float>::quiet_NaN();
    float xyOffsetX = std::numeric_limits<float>::quiet_NaN();
    float xyOffsetY = std::numeric_limits<float>::quiet_NaN();
    float xyScaleX = std::numeric_limits<float>::quiet_NaN();
    float xyScaleY = std::numeric_limits<float>::quiet_NaN();
    float zOffset = std::numeric_limits<float>::quiet_NaN();
    float zScale = std::numeric_limits<float>::quiet_NaN();
    float backDepth = std::numeric_limits<float>::quiet_NaN();
    float frontDepth = std::numeric_limits<float>::quiet_NaN();
    float sampleDepthScale = std::numeric_limits<float>::quiet_NaN();
    float alphaThreshold = std::numeric_limits<float>::quiet_NaN();
    std::int32_t channel{};
    std::int32_t convolution{};
    std::int32_t customRadius{};
};
struct DepthDrawGpuBindingPacket {
    DepthDrawString layerId{};
    std::uint32_t layerIndex{};
    std::uint32_t mergePolicy{};
    std::uint32_t flags{};
    float coverageThreshold = std::numeric_limits<float>::quiet_NaN();
};
struct DepthDrawGpuComposePacket {
    std::uint64_t targetGridUuid{};
    std::int32_t documentWidth{};
    std::int32_t documentHeight{};
    compat::Slice<nicxlive::core::math::Vec2> vertices{};
    compat::Slice<nicxlive::core::math::Vec2> documentPositions{};
    compat::Slice<float> baseDepths{};
    compat::Slice<DepthDrawGpuLayerPacket> layers{};
    compat::Slice<DepthDrawGpuBindingPacket> bindings{};
    compat::Slice<std::uint8_t> depthPixels{};
    compat::Slice<std::uint8_t> normalCoveragePixels{};
};
struct DepthDrawGpuLayerSamplePacket {
    std::uint64_t targetGridUuid{};
    std::uint32_t layerIndex{};
    DepthDrawGpuLayerPacket layer{};
    DepthDrawGpuBindingPacket binding{};
    compat::Slice<nicxlive::core::math::Vec2> documentPositions{};
    compat::Slice<std::uint8_t> depthPixels{};
    compat::Slice<std::uint8_t> normalCoveragePixels{};
};
using DepthDrawGpuReadOnlyBytes = compat::ConstSlice<std::uint8_t>;
struct DepthDrawGpuLayerSampleUpload {
    compat::Slice<float> documentPositions{};
    compat::Slice<float> layer{};
    compat::Slice<float> binding{};
    DepthDrawGpuReadOnlyBytes depthPixels{};
    DepthDrawGpuReadOnlyBytes normalCoveragePixels{};
};
struct DepthDrawGpuLayerReadback {
    std::uint32_t layerIndex{};
    compat::Slice<std::uint8_t> validSamples{};
    compat::Slice<float> sampleDepths{};
};
struct DepthDrawGpuComposeReadback {
    std::uint64_t targetGridUuid{};
    compat::Slice<float> depths{};
    compat::Slice<std::int32_t> winningLayerIndices{};
    compat::Slice<DepthDrawGpuLayerReadback> layers{};
};
struct DepthDrawGpuDispatchPollResult {
    bool ready{};
    DepthDrawGpuComposeReadback readback{};
};
struct DepthDrawGpuLayerSamplePollResult {
    bool ready{};
    DepthDrawGpuLayerReadback readback{};
};
struct DepthDrawGpuTargetComposeJob {
    std::uint32_t jobId{};
    DepthDrawGpuComposePacket packet{};
};
struct DepthDrawGpuTargetComposePollResult {
    bool ready{};
    DepthDrawGpuComposeReadback readback{};
    DepthDrawComposeResult result{};
};
using DepthDrawGpuSupportHook = bool(*)();
using DepthDrawGpuSubmitHook = bool(*)(DepthDrawGpuComposePacket&, std::uint32_t&, GpuError&);
using DepthDrawGpuPollHook = bool(*)(std::uint32_t, DepthDrawGpuDispatchPollResult&, GpuError&);
void ngSetDepthDrawGpuTestHooks(DepthDrawGpuSupportHook, DepthDrawGpuSubmitHook, DepthDrawGpuPollHook);
void ngClearDepthDrawGpuTestHooks();
bool ngDepthDrawGpuSupported();
compat::Slice<compat::RetainedStringSlice> ngDepthDrawGpuMissingRequirements();
GpuError ngDepthDrawGpuSupportDiagnostic();
compat::Slice<compat::RetainedStringSlice> ngDepthDrawGpuLayerSampleMissingRequirements();
bool ngDepthDrawGpuLayerSampleSupported();
bool ngDepthDrawGpuLayerSampleSupportsConvolution(std::int32_t);
GpuError ngDepthDrawGpuLayerSampleSupportDiagnostic();
DepthDrawGpuLayerSamplePacket ngBuildDepthDrawGpuLayerSamplePacket(const DepthDrawGpuComposePacket&, std::size_t);
void ngValidateDepthDrawGpuLayerSamplePacket(const DepthDrawGpuLayerSamplePacket&);
DepthDrawGpuLayerSampleUpload ngBuildDepthDrawGpuLayerSampleUpload(const DepthDrawGpuLayerSamplePacket&);
compat::Slice<float> ngFlattenDepthDrawGpuDocumentPositions(compat::ConstSlice<nicxlive::core::math::Vec2>);
compat::Slice<float> ngFlattenDepthDrawGpuLayers(compat::ConstSlice<DepthDrawGpuLayerPacket>);
compat::Slice<float> ngFlattenDepthDrawGpuBindings(compat::ConstSlice<DepthDrawGpuBindingPacket>);
void ngValidateDepthDrawGpuComposePacket(const DepthDrawGpuComposePacket&);
void ngValidateDepthDrawGpuComposeReadback(const DepthDrawGpuComposePacket&, const DepthDrawGpuComposeReadback&);
DepthDrawGpuComposeReadback ngBuildDepthDrawGpuComposeReadback(const DepthDrawGpuComposePacket&,
    compat::ConstSlice<DepthDrawGpuLayerReadback>);
nicxlive::core::math::Vec2 ngDepthDrawGpuLayerPixelFromDocument(DepthDrawGpuLayerPacket, nicxlive::core::math::Vec2);
DepthDrawComposeResult ngDepthDrawComposeResultFromGpuReadback(DepthDrawGpuComposePacket, DepthDrawGpuComposeReadback);
DepthDrawGpuComposePacket ngBuildDepthDrawGpuComposePacket(std::shared_ptr<DepthDrawSession>,
    std::shared_ptr<common::targetview::DepthTargetView>, std::int32_t, std::int32_t);
bool ngSubmitDepthDrawGpuLayerSample(DepthDrawGpuLayerSamplePacket&, std::uint32_t&, GpuError&);
bool ngPollDepthDrawGpuLayerSample(std::uint32_t, DepthDrawGpuLayerSamplePollResult&, GpuError&);
std::size_t ngPendingDepthDrawGpuLayerSampleJobCount();
bool ngSubmitDepthDrawGpuCompose(DepthDrawGpuComposePacket&, std::uint32_t&, GpuError&);
bool ngPollDepthDrawGpuCompose(std::uint32_t, DepthDrawGpuDispatchPollResult&, GpuError&);
void ngCancelDepthDrawGpuCompose(std::uint32_t);
std::size_t ngPendingDepthDrawGpuComposeJobCount();
bool ngSubmitDepthDrawGpuTargetCompose(std::shared_ptr<DepthDrawSession>, std::shared_ptr<common::targetview::DepthTargetView>,
    std::int32_t, std::int32_t, DepthDrawGpuTargetComposeJob&, GpuError&);
bool ngPollDepthDrawGpuTargetCompose(DepthDrawGpuTargetComposeJob&, DepthDrawGpuTargetComposePollResult&, GpuError&);
void ngCancelDepthDrawGpuTargetCompose(DepthDrawGpuTargetComposeJob&);
}

namespace nijigenerate::viewport::depth::draw::gpu {
using ::nijigenerate::viewport::depth::draw::GpuError;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuLayerStride;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuBindingStride;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuDocumentPositionStride;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuRgbaPixelStride;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuMaxVertices;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuMaxLayers;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuMaxCustomRadius;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuLayerField;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuBindingField;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuLayerPacket;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuBindingPacket;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuComposePacket;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuLayerSamplePacket;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuReadOnlyBytes;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuLayerSampleUpload;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuLayerReadback;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuComposeReadback;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuDispatchPollResult;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuLayerSamplePollResult;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuTargetComposeJob;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuTargetComposePollResult;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuSupportHook;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuSubmitHook;
using ::nijigenerate::viewport::depth::draw::DepthDrawGpuPollHook;
using ::nijigenerate::viewport::depth::draw::ngSetDepthDrawGpuTestHooks;
using ::nijigenerate::viewport::depth::draw::ngClearDepthDrawGpuTestHooks;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGpuSupported;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGpuMissingRequirements;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGpuSupportDiagnostic;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGpuLayerSampleMissingRequirements;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGpuLayerSampleSupported;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGpuLayerSampleSupportsConvolution;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGpuLayerSampleSupportDiagnostic;
using ::nijigenerate::viewport::depth::draw::ngBuildDepthDrawGpuLayerSamplePacket;
using ::nijigenerate::viewport::depth::draw::ngValidateDepthDrawGpuLayerSamplePacket;
using ::nijigenerate::viewport::depth::draw::ngBuildDepthDrawGpuLayerSampleUpload;
using ::nijigenerate::viewport::depth::draw::ngFlattenDepthDrawGpuDocumentPositions;
using ::nijigenerate::viewport::depth::draw::ngFlattenDepthDrawGpuLayers;
using ::nijigenerate::viewport::depth::draw::ngFlattenDepthDrawGpuBindings;
using ::nijigenerate::viewport::depth::draw::ngValidateDepthDrawGpuComposePacket;
using ::nijigenerate::viewport::depth::draw::ngValidateDepthDrawGpuComposeReadback;
using ::nijigenerate::viewport::depth::draw::ngBuildDepthDrawGpuComposeReadback;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGpuLayerPixelFromDocument;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawComposeResultFromGpuReadback;
using ::nijigenerate::viewport::depth::draw::ngBuildDepthDrawGpuComposePacket;
using ::nijigenerate::viewport::depth::draw::ngSubmitDepthDrawGpuLayerSample;
using ::nijigenerate::viewport::depth::draw::ngPollDepthDrawGpuLayerSample;
using ::nijigenerate::viewport::depth::draw::ngPendingDepthDrawGpuLayerSampleJobCount;
using ::nijigenerate::viewport::depth::draw::ngSubmitDepthDrawGpuCompose;
using ::nijigenerate::viewport::depth::draw::ngPollDepthDrawGpuCompose;
using ::nijigenerate::viewport::depth::draw::ngCancelDepthDrawGpuCompose;
using ::nijigenerate::viewport::depth::draw::ngPendingDepthDrawGpuComposeJobCount;
using ::nijigenerate::viewport::depth::draw::ngSubmitDepthDrawGpuTargetCompose;
using ::nijigenerate::viewport::depth::draw::ngPollDepthDrawGpuTargetCompose;
using ::nijigenerate::viewport::depth::draw::ngCancelDepthDrawGpuTargetCompose;
} // namespace nijigenerate::viewport::depth::draw::gpu

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.draw.gpu." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.draw.gpu." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuLayerField);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuBindingField);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuLayerPacket);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuBindingPacket);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuComposePacket);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuLayerSamplePacket);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuLayerSampleUpload);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuLayerReadback);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuComposeReadback);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuDispatchPollResult);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuLayerSamplePollResult);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuTargetComposeJob);
NG_DEPTH_DRAW_GPU_VALUE_ORIGIN(DepthDrawGpuTargetComposePollResult);
#undef NG_DEPTH_DRAW_GPU_VALUE_ORIGIN
}
