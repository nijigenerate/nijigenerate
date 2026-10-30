#pragma once
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include "string.hpp"

#include <nijigenerate/io/depthimage.hpp>
#include <math/types.hpp>
#include <compat/object_metadata.hpp>
#include <cstdint>
#include <memory>
#include <string>

namespace nijigenerate::viewport::depth::draw {

struct DepthDrawRect {
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};
enum class DepthDrawLayerCleanupKind : std::int32_t {
    AlphaDepthGapFill = 0,
    ContourRepair = 1,
};
struct DepthDrawLayerCleanupOperation {
    DepthDrawLayerCleanupKind kind = DepthDrawLayerCleanupKind::AlphaDepthGapFill;
    std::int32_t contourThickness = 2;
    compat::Slice<io::depthimage::DepthDrawAlphaDepthFocusedRule> focusedRules;
};
struct DepthDrawLayer {
    DepthDrawString id;
    DepthDrawString sourcePath;
    DepthDrawString layerPath;
    DepthDrawString displayName;
    std::int32_t width = 0;
    std::int32_t height = 0;
    DepthDrawRect bounds{};
    compat::Slice<std::uint8_t> rgba;
    compat::Slice<std::uint8_t> depthPixels;
    compat::Slice<std::uint8_t> alphaMask;
    compat::Slice<std::uint8_t> normalCoverage;
    compat::Slice<DepthDrawLayerCleanupOperation> cleanupOperations;
    float opacity = 1.0f;
    bool visible = true;
    bool enabled = true;
    nicxlive::core::math::Vec2 xyOffset{0.0f, 0.0f};
    nicxlive::core::math::Vec2 xyScale{1.0f, 1.0f};
    float zOffset = 0.0f;
    float zScale = 1.0f;
    float backDepth = -1.0f;
    float frontDepth = 1.0f;
    float sampleDepthScale = 1.0f;
    bool invert = false;
    io::depthimage::DepthImageChannel channel = io::depthimage::DepthImageChannel::AverageRGB;
    io::depthimage::DepthImageConvolution convolution = io::depthimage::DepthImageConvolution::Gaussian3x3;
    std::int32_t customRadius = 3;
    float alphaThreshold = 0.01f;

    bool hasDepthPixels() const;
    bool hasNormalCoverage() const;
    io::depthimage::DepthImageSampleSettings sampleSettings() const;
    float applyZTransform(float rawDepth) const;
};

inline constexpr std::int32_t DepthDrawMaxContourThickness = 64;
inline constexpr std::int32_t DepthDrawMaxFocusedRuleRadius = 64;

std::int32_t ngNormalizeDepthDrawContourThickness(std::int32_t thickness);
io::depthimage::DepthDrawAlphaDepthFocusedRule ngNormalizeDepthDrawFocusedRule(
    io::depthimage::DepthDrawAlphaDepthFocusedRule rule, std::int32_t width, std::int32_t height);

} // namespace nijigenerate::viewport::depth::draw
namespace nijigenerate::viewport::depth::draw::layer {
using ::nijigenerate::viewport::depth::draw::DepthDrawRect;
using ::nijigenerate::viewport::depth::draw::DepthDrawLayerCleanupKind;
using ::nijigenerate::viewport::depth::draw::DepthDrawLayerCleanupOperation;
using ::nijigenerate::viewport::depth::draw::DepthDrawLayer;
using ::nijigenerate::viewport::depth::draw::DepthDrawMaxContourThickness;
using ::nijigenerate::viewport::depth::draw::DepthDrawMaxFocusedRuleRadius;
using ::nijigenerate::viewport::depth::draw::ngNormalizeDepthDrawContourThickness;
using ::nijigenerate::viewport::depth::draw::ngNormalizeDepthDrawFocusedRule;
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::viewport::depth::draw::DepthDrawLayerCleanupKind> {
    using Enum = ::nijigenerate::viewport::depth::draw::DepthDrawLayerCleanupKind;
    inline static constexpr std::string_view name = "DepthDrawLayerCleanupKind";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::AlphaDepthGapFill, "AlphaDepthGapFill"},
        FormatEnumMember<Enum>{Enum::ContourRepair, "ContourRepair"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_LAYER_VALUE_SOURCE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.draw.layer." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.draw.layer." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_DEPTH_LAYER_VALUE_SOURCE_ORIGIN(DepthDrawRect);
NG_DEPTH_LAYER_VALUE_SOURCE_ORIGIN(DepthDrawLayerCleanupKind);
NG_DEPTH_LAYER_VALUE_SOURCE_ORIGIN(DepthDrawLayerCleanupOperation);
NG_DEPTH_LAYER_VALUE_SOURCE_ORIGIN(DepthDrawLayer);
#undef NG_DEPTH_LAYER_VALUE_SOURCE_ORIGIN
} // namespace nicxlive::core::d_runtime
