module nijigenerate.viewport.depth.draw.readback;

import nijigenerate.viewport.depth.draw.composer : DepthDrawComposeResult;
import std.math : abs, isFinite;

struct DepthDrawReadbackComparison {
    bool ok = true;
    string error;
    size_t depthMismatches;
    size_t winnerMismatches;
    size_t missingFlagMismatches;
    size_t targetMismatches;
    size_t layerStatMismatches;
    size_t aggregateStatMismatches;
}

private bool sameDepth(float a, float b, float tolerance) {
    if (a != a || b != b) return a != a && b != b;
    return abs(a - b) <= tolerance;
}

private void fail(ref DepthDrawReadbackComparison result, string error) {
    if (result.ok) {
        result.ok = false;
        result.error = error;
    }
}

DepthDrawReadbackComparison ngCompareDepthDrawReadback(
    const(float)[] cpuDepths,
    const(string)[] cpuWinners,
    const(float)[] readbackDepths,
    const(string)[] readbackWinners,
    float tolerance = 0.00001f
) {
    DepthDrawReadbackComparison result;
    if (!tolerance.isFinite || tolerance < 0.0f) {
        fail(result, "DepthDraw readback tolerance must be finite and non-negative");
        return result;
    }
    if (cpuDepths.length != readbackDepths.length) {
        fail(result, "DepthDraw readback depth length mismatch");
        return result;
    }
    if (cpuWinners.length != readbackWinners.length) {
        fail(result, "DepthDraw readback winner length mismatch");
        return result;
    }
    if (cpuDepths.length != cpuWinners.length) {
        fail(result, "DepthDraw CPU depth/winner length mismatch");
        return result;
    }

    foreach (i, cpuDepth; cpuDepths) {
        if (!sameDepth(cpuDepth, readbackDepths[i], tolerance)) {
            result.depthMismatches++;
            fail(result, "DepthDraw readback depth mismatch");
        }
        auto cpuMissing = cpuWinners[i].length == 0;
        auto readbackMissing = readbackWinners[i].length == 0;
        if (cpuMissing != readbackMissing) {
            result.missingFlagMismatches++;
            fail(result, "DepthDraw readback missing flag mismatch");
        }
        if (cpuWinners[i] != readbackWinners[i]) {
            result.winnerMismatches++;
            fail(result, "DepthDraw readback winner mismatch");
        }
    }
    return result;
}

DepthDrawReadbackComparison ngCompareDepthDrawComposeReadback(
    const(DepthDrawComposeResult) cpu,
    const(DepthDrawComposeResult) readback,
    float tolerance = 0.00001f
) {
    auto result = ngCompareDepthDrawReadback(
        cpu.depths,
        cpu.winningLayerIds,
        readback.depths,
        readback.winningLayerIds,
        tolerance
    );
    if (cpu.targetGridUuid != readback.targetGridUuid) {
        result.targetMismatches++;
        fail(result, "DepthDraw readback target GridDeformer mismatch");
    }
    compareAggregateStats(result, cpu, readback, tolerance);
    compareLayerStats(result, cpu, readback, tolerance);
    return result;
}

private void compareAggregateStats(
    ref DepthDrawReadbackComparison result,
    const(DepthDrawComposeResult) cpu,
    const(DepthDrawComposeResult) readback,
    float tolerance
) {
    if (cpu.sampledVertices != readback.sampledVertices ||
        cpu.missingVertices != readback.missingVertices ||
        cpu.hasDepthRange != readback.hasDepthRange ||
        (cpu.hasDepthRange && (
            !sameDepth(cpu.minDepth, readback.minDepth, tolerance) ||
            !sameDepth(cpu.maxDepth, readback.maxDepth, tolerance)
        ))
    ) {
        result.aggregateStatMismatches++;
        fail(result, "DepthDraw readback aggregate stats mismatch");
    }
}

private void compareLayerStats(
    ref DepthDrawReadbackComparison result,
    const(DepthDrawComposeResult) cpu,
    const(DepthDrawComposeResult) readback,
    float tolerance
) {
    if (cpu.layerStats.length != readback.layerStats.length) {
        result.layerStatMismatches++;
        fail(result, "DepthDraw readback layer stats length mismatch");
        return;
    }
    foreach (i, cpuStats; cpu.layerStats) {
        auto readbackStats = readback.layerStats[i];
        if (cpuStats.layerId != readbackStats.layerId ||
            cpuStats.sampledVertices != readbackStats.sampledVertices ||
            cpuStats.missingVertices != readbackStats.missingVertices ||
            cpuStats.winningVertices != readbackStats.winningVertices ||
            cpuStats.contributedVertices != readbackStats.contributedVertices ||
            cpuStats.additiveMerge != readbackStats.additiveMerge ||
            cpuStats.hasDepthRange != readbackStats.hasDepthRange ||
            (cpuStats.hasDepthRange && (
                !sameDepth(cpuStats.minDepth, readbackStats.minDepth, tolerance) ||
                !sameDepth(cpuStats.maxDepth, readbackStats.maxDepth, tolerance)
            ))
        ) {
            result.layerStatMismatches++;
            fail(result, "DepthDraw readback layer stats mismatch");
        }
    }
}
