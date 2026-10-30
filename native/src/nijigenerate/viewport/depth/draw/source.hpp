#pragma once
#include "session.hpp"
#include "nijigenerate/io/psd/layer.hpp"
#include <nijigenerate/compat/const_slice.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
namespace nijigenerate::viewport::depth::draw {
struct DepthDrawPsdLoadResult {
    int documentWidth = 0, documentHeight = 0;
    compat::Slice<DepthDrawLayer> layers;
    std::shared_ptr<DepthDrawSession> session;
};
struct DepthDrawLayerPairingResult {
    std::size_t matchedDepthLayers = 0, missingNormalCoverage = 0;
    compat::Slice<DepthDrawString> matchedDepthLayerIds, missingNormalLayerIds;
};
inline constexpr std::uint64_t DepthDrawPsdRetainedBytesPerPixel = 9;
inline constexpr std::uint64_t DepthDrawMaxPsdRetainedBytes = 256ULL*1024*1024;
inline constexpr std::uint64_t DepthDrawPngRetainedBytesPerPixel = 8;
inline constexpr std::uint64_t DepthDrawMaxPngRetainedBytes = 256ULL*1024*1024;
bool ngReserveDepthDrawPsdRetainedLayer(std::int64_t width, std::int64_t height, std::uint64_t& retainedBytes);
bool ngReserveDepthDrawPngRetainedLayer(std::int64_t width, std::int64_t height, std::uint64_t& retainedBytes);
DepthDrawLayer ngLoadDepthDrawPngLayer(DepthDrawString path, DepthDrawString id = {});
DepthDrawLayer ngLoadDepthDrawPngLayer(DepthDrawString path, DepthDrawString id, std::uint64_t& retainedBytes);
void ngDepthDrawApplyClippingBaseCoverage(DepthDrawLayer& clippedLayer, const DepthDrawLayer& clippingBase,
    float sharedGroupOpacity = 1.0f);
bool ngDepthDrawPsdLayerHasPixelData(io::psd::Layer& layer);
DepthDrawPsdLoadResult ngLoadDepthDrawPsd(DepthDrawString path);
void ngDepthDrawApplyMaskToLayerAlpha(DepthDrawLayer& layer, const compat::ConstSlice<std::uint8_t> mask,
    int maskWidth, int maskHeight, int maskLeft, int maskTop, bool positionRelativeToLayer,
    std::uint8_t defaultMask = 255);
DepthDrawLayerPairingResult ngDepthDrawAttachNormalCoverage(std::shared_ptr<DepthDrawSession> depthSession,
    const compat::ConstSlice<DepthDrawLayer> normalLayers);
}

namespace nijigenerate::viewport::depth::draw::source {
using ::nijigenerate::viewport::depth::draw::DepthDrawPsdLoadResult;
using ::nijigenerate::viewport::depth::draw::DepthDrawLayerPairingResult;
using ::nijigenerate::viewport::depth::draw::DepthDrawPsdRetainedBytesPerPixel;
using ::nijigenerate::viewport::depth::draw::DepthDrawMaxPsdRetainedBytes;
using ::nijigenerate::viewport::depth::draw::DepthDrawPngRetainedBytesPerPixel;
using ::nijigenerate::viewport::depth::draw::DepthDrawMaxPngRetainedBytes;
using ::nijigenerate::viewport::depth::draw::ngReserveDepthDrawPsdRetainedLayer;
using ::nijigenerate::viewport::depth::draw::ngReserveDepthDrawPngRetainedLayer;
using ::nijigenerate::viewport::depth::draw::ngLoadDepthDrawPngLayer;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawApplyClippingBaseCoverage;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawPsdLayerHasPixelData;
using ::nijigenerate::viewport::depth::draw::ngLoadDepthDrawPsd;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawApplyMaskToLayerAlpha;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawAttachNormalCoverage;
}

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_SOURCE_VALUE_SOURCE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.draw.source." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.draw.source." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_DEPTH_SOURCE_VALUE_SOURCE_ORIGIN(DepthDrawPsdLoadResult);
NG_DEPTH_SOURCE_VALUE_SOURCE_ORIGIN(DepthDrawLayerPairingResult);
#undef NG_DEPTH_SOURCE_VALUE_SOURCE_ORIGIN
} // namespace nicxlive::core::d_runtime
