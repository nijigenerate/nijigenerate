// Independent depthmap_psd.d full PSD-source import branch.
#include "depthmap_psd_loading.hpp"
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include "depthmap_psd_math.hpp"
#include "pngheader.hpp"
#include "psdlayers.hpp"
#include "psdbudget.hpp"
#include <texture.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>
namespace nijigenerate::io::depthmap_psd {
using namespace detail;
using nicxlive::core::ShallowTexture;
using nicxlive::core::nodes::Part;
PsdDepthImportResult ngBuildPsdDepthsFromPSD(detail::PuppetRef puppet, compat::RetainedString path, PsdDepthImportSettings settings) {
    enforce(puppet != nullptr && puppet->root != nullptr, "No active puppet", 3031);
    enforce(path.size() > 0, "Path not provided", 3032);
    enforce(std::isfinite(settings.backDepth) && std::isfinite(settings.frontDepth),
        "Depth range must be finite", 3033);
    enforce(std::isfinite(settings.depthScale), "Depth scale must be finite", 3034);
    enforce(settings.depthScale >= 0.0f, "Depth scale must be non-negative", 3035);
    enforce(settings.alphaThreshold >= 0.0f && settings.alphaThreshold <= 1.0f,
        "Alpha threshold must be in [0, 1]", 3036);
    enforce(settings.customRadius >= 1 && settings.customRadius <= 64,
        "Custom radius must be in [1, 64]", 3037);
    settings.zeroDepthIsMissing = true;

    psd::RetainedFile file(path);
    const auto buildResult = [&]() -> PsdDepthImportResult {
    auto document = psd::parseDocument(file);
    
    std::uint64_t retainedPsdBytes = 0;
    bool hasExplicitColorSource = settings.colorSourcePath.size() > 0;
    bool hasFlatColorSource = hasExplicitColorSource && sourceExtensionLower(settings.colorSourcePath).view() == ".png";
    bool hasPsdColorSource = hasExplicitColorSource && sourceExtensionLower(settings.colorSourcePath).view() == ".psd";
    ShallowTexture flatColorTexture;
    compat::RetainedString flatColorLayerName;
    compat::RetainedString flatColorLayerPath;
    if (hasFlatColorSource) {
        int inspectedWidth = 0;
        int inspectedHeight = 0;
        pngheader::ngInspectPngDimensions(
            compat::RetainedStringSlice(settings.colorSourcePath), inspectedWidth, inspectedHeight);
        enforce(inspectedWidth == document.width && inspectedHeight == document.height,
            "Color and depth source dimensions must match for PSD depth composition", 3055);
        enforce(ngReservePsdDepthRetainedLayer(inspectedWidth, inspectedHeight,
            PsdDepthRetainedColorLayerBytesPerPixel, retainedPsdBytes),
            "PSD Depth Map color source exceeds the retained image memory budget", 3057);
        flatColorTexture = ShallowTexture(settings.colorSourcePath, 4);
        enforce(flatColorTexture.width == inspectedWidth && flatColorTexture.height == inspectedHeight,
            "Decoded PNG dimensions do not match the IHDR header", 3061);
        flatColorLayerName = sourceStem(settings.colorSourcePath);
        flatColorLayerPath = allocateImportBytes([&] {
            return compat::RetainedString("/" + std::string(flatColorLayerName.view()));
        });
    } else if (hasExplicitColorSource && !hasPsdColorSource) {
        enforce(false, "Unsupported color source format for PSD depth composition", 3066);
    }

    PsdDepthImportResult result;
    result.gpuCompositionRequested = settings.useGpuComposition;
    result.smoothWavySurface = settings.smoothWavySurface;
    setCompositionMode(result, PsdDepthCompositionMode::NToN);
    result.compositionWidth = document.width;
    result.compositionHeight = document.height;
    result.globalDepthScale = settings.depthScale;
    result.missingPolicy = settings.missingPolicy;
    result.missingBackDepth = settings.backDepth;
    result.depthSource.kind = PsdDepthCompositeSourceKind::PsdLayers;
    result.depthSource.kindName = ngPsdDepthCompositeSourceKindName(result.depthSource.kind);
    result.depthSource.sourcePath = path;
    result.depthSource.width = document.width;
    result.depthSource.height = document.height;
    result.colorSource.kind = hasPsdColorSource ? PsdDepthCompositeSourceKind::PsdLayers :
        (hasFlatColorSource ? PsdDepthCompositeSourceKind::FlatImage : PsdDepthCompositeSourceKind::ActiveArtTargets);
    result.colorSource.kindName = ngPsdDepthCompositeSourceKindName(result.colorSource.kind);
    result.colorSource.sourcePath = hasExplicitColorSource ? settings.colorSourcePath : compat::RetainedString{};
    result.colorSource.width = document.width;
    result.colorSource.height = document.height;
    if (hasFlatColorSource) {
        result.colorLayerCount = 1;
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
        enforce(result.colorSource.width == document.width && result.colorSource.height == document.height,
            "Color and depth source dimensions must match for PSD depth composition", 3107);
    }
    compat::Slice<DepthLayerImage> layers;
    nicxlive::core::common::SharedArray<bool> matchedColorSourceLayers;
    if (hasPsdColorSource) matchedColorSourceLayers.resize(result.colorSource.layers.size());

    auto groupStates = psdlayers::ngPsdLayerGroupStates({document.layers.data(), document.layers.size()});
    PsdLayerPathOccurrences layerPathOccurrences;
    nicxlive::core::common::StringKeyMap<PsdClippingBaseState> clippingBaseByGroup;
    for (auto i = document.layers.size(); i-- > 0;) {
        auto layer = document.layers[i];
        if (!ngPsdDepthLayerHasPixelData(layer)) continue;
        auto groupState = groupStates[i];

        result.sourceDepthLayerCount++;

        auto pathBytes = allocateImportBytes([&] {
            return compat::RetainedString(
                std::string(groupState.path.view()) + "/" + std::string(layer.name.view()));
        });
        auto layerPath = uniquePsdLayerPath(std::move(pathBytes), layerPathOccurrences);
        auto layerVisible = groupState.visible && psdLayerVisible(layer);
        auto effectiveLayerOpacity = groupState.opacity * layerOpacity01(layer.opacity);
        PsdDepthLayerMapping mapping;
        mapping.layerPath = layerPath;
        mapping.layerName = layer.name;

        enforce(psdbudget::ngReservePsdLayerExtraction(layer,
            PsdDepthRetainedDepthLayerBytesPerPixel, PsdDepthMaxRetainedBytes, retainedPsdBytes),
            "PSD Depth Map layers exceed the retained image memory budget", 3130);
        layer.extractLayerImage();
        bool hasLayerImage = layer.data.size() > 0;
        DepthLayerImage image;
        if (hasLayerImage) {
            image.layerPath = layerPath;
            image.layerName = layer.name;
            image.left = layer.left;
            image.top = layer.top;
            image.width = std::bit_cast<std::int32_t>(layer.width());
            image.height = std::bit_cast<std::int32_t>(layer.height());
            image.documentWidth = document.width;
            image.documentHeight = document.height;
            image.opacity = effectiveLayerOpacity;
            image.data = layer.data;
            layer.data = {};
            if (layer.clipping) {
                clippingBaseByGroup.setRetained(groupState.path, psdClippingBaseState(
                    image, layerVisible, effectiveLayerOpacity));
            } else if (auto clippingBase = clippingBaseByGroup.findRetained(groupState.path.view())) {
                layerVisible = layerVisible && clippingBase->visible;
                auto baseLocalOpacity = groupState.opacity > 0.0f
                    ? clippingBase->opacity / groupState.opacity
                    : 0.0f;
                effectiveLayerOpacity *= std::max(0.0f, std::min(1.0f, baseLocalOpacity));
                image.opacity = effectiveLayerOpacity;
                applyPsdClippingBaseAlpha(image, *clippingBase);
            }
            if (hasPsdColorSource) {
                image.coverageWidth = image.width;
                image.coverageHeight = image.height;
                image.coverageChannels = 4;
                image.coverageOpacity = 1.0f;
                image.coverageData.resize((size_t)image.width * (size_t)image.height * 4);
                auto depthCandidate = makeCompositeSourceLayer(
                    layerPath,
                    layer.name,
                    layer.left,
                    layer.top,
                    std::bit_cast<std::int32_t>(layer.width()),
                    std::bit_cast<std::int32_t>(layer.height()),
                    layerVisible,
                    true,
                    0
                );
                auto colorIndex = bestMatchingColorSourceLayer(
                    depthCandidate, result.colorSource.layers, matchedColorSourceLayers);
                if (colorIndex >= 0) {
                    matchedColorSourceLayers[(size_t)colorIndex] = true;
                    attachCompositeSourceCoverage(image, result.colorSource.layers[(size_t)colorIndex]);
                }
            } else if (hasFlatColorSource) {
                image.coverageWidth = flatColorTexture.width;
                image.coverageHeight = flatColorTexture.height;
                image.coverageChannels = flatColorTexture.channels;
                image.coverageOpacity = 1.0f;
                image.coverageData = compat::Slice<std::uint8_t>::retainedView(
                    flatColorTexture.data.retentionOwner(),
                    flatColorTexture.data.data(), flatColorTexture.data.size()).duplicate();
            }
            auto depthSourceLayer = makeCompositeSourceLayer(
                layerPath,
                layer.name,
                layer.left,
                layer.top,
                std::bit_cast<std::int32_t>(layer.width()),
                std::bit_cast<std::int32_t>(layer.height()),
                layerVisible,
                true,
                computeLayerDepthStats(image, settings).maskedPixels
            );
            depthSourceLayer.opacity = effectiveLayerOpacity;
            auto depthPixels = image.data;
            auto depthMask = buildDepthMaskPreview(image, settings);
            setCompositeSourceLayerPixels(depthSourceLayer, depthPixels, depthMask);
            result.depthSource.layers.append(depthSourceLayer);
        }

        DeformableRef grid;
        NodeRef matchedNode;
        if (auto ignored = settings.ignoredLayerPaths.findRetained(layerPath.view())) {
            if (*ignored) {
                mapping.ignored = true;
                mapping.manual = true;
                mapping.status = compat::RetainedString::fromStaticLiteral("Ignored");
                result.mappings.append(mapping);
                if (hasLayerImage) {
                    addComposedLayer(result, path, image, settings, layerVisible, false);
                }
                layer.data = {};
                continue;
            }
        }

        if (auto overrideUuid = settings.layerTargetGridUuidOverrides.findRetained(layerPath.view())) {
            grid = findGridByUuid(puppet, *overrideUuid);
            mapping.manual = true;
            if (grid == nullptr) {
                mapping.status = compat::RetainedString::fromStaticLiteral("UnmatchedManualGrid");
                result.unmatchedLayers++;
                result.mappings.append(mapping);
                if (hasLayerImage) {
                    addComposedLayer(result, path, image, settings, layerVisible, true);
                }
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("missing-manual-target"),
                    compat::RetainedString::fromStaticLiteral(
                        "Manual target binding points to a missing GridDeformer or PathDeformer."),
                    layerPath, layer.name);
                layer.data = {};
                continue;
            }
            matchedNode = grid;
            mapping.matched = true;
            mapping.matchedNodeName = ext::nodes::ngRetainNodeName(grid->getName()).retainedString();
            mapping.matchedNodeUuid = grid->getUUID();
            mapping.targetGridName = ext::nodes::ngRetainNodeName(grid->getName()).retainedString();
            mapping.targetGridUuid = grid->getUUID();
            mapping.status = compat::RetainedString::fromStaticLiteral("Manual");
        } else {
            auto candidates = matchCandidates(puppet, layerPath, layer.name, settings,
                layer.left, layer.top, std::bit_cast<std::int32_t>(layer.width()), std::bit_cast<std::int32_t>(layer.height()), document.width, document.height, true);
            if (candidates.size() == 0) {
                mapping.status = compat::RetainedString::fromStaticLiteral("Unmatched");
                result.unmatchedLayers++;
                result.mappings.append(mapping);
                if (hasLayerImage) {
                    addComposedLayer(result, path, image, settings, layerVisible, true);
                }
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("unused-depth-layer"),
                    compat::RetainedString::fromStaticLiteral("Depth layer did not match any color/target layer."),
                    layerPath, layer.name);
                layer.data = {};
                continue;
            }

            auto resolution = resolveCandidateTarget(puppet, candidates);
            mapping.ambiguous = resolution.ambiguous;
            if (resolution.ambiguous) {
                mapping.status = compat::RetainedString::fromStaticLiteral("Ambiguous");
                result.ambiguousLayers++;
                result.unmatchedLayers++;
                result.mappings.append(mapping);
                if (hasLayerImage) addComposedLayer(result, path, image, settings, layerVisible, false);
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("ambiguous-target"),
                    compat::RetainedString::fromStaticLiteral(
                        "Depth layer matches multiple targets and requires a manual binding."),
                    layerPath, layer.name);
                layer.data = {};
                continue;
            }
            matchedNode = resolution.node;
            grid = resolution.target;
            mapping.matchedNodeName = matchedNode == nullptr ? compat::RetainedString{}
                : ext::nodes::ngRetainNodeName(matchedNode->getName()).retainedString();
            mapping.matchedNodeUuid = matchedNode == nullptr ? 0 : matchedNode->getUUID();
            mapping.status = compat::RetainedString::fromStaticLiteral("Matched");

            if (grid == nullptr) {
                mapping.status = compat::RetainedString::fromStaticLiteral("UnmatchedWithoutGrid");
                result.unmatchedLayers++;
                result.mappings.append(mapping);
                if (hasLayerImage) {
                    addComposedLayer(result, path, image, settings, layerVisible, true);
                }
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("unused-depth-layer"),
                    compat::RetainedString::fromStaticLiteral(
                        "Depth layer matched a node that is not inside a depth target."),
                    layerPath, layer.name);
                layer.data = {};
                continue;
            }

            mapping.matched = true;
            mapping.targetGridName = ext::nodes::ngRetainNodeName(grid->getName()).retainedString();
            mapping.targetGridUuid = grid->getUUID();
        }

        result.matchedLayers++;
        result.mappings.append(mapping);

        if (!hasLayerImage) continue;
        image.grid = grid;
        if (!hasExplicitColorSource) attachMatchedCoverage(image, puppet, matchedNode, grid);
        buildCoverageCache(image);
        addComposedLayer(result, path, image, settings, layerVisible,
            ngPsdDepthGridLayerEnabled(settings, grid->getUUID(), layerPath), grid);

        if (layerVisible) layers.append(image);
        layer.data = {};
    }

    compat::Slice<GridAccum> accums;
    for (auto currentLayers = layers; auto& layer : currentLayers) {
        auto sourceAt = [](auto& values, std::size_t index, std::size_t sourceLine) -> decltype(auto) {
            if (index >= values.size())
                throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"),
                    sourceLine);
            return values[index];
        };
        auto accumIndex = findAccum(accums, layer.grid);
        auto layerMaskIndex = findLayerMask(sourceAt(accums, accumIndex, 3314), layer.layerPath, layer.layerName);
        const auto gridUuid = compat::ngRequireObject(layer.grid)->getUUID();
        if (!ngPsdDepthGridEnabled(settings, gridUuid)) continue;
        const auto layerGridUuid = compat::ngRequireObject(layer.grid)->getUUID();
        if (!ngPsdDepthGridLayerEnabled(settings, layerGridUuid, layer.layerPath)) continue;
        auto vertices = compat::ngRequireObject(layer.grid)->getVertices();
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            auto documentGrid = layer.grid;
            const auto documentVertex = sourceAt(vertices, i, 3319);
            auto documentPoint = gridVertexDocumentPosition(std::move(documentGrid), documentVertex,
                document.width, document.height);
            auto layerX = layerToSourceX(layer, documentPoint.x);
            auto layerY = layerToSourceY(layer, documentPoint.y);
            auto sample = sampleLayer(layer, layerX, layerY, settings);
            if (!sample.valid) continue;
            sourceAt(sourceAt(accums, accumIndex, 3324).layerMasks, layerMaskIndex, 3324).sampledVertices++;
            if (!sourceAt(sourceAt(accums, accumIndex, 3325).has, i, 3325) ||
                sample.value > sourceAt(sourceAt(accums, accumIndex, 3325).best, i, 3325)) {
                sourceAt(sourceAt(accums, accumIndex, 3326).has, i, 3326) = true;
                sourceAt(sourceAt(accums, accumIndex, 3327).best, i, 3327) = sample.value;
                sourceAt(sourceAt(accums, accumIndex, 3328).baseBest, i, 3328) = sample.value;
                auto& winnerPath = sourceAt(sourceAt(accums, accumIndex, 3329).winnerLayerPaths, i, 3329);
                winnerPath = layer.layerPath;
            }
        }
    }

    for (auto& accum : accums) {
        PsdDepthGridResult gridResult;
        gridResult.documentWidth = document.width;
        gridResult.documentHeight = document.height;
        finalizeGridResult(gridResult, accum, settings);
        for (auto& layer : layers) {
            if (layer.grid == accum.grid) gridResult.coverageSources += layer.coverageSources.size();
        }
        buildCompositePreview(gridResult, layers, accum.grid, settings);
        if (gridResult.skipped) result.skippedGrids++;
        result.grids.append(gridResult);
    }

    if (hasPsdColorSource) {
        nicxlive::core::common::SharedArray<bool> matchedDepthSourceLayers;
        matchedDepthSourceLayers.resize(result.depthSource.layers.size());
        for (auto colorLayer : result.colorSource.layers) {
            auto depthIndex = bestMatchingDepthSourceLayer(
                colorLayer, result.depthSource.layers, matchedDepthSourceLayers);
            if (depthIndex < 0) {
                PsdDepthComposedLayer missingLayer;
                missingLayer.id = colorLayer.id;
                missingLayer.layerPath = colorLayer.id;
                missingLayer.layerName = colorLayer.name;
                missingLayer.colorLayerPath = colorLayer.id;
                missingLayer.colorLayerName = colorLayer.name;
                missingLayer.left = colorLayer.left;
                missingLayer.top = colorLayer.top;
                missingLayer.width = colorLayer.width;
                missingLayer.height = colorLayer.height;
                missingLayer.visible = colorLayer.visible;
                missingLayer.enabled = false;
                missingLayer.puppetFitEnabled = true;
                result.composedLayers.append(missingLayer);
                result.composedLayerCount = result.composedLayers.size();
                addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("missing-depth-layer"),
                    compat::RetainedString::fromStaticLiteral("PSD color layer has no matched PSD depth layer."),
                    colorLayer.id, colorLayer.name);
                continue;
            }

            matchedDepthSourceLayers[(size_t)depthIndex] = true;
            auto depthLayer = result.depthSource.layers[(size_t)depthIndex];
            for (auto& composedLayer : result.composedLayers) {
                if (composedLayer.layerPath == depthLayer.id) {
                    composedLayer.colorLayerPath = colorLayer.id;
                    composedLayer.colorLayerName = colorLayer.name;
                    break;
                }
            }
        }
        for (std::size_t i = 0; i < matchedDepthSourceLayers.size(); ++i) {
        auto matched = matchedDepthSourceLayers[i];
            if (matched) continue;
            auto depthLayer = result.depthSource.layers[i];
            addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("unused-depth-layer"),
                compat::RetainedString::fromStaticLiteral("PSD depth layer did not match any PSD color layer."),
                depthLayer.id, depthLayer.name);
        
    }
    }

    nicxlive::core::common::StringKeyMap<bool> accountedLayerPaths;
    for (auto mapping : result.mappings) {
        if (mapping.layerPath.size() && ngPsdDepthMappingAccountsForColorLayer(mapping)) {
            accountedLayerPaths.setRetained(mapping.layerPath, true);
        }
    }
    if (!hasExplicitColorSource) {
        UsedArtLayerPaths usedLayerPaths;
        for (auto part : puppet->findNodesType<Part>(puppet->root)) {
            if (part->textures.size() == 0 || part->textures[0] == nullptr) continue;
            auto target = activeArtDepthTarget(puppet, part);
            if (target == nullptr) continue;
            auto layerName = activeArtLayerName(part);
            auto layerPath = uniqueActiveArtLayerPath(part, usedLayerPaths);
            if (layerPath.size() == 0) continue;
            DepthLayerImage image;
            image.layerPath = layerPath;
            image.layerName = layerName;
            image.left = 0;
            image.top = 0;
            image.width = result.compositionWidth;
            image.height = result.compositionHeight;
            image.documentWidth = result.compositionWidth;
            image.documentHeight = result.compositionHeight;
            attachPartCoverage(image, part);
            tightenDepthLayerImageToCoverageBounds(image);
            result.colorLayerCount++;
            result.colorSource.layers.append(makeCompositeSourceLayer(
                layerPath,
                layerName,
                image.left,
                image.top,
                image.width,
                image.height,
                true,
                true,
                0,
                target
            ));
            if (accountedLayerPaths.contains(layerPath.view())) continue;
            addMissingComposedLayer(result, layerPath, layerName, image.left, image.top, image.width, image.height, target);
            addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("missing-depth-layer"),
                compat::RetainedString::fromStaticLiteral("Color/art layer has no matched depth layer."),
                layerPath, layerName);
        }
    }
    auto explicitColorLayerCount = hasExplicitColorSource ? result.colorLayerCount : settings.explicitColorLayerCount;
    if (explicitColorLayerCount > 0) {
        result.colorLayerCount = explicitColorLayerCount;
        auto explicitMode = ngPsdDepthCompositionModeForCounts(explicitColorLayerCount, result.sourceDepthLayerCount);
        if (explicitMode == PsdDepthCompositionMode::UnsupportedOneToN) {
            setCompositionMode(result, explicitMode);
            result.grids = {};
            addCompositionDiagnostic(result, compat::RetainedString::fromStaticLiteral("unsupported-1:N"),
                compat::RetainedString::fromStaticLiteral(
                    "A single color layer cannot be composed with multiple depth layers without a split policy."));
        } else if (hasFlatColorSource) {
            setCompositionMode(result, explicitMode);
        }
    }

    validateCompositionDimensions(result, compat::RetainedString::fromStaticLiteral("PSD depth composition"));
    return result;
    };
    // Construct the source result inside the body, before explicit file cleanup.
    std::optional<PsdDepthImportResult> completed;
    compat::ngRunWithScopeExit([&] { completed.emplace(buildResult()); }, [&] { file.close(); });
    return std::move(*completed);
}


}
