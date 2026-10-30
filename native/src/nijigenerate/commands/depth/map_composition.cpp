#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <ctime>
#endif

#include "map_composition.hpp"
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/io/depthmap_psd_composition.hpp>
#include <nijigenerate/io/depthmap_psd_enum_functions.hpp>
#include <nijigenerate/io/depthsample.hpp>
#include <nijigenerate/viewport/depth/draw/coordinate.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nodes/grid_deformer.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
namespace nijigenerate::commands::depth::map {
using namespace io::depthmap_psd;
using namespace io::depthimage;
using namespace io::depthsample;
using namespace viewport::depth::draw;
using detail::LayerMap;
using detail::updatePsdDepthGridRange;
using viewport::depth::draw::coordinate::ngDepthDrawLayerPixelFromDocument;
using viewport::depth::common::targetview::DepthTargetView;
namespace {
DepthDrawString drawString(compat::RetainedString s) {
    return s;
}
compat::RetainedString psdString(DepthDrawString s) {
    return s;
}
void sleepPsdDepthGpuPoll() {
    // The source calls Thread.sleep with the fixed positive duration of one millisecond.
#if defined(_WIN32)
    ::Sleep(1);
#else
    timespec requested{0, 1000000};
    timespec remaining;
    while (::nanosleep(&requested, &remaining) != 0) {
        if (errno != EINTR) {
            compat::ngAssertUnreachable("Unable to sleep for the specified duration", "core/thread/osthread.d", 1047);
        }
        requested = remaining;
    }
#endif
}
}
bool finalizePsdDepthTarget(PsdDepthImportResult&, PsdDepthGridResult&, compat::ConstSlice<float>,
    LayerMap&, compat::Slice<DepthDrawLayer>, DepthDrawComposeResult&, PsdDepthGridResult&, compat::RetainedString&);
bool composePsdDepthImportGpu(PsdDepthImportResult&, compat::RetainedString&);
DepthImageChannel psdDepthChannelToDepthImage(PsdDepthChannel channel) {
    switch (channel) {
        case PsdDepthChannel::AverageRGB: return DepthImageChannel::AverageRGB;
        case PsdDepthChannel::R: return DepthImageChannel::R;
        case PsdDepthChannel::G: return DepthImageChannel::G;
        case PsdDepthChannel::B: return DepthImageChannel::B;
        case PsdDepthChannel::Luminance: return DepthImageChannel::Luminance;
    }
    throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 391);
}

DepthImageConvolution ngPsdDepthConvolutionToDepthImage(PsdDepthConvolution convolution) {
    switch (convolution) {
        case PsdDepthConvolution::Nearest: return DepthImageConvolution::Nearest;
        case PsdDepthConvolution::Box3x3: return DepthImageConvolution::Box3x3;
        case PsdDepthConvolution::Box5x5: return DepthImageConvolution::Box5x5;
        case PsdDepthConvolution::Gaussian3x3: return DepthImageConvolution::Gaussian3x3;
        case PsdDepthConvolution::Gaussian5x5: return DepthImageConvolution::Gaussian5x5;
        case PsdDepthConvolution::Median3x3: return DepthImageConvolution::Median3x3;
        case PsdDepthConvolution::Frontmost3x3: return DepthImageConvolution::Frontmost3x3;
        case PsdDepthConvolution::Backmost3x3: return DepthImageConvolution::Backmost3x3;
        case PsdDepthConvolution::BoxCustom: return DepthImageConvolution::BoxCustom;
        case PsdDepthConvolution::GaussianCustom: return DepthImageConvolution::GaussianCustom;
        case PsdDepthConvolution::MedianCustom: return DepthImageConvolution::MedianCustom;
        case PsdDepthConvolution::FrontmostCustom: return DepthImageConvolution::FrontmostCustom;
        case PsdDepthConvolution::BackmostCustom: return DepthImageConvolution::BackmostCustom;
    }
    throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 401);
}

std::shared_ptr<PsdDepthComposedLayer> psdDepthComposedLayerByPath(
    PsdDepthImportResult& imported,
    compat::RetainedString layerPath,
    std::uint64_t targetGridUuid
) {
    if (targetGridUuid == 0) return nullptr;
    auto layers = imported.composedLayers;
    for (auto& layer : layers) {
        if (layer.layerPath == layerPath && layer.targetGridUuid == targetGridUuid) {
            auto* match = &layer;
            return std::shared_ptr<PsdDepthComposedLayer>(layers.retentionOwner(), match);
        }
    }
    return nullptr;
}

DepthDrawLayer psdDepthComposedLayerToDepthDrawLayer(PsdDepthComposedLayer& composedLayer) {
    DepthDrawLayer layer;
    layer.id = drawString(composedLayer.id.size() ? composedLayer.id : composedLayer.layerPath);
    layer.sourcePath = drawString(composedLayer.sourcePath);
    layer.layerPath = drawString(composedLayer.layerPath);
    layer.displayName = drawString(composedLayer.layerName);
    layer.width = composedLayer.width;
    layer.height = composedLayer.height;
    layer.bounds = DepthDrawRect(composedLayer.left, composedLayer.top, composedLayer.width, composedLayer.height);
    layer.rgba = composedLayer.depthRgba.duplicate();
    layer.depthPixels = composedLayer.depthRgba.duplicate();
    layer.alphaMask = composedLayer.maskRgba.duplicate();
    layer.opacity = 1.0f;
    layer.visible = composedLayer.visible;
    layer.enabled = composedLayer.enabled;
    layer.zOffset = composedLayer.depthOffset;
    layer.zScale = composedLayer.depthScale;
    layer.backDepth = composedLayer.backDepth;
    layer.frontDepth = composedLayer.frontDepth;
    layer.sampleDepthScale = composedLayer.sourceDepthScale;
    layer.invert = composedLayer.invert;
    layer.channel = DepthImageChannel::AverageRGB;
    layer.convolution = ngPsdDepthConvolutionToDepthImage(composedLayer.convolution);
    layer.customRadius = composedLayer.customRadius;
    layer.alphaThreshold = composedLayer.alphaThreshold;
    return layer;
}

std::shared_ptr<DepthDrawSession> ngPsdDepthComposedSourceToDepthDrawSession(PsdDepthComposedSource source) {
    auto session = std::allocate_shared<DepthDrawSession>(
        nicxlive::core::d_runtime::SourceAllocator<DepthDrawSession>{});
    session->documentWidth = source.width;
    session->documentHeight = source.height;
    int order{};
    for (auto& composedLayer : source.layers) {
        DepthDrawLayer layer = psdDepthComposedLayerToDepthDrawLayer(composedLayer);
        if (layer.id.empty()) layer.id = drawString(composedLayer.colorLayerPath.size() ?
            composedLayer.colorLayerPath : composedLayer.layerPath);
        session->layers.append(layer);
        if (session->selectedLayerId.empty() && layer.enabled) {
            session->selectedLayerId = layer.id;
        }
        if (composedLayer.targetGridUuid != 0) {
            DepthDrawBinding binding;
            binding.layerId = layer.id;
            binding.targetNodeUuid = composedLayer.targetGridUuid;
            binding.targetGridUuid = composedLayer.targetGridUuid;
            binding.order = order;
            order = io::depthsample::detail::add32(order, 1);
            binding.enabled = composedLayer.enabled;
            binding.useNormalLayerAlpha = false;
            binding.coverageThreshold = 0.0f;
            binding.mergePolicy = DepthMergePolicy::Frontmost;
            session->bindings.append(binding);
            if (session->selectedGridUuid == 0 && binding.enabled) {
                session->selectedGridUuid = binding.targetGridUuid;
            }
        }
    }
    return session;
}

std::shared_ptr<DepthDrawSession> ngPsdDepthImportResultToDepthDrawSession(PsdDepthImportResult imported) {
    return ngPsdDepthComposedSourceToDepthDrawSession(ngPsdDepthComposedSourceFromImportResult(imported));
}

DepthDrawPngExportResult ngExportPsdDepthComposedSourcePng(
    PsdDepthComposedSource source,
    compat::RetainedString outputDir,
    compat::RetainedString manifestPath
) {
    auto session = ngPsdDepthComposedSourceToDepthDrawSession(source);
    return ngExportDepthDrawPngSession(session, drawString(outputDir), drawString(manifestPath));
}

void buildPsdDepthDrawCompositePreview(PsdDepthGridResult& gridResult, compat::Slice<DepthDrawLayer> layers) {
    constexpr int MaxPreviewSize = 192;

    bool hasBounds{};
    int left{};
    int top{};
    int right{};
    int bottom{};
    for (auto& layer : layers) {
        if (!layer.enabled || !layer.visible || !layer.hasDepthPixels()) continue;
        auto layerRight = io::depthsample::detail::add32(layer.bounds.left, layer.bounds.width);
        auto layerBottom = io::depthsample::detail::add32(layer.bounds.top, layer.bounds.height);
        if (!hasBounds) {
            left = layer.bounds.left;
            top = layer.bounds.top;
            right = layerRight;
            bottom = layerBottom;
            hasBounds = true;
        } else {
            left = std::min(left, layer.bounds.left);
            top = std::min(top, layer.bounds.top);
            right = std::max(right, layerRight);
            bottom = std::max(bottom, layerBottom);
        }
    }
    if (!hasBounds || right <= left || bottom <= top) {
        gridResult.previewWidth = 0;
        gridResult.previewHeight = 0;
        gridResult.rawCompositePreviewRgba = {};
        gridResult.compositePreviewRgba = {};
        return;
    }

    auto sourceWidth = io::depthsample::detail::sub32(right, left);
    auto sourceHeight = io::depthsample::detail::sub32(bottom, top);
    auto scale = std::min(
        (float)MaxPreviewSize / (float)sourceWidth,
        (float)MaxPreviewSize / (float)sourceHeight
    );
    if (scale > 1.0f) scale = 1.0f;
    if (scale <= 0.0f) scale = 1.0f;

    auto width = std::max(1, compat::ngFloatToInt32(std::round((float)sourceWidth * scale)));
    auto height = std::max(1, compat::ngFloatToInt32(std::round((float)sourceHeight * scale)));
    gridResult.previewLeft = left;
    gridResult.previewTop = top;
    gridResult.previewWidth = width;
    gridResult.previewHeight = height;
    gridResult.rawCompositePreviewRgba.resize((std::size_t)width * (std::size_t)height * 4);
    gridResult.compositePreviewRgba.resize(gridResult.rawCompositePreviewRgba.size());
    std::fill(gridResult.rawCompositePreviewRgba.begin(), gridResult.rawCompositePreviewRgba.end(), 0);
    std::fill(gridResult.compositePreviewRgba.begin(), gridResult.compositePreviewRgba.end(), 0);

    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            auto documentPoint = nicxlive::core::math::Vec2(
                (float)left + ((float)px + 0.5f) / scale,
                (float)top + ((float)py + 0.5f) / scale
            );
            bool hasSample{};
            float bestDepth = std::numeric_limits<float>::quiet_NaN();
            float bestBackDepth = -1.0f;
            float bestFrontDepth = 1.0f;
            float bestDepthScale = 1.0f;
            for (auto& layer : layers) {
                if (!layer.enabled || !layer.visible || !layer.hasDepthPixels()) continue;
                auto layerPoint = ngDepthDrawLayerPixelFromDocument(layer, documentPoint);
                auto sample = io::depthimage::ngDepthImageSampleRgbaWithOpacitySource(
                    layer.depthPixels,
                    layer.width,
                    layer.height,
                    layerPoint.x,
                    layerPoint.y,
                    layer.opacity,
                    layer.sampleSettings()
                );
                if (!sample.valid) continue;
                auto depth = layer.applyZTransform(sample.value);
                if (!hasSample || depth > bestDepth) {
                    hasSample = true;
                    bestDepth = depth;
                    bestBackDepth = layer.backDepth;
                    bestFrontDepth = layer.frontDepth;
                    bestDepthScale = layer.sampleDepthScale;
                }
            }
            if (!hasSample) continue;
            auto depthIndex = (std::size_t)py * (std::size_t)width + (std::size_t)px;
            auto normalized = ngDepthSampleValueToDepth01(bestDepth, bestBackDepth, bestFrontDepth, bestDepthScale);
            auto gray = (std::uint8_t)std::round(normalized * 255.0f);
            auto outIndex = depthIndex * 4;
            gridResult.rawCompositePreviewRgba[outIndex + 0] = gray;
            gridResult.rawCompositePreviewRgba[outIndex + 1] = gray;
            gridResult.rawCompositePreviewRgba[outIndex + 2] = gray;
            gridResult.rawCompositePreviewRgba[outIndex + 3] = 255;
            gridResult.compositePreviewRgba[outIndex + 0] = gray;
            gridResult.compositePreviewRgba[outIndex + 1] = gray;
            gridResult.compositePreviewRgba[outIndex + 2] = gray;
            gridResult.compositePreviewRgba[outIndex + 3] = 255;
        }
    }
}

std::shared_ptr<DepthDrawSession> buildPsdDepthDrawSessionForGrid(
    PsdDepthImportResult& imported,
    PsdDepthGridResult& gridResult,
    LayerMap& layerIdToPath,
    compat::Slice<DepthDrawLayer>& previewLayers
) {
    layerIdToPath = {}; previewLayers = {};
    auto session = std::allocate_shared<DepthDrawSession>(
        nicxlive::core::d_runtime::SourceAllocator<DepthDrawSession>{});
    int order{};
    const auto layerMasks = gridResult.layerMasks;
    for (auto layerMask : layerMasks) {
        auto composedLayer = psdDepthComposedLayerByPath(imported, layerMask.layerPath,
            compat::ngRequireObject(gridResult.grid)->getUUID());
        if (composedLayer == nullptr || composedLayer->depthRgba.size() == 0) continue;

        const auto appendLayer = [&](PsdDepthComposedLayer& source, compat::RetainedString id) {
            auto layer = psdDepthComposedLayerToDepthDrawLayer(source);
            layer.id = drawString(id);
            session->layers.append(layer);
            previewLayers.append(layer);
            layerIdToPath[compat::RetainedStringSlice(psdString(layer.id))] = layerMask.layerPath;

            DepthDrawBinding binding;
            binding.layerId = layer.id;
            binding.targetNodeUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
            binding.targetGridUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
            binding.order = order;
            order = io::depthsample::detail::add32(order, 1);
            binding.enabled = true;
            binding.useNormalLayerAlpha = false;
            binding.coverageThreshold = 0.0f;
            binding.mergePolicy = DepthMergePolicy::Frontmost;
            session->bindings.append(binding);
        };

        if (composedLayer->depthEnabled) {
            auto id = composedLayer->id.size() ? composedLayer->id : composedLayer->layerPath;
            appendLayer(*composedLayer, id);
            continue;
        }

        size_t composedIndex = std::numeric_limits<std::size_t>::max();
        auto candidates = imported.composedLayers;
        for (std::size_t i=0; i < candidates.size(); ++i) { auto& candidate = candidates[i];
            if (&candidate == composedLayer.get()) {
                composedIndex = i;
                break;
            }
        }
        if (composedIndex == std::numeric_limits<std::size_t>::max()) continue;

        detail::DepthMapIntegerMap<std::size_t, compat::Slice<std::uint8_t>> depthPixelsBySource;
        compat::Slice<std::size_t> sourceIndices;
        for (int y = 0; y < composedLayer->height; ++y) {
            for (int x = 0; x < composedLayer->width; ++x) {
                auto targetIndex = ((std::size_t)y * (std::size_t)composedLayer->width + (std::size_t)x) * 4;
                if (targetIndex + 3 >= composedLayer->maskRgba.size() ||
                    composedLayer->maskRgba[targetIndex + 3] == 0) continue;
                if (composedLayer->colorRgba.size() >= targetIndex + 4 &&
                    composedLayer->colorRgba[targetIndex + 3] < 3) continue;

                std::ptrdiff_t sourceIndex{};
                std::array<std::uint8_t, 4> sourcePixel{};
                if (!ngPsdDepthResolvedPixelAt(imported, composedIndex,
                    io::depthsample::detail::add32(composedLayer->left, x),
                    io::depthsample::detail::add32(composedLayer->top, y), sourceIndex, sourcePixel)) continue;
                auto sourceKey = (std::size_t)sourceIndex;
                auto pixels = depthPixelsBySource.find(sourceKey);
                if (pixels == depthPixelsBySource.end()) {
                    compat::Slice<std::uint8_t> data;
                    data.resize((std::size_t)composedLayer->width * (std::size_t)composedLayer->height * 4);
                    depthPixelsBySource[sourceKey] = data;
                    sourceIndices.append(sourceKey);
                    pixels = depthPixelsBySource.find(sourceKey);
                }
                auto destination = pixels->second.slice(targetIndex, targetIndex + 4);
                std::copy(sourcePixel.begin(), sourcePixel.end(), destination.begin());
            }
        }

        for (auto sourceIndex : sourceIndices) {
            auto resolved = imported.composedLayers[sourceIndex];
            resolved.id = compat::RetainedString(std::string((composedLayer->id.size() ?
                composedLayer->id : composedLayer->layerPath).view()) + "\x1F" "attached:" + std::to_string(sourceIndex));
            resolved.layerPath = composedLayer->layerPath;
            resolved.layerName = composedLayer->layerName;
            resolved.left = composedLayer->left;
            resolved.top = composedLayer->top;
            resolved.width = composedLayer->width;
            resolved.height = composedLayer->height;
            resolved.visible = composedLayer->visible;
            resolved.enabled = composedLayer->enabled;
            resolved.colorRgba = composedLayer->colorRgba;
            resolved.maskRgba = composedLayer->maskRgba;
            resolved.coverageMaskRgba = composedLayer->coverageMaskRgba;
            resolved.depthRgba = depthPixelsBySource.find(sourceIndex)->second;
            appendLayer(resolved, resolved.id);
        }
    }
    return session;
}

void detail::updatePsdDepthGridRange(PsdDepthGridResult& gridResult) {
    bool hasDepth{};
    for (auto depth : gridResult.depths) {
        // The original isFinite inspects the exponent without a floating operation.
        if ((std::bit_cast<std::uint32_t>(depth) & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000)) continue;
        if (!hasDepth) {
            gridResult.minDepth = depth;
            gridResult.maxDepth = depth;
            hasDepth = true;
        } else {
            gridResult.minDepth = std::min(gridResult.minDepth, depth);
            gridResult.maxDepth = std::max(gridResult.maxDepth, depth);
        }
    }
    if (!hasDepth) {
        gridResult.minDepth = 0.0f;
        gridResult.maxDepth = 0.0f;
    }
}

void applyPsdDepthMissingPolicy(
    PsdDepthGridResult& composed,
    PsdDepthImportResult& imported,
    compat::ConstSlice<float> existingDepths
) {
    nicxlive::core::math::Vec2Array vertices;
    if (composed.grid != nullptr) vertices = composed.grid->getVertices();
    const auto missingMask = composed.missingVertexMask;
    for (std::size_t i=0; i < missingMask.size(); ++i) { auto isMissing = missingMask[i];
        if (!isMissing) continue;
        switch (imported.missingPolicy) {
            case PsdDepthMissingPolicy::KeepExisting: {
                bool foundSample{};
                float nearestDepth = std::numeric_limits<float>::quiet_NaN();
                float nearestDistanceSquared = std::numeric_limits<float>::quiet_NaN();
                if (vertices.size() == composed.depths.size() && i < vertices.size()) {
                    const auto candidateDepths = composed.depths;
                    for (std::size_t j=0; j < candidateDepths.size(); ++j) { auto candidateDepth = candidateDepths[j];
                        if (j >= composed.missingVertexMask.size() || composed.missingVertexMask[j] ||
                            !std::isfinite(candidateDepth)) continue;
                        auto dx = vertices[j].x - vertices[i].x;
                        auto dy = vertices[j].y - vertices[i].y;
                        auto distanceSquared = dx * dx + dy * dy;
                        if (foundSample && distanceSquared >= nearestDistanceSquared) continue;
                        foundSample = true;
                        nearestDepth = candidateDepth;
                        nearestDistanceSquared = distanceSquared;
                    }
                }
                if (foundSample) {
                    composed.depths[i] = nearestDepth;
                } else if (i < existingDepths.size()) {
                    composed.depths[i] = existingDepths[i];
                }
                break;
            }
            case PsdDepthMissingPolicy::SetZero:
                composed.depths[i] = 0.0f;
                break;
            case PsdDepthMissingPolicy::SetBack: {
                auto backDepth = imported.missingBackDepth * imported.globalDepthScale;
                composed.depths[i] = std::isfinite(backDepth) ? backDepth : 0.0f;
                break;
            }
            case PsdDepthMissingPolicy::SkipGrid:
                composed.skipped = true;
                composed.depths = existingDepths.empty() ? compat::Slice<float>{}
                    : compat::Slice<float>(existingDepths.begin(), existingDepths.end());
                return;
            default:
                throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
                    "No appropriate switch clause found"),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 728);
        }
    }
}

void smoothPsdDepthGridResult(PsdDepthGridResult& composed, PsdDepthImportResult& imported) {
    if (!imported.smoothWavySurface || composed.grid == nullptr || composed.depths.size() == 0) return;
    auto vertices = composed.grid->getVertices();
    if (vertices.size() != composed.depths.size()) return;

    size_t cols = vertices.size();
    size_t rows = 1;
    if (std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(composed.grid) != nullptr && vertices.size() > 1) {
        cols = 1;
        auto firstY = vertices[0].y;
        while (cols < vertices.size() && std::abs(vertices[cols].y - firstY) <= 0.00001f) cols++;
        if (cols == 0 || vertices.size() % cols != 0) {
            cols = vertices.size();
        } else {
            rows = vertices.size() / cols;
        }
    }

    compat::Slice<std::uint8_t> valid;
    compat::Slice<int> groups;
    valid.resize(composed.depths.size());
    groups.resize(composed.depths.size());
    std::fill(groups.begin(), groups.end(), -1);
    detail::DepthMapStringMap<std::int32_t> groupByLayer;
    int nextGroup{};
    const auto depthCount = composed.depths.size();
    for (std::size_t i = 0; i < depthCount; ++i) {
        if (i >= composed.winnerLayerPaths.size() || composed.winnerLayerPaths[i].empty()) continue;
        valid[i] = 1;
        auto path = composed.winnerLayerPaths[i];
        const compat::RetainedStringSlice key(path);
        auto group = groupByLayer.find(key);
        if (group == groupByLayer.end()) {
            groupByLayer[key] = nextGroup;
            groups[i] = nextGroup;
            nextGroup = io::depthsample::detail::add32(nextGroup, 1);
        } else {
            groups[i] = group->second;
        }
    }
    float effectiveDepthScale = 0.0001f;
    auto composedLayers = imported.composedLayers;
    for (auto& layer : composedLayers) {
        effectiveDepthScale = std::max(effectiveDepthScale,
            std::abs((layer.frontDepth - layer.backDepth) * layer.sourceDepthScale * layer.depthScale));
    }
    composed.depths = ngDepthDrawSmoothGridDepthValuesSource(
        composed.depths, valid, groups,
        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(cols)),
        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(rows)), 18.0f, effectiveDepthScale);
}

bool ngComposePsdDepthTarget(
    PsdDepthImportResult& imported,
    PsdDepthGridResult& gridResult,
    PsdDepthGridResult& composed,
    compat::RetainedString& error
) {
    composed = {};
    error = {};
    composed = gridResult;
    error = nullptr;
    if (gridResult.grid == nullptr || gridResult.skipped) return true;
    auto depthMapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(gridResult.grid);
    if (depthMapped == nullptr) {
        error = compat::RetainedString::fromStaticLiteral(
            "PSD depth map composition target does not support depth maps");
        return false;
    }

    LayerMap layerIdToPath;
    compat::Slice<DepthDrawLayer> previewLayers;
    auto session = buildPsdDepthDrawSessionForGrid(imported, gridResult, layerIdToPath, previewLayers);
    if (compat::ngRequireObject(session)->layers.size() == 0) {
        error = compat::RetainedString::fromStaticLiteral(
            "PSD depth map composition has no enabled source layers for target");
        return false;
    }

    auto target = std::allocate_shared<DepthTargetView>(
        nicxlive::core::d_runtime::SourceAllocator<DepthTargetView>{}, gridResult.grid);
    auto oldDepths = depthMapped->copyDepths();
    if (oldDepths.isNull() ||
        oldDepths.size() != compat::ngRequireObject(gridResult.grid)->getVertices().size()) {
        oldDepths.resize(compat::ngRequireObject(gridResult.grid)->getVertices().size());
        std::fill(oldDepths.begin(), oldDepths.end(), 0.0f);
    }
    target->baseDepths = oldDepths.duplicate();
    target->depths = oldDepths.duplicate();

    auto result = ngComposeDepthDrawTarget(session, target, gridResult.documentWidth, gridResult.documentHeight);
    return finalizePsdDepthTarget(
        imported, gridResult, oldDepths, layerIdToPath, previewLayers, result, composed, error);
}

bool finalizePsdDepthTarget(
    PsdDepthImportResult& imported,
    PsdDepthGridResult& gridResult,
    compat::ConstSlice<float> oldDepths,
    LayerMap& layerIdToPath,
    compat::Slice<DepthDrawLayer> previewLayers,
    DepthDrawComposeResult& result,
    PsdDepthGridResult& composed,
    compat::RetainedString& error
) {
    composed = {};
    error = {};
    composed = gridResult;
    error = nullptr;
    if (gridResult.grid == nullptr || result.depths.size() != gridResult.grid->getVertices().size()) {
        error = "PSD depth map composition depth count mismatch";
        return false;
    }
    composed.depths = result.depths.duplicate();
    composed.baseDepths = oldDepths.empty() ? compat::Slice<float>{}
        : compat::Slice<float>(oldDepths.begin(), oldDepths.end());
    composed.winnerLayerPaths.resize(composed.depths.size());
    composed.missingVertexMask.resize(composed.depths.size());
    composed.sampledVertices = 0;
    composed.missingVertices = 0;
    for (std::size_t i = 0; i < composed.depths.size(); ++i) {
        auto winnerId = i < result.winningLayerIds.size() ? result.winningLayerIds[i] : DepthDrawString{};
        auto hasWinner = !winnerId.empty();
        composed.missingVertexMask[i] = !hasWinner;
        if (hasWinner) {
            composed.sampledVertices++;
            if (auto layerPath = layerIdToPath.find(compat::RetainedStringSlice(psdString(winnerId)));
                layerPath != layerIdToPath.end()) {
                composed.winnerLayerPaths[i] = layerPath->second;
            } else {
                composed.winnerLayerPaths[i] = psdString(winnerId);
            }
        } else {
            composed.missingVertices++;
            composed.winnerLayerPaths[i] = nullptr;
        }
    }
    applyPsdDepthMissingPolicy(composed, imported, oldDepths);
    if (!composed.skipped) smoothPsdDepthGridResult(composed, imported);
    updatePsdDepthGridRange(composed);
    for (auto& layerMask : composed.layerMasks) {
        layerMask.sampledVertices = 0;
        layerMask.selectedVertices = 0;
        for (auto stats : result.layerStats) {
            compat::RetainedString statsLayerPath = psdString(stats.layerId);
            if (auto path = layerIdToPath.find(compat::RetainedStringSlice(statsLayerPath)); path != layerIdToPath.end())
                statsLayerPath = path->second;
            if (statsLayerPath != layerMask.layerPath) continue;
            layerMask.sampledVertices += stats.sampledVertices;
            layerMask.selectedVertices += stats.winningVertices;
        }
    }
    buildPsdDepthDrawCompositePreview(composed, previewLayers);
    return true;
}

bool ngComposePsdDepthImportResult(
    PsdDepthImportResult& imported,
    PsdDepthComposedView& composed,
    compat::RetainedString& error
) {
    composed = PsdDepthComposedView{};
    error = nullptr;
    if (imported.gpuCompositionRequested) {
        if (!composePsdDepthImportGpu(imported, error)) return false;
    } else {
        auto grids = imported.grids;
        for (std::size_t i=0; i < grids.size(); ++i) { auto& gridResult = grids[i];
            PsdDepthGridResult composedGrid;
            if (!ngComposePsdDepthTarget(imported, gridResult, composedGrid, error)) return false;
            imported.grids[i] = composedGrid;
        }
    }
    detail::PsdDepthAccess::imported(composed) = &imported;
    return true;
}

bool ngSubmitPsdDepthImportGpu(
    PsdDepthImportResult& imported,
    PsdDepthGpuComposeWork& work,
    compat::RetainedString& error
) {
    work = PsdDepthGpuComposeWork{};
    error = nullptr;
    work.composedGrids = imported.grids.duplicate();
    auto grids = imported.grids;
    for (std::size_t i=0; i < grids.size(); ++i) { auto& gridResult = grids[i];
        if (gridResult.grid == nullptr || gridResult.skipped) continue;
        auto depthMapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(gridResult.grid);
        if (depthMapped == nullptr) {
            error = "PSD depth map GPU composition target does not support depth maps";
            ngCancelPsdDepthImportGpu(work);
            return false;
        }

        LayerMap layerIdToPath;
        compat::Slice<DepthDrawLayer> previewLayers;
        auto session = buildPsdDepthDrawSessionForGrid(imported, gridResult, layerIdToPath, previewLayers);
        if (session->layers.size() == 0) {
            error = "PSD depth map GPU composition has no enabled source layers for target";
            ngCancelPsdDepthImportGpu(work);
            return false;
        }

        auto target = std::allocate_shared<DepthTargetView>(
            nicxlive::core::d_runtime::SourceAllocator<DepthTargetView>{}, gridResult.grid);
        auto oldDepths = depthMapped->copyDepths();
        if (oldDepths.isNull() || oldDepths.size() != gridResult.grid->getVertices().size()) {
            oldDepths.resize(gridResult.grid->getVertices().size());
            std::fill(oldDepths.begin(), oldDepths.end(), 0.0f);
        }
        target->baseDepths = oldDepths.duplicate();
        target->depths = oldDepths.duplicate();

        DepthDrawGpuTargetComposeJob job;
        if (!ngSubmitDepthDrawGpuTargetCompose(session, target, gridResult.documentWidth, gridResult.documentHeight,
            job, error)) {
            ngCancelPsdDepthImportGpu(work);
            if (error.size() == 0) error = "CPU fallback is disabled for PSD depth map GPU composition";
            else error = compat::RetainedString("CPU fallback is disabled for PSD depth map GPU composition: " + std::string(error.view()));
            return false;
        }
        PsdDepthGpuGridComposeWork gridWork;
        gridWork.gridIndex = i;
        gridWork.sourceGrid = gridResult;
        gridWork.oldDepths = oldDepths;
        gridWork.layerIdToPath = layerIdToPath;
        gridWork.previewLayers = previewLayers;
        gridWork.job = job;
        work.grids.append(gridWork);
    }
    work.active = true;
    return true;
}

bool ngPollPsdDepthImportGpu(
    PsdDepthImportResult& imported,
    PsdDepthGpuComposeWork& work,
    bool& ready,
    PsdDepthComposedView& composed,
    compat::RetainedString& error
) {
    ready = false;
    composed = PsdDepthComposedView{};
    error = nullptr;
    if (!work.active) {
        error = "PSD depth map GPU composition work is not active";
        return false;
    }

    bool pending{};
    auto grids = work.grids;
    for (auto& gridWork : grids) {
        if (gridWork.completed) continue;
        DepthDrawGpuTargetComposePollResult pollResult;
        if (!ngPollDepthDrawGpuTargetCompose(gridWork.job, pollResult, error)) {
            ngCancelPsdDepthImportGpu(work);
            return false;
        }
        if (!pollResult.ready) {
            pending = true;
            continue;
        }

        PsdDepthGridResult composedGrid;
        if (!finalizePsdDepthTarget(imported, gridWork.sourceGrid, gridWork.oldDepths,
            gridWork.layerIdToPath, gridWork.previewLayers, pollResult.result, composedGrid, error)) {
            ngCancelPsdDepthImportGpu(work);
            return false;
        }
        work.composedGrids[gridWork.gridIndex] = composedGrid;
        gridWork.completed = true;
        gridWork.job = DepthDrawGpuTargetComposeJob{};
    }
    if (pending) return true;

    imported.grids = work.composedGrids;
    work = PsdDepthGpuComposeWork{};
    detail::PsdDepthAccess::imported(composed) = &imported;
    ready = true;
    return true;
}

void ngCancelPsdDepthImportGpu(PsdDepthGpuComposeWork& work) {
    auto grids = work.grids;
    for (auto& gridWork : grids) {
        if (!gridWork.completed && gridWork.job.jobId != 0) {
            ngCancelDepthDrawGpuTargetCompose(gridWork.job);
        }
    }
    work = PsdDepthGpuComposeWork{};
}

bool composePsdDepthImportGpu(PsdDepthImportResult& imported, compat::RetainedString& error) {
    error = {};
    PsdDepthGpuComposeWork work;
    if (!ngSubmitPsdDepthImportGpu(imported, work, error)) return false;
    for (int _ = 0; _ < 30000; ++_) {
        bool ready{};
        PsdDepthComposedView composed;
        if (!ngPollPsdDepthImportGpu(imported, work, ready, composed, error)) return false;
        if (ready) return true;
        sleepPsdDepthGpuPoll();
    }
    ngCancelPsdDepthImportGpu(work);
    error = compat::RetainedString::fromStaticLiteral("PSD depth map GPU composition timed out");
    return false;
}


}
