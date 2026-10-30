#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <compat/object_metadata.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace nijigenerate::windows::psddepthmap {

struct PsdDepthDialogLayerPixels {
    compat::RetainedStringSlice layerPath;
    std::uint64_t targetGridUuid = 0;
    compat::Slice<std::uint8_t> depthRgba;
    bool alphaDepthGapFillApplied = false;
};

// D float.init is NaN; aggregate copies retain each Slice's existing storage.
struct PsdDepthDialogOverallPreview {
    std::int32_t width = 0;
    std::int32_t height = 0;
    float yaw = std::numeric_limits<float>::quiet_NaN();
    float pitch = std::numeric_limits<float>::quiet_NaN();
    float zoom = std::numeric_limits<float>::quiet_NaN();
    float panX = std::numeric_limits<float>::quiet_NaN();
    float panY = std::numeric_limits<float>::quiet_NaN();
    std::size_t coveredPixels = 0;
    float minDepth = std::numeric_limits<float>::quiet_NaN();
    float maxDepth = std::numeric_limits<float>::quiet_NaN();
    compat::Slice<float> depths;
    compat::Slice<std::uint8_t> rgba;
};

struct PsdDepth3DAdjustSample {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint8_t depthByte = 0;
    float sourceDepth = std::numeric_limits<float>::quiet_NaN();
    float depth = std::numeric_limits<float>::quiet_NaN();
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
};

struct PsdDepth3DAdjustMesh {
    compat::Slice<float> vertexData;
    compat::Slice<float> sourceDepths;
    compat::Slice<std::uint32_t> indices;
};

} // namespace nijigenerate::windows::psddepthmap

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PsdDepthDialogLayerPixels> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.psddepthmap.PsdDepthDialogLayerPixels)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthDialogLayerPixels)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.psddepthmap.PsdDepthDialogLayerPixels";
    inline static constexpr char unqualifiedElement[] = "PsdDepthDialogLayerPixels";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PsdDepthDialogOverallPreview> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.psddepthmap.PsdDepthDialogOverallPreview)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthDialogOverallPreview)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.psddepthmap.PsdDepthDialogOverallPreview";
    inline static constexpr char unqualifiedElement[] = "PsdDepthDialogOverallPreview";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PsdDepth3DAdjustSample> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.psddepthmap.PsdDepth3DAdjustSample)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepth3DAdjustSample)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.psddepthmap.PsdDepth3DAdjustSample";
    inline static constexpr char unqualifiedElement[] = "PsdDepth3DAdjustSample";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PsdDepth3DAdjustMesh> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.psddepthmap.PsdDepth3DAdjustMesh)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepth3DAdjustMesh)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.psddepthmap.PsdDepth3DAdjustMesh";
    inline static constexpr char unqualifiedElement[] = "PsdDepth3DAdjustMesh";
};
}
