module nijigenerate.viewport.depth.draw.fitgap;

import nijigenerate.viewport.depth.draw.layer;
import std.algorithm : max, min;
import std.math : isFinite;

struct DepthDrawRange {
    bool valid;
    float minDepth;
    float maxDepth;
}

struct DepthDrawFitZResult {
    bool succeeded;
    string error;
    float zScale;
    float zOffset;
    float targetMin;
    float targetMax;
}

struct DepthDrawFitZGap {
    bool valid;
    string error;
    float back;
    float front;
}

struct DepthDrawFitZDiagnostics {
    bool rawRangeValid;
    float rawMin;
    float rawMax;
    bool gapValid;
    float gapBack;
    float gapFront;
    float targetMin;
    float targetMax;
    float zScale;
    float zOffset;
    bool succeeded;
    string error;
}

DepthDrawRange ngDepthDrawRangeFromValues(const(float)[] values) {
    DepthDrawRange result;
    foreach (value; values) {
        if (!value.isFinite) continue;
        if (!result.valid) {
            result.valid = true;
            result.minDepth = value;
            result.maxDepth = value;
        } else {
            result.minDepth = min(result.minDepth, value);
            result.maxDepth = max(result.maxDepth, value);
        }
    }
    return result;
}

DepthDrawFitZGap ngDepthDrawGapFromAdjacentRanges(const(DepthDrawRange)[] layerRanges, size_t layerIndex) {
    DepthDrawFitZGap gap;
    if (layerIndex == 0 || layerIndex + 1 >= layerRanges.length) {
        gap.error = "adjacent layers are required";
        return gap;
    }

    auto backRange = layerRanges[layerIndex - 1];
    auto frontRange = layerRanges[layerIndex + 1];
    if (!backRange.valid) {
        gap.error = "back adjacent layer has no usable range";
        return gap;
    }
    if (!frontRange.valid) {
        gap.error = "front adjacent layer has no usable range";
        return gap;
    }

    gap.back = backRange.maxDepth;
    gap.front = frontRange.minDepth;
    if (gap.front < gap.back) {
        gap.error = "adjacent layer ranges overlap";
        return gap;
    }
    gap.valid = true;
    return gap;
}

DepthDrawFitZGap ngDepthDrawGapFromTargetRange(DepthDrawRange targetRange) {
    DepthDrawFitZGap gap;
    if (!targetRange.valid) {
        gap.error = "target has no usable depth range";
        return gap;
    }
    gap.valid = true;
    gap.back = targetRange.minDepth;
    gap.front = targetRange.maxDepth;
    return gap;
}

DepthDrawFitZGap ngDepthDrawGapFromSelectedLayerRanges(DepthDrawRange backRange, DepthDrawRange frontRange) {
    DepthDrawFitZGap gap;
    if (!backRange.valid) {
        gap.error = "selected back layer has no usable range";
        return gap;
    }
    if (!frontRange.valid) {
        gap.error = "selected front layer has no usable range";
        return gap;
    }
    gap.back = backRange.maxDepth;
    gap.front = frontRange.minDepth;
    if (gap.front < gap.back) {
        gap.error = "selected layer ranges overlap";
        return gap;
    }
    gap.valid = true;
    return gap;
}

DepthDrawFitZResult ngDepthDrawFitZToGap(
    DepthDrawRange rawRange,
    DepthDrawFitZGap gap,
    float margin = 0.0f
) {
    if (!gap.valid) {
        DepthDrawFitZResult result;
        result.error = gap.error.length ? gap.error : "gap is invalid";
        return result;
    }
    return ngDepthDrawFitZToGap(rawRange, gap.back, gap.front, margin);
}

DepthDrawFitZResult ngDepthDrawFitZToGap(
    DepthDrawRange rawRange,
    float gapBack,
    float gapFront,
    float margin = 0.0f
) {
    DepthDrawFitZResult result;
    if (!rawRange.valid) {
        result.error = "raw range has no usable samples";
        return result;
    }
    if (!gapBack.isFinite || !gapFront.isFinite || !margin.isFinite) {
        result.error = "gap values must be finite";
        return result;
    }

    result.targetMin = gapBack + margin;
    result.targetMax = gapFront - margin;
    if (result.targetMax <= result.targetMin) {
        result.error = "gap is empty after margin";
        return result;
    }

    auto rawSpan = rawRange.maxDepth - rawRange.minDepth;
    if (rawSpan == 0.0f) {
        result.zScale = 1.0f;
        result.zOffset = ((result.targetMin + result.targetMax) * 0.5f) - rawRange.minDepth;
    } else {
        result.zScale = (result.targetMax - result.targetMin) / rawSpan;
        result.zOffset = result.targetMin - rawRange.minDepth * result.zScale;
    }
    result.succeeded = true;
    return result;
}

DepthDrawFitZDiagnostics ngDepthDrawFitZDiagnostics(
    DepthDrawRange rawRange,
    DepthDrawFitZGap gap,
    DepthDrawFitZResult fit
) {
    DepthDrawFitZDiagnostics diagnostics;
    diagnostics.rawRangeValid = rawRange.valid;
    diagnostics.rawMin = rawRange.minDepth;
    diagnostics.rawMax = rawRange.maxDepth;
    diagnostics.gapValid = gap.valid;
    diagnostics.gapBack = gap.back;
    diagnostics.gapFront = gap.front;
    diagnostics.targetMin = fit.targetMin;
    diagnostics.targetMax = fit.targetMax;
    diagnostics.zScale = fit.zScale;
    diagnostics.zOffset = fit.zOffset;
    diagnostics.succeeded = fit.succeeded;
    diagnostics.error = fit.error.length ? fit.error : gap.error;
    return diagnostics;
}

DepthDrawFitZResult ngDepthDrawApplyFitZToGap(
    ref DepthDrawLayer layer,
    DepthDrawRange rawRange,
    float gapBack,
    float gapFront,
    float margin = 0.0f
) {
    auto result = ngDepthDrawFitZToGap(rawRange, gapBack, gapFront, margin);
    if (result.succeeded) {
        layer.zScale = result.zScale;
        layer.zOffset = result.zOffset;
    }
    return result;
}
