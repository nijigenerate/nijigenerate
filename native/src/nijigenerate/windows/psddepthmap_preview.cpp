#include "psddepthmap.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include "detail/psddepthmap_gpu.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/texture_pixels.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/io/depthimage.hpp>
#include <nijigenerate/io/depthmap_psd.hpp>
#include <nijigenerate/io/depthmap_psd_composition.hpp>
#include <nijigenerate/io/depthmap_psd_targets.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <puppet.hpp>
#include <nodes/deformable.hpp>
#include <algorithm>
namespace nijigenerate::windows::psddepthmap {
using namespace detail::helpers;
using namespace commands::depth::map;
using namespace io::depthimage;
namespace {
void text(compat::RetainedString value) { widgets::ngText(compat::RetainedStringSlice(value)); }
void text(std::string value) { widgets::ngText(value); }
}
compat::RetainedString PSDDepthMapWindow::cleanupLayerKey(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        return format("%s\n%s", layerPath, targetGridUuid);
    }

bool PSDDepthMapWindow::applyAlphaDepthGapFillToLayer(PsdDepthComposedLayer& layer, size_t layerIndex) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        static const std::array<DepthDrawAlphaDepthFocusedRule, 5> focusedRules = {
            DepthDrawAlphaDepthFocusedRule{17, 72, 0, 122, 132, 10, 18},
            DepthDrawAlphaDepthFocusedRule{6, 0, 0, 112, 126, 10, 20},
            DepthDrawAlphaDepthFocusedRule{6, 40, 0, 150, 170, 12, 24},
            DepthDrawAlphaDepthFocusedRule{10, 0, 0, 96, 120, 8, 16},
            DepthDrawAlphaDepthFocusedRule{14, 0, 44, 96, 156, 10, 20},
        };
        auto pixelCount = (size_t)dmax(0, mul32(layer.width, layer.height));
        if (pixelCount == 0 || layer.depthRgba.size() != pixelCount * 4 ||
            layer.maskRgba.size() != pixelCount * 4) return false;

        auto depth = ngDepthDrawDecodeGrayscaleDepthPixelsFromRgba(layer.depthRgba);
        compat::Slice<std::uint8_t> mask;
        mask.resize(pixelCount);
        for (std::size_t i = 0; i < static_cast<std::size_t>(pixelCount); ++i) {
            auto offset = i * 4;
            compat::ngArrayAt(mask, i) = compat::ngArrayAt(layer.maskRgba, offset) != 0 &&
                compat::ngArrayAt(layer.maskRgba, offset + 3) != 0 ? 1 : 0;
        }
        auto detected = ngDepthDrawDetectAlphaDepthGaps(
            depth, mask, layer.width, layer.height, (int)layerIndex, focusedRules);
        auto filled = ngDepthDrawMedianFillDepth(depth, mask, detected.mask, layer.width, layer.height);
        bool changed = 0;
        auto filledDepths = filled.depth;
        for (std::size_t i = 0; i < filledDepths.size(); ++i) {
            auto value = compat::ngArrayAt(filledDepths, i);
            if (!compat::ngArrayAt(detected.mask, i) || compat::ngArrayAt(depth, i) == value) continue;
            auto offset = i * 4;
            changed = true;
            compat::ngArrayAt(layer.depthRgba, offset) = value;
            compat::ngArrayAt(layer.depthRgba, offset + 1) = value;
            compat::ngArrayAt(layer.depthRgba, offset + 2) = value;
            compat::ngArrayAt(layer.depthRgba, offset + 3) = compat::ngArrayAt(mask, i) && value > 0 ? 255 : 0;
        }
        return changed;
    }

void PSDDepthMapWindow::replayAlphaDepthGapFills() {
        const auto keepAlive = weak_from_this().lock();
        auto layers = preview.composedLayers;
        for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
            auto& layer = compat::ngArrayAt(layers, layerIndex);
            const auto key = compat::RetainedStringSlice(cleanupLayerKey(layer.layerPath, layer.targetGridUuid));
            auto applied = lookup(alphaDepthGapFillAppliedByLayer, key);
            if (applied == nullptr || !*applied) continue;
            applyAlphaDepthGapFillToLayer(layer, layerIndex);
        }
    }

void PSDDepthMapWindow::rebuildPreview() {
        const auto keepAlive = weak_from_this().lock();
        cancelPendingPreviewGpuComposition();
        disposePreviewTextures();
        pending3DAdjustLayerChanges = {};
        auto restoredLayerStates = pendingLayerStatesAfterRebuild;
        pendingLayerStatesAfterRebuild = {};
        auto previous = preview;
        preview = PsdDepthImportResult{};
        composedPreview = PsdDepthComposedView{};
        errorMessage = {};
        lastApplyErrorMessage = {};
        auto puppet = project::incActivePuppet();
        if (puppet == nullptr) {
            errorMessage = tr("No active puppet");
            previewDirty = false;
            return;
        }
        std::shared_ptr<compat::Throwable> exception;
        try { preview = ngBuildPsdDepthsFromSource(puppet, path, settings); }
        catch (compat::ThrowableException& ex) {
            exception = nicxlive::core::d_runtime::ngRetainCurrentThrowable(ex);
        }
        if (exception) {
            errorMessage = exception->msg;
        } else {
            if (previous.composedLayers.size() > 0) {
                ngPsdDepthApplyPreviousComposedLayerState(preview, previous);
            }
            for (auto state : restoredLayerStates) {
                if (auto layer = findComposedLayer(state.layerPath, state.targetGridUuid)) {
                    applyDialogLayerStateFields(*layer, state);
                }
            }
            replayAlphaDepthGapFills();
            startPreviewComposition();
        }
        previewDirty = false;
    }

bool PSDDepthMapWindow::previewCompositionPending() const {
        return pendingPreviewGpuComposition.isActive();
    }

void PSDDepthMapWindow::cancelPendingPreviewGpuComposition() {
        const auto keepAlive = weak_from_this().lock();
        if (pendingPreviewGpuComposition.isActive()) {
            ngCancelPsdDepthImportGpu(pendingPreviewGpuComposition);
        }
    }

bool PSDDepthMapWindow::startPreviewComposition() {
        const auto keepAlive = weak_from_this().lock();
        cancelPendingPreviewGpuComposition();
        composedPreview = PsdDepthComposedView{};
        compat::RetainedString composeError;
        #if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
            if (failNextPreviewCompositionForRegression) {
                failNextPreviewCompositionForRegression = false;
                errorMessage = compat::RetainedString::fromStaticLiteral("Forced PSD depth preview composition failure");
                return false;
            }
#endif
        if (!preview.gpuCompositionRequested) {
            if (!ngComposePsdDepthImportResult(preview, composedPreview, composeError)) {
                errorMessage = composeError.size() ? composeError
                    : compat::RetainedString::fromStaticLiteral("PSD depth map composition failed");
                return false;
            }
            errorMessage = {};
            return true;
        }

        PsdDepthGpuComposeWork work;
        if (!ngSubmitPsdDepthImportGpu(preview, work, composeError)) {
            errorMessage = composeError.size() ? composeError
                : compat::RetainedString::fromStaticLiteral("PSD depth map GPU composition failed");
            return false;
        }
        bool ready = 0;
        if (!ngPollPsdDepthImportGpu(preview, work, ready, composedPreview, composeError)) {
            errorMessage = composeError.size() ? composeError
                : compat::RetainedString::fromStaticLiteral("PSD depth map GPU composition failed");
            return false;
        }
        if (!ready) pendingPreviewGpuComposition = work;
        errorMessage = {};
        return true;
    }

void PSDDepthMapWindow::pollPendingPreviewGpuComposition() {
        const auto keepAlive = weak_from_this().lock();
        if (!pendingPreviewGpuComposition.isActive()) return;
        bool ready = 0;
        compat::RetainedString composeError;
        if (!ngPollPsdDepthImportGpu(preview, pendingPreviewGpuComposition,
            ready, composedPreview, composeError)) {
            errorMessage = composeError.size() ? composeError
                : compat::RetainedString::fromStaticLiteral("PSD depth map GPU composition failed");
            return;
        }
        if (ready) {
            errorMessage = {};
            lastApplyErrorMessage = {};
        }
    }

void PSDDepthMapWindow::applyAlphaDepthGapFill() {
        const auto keepAlive = weak_from_this().lock();
        if (previewDirty) rebuildPreview();
        if (errorMessage.size()) return;

        auto layers = preview.composedLayers;
        for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
            auto& layer = compat::ngArrayAt(layers, layerIndex);
            if (applyAlphaDepthGapFillToLayer(layer, layerIndex)) {
                alphaDepthGapFillAppliedByLayer[compat::RetainedStringSlice(
                    cleanupLayerKey(layer.layerPath, layer.targetGridUuid))] = true;
            }
        }

        disposePreviewTextures();
        startPreviewComposition();
        previewDirty = false;
    }

void PSDDepthMapWindow::disposePreviewTextures() {
        const auto keepAlive = weak_from_this().lock();
        if (auto entries = originalPreviewTextures; !entries.empty()) {
            for (auto [key, texture] : entries) {
                if (texture != nullptr) texture->dispose();
            }
        }
        if (auto entries = depthMaskPreviewTextures; !entries.empty()) {
            for (auto [key, texture] : entries) {
                if (texture != nullptr) texture->dispose();
            }
        }
        if (auto entries = rawCompositePreviewTextures; !entries.empty()) {
            for (auto [key, texture] : entries) {
                if (texture != nullptr) texture->dispose();
            }
        }
        if (threeDAdjustPreviewTexture != nullptr) {
            const auto receiver = threeDAdjustPreviewTexture;
            receiver->dispose();
        }
        originalPreviewTextures = {};
        depthMaskPreviewTextures = {};
        rawCompositePreviewTextures = {};
        threeDAdjustSamples = {};
        threeDAdjustMeshes = {};
        threeDAdjustPreparedTransforms = {};
        threeDAdjustFrontIntersections = {};
        threeDAdjustDisplayDepthDirty = {};
        threeDAdjustSamplesPrepared = false;
        if (threeDAdjustGpuRenderer != nullptr) {
            const auto receiver = threeDAdjustGpuRenderer;
            receiver->dispose();
        }
        threeDAdjustGpuRenderer = {};
        threeDAdjustPreviewTexture = {};
        threeDAdjustPreviewWidth = 0;
        threeDAdjustPreviewHeight = 0;
        threeDAdjustPreviewCaptureRgba = {};
        threeDAdjustPreviewCaptureWidth = 0;
        threeDAdjustPreviewCaptureHeight = 0;
        threeDAdjustPreviewDirty = true;
        threeDAdjustCameraLeft = INT32_MIN;
        threeDAdjustCameraTop = INT32_MIN;
        threeDAdjustCameraRight = INT32_MIN;
        threeDAdjustCameraBottom = INT32_MIN;
    }

compat::RetainedString PSDDepthMapWindow::layerCacheKey(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        return allocatePsdDepthMemory([&] {
            auto prefix = std::string(layerPath.view()) + char{0};
            const auto suffix = std::to_string(targetGridUuid);
            return compat::RetainedString(prefix + suffix);
        });
    }

std::shared_ptr<PsdDepthComposedLayer> PSDDepthMapWindow::findLayerPreview(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        auto layers = preview.composedLayers;
        for (auto& layerPreview : layers) {
            if (layerPreview.layerPath == layerPath && layerPreview.targetGridUuid == targetGridUuid) {
                return retainedElement(layers, &layerPreview);
            }
        }
        return {};
    }

std::shared_ptr<PsdDepthComposedLayer> PSDDepthMapWindow::findComposedLayer(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        auto layers = preview.composedLayers;
        for (auto& layer : layers) {
            if (layer.layerPath == layerPath && layer.targetGridUuid == targetGridUuid)
                return retainedElement(layers, &layer);
        }
        return {};
    }

std::shared_ptr<PsdDepthLayerMapping> PSDDepthMapWindow::findMapping(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        auto mappings = preview.mappings;
        for (auto& mapping : mappings) {
            if (mapping.layerPath == layerPath && mapping.targetGridUuid == targetGridUuid)
                return retainedElement(mappings, &mapping);
        }
        return {};
    }

bool PSDDepthMapWindow::hasOtherMappings() {
        for (auto& mapping : preview.mappings) {
            if (!mapping.matched || mapping.targetGridUuid == 0) return true;
        }
        for (auto& layer : preview.composedLayers) {
            if (layer.targetGridUuid == 0) return true;
        }
        return false;
    }

compat::RetainedString PSDDepthMapWindow::firstOtherLayerPath() {
        for (auto& mapping : preview.mappings) {
            if (mapping.matched && mapping.targetGridUuid != 0) continue;
            if (findLayerPreview(mapping.layerPath, mapping.targetGridUuid) != nullptr) return mapping.layerPath;
        }
        for (auto& layer : preview.composedLayers) {
            if (layer.targetGridUuid != 0) continue;
            if (findLayerPreview(layer.layerPath, layer.targetGridUuid) != nullptr) return layer.layerPath;
        }
        return {};
    }

TextureRef PSDDepthMapWindow::firstOtherPreviewTexture() {
        const auto keepAlive = weak_from_this().lock();
        auto layerPath = firstOtherLayerPath();
        return layerPath.size() ? layerPreviewTexture(layerPath, 0, true) : TextureRef{};
    }

bool PSDDepthMapWindow::isOthersSelected() {
        return selectedGridIndex == (std::ptrdiff_t)preview.grids.size();
    }

compat::Slice<compat::RetainedString> PSDDepthMapWindow::diagnostics() {
        compat::Slice<compat::RetainedString> lines;
        const auto keepAlive = weak_from_this().lock();
        if (errorMessage.size()) {
            const auto pattern = tr("Preview failed: %s");
            const auto message = errorMessage;
            lines.append(format(pattern, message));
            return lines;
        }
        if (lastApplyErrorMessage.size()) {
            const auto pattern = tr("GPU/apply failed: %s");
            const auto message = lastApplyErrorMessage;
            lines.append(format(pattern, message));
        }
        if (previewCompositionPending()) {
            lines.append(tr("GPU preview composition is still running."));
        }
        if (preview.compositionModeName.size()) {
            const auto pattern = tr(
                "Composition: %s  %dx%d  color layers: %d  depth layers: %d  composed layers: %d  scale: %.3f");
            const auto mode = preview.compositionModeName;
            const auto width = preview.compositionWidth;
            const auto height = preview.compositionHeight;
            const auto colorLayers = static_cast<int>(preview.colorLayerCount);
            const auto depthLayers = static_cast<int>(preview.sourceDepthLayerCount);
            const auto composedLayers = static_cast<int>(preview.composedLayerCount);
            const auto scale = preview.globalDepthScale;
            lines.append(format(pattern, mode, width, height, colorLayers, depthLayers, composedLayers, scale));
        }
        if (preview.colorSource.kindName.size() || preview.depthSource.kindName.size()) {
            const auto pattern = tr("Sources: color=%s(%d) depth=%s(%d)");
            const auto colorKind = preview.colorSource.kindName.size()
                ? preview.colorSource.kindName : compat::RetainedString::fromStaticLiteral("-");
            const auto colorLayers = static_cast<int>(preview.colorSource.layers.size());
            const auto depthKind = preview.depthSource.kindName.size()
                ? preview.depthSource.kindName : compat::RetainedString::fromStaticLiteral("-");
            const auto depthLayers = static_cast<int>(preview.depthSource.layers.size());
            lines.append(format(pattern, colorKind, colorLayers, depthKind, depthLayers));
        }
        for (auto diagnostics = preview.compositionDiagnostics; auto diagnostic : diagnostics) {
            auto subject = diagnostic.layerPath.size() ? diagnostic.layerPath : diagnostic.layerName;
            if (subject.size()) {
                const auto pattern = tr("%s: %s");
                lines.append(format(pattern, diagnostic.type, subject));
            } else if (diagnostic.message.size()) {
                const auto pattern = tr("%s: %s");
                lines.append(format(pattern, diagnostic.type, diagnostic.message));
            } else {
                lines.append(diagnostic.type);
            }
        }
        if (preview.composedLayers.size() == 0) {
            lines.append(tr("No source layers were loaded from the selected depth source."));
        }
        if (preview.mappings.size() > 0 && preview.grids.size() == 0) {
            lines.append(tr("No mapped targets were found. Remap a source layer to a GridDeformer or PathDeformer."));
        }
        for (auto mappings = preview.mappings; auto mapping : mappings) {
            if (mapping.status == "UnmatchedWithoutGrid" || mapping.status == "AmbiguousWithoutGrid") {
                const auto pattern = tr("Unsupported target type for layer: %s");
                lines.append(format(pattern, mapping.layerPath));
            } else if (mapping.status == "UnmatchedManualGrid") {
                const auto pattern = tr("Mapped target is missing for layer: %s");
                lines.append(format(pattern, mapping.layerPath));
            }
        }
        if (preview.gpuCompositionRequested) {
            lines.append(tr("GPU Composition is selected. Apply must submit GPU work; CPU fallback is disabled."));
        }
        return lines;
    }

void PSDDepthMapWindow::drawDiagnostics() {
        auto lines = diagnostics();
        for (auto line : lines) text(line);
    }

compat::Slice<std::uint8_t> PSDDepthMapWindow::transformedDepthPreviewRgba(PsdDepthComposedLayer& layerPreview) {
        compat::Slice<std::uint8_t> rgba;
        const auto keepAlive = weak_from_this().lock();
        if (layerPreview.width <= 0 || layerPreview.height <= 0) return rgba;
        // Retain the actual input cells across allocation and sampling callbacks.
        const auto inputLayers = preview.composedLayers;
        auto pixelCount = (size_t)layerPreview.width * (size_t)layerPreview.height;
        rgba.resize(pixelCount * 4);
        auto back = layerPreview.backDepth;
        auto front = layerPreview.frontDepth;
        auto minDepth = dmin(back, front);
        auto maxDepth = dmax(back, front);
        auto range = maxDepth - minDepth;
        if (range <= 0.000001f) range = 1.0f;
        for (std::size_t i = 0; i < static_cast<std::size_t>(pixelCount); ++i) {
            auto index = i * 4;
            if (index + 3 >= layerPreview.maskRgba.size() ||
                index + 3 >= layerPreview.colorRgba.size() ||
                compat::ngArrayAt(layerPreview.maskRgba, index) == 0 ||
                compat::ngArrayAt(layerPreview.maskRgba, index + 3) == 0 ||
                compat::ngArrayAt(layerPreview.colorRgba, index + 3) < 3) {
                continue;
            }
            float depth = std::numeric_limits<float>::quiet_NaN();
            auto x = (int)(i % (size_t)layerPreview.width);
            auto y = (int)(i / (size_t)layerPreview.width);
            if (!threeDAdjustDepthAt(layerPreview, x, y, depth)) continue;
            auto normalized = dclamp((depth - minDepth) / range, 0.0f, 1.0f);
            auto gray = (std::uint8_t)dclamp(toSourceInt((normalized * 255.0f + 0.5f)), 0, 255);
            compat::ngArrayAt(rgba, index + 0) = gray;
            compat::ngArrayAt(rgba, index + 1) = gray;
            compat::ngArrayAt(rgba, index + 2) = gray;
            compat::ngArrayAt(rgba, index + 3) = 255;
        }
        return rgba;
    }

TextureRef PSDDepthMapWindow::layerPreviewTexture(compat::RetainedString layerPath, std::uint64_t targetGridUuid, bool depthMask) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        auto layerPreview = findLayerPreview(layerPath, targetGridUuid);
        if (layerPreview == nullptr) return {};
        const auto key = compat::RetainedStringSlice(layerCacheKey(layerPath, targetGridUuid));
        auto existing = depthMask ? lookup(depthMaskPreviewTextures, key) : lookup(originalPreviewTextures, key);
        if (existing != nullptr) return *existing;

        auto rgba = depthMask ? transformedDepthPreviewRgba(*layerPreview) : layerPreview->colorRgba.duplicate();
        auto textureWidth = depthMask || layerPreview->width <= 0 ? layerPreview->width : layerPreview->width;
        auto textureHeight = depthMask || layerPreview->height <= 0 ? layerPreview->height : layerPreview->height;
        core::texture_pixels::ngTexPremultiply(std::span<std::uint8_t>(rgba.data(), rgba.size()));
        const auto textureData = nicxlive::core::SourceTextureBytes::retainedView(
            rgba.retentionOwner(), rgba.data(), rgba.size());
        auto texture = nicxlive::core::Texture::createSource(textureData, textureWidth, textureHeight);
        if (depthMask) depthMaskPreviewTextures[key] = texture;
        else originalPreviewTextures[key] = texture;
        return texture;
    }

TextureRef PSDDepthMapWindow::compositePreviewTexture(PsdDepthGridResult& gridResult) {
        const auto keepAlive = weak_from_this().lock();
        const auto gridStorage = preview.grids;
        auto source = gridResult.rawCompositePreviewRgba;
        if (gridResult.grid == nullptr || source.size() == 0 ||
            gridResult.previewWidth <= 0 || gridResult.previewHeight <= 0) return {};

        const auto gridUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
        const auto key = compat::RetainedStringSlice(decimal(gridUuid));
        auto existing = lookup(rawCompositePreviewTextures, key);
        if (existing) return *existing;

        auto rgba = source.duplicate();
        core::texture_pixels::ngTexPremultiply(std::span<std::uint8_t>(rgba.data(), rgba.size()));
        const auto textureData = nicxlive::core::SourceTextureBytes::retainedView(
            rgba.retentionOwner(), rgba.data(), rgba.size());
        auto texture = nicxlive::core::Texture::createSource(textureData,
            gridResult.previewWidth, gridResult.previewHeight);
        rawCompositePreviewTextures[key] = texture;
        return texture;
    }

void PSDDepthMapWindow::drawLayerPreviewTooltip(compat::RetainedString layerPath, std::uint64_t targetGridUuid, bool depthMask) {
        const auto keepAlive = weak_from_this().lock();
        const auto layerStorage = preview.composedLayers;
        auto layerPreview = findLayerPreview(layerPath, targetGridUuid);
        if (layerPreview == nullptr) return;
        auto texture = layerPreviewTexture(layerPath, targetGridUuid, depthMask);
        if (texture == nullptr) return;

        ImGui::BeginTooltip();
        text(depthMask ? tr("Depth Mask Preview") : tr("Layer Preview"));
        text(format("%s  %dx%d  (%d, %d)", 
            layerPreview->layerName,
            layerPreview->width,
            layerPreview->height,
            layerPreview->left,
            layerPreview->top
        ));
        auto maxDimension = (float)(layerPreview->width > layerPreview->height ? layerPreview->width : layerPreview->height);
        auto scale = maxDimension > 0 ? dmin(PreviewSize / maxDimension, 1.0f) : 1.0f;
        const auto image = compat::ngRequireObject(texture)->getTextureId();
        ImGui::Image(
            static_cast<ImTextureID>(image),
            ImVec2((float)layerPreview->width * scale, (float)layerPreview->height * scale)
        );
        ImGui::EndTooltip();
    }

void PSDDepthMapWindow::drawLayerPreviewHoverLabel(compat::RetainedString label, compat::RetainedString layerPath, std::uint64_t targetGridUuid, bool depthMask) {
        const auto keepAlive = weak_from_this().lock();
        text(label);
        if (ImGui::IsItemHovered()) drawLayerPreviewTooltip(layerPath, targetGridUuid, depthMask);
    }

void PSDDepthMapWindow::drawLayerPreviewHoverText(compat::RetainedString label, compat::RetainedString layerPath, std::uint64_t targetGridUuid, bool depthMask) {
        const auto keepAlive = weak_from_this().lock();
        text(label.size() ? label : compat::RetainedString::fromStaticLiteral("-"));
        if (ImGui::IsItemHovered()) drawLayerPreviewTooltip(layerPath, targetGridUuid, depthMask);
    }

ExPartRef PSDDepthMapWindow::matchedPart(PsdDepthLayerMapping& mapping) {
        const auto keepAlive = weak_from_this().lock();
        if (mapping.matchedNodeUuid == 0) return {};
        auto puppet = project::incActivePuppet();
        if (puppet == nullptr || puppet->root == nullptr) return {};
        for (auto part : puppet->findNodesType<ext::nodes::ExPart>(puppet->root)) {
            const auto uuid = compat::ngRequireObject(part)->getUUID();
            if (uuid == mapping.matchedNodeUuid) return part;
        }
        return {};
    }

void PSDDepthMapWindow::drawMatchedNodeTooltip(PsdDepthLayerMapping& mapping) {
        const auto keepAlive = weak_from_this().lock();
        const auto mappingStorage = preview.mappings;
        auto part = matchedPart(mapping);
        if (part == nullptr || part->textures.size() == 0 || compat::ngArrayAt(part->textures, 0) == nullptr) return;

        auto texture = compat::ngArrayAt(part->textures, 0);
        ImGui::BeginTooltip();
        text(tr("Matched Node Preview"));
        text(mapping.matchedNodeName);
        const auto textureWidth = texture->width();
        const auto textureHeight = texture->height();
        auto maxDimension = static_cast<float>(dmax(textureWidth, textureHeight));
        auto scale = maxDimension > 0 ? dmin(PreviewSize / maxDimension, 1.0f) : 1.0f;
        const auto image = compat::ngRequireObject(texture)->getTextureId();
        const auto imageWidth = texture->width();
        const auto imageHeight = texture->height();
        ImGui::Image(static_cast<ImTextureID>(image),
            ImVec2(static_cast<float>(imageWidth) * scale, static_cast<float>(imageHeight) * scale));
        ImGui::EndTooltip();
    }

void PSDDepthMapWindow::drawMatchedNodeText(PsdDepthLayerMapping& mapping) {
        const auto keepAlive = weak_from_this().lock();
        const auto mappingStorage = preview.mappings;
        text(mapping.matchedNodeName.size() ? mapping.matchedNodeName
            : compat::RetainedString::fromStaticLiteral("-"));
        if (ImGui::IsItemHovered()) drawMatchedNodeTooltip(mapping);
    }

ContextRef PSDDepthMapWindow::dialogCommandContext() {
        const auto keepAlive = weak_from_this().lock();
        auto ctx = allocatePsdDepthObject<commands::Context>();
        auto puppet = project::incActivePuppet();
        if (puppet != nullptr) ctx->puppet(puppet);
        auto nodes = selectedDialogContextNodes();
        if (nodes.size()) ctx->nodes(nodes);
        return ctx;
    }

ContextRef PSDDepthMapWindow::dialogLayerCommandContext(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        const auto keepAlive = weak_from_this().lock();
        auto ctx = allocatePsdDepthObject<commands::Context>();
        auto puppet = project::incActivePuppet();
        if (puppet != nullptr) ctx->puppet(puppet);
        auto nodes = dialogContextNodesForLayer(layerPath, targetGridUuid);
        if (nodes.size()) ctx->nodes(nodes);
        return ctx;
    }

ContextRef PSDDepthMapWindow::dialogNodeCommandContext(NodeRef node) {
        const auto keepAlive = weak_from_this().lock();
        auto ctx = allocatePsdDepthObject<commands::Context>();
        auto puppet = project::incActivePuppet();
        if (puppet != nullptr) ctx->puppet(puppet);
        if (node != nullptr) ctx->nodes({node});
        return ctx;
    }


}
