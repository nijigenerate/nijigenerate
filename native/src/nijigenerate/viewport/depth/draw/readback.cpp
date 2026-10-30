#include "nijigenerate/viewport/depth/draw/readback.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <cmath>
#include <string_view>

namespace nijigenerate::viewport::depth::draw {
namespace {
std::string_view text(const DepthDrawString& value) {
    return value.view();
}
bool sameDepth(float a, float b, float tolerance) {
    if (a != a || b != b) return a != a && b != b;
    return std::abs(a - b) <= tolerance;
}
void fail(DepthDrawReadbackComparison& result, compat::RetainedStringSlice error) {
    if (result.ok) {
        result.ok = false;
        result.error = error.retainedString();
    }
}
template<std::size_t N> void fail(DepthDrawReadbackComparison& result, const char (&error)[N]) {
    fail(result, compat::RetainedString::fromStaticLiteral(error));
}
void compareAggregateStats(DepthDrawReadbackComparison& result,
    const DepthDrawComposeResult cpu, const DepthDrawComposeResult readback, float tolerance) {
    if (cpu.sampledVertices != readback.sampledVertices || cpu.missingVertices != readback.missingVertices ||
        cpu.hasDepthRange != readback.hasDepthRange || (cpu.hasDepthRange &&
            (!sameDepth(cpu.minDepth, readback.minDepth, tolerance) ||
             !sameDepth(cpu.maxDepth, readback.maxDepth, tolerance)))) {
        ++result.aggregateStatMismatches;
        fail(result, "DepthDraw readback aggregate stats mismatch");
    }
}
void compareLayerStats(DepthDrawReadbackComparison& result,
    const DepthDrawComposeResult cpu, const DepthDrawComposeResult readback, float tolerance) {
    if (cpu.layerStats.size() != readback.layerStats.size()) {
        ++result.layerStatMismatches;
        fail(result, "DepthDraw readback layer stats length mismatch");
        return;
    }
    for (std::size_t i = 0; i < cpu.layerStats.size(); ++i) {
        const auto a = cpu.layerStats[i];
        const auto b = readback.layerStats[i];
        if (text(a.layerId) != text(b.layerId) || a.sampledVertices != b.sampledVertices ||
            a.missingVertices != b.missingVertices || a.winningVertices != b.winningVertices ||
            a.contributedVertices != b.contributedVertices || a.additiveMerge != b.additiveMerge ||
            a.hasDepthRange != b.hasDepthRange || (a.hasDepthRange &&
                (!sameDepth(a.minDepth, b.minDepth, tolerance) || !sameDepth(a.maxDepth, b.maxDepth, tolerance)))) {
            ++result.layerStatMismatches;
            fail(result, "DepthDraw readback layer stats mismatch");
        }
    }
}
}
DepthDrawReadbackComparison ngCompareDepthDrawReadback(
    const compat::ConstSlice<float> cpuDepths,
    const compat::ConstSlice<DepthDrawString> cpuWinners,
    const compat::ConstSlice<float> readbackDepths,
    const compat::ConstSlice<DepthDrawString> readbackWinners,
    float tolerance) {
    DepthDrawReadbackComparison result;
    if (!std::isfinite(tolerance) || tolerance < 0.0f) {
        fail(result, "DepthDraw readback tolerance must be finite and non-negative");
        return result;
    }
    if (cpuDepths.size() != readbackDepths.size()) {
        fail(result, "DepthDraw readback depth length mismatch");
        return result;
    }
    if (cpuWinners.size() != readbackWinners.size()) {
        fail(result, "DepthDraw readback winner length mismatch");
        return result;
    }
    if (cpuDepths.size() != cpuWinners.size()) {
        fail(result, "DepthDraw CPU depth/winner length mismatch");
        return result;
    }
    for (std::size_t i = 0; i < cpuDepths.size(); ++i) {
        if (!sameDepth(cpuDepths[i], readbackDepths[i], tolerance)) {
            ++result.depthMismatches;
            fail(result, "DepthDraw readback depth mismatch");
        }
        const auto cpu = text(cpuWinners[i]), readback = text(readbackWinners[i]);
        if (cpu.empty() != readback.empty()) {
            ++result.missingFlagMismatches;
            fail(result, "DepthDraw readback missing flag mismatch");
        }
        if (cpu != readback) {
            ++result.winnerMismatches;
            fail(result, "DepthDraw readback winner mismatch");
        }
    }
    return result;
}
DepthDrawReadbackComparison ngCompareDepthDrawComposeReadback(
    const DepthDrawComposeResult cpu, const DepthDrawComposeResult readback, float tolerance) {
    auto result = ngCompareDepthDrawReadback(cpu.depths, cpu.winningLayerIds,
        readback.depths, readback.winningLayerIds, tolerance);
    if (cpu.targetGridUuid != readback.targetGridUuid) {
        ++result.targetMismatches;
        fail(result, "DepthDraw readback target GridDeformer mismatch");
    }
    compareAggregateStats(result, cpu, readback, tolerance);
    compareLayerStats(result, cpu, readback, tolerance);
    return result;
}
} // namespace nijigenerate::viewport::depth::draw
