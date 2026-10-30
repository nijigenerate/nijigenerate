#include "gpu_internal.hpp"
#include <cmath>
namespace nijigenerate::viewport::depth::draw::gpu_detail {
using io::depthimage::DepthImageChannel;
using io::depthimage::DepthImageConvolution;
bool isValidDepthDrawGpuChannel(int channelValue) {
    if (channelValue < static_cast<std::int32_t>(DepthImageChannel::AverageRGB) ||
        channelValue > static_cast<std::int32_t>(DepthImageChannel::Luminance)) return false;
    auto channel = static_cast<DepthImageChannel>(channelValue);
    switch (channel) {
        case DepthImageChannel::AverageRGB:
        case DepthImageChannel::R:
        case DepthImageChannel::G:
        case DepthImageChannel::B:
        case DepthImageChannel::Luminance:
            return true;
        default:
            return false;
    }
}

bool isValidDepthDrawGpuConvolution(int convolutionValue) {
    if (convolutionValue < static_cast<std::int32_t>(DepthImageConvolution::Nearest) ||
        convolutionValue > static_cast<std::int32_t>(DepthImageConvolution::BackmostCustom)) return false;
    auto convolution = static_cast<DepthImageConvolution>(convolutionValue);
    switch (convolution) {
        case DepthImageConvolution::Nearest:
        case DepthImageConvolution::Box3x3:
        case DepthImageConvolution::Box5x5:
        case DepthImageConvolution::Gaussian3x3:
        case DepthImageConvolution::Gaussian5x5:
        case DepthImageConvolution::Median3x3:
        case DepthImageConvolution::Frontmost3x3:
        case DepthImageConvolution::Backmost3x3:
        case DepthImageConvolution::BoxCustom:
        case DepthImageConvolution::GaussianCustom:
        case DepthImageConvolution::MedianCustom:
        case DepthImageConvolution::FrontmostCustom:
        case DepthImageConvolution::BackmostCustom:
            return true;
        default:
            return false;
    }
}

void validateDepthDrawGpuPacket(const DepthDrawGpuComposePacket& packet) {
    enforce(packet.vertices.size() > 0, "DepthDraw GPU packet has no vertices", 1373);
    enforce(packet.vertices.size() <= DepthDrawGpuMaxVertices, "DepthDraw GPU packet exceeds maximum vertex count", 1374);
    enforce(packet.documentPositions.size() == packet.vertices.size(),
        "DepthDraw GPU document-position buffer length mismatch", 1375);
    enforce(packet.baseDepths.size() == packet.vertices.size(),
        "DepthDraw GPU base-depth buffer length mismatch", 1377);
    enforce(packet.layers.size() > 0, "DepthDraw GPU packet has no layers", 1379);
    enforce(packet.layers.size() <= DepthDrawGpuMaxLayers, "DepthDraw GPU packet exceeds maximum layer count", 1380);
    enforce(packet.bindings.size() == packet.layers.size(),
        "DepthDraw GPU binding/layer count mismatch", 1381);

    for (std::size_t i=0; i<packet.layers.size(); ++i) { const auto layer=packet.layers[i];
        enforce(layer.width > 0 && layer.height > 0, "DepthDraw GPU layer has invalid dimensions", 1385);
        auto pixelCount = static_cast<std::size_t>(layer.width) * static_cast<std::size_t>(layer.height) * 4;
        enforce(static_cast<std::size_t>(layer.depthPixelOffset) + pixelCount <= packet.depthPixels.size(),
            "DepthDraw GPU layer depth pixel range is out of bounds", 1387);
        if (layer.normalCoverageOffset != UINT32_MAX) {
            enforce(static_cast<std::size_t>(layer.normalCoverageOffset) + pixelCount <= packet.normalCoveragePixels.size(),
                "DepthDraw GPU layer normal coverage range is out of bounds", 1390);
        }
        enforce(std::isfinite(layer.xyScaleX) && std::isfinite(layer.xyScaleY) &&
            std::isfinite(layer.zOffset) && std::isfinite(layer.zScale) &&
            std::isfinite(layer.backDepth) && std::isfinite(layer.frontDepth) &&
            std::isfinite(layer.alphaThreshold),
            "DepthDraw GPU layer contains non-finite transform or sampling values", 1393);
        enforce(isValidDepthDrawGpuChannel(layer.channel), "DepthDraw GPU layer contains invalid channel", 1398);
        enforce(isValidDepthDrawGpuConvolution(layer.convolution), "DepthDraw GPU layer contains invalid convolution", 1399);
        enforce(layer.customRadius >= 0 && layer.customRadius <= DepthDrawGpuMaxCustomRadius,
            "DepthDraw GPU layer contains invalid custom radius", 1400);
        enforce(packet.bindings[i].layerIndex == i, "DepthDraw GPU binding layer index mismatch", 1402);
    }
}

void validateDepthDrawGpuLayerSamplePacket(const DepthDrawGpuLayerSamplePacket& packet) {
    enforce(packet.documentPositions.size() > 0, "DepthDraw GPU layer sample packet has no document positions", 1407);
    enforce(packet.documentPositions.size() <= DepthDrawGpuMaxVertices,
        "DepthDraw GPU layer sample packet exceeds maximum vertex count", 1408);
    enforce(packet.layer.width > 0 && packet.layer.height > 0,
        "DepthDraw GPU layer sample packet has invalid layer dimensions", 1410);
    enforce(packet.layer.depthPixelOffset == 0,
        "DepthDraw GPU layer sample packet must use zero depth-pixel offset", 1412);
    enforce(packet.layer.normalCoverageOffset == 0 || packet.layer.normalCoverageOffset == UINT32_MAX,
        "DepthDraw GPU layer sample packet must use zero normal-coverage offset", 1414);
    enforce(packet.binding.layerIndex == 0,
        "DepthDraw GPU layer sample binding must reference the zero-offset layer", 1416);
    auto pixelCount = static_cast<std::size_t>(packet.layer.width) * static_cast<std::size_t>(packet.layer.height) * DepthDrawGpuRgbaPixelStride;
    enforce(packet.depthPixels.size() == pixelCount,
        "DepthDraw GPU layer sample packet has invalid depth pixel length", 1419);
    if (packet.layer.normalCoverageOffset != UINT32_MAX) {
        enforce(packet.normalCoveragePixels.size() == pixelCount,
            "DepthDraw GPU layer sample packet has invalid normal coverage pixel length", 1422);
    } else {
        enforce(packet.normalCoveragePixels.size() == 0,
            "DepthDraw GPU layer sample packet has unexpected normal coverage pixels", 1425);
    }
    enforce(std::isfinite(packet.layer.xyScaleX) && std::isfinite(packet.layer.xyScaleY) &&
        std::isfinite(packet.layer.zOffset) && std::isfinite(packet.layer.zScale) &&
        std::isfinite(packet.layer.backDepth) && std::isfinite(packet.layer.frontDepth) &&
        std::isfinite(packet.layer.alphaThreshold),
        "DepthDraw GPU layer sample packet contains non-finite transform or sampling values", 1428);
    enforce(isValidDepthDrawGpuChannel(packet.layer.channel),
        "DepthDraw GPU layer sample packet contains invalid channel", 1433);
    enforce(isValidDepthDrawGpuConvolution(packet.layer.convolution),
        "DepthDraw GPU layer sample packet contains invalid convolution", 1435);
    enforce(packet.layer.customRadius >= 0 && packet.layer.customRadius <= DepthDrawGpuMaxCustomRadius,
        "DepthDraw GPU layer sample packet contains invalid custom radius", 1437);
}

void validateDepthDrawGpuReadback(
    const DepthDrawGpuComposePacket& packet,
    const DepthDrawGpuComposeReadback& readback
) {
    enforce(readback.targetGridUuid == packet.targetGridUuid, "DepthDraw GPU readback target mismatch", 1485);
    enforce(readback.depths.size() == packet.vertices.size(), "DepthDraw GPU readback depth length mismatch", 1486);
    enforce(readback.winningLayerIndices.size() == packet.vertices.size(),
        "DepthDraw GPU readback winner length mismatch", 1487);
    enforce(readback.layers.size() == packet.layers.size(), "DepthDraw GPU readback layer count mismatch", 1489);

    for (std::size_t i=0; i<readback.depths.size(); ++i) { const auto depth=readback.depths[i];
        enforce(std::isfinite(depth), "DepthDraw GPU readback contains non-finite final depth", 1492);
        auto winner = readback.winningLayerIndices[i];
        enforce(winner >= -1 && winner < static_cast<std::int32_t>(packet.layers.size()),
            "DepthDraw GPU readback winner index is out of bounds", 1494);
    }

    for (std::size_t i=0; i<readback.layers.size(); ++i) { const auto layerReadback=readback.layers[i];
        enforce(layerReadback.layerIndex == i, "DepthDraw GPU readback layer index mismatch", 1499);
        enforce(layerReadback.validSamples.size() == packet.vertices.size(),
            "DepthDraw GPU readback valid-sample length mismatch", 1500);
        enforce(layerReadback.sampleDepths.size() == packet.vertices.size(),
            "DepthDraw GPU readback sample-depth length mismatch", 1502);
        for (std::size_t j=0; j<layerReadback.validSamples.size(); ++j) { const auto valid=layerReadback.validSamples[j];
            enforce(valid == 0 || valid == 1, "DepthDraw GPU readback valid-sample value must be 0 or 1", 1505);
            if (valid != 0) {
                enforce(std::isfinite(layerReadback.sampleDepths[j]),
                    "DepthDraw GPU readback contains non-finite sample depth", 1507);
            }
        }
    }
}
}
