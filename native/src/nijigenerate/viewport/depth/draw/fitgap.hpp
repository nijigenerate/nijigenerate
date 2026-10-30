#pragma once
#include "nijigenerate/compat/retained_string.hpp"
#include <limits>
#include <nijigenerate/compat/const_slice.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::viewport::depth::draw {
struct DepthDrawLayer;
struct DepthDrawRange {
    bool valid{};
    float minDepth = std::numeric_limits<float>::quiet_NaN();
    float maxDepth = std::numeric_limits<float>::quiet_NaN();
};
struct DepthDrawFitZResult {
    bool succeeded{};
    compat::RetainedString error;
    float zScale = std::numeric_limits<float>::quiet_NaN();
    float zOffset = std::numeric_limits<float>::quiet_NaN();
    float targetMin = std::numeric_limits<float>::quiet_NaN();
    float targetMax = std::numeric_limits<float>::quiet_NaN();
};
struct DepthDrawFitZGap {
    bool valid{};
    compat::RetainedString error;
    float back = std::numeric_limits<float>::quiet_NaN();
    float front = std::numeric_limits<float>::quiet_NaN();
};
struct DepthDrawFitZDiagnostics {
    bool rawRangeValid{};
    float rawMin = std::numeric_limits<float>::quiet_NaN();
    float rawMax = std::numeric_limits<float>::quiet_NaN();
    bool gapValid{};
    float gapBack = std::numeric_limits<float>::quiet_NaN();
    float gapFront = std::numeric_limits<float>::quiet_NaN();
    float targetMin = std::numeric_limits<float>::quiet_NaN();
    float targetMax = std::numeric_limits<float>::quiet_NaN();
    float zScale = std::numeric_limits<float>::quiet_NaN();
    float zOffset = std::numeric_limits<float>::quiet_NaN();
    bool succeeded{};
    compat::RetainedString error;
};

DepthDrawRange ngDepthDrawRangeFromValues(const compat::ConstSlice<float> values);
DepthDrawFitZGap ngDepthDrawGapFromAdjacentRanges(const compat::ConstSlice<DepthDrawRange> ranges,
    std::size_t layerIndex);
DepthDrawFitZGap ngDepthDrawGapFromTargetRange(DepthDrawRange range);
DepthDrawFitZGap ngDepthDrawGapFromSelectedLayerRanges(DepthDrawRange back, DepthDrawRange front);
DepthDrawFitZResult ngDepthDrawFitZToGap(DepthDrawRange rawRange, DepthDrawFitZGap gap, float margin = 0.0f);
DepthDrawFitZResult ngDepthDrawFitZToGap(DepthDrawRange rawRange,
    float gapBack, float gapFront, float margin = 0.0f);
DepthDrawFitZDiagnostics ngDepthDrawFitZDiagnostics(DepthDrawRange rawRange,
    DepthDrawFitZGap gap, DepthDrawFitZResult fit);
DepthDrawFitZResult ngDepthDrawApplyFitZToGap(DepthDrawLayer& layer,
    DepthDrawRange rawRange, float gapBack, float gapFront, float margin = 0.0f);
} // namespace nijigenerate::viewport::depth::draw

namespace nijigenerate::viewport::depth::draw::fitgap {
using ::nijigenerate::viewport::depth::draw::DepthDrawRange;
using ::nijigenerate::viewport::depth::draw::DepthDrawFitZResult;
using ::nijigenerate::viewport::depth::draw::DepthDrawFitZGap;
using ::nijigenerate::viewport::depth::draw::DepthDrawFitZDiagnostics;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawRangeFromValues;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGapFromAdjacentRanges;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGapFromTargetRange;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawGapFromSelectedLayerRanges;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawFitZToGap;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawFitZDiagnostics;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawApplyFitZToGap;
}

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_FITGAP_VALUE_SOURCE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.draw.fitgap." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.draw.fitgap." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_DEPTH_FITGAP_VALUE_SOURCE_ORIGIN(DepthDrawRange);
NG_DEPTH_FITGAP_VALUE_SOURCE_ORIGIN(DepthDrawFitZResult);
NG_DEPTH_FITGAP_VALUE_SOURCE_ORIGIN(DepthDrawFitZGap);
NG_DEPTH_FITGAP_VALUE_SOURCE_ORIGIN(DepthDrawFitZDiagnostics);
#undef NG_DEPTH_FITGAP_VALUE_SOURCE_ORIGIN
} // namespace nicxlive::core::d_runtime
