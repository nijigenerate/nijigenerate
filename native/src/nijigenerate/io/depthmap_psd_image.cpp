// Whole ngBuildPsdDepthsFromImage body from the independent depthmap_psd.d.
// Copyright 2025 nijigenerate Project. Distributed under BSD-2-Clause.
#include "depthmap_psd_loading.hpp"
#include "depthmap_psd_math.hpp"
#include "pngheader.hpp"
#include "depthsample.hpp"
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include "detail/psd_binding_sort.hpp"
#include <texture.hpp>
#include <nodes/drawable.hpp>
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace nijigenerate::io::depthmap_psd {
using namespace detail;
using namespace depthimage;
using nicxlive::core::ShallowTexture;
namespace {
void sortPngBindings(compat::Slice<PngComposedBinding>& bindings) {
    const auto less = [](PngComposedBinding a, PngComposedBinding b) {
        const auto drawableA = std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(a.matchedTarget);
        const auto drawableB = std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(b.matchedTarget);
        const auto zA = drawableA ? drawableA->zSort() : (a.target ? a.target->zSort() : 0.0f);
        const auto zB = drawableB ? drawableB->zSort() : (b.target ? b.target->zSort() : 0.0f);
        if (zA == zB) return a.image.layerPath.view() < b.image.layerPath.view();
        return zA > zB;
    };
    detail::binding_sort::sort(bindings, less);
}
}
PsdDepthImportResult ngBuildPsdDepthsFromImage(PuppetRef puppet, compat::RetainedString path, PsdDepthImportSettings settings) {
    enforce(puppet != nullptr && puppet->root != nullptr, "No active puppet", 3456);
    enforce(path.size() > 0, "Path not provided", 3457);
    enforce(std::isfinite(settings.backDepth) && std::isfinite(settings.frontDepth),
        "Depth range must be finite", 3458);
    enforce(std::isfinite(settings.depthScale), "Depth scale must be finite", 3459);
    enforce(settings.depthScale >= 0.0f, "Depth scale must be non-negative", 3460);
    enforce(settings.alphaThreshold >= 0.0f && settings.alphaThreshold <= 1.0f,
        "Alpha threshold must be in [0, 1]", 3461);
    enforce(settings.customRadius >= 1 && settings.customRadius <= 64,
        "Custom radius must be in [1, 64]", 3462);
    settings.zeroDepthIsMissing = true;

    std::uint64_t retainedPsdBytes = 0;
    int inspectedDepthWidth = 0;
    int inspectedDepthHeight = 0;
    pngheader::ngInspectPngDimensions(compat::RetainedStringSlice(path), inspectedDepthWidth, inspectedDepthHeight);
    enforce(ngReservePsdDepthRetainedLayer(inspectedDepthWidth, inspectedDepthHeight,
        PsdDepthRetainedDepthLayerBytesPerPixel, retainedPsdBytes),
        "PSD Depth Map source exceeds the retained image memory budget", 3469);
    auto texture = ShallowTexture(path, 4);
    enforce(texture.width == inspectedDepthWidth && texture.height == inspectedDepthHeight,
        "Decoded PNG dimensions do not match the IHDR header", 3473);
    auto layerName = sourceStem(path);
    auto layerPath = allocateImportBytes([&] {
        return compat::RetainedString("/" + std::string(layerName.view()));
    });
    bool hasExplicitColorSource = settings.colorSourcePath.size() > 0;
    bool hasFlatColorSource = hasExplicitColorSource && sourceExtensionLower(settings.colorSourcePath).view() == ".png";
    bool hasPsdColorSource = hasExplicitColorSource && sourceExtensionLower(settings.colorSourcePath).view() == ".psd";
    ShallowTexture flatColorTexture;
    compat::RetainedString flatColorLayerName;
    compat::RetainedString flatColorLayerPath;
    if (hasFlatColorSource) {
        int inspectedColorWidth = 0;
        int inspectedColorHeight = 0;
        pngheader::ngInspectPngDimensions(
            compat::RetainedStringSlice(settings.colorSourcePath), inspectedColorWidth, inspectedColorHeight);
        enforce(inspectedColorWidth == inspectedDepthWidth && inspectedColorHeight == inspectedDepthHeight,
            "Color and depth source dimensions must match for 1:1 composition", 3487);
        enforce(ngReservePsdDepthRetainedLayer(inspectedColorWidth, inspectedColorHeight,
            PsdDepthRetainedColorLayerBytesPerPixel, retainedPsdBytes),
            "PSD Depth Map color source exceeds the retained image memory budget", 3489);
        flatColorTexture = ShallowTexture(settings.colorSourcePath, 4);
        enforce(flatColorTexture.width == inspectedColorWidth && flatColorTexture.height == inspectedColorHeight,
            "Decoded PNG dimensions do not match the IHDR header", 3493);
        flatColorLayerName = sourceStem(settings.colorSourcePath);
        flatColorLayerPath = allocateImportBytes([&] {
            return compat::RetainedString("/" + std::string(flatColorLayerName.view()));
        });
    } else if (hasExplicitColorSource && !hasPsdColorSource) {
        enforce(false, "Unsupported color source format for flat depth composition", 3498);
    }

    PsdDepthImportResult result;
    result.gpuCompositionRequested = settings.useGpuComposition;
    result.smoothWavySurface = settings.smoothWavySurface;
    setCompositionMode(result, PsdDepthCompositionMode::Unknown);
    result.compositionWidth = texture.width;
    result.compositionHeight = texture.height;
    result.globalDepthScale = settings.depthScale;
    result.missingPolicy = settings.missingPolicy;
    result.missingBackDepth = settings.backDepth;
    result.sourceDepthLayerCount = 1;
    result.depthSource.kind = PsdDepthCompositeSourceKind::FlatImage;
    result.depthSource.kindName = ngPsdDepthCompositeSourceKindName(result.depthSource.kind);
    result.depthSource.sourcePath = path;
    result.depthSource.width = texture.width;
    result.depthSource.height = texture.height;
    auto pngDepthSourceLayer = makeCompositeSourceLayer(
        layerPath,
        layerName,
        0,
        0,
        texture.width,
        texture.height,
        true,
        true,
        0
    );
    auto normalizedFlatDepthRgba = normalizeFlatDepthRgba(texture.data, texture.width, texture.height, texture.channels);
    setCompositeSourceLayerPixels(pngDepthSourceLayer, normalizedFlatDepthRgba);
    result.depthSource.layers.append(pngDepthSourceLayer);
    result.colorSource.kind = hasPsdColorSource ? PsdDepthCompositeSourceKind::PsdLayers :
        (hasFlatColorSource ? PsdDepthCompositeSourceKind::FlatImage : PsdDepthCompositeSourceKind::ActiveArtTargets);
    result.colorSource.kindName = ngPsdDepthCompositeSourceKindName(result.colorSource.kind);
    result.colorSource.sourcePath = hasExplicitColorSource ? settings.colorSourcePath : compat::RetainedString{};
    result.colorSource.width = texture.width;
    result.colorSource.height = texture.height;

    auto finishResult = [&]() -> PsdDepthImportResult {
        validateCompositionDimensions(result, hasFlatColorSource
            ? compat::RetainedString::fromStaticLiteral("1:1 composition")
            : (hasPsdColorSource ? compat::RetainedString::fromStaticLiteral("N:1 composition")
                : compat::RetainedString::fromStaticLiteral("active color/depth composition")));
        return result;
    };

    if (hasFlatColorSource) {
        result.colorLayerCount = 1;
        setCompositionMode(result, PsdDepthCompositionMode::OneToOne);
        auto colorSourceLayer = makeCompositeSourceLayer(
            flatColorLayerPath,
            flatColorLayerName,
            0,
            0,
            flatColorTexture.width,
            flatColorTexture.height,
            true,
            true,
            0
        );
        setCompositeSourceLayerPixels(colorSourceLayer, compat::Slice<std::uint8_t>::retainedView(
            flatColorTexture.data.retentionOwner(),
            flatColorTexture.data.data(), flatColorTexture.data.size()));
        result.colorSource.layers.append(colorSourceLayer);
    } else if (hasPsdColorSource) {
        loadPsdCompositeSourceLayers(
            settings.colorSourcePath, result.colorSource, result.colorLayerCount, retainedPsdBytes);
        enforce(result.colorSource.width == texture.width && result.colorSource.height == texture.height,
            "Color and depth source dimensions must match for N:1 composition", 3562);
        setCompositionMode(result, ngPsdDepthCompositionModeForCounts(result.colorLayerCount, result.sourceDepthLayerCount));
    }
    PsdDepthLayerMapping mapping;
    mapping.layerPath = layerPath;
    mapping.layerName = layerName;

    auto basePngImage = [&](compat::RetainedString imageLayerPath = {}, compat::RetainedString imageLayerName = {}) -> DepthLayerImage {
        DepthLayerImage image;
        image.layerPath = imageLayerPath.size() ? imageLayerPath : (hasFlatColorSource ? flatColorLayerPath : layerPath);
        image.layerName = imageLayerName.size() ? imageLayerName : (hasFlatColorSource ? flatColorLayerName : layerName);
        image.left = 0;
        image.top = 0;
        image.width = texture.width;
        image.height = texture.height;
        image.documentWidth = texture.width;
        image.documentHeight = texture.height;
        image.opacity = 1.0f;
        image.data = normalizedFlatDepthRgba.duplicate();
        if (hasFlatColorSource) {
            image.coverageWidth = flatColorTexture.width;
            image.coverageHeight = flatColorTexture.height;
            image.coverageChannels = flatColorTexture.channels;
            image.coverageOpacity = 1.0f;
            image.coverageData = compat::Slice<std::uint8_t>::retainedView(
                flatColorTexture.data.retentionOwner(),
                flatColorTexture.data.data(), flatColorTexture.data.size()).duplicate();
        }
        return image;
    };

    DeformableRef grid;
    NodeRef matchedNode;
    if (const auto ignored = settings.ignoredLayerPaths.findRetained(layerPath.view())) {
        if (*ignored) {
            mapping.ignored = true;
            mapping.manual = true;
            mapping.status = compat::RetainedString::fromStaticLiteral("Ignored");
            result.mappings.append(mapping);
            auto image = basePngImage();
            addComposedLayer(result, path, image, settings, true, false);
            return finishResult();
        }
    }

    auto addMappedPngLayer = [&](DepthLayerImage& image, DeformableRef target, NodeRef matchedTarget,
        compat::RetainedString status, bool manual, compat::Slice<DepthLayerImage>& layers) -> void {
        PsdDepthLayerMapping targetMapping;
        targetMapping.layerPath = image.layerPath;
        targetMapping.layerName = image.layerName;
        targetMapping.manual = manual;
        targetMapping.matched = true;
        if (!matchedTarget) actions::node::detail::failActionObjectReference();
        targetMapping.matchedNodeName = ext::nodes::ngRetainNodeName(matchedTarget->getName()).retainedString();
        targetMapping.matchedNodeUuid = matchedTarget->getUUID();
        if (!target) actions::node::detail::failActionObjectReference();
        targetMapping.targetGridName = ext::nodes::ngRetainNodeName(target->getName()).retainedString();
        targetMapping.targetGridUuid = target->getUUID();
        targetMapping.status = status;
        result.matchedLayers++;
        result.mappings.append(targetMapping);

        image.grid = target;
        if (!hasPsdColorSource && (status.view() == "ComposedN1" || status.view() == "Manual")) {
            bool existsColorLayer = false;
            for (auto sourceLayer : result.colorSource.layers) {
                if (sourceLayer.targetGridUuid == target->getUUID() && sourceLayer.id == image.layerPath) {
                    existsColorLayer = true;
                    break;
                }
            }
            if (!existsColorLayer) {
                auto colorSourceLayer = makeCompositeSourceLayer(
                    image.layerPath,
                    image.layerName,
                    image.left,
                    image.top,
                    image.width,
                    image.height,
                    true,
                    image.coverageSources.size() > 0 || image.coverageData.size() > 0,
                    0,
                    target
                );
                auto colorMask = image.coverageData.size() ? image.coverageData : image.data;
                setCompositeSourceLayerPixels(colorSourceLayer, colorMask);
                result.colorSource.layers.append(colorSourceLayer);
            }
        }
        addComposedLayer(result, path, image, settings, true,
            ngPsdDepthGridLayerEnabled(settings, target->getUUID(), image.layerPath), target);

        layers.append(image);
    };

    auto applyDepthDrawSplitToPngBindings = [&](compat::Slice<PngComposedBinding>& bindings) -> void {
        sortPngBindings(bindings);

        compat::Slice<DepthDrawSplitLayer> splitLayers;
        for (auto& binding : bindings) {
            if (binding.image.width <= 0 || binding.image.height <= 0) continue;
            splitLayers.append(depthDrawSplitLayerFromDepthLayerImage(binding.image));
        }
        if (splitLayers.size() == 0) return;

        auto yFlippedFlatDepthRgba = flipRgbaY(normalizedFlatDepthRgba, texture.width, texture.height);
        size_t normalCropOverlap = 0;
        size_t flippedCropOverlap = 0;
        for (auto& binding : bindings) {
            if (binding.image.width <= 0 || binding.image.height <= 0) continue;
            auto surfacePreview = buildColorLayerPreviewRgba(binding.image);
            auto normalCrop = sampleDepthToSurfaceLayer(
                normalizedFlatDepthRgba,
                texture.width,
                texture.height,
                binding.image.left,
                binding.image.top,
                binding.image.width,
                binding.image.height
            );
            auto flippedCrop = sampleDepthToSurfaceLayer(
                yFlippedFlatDepthRgba,
                texture.width,
                texture.height,
                binding.image.left,
                binding.image.top,
                binding.image.width,
                binding.image.height
            );
            normalCropOverlap += depthSurfaceOverlap(normalCrop, surfacePreview.rgba);
            flippedCropOverlap += depthSurfaceOverlap(flippedCrop, surfacePreview.rgba);
        }
        auto splitDepthRgba = flippedCropOverlap > normalCropOverlap ? yFlippedFlatDepthRgba : normalizedFlatDepthRgba;
        auto stableDepthPixels = ngDepthDrawDecodeDepthPixelsFromRgbaSource(
            splitDepthRgba, sampleChannel(settings.channel));
        auto visibleLayerMap = ngDepthDrawBuildVisibleLayerMapSource(texture.width, texture.height, splitLayers);
        int splitIndex = 0;
        for (auto& binding : bindings) {
            if (binding.image.width <= 0 || binding.image.height <= 0) continue;
            auto splitLayer = depthDrawSplitLayerFromDepthLayerImage(binding.image);
            seedLayerDepthFromDepthDrawSplit(
                binding.image,
                splitLayer,
                splitIndex,
                texture.width,
                texture.height,
                stableDepthPixels,
                visibleLayerMap,
                splitLayers,
                settings.repairContourBand
            );
            splitIndex = depthsample::detail::add32(splitIndex, 1);
            buildCoverageCache(binding.image);
            maskDepthToCoverage(binding.image, settings);
            applyDepthDrawLayerDepthCleanup(binding.image, settings);
        }
    };

    auto addMappedPngGridResults = [&](compat::Slice<DepthLayerImage>& layers) -> void {
        auto sourceAt = [](auto& values, std::size_t index, std::size_t sourceLine) -> decltype(auto) {
            if (index >= values.size())
                throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"),
                    sourceLine);
            return values[index];
        };
        compat::Slice<GridAccum> accums;
        for (auto currentLayers = layers; auto& layer : currentLayers) {
            if (layer.grid == nullptr) continue;
            auto accumIndex = findAccum(accums, layer.grid);
            auto layerMaskIndex = findLayerMask(sourceAt(accums, accumIndex, 3732),
                layer.layerPath, layer.layerName);
            const auto gridUuid = compat::ngRequireObject(layer.grid)->getUUID();
            if (!ngPsdDepthGridEnabled(settings, gridUuid)) continue;
            const auto layerGridUuid = compat::ngRequireObject(layer.grid)->getUUID();
            if (!ngPsdDepthGridLayerEnabled(settings, layerGridUuid, layer.layerPath)) continue;
            auto vertices = compat::ngRequireObject(layer.grid)->getVertices();
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                auto documentGrid = layer.grid;
                const auto documentVertex = sourceAt(vertices, i, 3737);
                auto documentPoint = gridVertexDocumentPosition(std::move(documentGrid), documentVertex,
                    texture.width, texture.height);
                auto layerX = layerToSourceX(layer, documentPoint.x);
                auto layerY = layerToSourceY(layer, documentPoint.y);
                auto sample = sampleLayer(layer, layerX, layerY, settings);
                if (!sample.valid) continue;
                sourceAt(sourceAt(accums, accumIndex, 3742).layerMasks, layerMaskIndex, 3742).sampledVertices++;
                if (!sourceAt(sourceAt(accums, accumIndex, 3743).has, i, 3743) ||
                    sample.value > sourceAt(sourceAt(accums, accumIndex, 3743).best, i, 3743)) {
                    sourceAt(sourceAt(accums, accumIndex, 3744).has, i, 3744) = true;
                    sourceAt(sourceAt(accums, accumIndex, 3745).best, i, 3745) = sample.value;
                    sourceAt(sourceAt(accums, accumIndex, 3746).baseBest, i, 3746) = sample.value;
                    auto& winnerPath = sourceAt(sourceAt(accums, accumIndex, 3747).winnerLayerPaths, i, 3747);
                    winnerPath = layer.layerPath;
                }
            }
        }

        for (auto currentAccums = accums; auto& accum : currentAccums) {
            PsdDepthGridResult gridResult;
            gridResult.documentWidth = texture.width;
            gridResult.documentHeight = texture.height;
            finalizeGridResult(gridResult, accum, settings);
            for (auto currentLayers = layers; auto& layer : currentLayers) {
                if (layer.grid == accum.grid) gridResult.coverageSources += layer.coverageSources.size();
            }
            buildCompositePreview(gridResult, layers, accum.grid, settings);
            if (gridResult.skipped) result.skippedGrids++;
            result.grids.append(gridResult);
        }
    };

    auto addMappedPngTarget = [&](DepthLayerImage& image, DeformableRef target, NodeRef matchedTarget, compat::RetainedString status, bool manual) -> void {
        compat::Slice<DepthLayerImage> layers;
        addMappedPngLayer(image, target, matchedTarget, status, manual, layers);
        addMappedPngGridResults(layers);
    };

    auto addDirectMatchedPngTarget = [&](DeformableRef target, NodeRef matchedTarget, compat::RetainedString status, bool manual) -> bool {
        setCompositionMode(result, PsdDepthCompositionMode::OneToOne);
        result.colorLayerCount = std::max(result.colorLayerCount, std::size_t{1});
        auto image = basePngImage();
        image.grid = target;
        attachMatchedCoverage(image, puppet, matchedTarget, target);
        if (!hasFlatColorSource) {
            setDepthLayerImageToCoverageDocumentBounds(image);
            compositeFlatDepthByCoverageSources(image, normalizedFlatDepthRgba, texture.width, texture.height);
        }
        buildCoverageCache(image);
        maskDepthToCoverage(image, settings);
        applyDepthDrawLayerDepthCleanup(image, settings);
        addMappedPngTarget(image, target, matchedTarget, status, manual);
        return true;
    };

    auto collectComposedTargetFromFlatDepth = [&](compat::Slice<PngComposedBinding>& bindings, DeformableRef target) -> bool {
        if (target == nullptr) return false;
        if (!targetHasArtCoverage(puppet, target)) return false;
        auto composedLayerName = target->getName().size()
            ? ext::nodes::ngRetainNodeName(target->getName()).retainedString() : layerName;
        auto composedLayerPath = allocateImportBytes([&] {
            return compat::RetainedString("/" + std::string(composedLayerName.view()));
        });
        if (const auto overrideUuid = settings.layerTargetGridUuidOverrides.findRetained(composedLayerPath.view())) {
            auto manualGrid = findGridByUuid(puppet, *overrideUuid);
            if (manualGrid == nullptr) return false;
            result.colorLayerCount++;
            setCompositionMode(result, PsdDepthCompositionMode::NToOne);
            auto image = basePngImage(composedLayerPath, composedLayerName);
            image.grid = manualGrid;
            attachMatchedCoverage(image, puppet, target, target);
            setDepthLayerImageToCoverageDocumentBounds(image);
            compositeFlatDepthByCoverageSources(image, normalizedFlatDepthRgba, texture.width, texture.height);
            buildCoverageCache(image);
            maskDepthToCoverage(image, settings);
            applyDepthDrawLayerDepthCleanup(image, settings);
            bindings.append(PngComposedBinding{image, manualGrid, manualGrid,
                compat::RetainedString::fromStaticLiteral("Manual"), true});
            return true;
        }
        auto image = basePngImage(composedLayerPath, composedLayerName);
        image.grid = target;
        attachMatchedCoverage(image, puppet, target, target);
        if (image.coverageSources.size() == 0 && image.coverageData.size() == 0) return false;
        result.colorLayerCount++;
        setCompositionMode(result, PsdDepthCompositionMode::NToOne);
        setDepthLayerImageToCoverageDocumentBounds(image);
        compositeFlatDepthByCoverageSources(image, normalizedFlatDepthRgba, texture.width, texture.height);
        buildCoverageCache(image);
        maskDepthToCoverage(image, settings);
        applyDepthDrawLayerDepthCleanup(image, settings);
        bindings.append(PngComposedBinding{image, target, target,
            compat::RetainedString::fromStaticLiteral("ComposedN1"), false});
        return true;
    };

    auto addComposedTargetsFromFlatDepth = [&]() -> bool {
        compat::Slice<PngComposedBinding> bindings;
        UsedArtLayerPaths usedLayerPaths;
        for (auto part : puppet->findNodesType<nicxlive::core::nodes::Part>(puppet->root)) {
            if (part->textures.size() == 0 || part->textures[0] == nullptr) continue;
            auto target = activeArtDepthTarget(puppet, part);
            if (target == nullptr) continue;
            auto composedLayerName = activeArtLayerName(part);
            auto composedLayerPath = uniqueActiveArtLayerPath(part, usedLayerPaths);
            if (composedLayerPath.size() == 0) continue;

            bool manual = false;
            NodeRef matchedTarget = part;
            if (const auto overrideUuid = settings.layerTargetGridUuidOverrides.findRetained(composedLayerPath.view())) {
                auto manualGrid = findGridByUuid(puppet, *overrideUuid);
                if (manualGrid == nullptr) continue;
                target = manualGrid;
                matchedTarget = manualGrid;
                manual = true;
            }

            auto image = basePngImage(composedLayerPath, composedLayerName);
            image.grid = target;
            attachPartCoverage(image, part);
            if (image.coverageSources.size() == 0 && image.coverageData.size() == 0) continue;
            result.colorLayerCount++;
            setCompositionMode(result, PsdDepthCompositionMode::NToOne);
            setDepthLayerImageToCoverageDocumentBounds(image);
            compositeFlatDepthByCoverageSources(image, normalizedFlatDepthRgba, texture.width, texture.height);
            buildCoverageCache(image);
            maskDepthToCoverage(image, settings);
            applyDepthDrawLayerDepthCleanup(image, settings);
            bindings.append(PngComposedBinding{image, target, matchedTarget,
                manual ? compat::RetainedString::fromStaticLiteral("Manual")
                       : compat::RetainedString::fromStaticLiteral("ComposedN1"), manual});
        }

        if (bindings.size() == 0) {
            for (auto target : puppet->findNodesType<nicxlive::core::nodes::GridDeformer>(puppet->root)) {
                collectComposedTargetFromFlatDepth(bindings, target);
            }
            for (auto target : puppet->findNodesType<nicxlive::core::nodes::PathDeformer>(puppet->root)) {
                collectComposedTargetFromFlatDepth(bindings, target);
            }
        }
        if (bindings.size() == 0) return false;

        applyDepthDrawSplitToPngBindings(bindings);

        compat::Slice<DepthLayerImage> composedImages;
        for (auto binding : bindings) composedImages.append(binding.image);
        compat::Slice<DepthLayerImage> layers;
        for (std::size_t i = 0; i < bindings.size(); ++i) {
        auto& binding = bindings[i];
            binding.image = composedImages[i];
            addMappedPngLayer(binding.image, binding.target, binding.matchedTarget, binding.status, binding.manual, layers);

    }
        addMappedPngGridResults(layers);
        return true;
    };

    auto imageFromColorSourceLayer = [&](PsdDepthCompositeSourceLayer sourceLayer) -> DepthLayerImage {
        DepthLayerImage image;
        image.layerPath = sourceLayer.id;
        image.layerName = sourceLayer.name;
        image.left = sourceLayer.left;
        image.top = sourceLayer.top;
        image.width = sourceLayer.width;
        image.height = sourceLayer.height;
        image.documentWidth = texture.width;
        image.documentHeight = texture.height;
        image.opacity = 1.0f;
        image.data = normalizeFlatDepthRgba(
            cropRgbaRect(texture.data, texture.width, texture.height,
                sourceLayer.left, sourceLayer.top, sourceLayer.width, sourceLayer.height),
            sourceLayer.width,
            sourceLayer.height
        );
        image.coverageWidth = sourceLayer.width;
        image.coverageHeight = sourceLayer.height;
        image.coverageChannels = 4;
        image.coverageOpacity = sourceLayer.opacity;
        image.coverageData = sourceLayer.rgba.duplicate();
        return image;
    };

    auto addPsdColorComposedTargetsFromFlatDepth = [&]() -> bool {
        if (!hasPsdColorSource) return false;
        if (result.colorSource.layers.size() == 0) {
            addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("missing-color-source"),
                compat::RetainedString::fromStaticLiteral("PSD color source has no pixel layers."),
                settings.colorSourcePath, {});
            return false;
        }

        compat::Slice<DepthLayerImage> composedImages;
        compat::Slice<DepthDrawSplitLayer> splitLayers;
        for (auto sourceLayer : result.colorSource.layers) {
            if (!sourceLayer.visible || sourceLayer.width <= 0 || sourceLayer.height <= 0) continue;
            splitLayers.append(depthDrawSplitLayerFromCompositeSource(sourceLayer));
        }
        auto stableDepthPixels = ngDepthDrawDecodeDepthPixelsFromRgbaSource(
            normalizedFlatDepthRgba, sampleChannel(settings.channel));
        auto visibleLayerMap = ngDepthDrawBuildVisibleLayerMapSource(texture.width, texture.height, splitLayers);
        int splitIndex = 0;
        for (auto sourceLayer : result.colorSource.layers) {
            if (!sourceLayer.visible || sourceLayer.width <= 0 || sourceLayer.height <= 0) continue;
            auto image = imageFromColorSourceLayer(sourceLayer);
            auto splitLayer = depthDrawSplitLayerFromCompositeSource(sourceLayer);
            seedLayerDepthFromDepthDrawSplit(
                image,
                splitLayer,
                splitIndex,
                texture.width,
                texture.height,
                stableDepthPixels,
                visibleLayerMap,
                splitLayers,
                settings.repairContourBand
            );
            splitIndex = depthsample::detail::add32(splitIndex, 1);
            buildCoverageCache(image);
            maskDepthToCoverage(image, settings);
            applyDepthDrawLayerDepthCleanup(image, settings);
            composedImages.append(image);
        }
        if (composedImages.size() == 0) {
            addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("missing-color-source"),
                compat::RetainedString::fromStaticLiteral("PSD color source has no visible pixel layers."),
                settings.colorSourcePath, {});
            return false;
        }
        compat::Slice<DepthLayerImage> layers;
        for (auto& image : composedImages) {
            DeformableRef target;
            NodeRef matchedTarget;
            bool manual = false;
            compat::RetainedString status = compat::RetainedString::fromStaticLiteral("ComposedN1");
            if (const auto overrideUuid = settings.layerTargetGridUuidOverrides.findRetained(image.layerPath.view())) {
                target = findGridByUuid(puppet, *overrideUuid);
                if (target != nullptr) {
                    matchedTarget = target;
                    manual = true;
                    status = compat::RetainedString::fromStaticLiteral("Manual");
                }
            }
            if (target == nullptr) {
                auto candidates = matchCandidates(puppet, image.layerPath, image.layerName, settings,
                    image.left, image.top, image.width, image.height, texture.width, texture.height, true);
                if (candidates.size() > 0) {
                    auto resolution = resolveCandidateTarget(puppet, candidates);
                    if (resolution.ambiguous) {
                        PsdDepthLayerMapping colorMapping;
                        colorMapping.layerPath = image.layerPath;
                        colorMapping.layerName = image.layerName;
                        colorMapping.status = compat::RetainedString::fromStaticLiteral("Ambiguous");
                        colorMapping.ambiguous = true;
                        result.ambiguousLayers++;
                        result.unmatchedLayers++;
                        result.mappings.append(colorMapping);
                        addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("ambiguous-target"),
                            compat::RetainedString::fromStaticLiteral(
                                "Composed layer matches multiple targets and requires a manual binding."),
                            image.layerPath, image.layerName);
                        addComposedLayer(result, path, image, settings, true, false);
                        continue;
                    }
                    matchedTarget = resolution.node;
                    target = resolution.target;
                }
            }
            if (target != nullptr && matchedTarget != nullptr) {
                addMappedPngLayer(image, target, matchedTarget, status, manual, layers);
            } else {
                PsdDepthLayerMapping colorMapping;
                colorMapping.layerPath = image.layerPath;
                colorMapping.layerName = image.layerName;
                colorMapping.status = compat::RetainedString::fromStaticLiteral("Unmatched");
                result.unmatchedLayers++;
                result.mappings.append(colorMapping);
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("unbound-composed-layer"),
                    compat::RetainedString::fromStaticLiteral(
                        "PSD color/depth N:1 composed layer has no matching target binding."),
                    image.layerPath, image.layerName);
                addComposedLayer(result, path, image, settings, true, true);
            }
        }
        addMappedPngGridResults(layers);
        return true;
    };

    if (hasPsdColorSource) {
        addPsdColorComposedTargetsFromFlatDepth();
        return finishResult();
    }

    if (const auto overrideUuid = settings.layerTargetGridUuidOverrides.findRetained(layerPath.view())) {
        grid = findGridByUuid(puppet, *overrideUuid);
        mapping.manual = true;
        if (grid == nullptr) {
            mapping.status = compat::RetainedString::fromStaticLiteral("UnmatchedManualGrid");
            result.unmatchedLayers++;
            result.mappings.append(mapping);
            addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("missing-manual-target"),
                compat::RetainedString::fromStaticLiteral(
                    "Manual target binding points to a missing GridDeformer or PathDeformer."),
                layerPath, layerName);
            auto image = basePngImage();
            addComposedLayer(result, path, image, settings, true, true);
            return finishResult();
        }
        matchedNode = grid;
        mapping.matched = true;
        mapping.matchedNodeName = ext::nodes::ngRetainNodeName(grid->getName()).retainedString();
        mapping.matchedNodeUuid = grid->getUUID();
        mapping.targetGridName = ext::nodes::ngRetainNodeName(grid->getName()).retainedString();
        mapping.targetGridUuid = grid->getUUID();
        mapping.status = compat::RetainedString::fromStaticLiteral("Manual");
        addDirectMatchedPngTarget(grid, matchedNode, compat::RetainedString::fromStaticLiteral("Manual"), true);
        return finishResult();
    } else {
        auto candidates = matchCandidates(puppet, layerPath, layerName, settings);
        if (candidates.size() > 0) {
            auto resolution = resolveCandidateTarget(puppet, candidates);
            if (resolution.ambiguous) {
                mapping.ambiguous = true;
                mapping.status = compat::RetainedString::fromStaticLiteral("Ambiguous");
                result.ambiguousLayers++;
                result.unmatchedLayers++;
                result.mappings.append(mapping);
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("ambiguous-target"),
                    compat::RetainedString::fromStaticLiteral(
                        "Depth source matches multiple targets and requires a manual binding."),
                    layerPath, layerName);
                auto image = basePngImage();
                addComposedLayer(result, path, image, settings, true, false);
                return finishResult();
            }
            matchedNode = resolution.node;
            grid = resolution.target;
            mapping.matchedNodeName = matchedNode == nullptr ? compat::RetainedString{}
                : ext::nodes::ngRetainNodeName(matchedNode->getName()).retainedString();
            mapping.matchedNodeUuid = matchedNode == nullptr ? 0 : matchedNode->getUUID();
            if (grid == nullptr) {
                mapping.status = compat::RetainedString::fromStaticLiteral("UnmatchedWithoutGrid");
                result.unmatchedLayers++;
                result.mappings.append(mapping);
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("missing-target"),
                    compat::RetainedString::fromStaticLiteral(
                        "Depth source matched a node that is not inside a depth target."),
                    layerPath, layerName);
                auto image = basePngImage();
                addComposedLayer(result, path, image, settings, true, true);
                return finishResult();
            }
            addDirectMatchedPngTarget(grid, matchedNode, compat::RetainedString::fromStaticLiteral("Matched"), false);
            return finishResult();
        }

        if (addComposedTargetsFromFlatDepth()) {
            return finishResult();
        }

        {
            mapping.status = compat::RetainedString::fromStaticLiteral("Unmatched");
            result.unmatchedLayers++;
            result.mappings.append(mapping);
            if (hasFlatColorSource) {
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("unbound-composed-layer"),
                    compat::RetainedString::fromStaticLiteral(
                        "Flat color/depth 1:1 composed layer has no matching target binding."),
                    flatColorLayerPath, flatColorLayerName);
            } else {
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("missing-color-source"),
                    compat::RetainedString::fromStaticLiteral(
                        "Flat depth source has no matching or color/art-backed target layer."),
                    layerPath, layerName);
            }
            auto image = basePngImage();
            addComposedLayer(result, path, image, settings, true, true);
        }
        return finishResult();
    }
}


}
