#include "nijigenerate/viewport/depth/draw/fitgap.hpp"
#include "nijigenerate/viewport/depth/draw/layer.hpp"
#include <algorithm>
#include <cmath>
#include <nijigenerate/compat/array_range_error.hpp>

namespace nijigenerate::viewport::depth::draw {
DepthDrawRange ngDepthDrawRangeFromValues(const compat::ConstSlice<float> values) {
    DepthDrawRange result;
    for (const float value : values) {
        if (!std::isfinite(value)) continue;
        if (!result.valid) {
            result.valid = true;
            result.minDepth = value;
            result.maxDepth = value;
        } else {
            result.minDepth = std::min(result.minDepth, value);
            result.maxDepth = std::max(result.maxDepth, value);
        }
    }
    return result;
}
DepthDrawFitZGap ngDepthDrawGapFromAdjacentRanges(const compat::ConstSlice<DepthDrawRange> ranges,
        std::size_t index) {
    DepthDrawFitZGap gap;
    if (index == 0 || index + 1 >= ranges.size()) {
        gap.error = compat::RetainedString::fromStaticLiteral("adjacent layers are required");
        return gap;
    }
    // Preserve the source's wrapping size_t check, then its checked array access.
    if (index - 1 >= ranges.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index - 1, ranges.size(),
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/depth/draw/fitgap.d"), 67);
    const auto back = ranges[index - 1];
    const auto front = ranges[index + 1];
    if (!back.valid) {
        gap.error = compat::RetainedString::fromStaticLiteral("back adjacent layer has no usable range");
        return gap;
    }
    if (!front.valid) {
        gap.error = compat::RetainedString::fromStaticLiteral("front adjacent layer has no usable range");
        return gap;
    }
    gap.back = back.maxDepth;
    gap.front = front.minDepth;
    if (gap.front < gap.back) {
        gap.error = compat::RetainedString::fromStaticLiteral("adjacent layer ranges overlap");
        return gap;
    }
    gap.valid = true;
    return gap;
}
DepthDrawFitZGap ngDepthDrawGapFromTargetRange(DepthDrawRange range) {
    DepthDrawFitZGap gap;
    if (!range.valid) {
        gap.error = compat::RetainedString::fromStaticLiteral("target has no usable depth range");
        return gap;
    }
    gap.valid = true;
    gap.back = range.minDepth;
    gap.front = range.maxDepth;
    return gap;
}
DepthDrawFitZGap ngDepthDrawGapFromSelectedLayerRanges(DepthDrawRange back, DepthDrawRange front) {
    DepthDrawFitZGap gap;
    if (!back.valid) {
        gap.error = compat::RetainedString::fromStaticLiteral("selected back layer has no usable range");
        return gap;
    }
    if (!front.valid) {
        gap.error = compat::RetainedString::fromStaticLiteral("selected front layer has no usable range");
        return gap;
    }
    gap.back = back.maxDepth;
    gap.front = front.minDepth;
    if (gap.front < gap.back) {
        gap.error = compat::RetainedString::fromStaticLiteral("selected layer ranges overlap");
        return gap;
    }
    gap.valid = true;
    return gap;
}
DepthDrawFitZResult ngDepthDrawFitZToGap(DepthDrawRange rawRange, DepthDrawFitZGap gap, float margin) {
    if (!gap.valid) {
        DepthDrawFitZResult result;
        result.error = !gap.error.empty() ? gap.error : compat::RetainedString::fromStaticLiteral("gap is invalid");
        return result;
    }
    return ngDepthDrawFitZToGap(rawRange, gap.back, gap.front, margin);
}
DepthDrawFitZResult ngDepthDrawFitZToGap(DepthDrawRange rawRange, float gapBack, float gapFront, float margin) {
    DepthDrawFitZResult result;
    if (!rawRange.valid) {
        result.error = compat::RetainedString::fromStaticLiteral("raw range has no usable samples");
        return result;
    }
    if (!std::isfinite(gapBack) || !std::isfinite(gapFront) || !std::isfinite(margin)) {
        result.error = compat::RetainedString::fromStaticLiteral("gap values must be finite");
        return result;
    }
    result.targetMin = gapBack + margin;
    result.targetMax = gapFront - margin;
    if (result.targetMax <= result.targetMin) {
        result.error = compat::RetainedString::fromStaticLiteral("gap is empty after margin");
        return result;
    }
    const auto span = rawRange.maxDepth - rawRange.minDepth;
    if (span == 0.0f) {
        result.zScale = 1.0f;
        result.zOffset = ((result.targetMin + result.targetMax) * 0.5f) - rawRange.minDepth;
    } else {
        result.zScale = (result.targetMax - result.targetMin) / span;
        result.zOffset = result.targetMin - rawRange.minDepth * result.zScale;
    }
    result.succeeded = true;
    return result;
}
DepthDrawFitZDiagnostics ngDepthDrawFitZDiagnostics(DepthDrawRange rawRange,
    DepthDrawFitZGap gap, DepthDrawFitZResult fit) {
    DepthDrawFitZDiagnostics result;
    result.rawRangeValid = rawRange.valid;
    result.rawMin = rawRange.minDepth; result.rawMax = rawRange.maxDepth;
    result.gapValid = gap.valid; result.gapBack = gap.back; result.gapFront = gap.front;
    result.targetMin = fit.targetMin; result.targetMax = fit.targetMax;
    result.zScale = fit.zScale; result.zOffset = fit.zOffset;
    result.succeeded = fit.succeeded;
    result.error = !fit.error.empty() ? fit.error : gap.error;
    return result;
}
DepthDrawFitZResult ngDepthDrawApplyFitZToGap(DepthDrawLayer& layer,
    DepthDrawRange rawRange, float back, float front, float margin) {
    auto result = ngDepthDrawFitZToGap(rawRange, back, front, margin);
    if (result.succeeded) {
        layer.zScale = result.zScale;
        layer.zOffset = result.zOffset;
    }
    return result;
}
} // namespace nijigenerate::viewport::depth::draw
