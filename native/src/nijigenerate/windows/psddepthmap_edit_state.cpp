#include "psddepthmap.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include "detail/psddepthmap_state.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/commands/depth/psd_dialog_commands.hpp>
#include <nijigenerate/core/shortcut/base.hpp>
#include <nijigenerate/io/depthmap_psd_enum_functions.hpp>
#include <nodes/deformable.hpp>
#include <string_view>
#include <functional>
namespace nijigenerate::windows::psddepthmap {
using namespace detail::helpers;
using namespace commands;
using namespace commands::depth::map;
using namespace commands::depth::psd_dialog;
using namespace core::actionstack;
using namespace core::shortcut;
namespace {
compat::Slice<NodeRef> psdDepthDialogSelectedNodes() {
    if (!detail::activePsdDepthMapWindow) return {};
    // Retain the evaluated receiver if the virtual call clears the active slot.
    const auto window = detail::activePsdDepthMapWindow;
    return window->selectedDialogContextNodes();
}
}
PsdDepthLayerEdit PSDDepthMapWindow::layerTransform(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        PsdDepthLayerEdit transform;
        if (auto layer = findComposedLayer(layerPath, targetGridUuid)) {
            transform.zOffset = layer->depthOffset;
            transform.zScale = layer->depthScale;
            transform.invert = layer->invert;
        }
        return transform;
    }

DepthSampleConvolution PSDDepthMapWindow::threeDAdjustSampleConvolution() {
        switch (settings.convolution) {
        case PsdDepthConvolution::Nearest:
            return DepthSampleConvolution::Nearest;
        case PsdDepthConvolution::Box3x3:
            return DepthSampleConvolution::Box3x3;
        case PsdDepthConvolution::Box5x5:
            return DepthSampleConvolution::Box5x5;
        case PsdDepthConvolution::Gaussian3x3:
            return DepthSampleConvolution::Gaussian3x3;
        case PsdDepthConvolution::Gaussian5x5:
            return DepthSampleConvolution::Gaussian5x5;
        case PsdDepthConvolution::Median3x3:
            return DepthSampleConvolution::Median3x3;
        case PsdDepthConvolution::Frontmost3x3:
            return DepthSampleConvolution::Frontmost3x3;
        case PsdDepthConvolution::Backmost3x3:
            return DepthSampleConvolution::Backmost3x3;
        case PsdDepthConvolution::BoxCustom:
            return DepthSampleConvolution::BoxCustom;
        case PsdDepthConvolution::GaussianCustom:
            return DepthSampleConvolution::GaussianCustom;
        case PsdDepthConvolution::MedianCustom:
            return DepthSampleConvolution::MedianCustom;
        case PsdDepthConvolution::FrontmostCustom:
            return DepthSampleConvolution::FrontmostCustom;
        case PsdDepthConvolution::BackmostCustom:
            return DepthSampleConvolution::BackmostCustom;
        }
        throw PsdDepthEnumSwitchError(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/windows/psddepthmap.d"), 1442);
    }

bool PSDDepthMapWindow::gridResultUsesLayer(PsdDepthGridResult& gridResult, compat::RetainedString layerPath) {
        for (auto& layerMask : gridResult.layerMasks) {
            if (layerMask.layerPath == layerPath) return true;
        }
        return false;
    }

bool PSDDepthMapWindow::layerTargetsGrid(compat::RetainedString layerPath, std::uint64_t gridUuid) {
        if (gridUuid == 0) return false;
        if (auto composedLayer = findComposedLayer(layerPath, gridUuid)) {
            if (composedLayer->targetGridUuid == gridUuid) return true;
        }
        if (auto mapping = findMapping(layerPath, gridUuid)) {
            if (mapping->targetGridUuid == gridUuid) return true;
        }
        return false;
    }

void PSDDepthMapWindow::disposeCompositePreviewTexture(PsdDepthGridResult& gridResult) {
        const auto keepAlive = weak_from_this().lock();
        const auto gridStorage = preview.grids;
        if (gridResult.grid == nullptr) return;
        const auto uuid = compat::ngRequireObject(gridResult.grid)->getUUID();
        const auto key = compat::RetainedStringSlice(decimal(uuid));
        if (auto texture = lookup(rawCompositePreviewTextures, key)) {
            const auto receiver = *texture;
            if (receiver != nullptr) receiver->dispose();
            rawCompositePreviewTextures.erase(key);
        }
    }

void PSDDepthMapWindow::disposeCompositePreviewTextures() {
        const auto keepAlive = weak_from_this().lock();
        if (auto entries = rawCompositePreviewTextures; !entries.empty()) {
            for (auto [key, texture] : entries) {
                if (texture != nullptr) texture->dispose();
            }
        }
        rawCompositePreviewTextures = {};
    }

void PSDDepthMapWindow::refreshGridResultDepths(PsdDepthGridResult& gridResult) {
        const auto keepAlive = weak_from_this().lock();
        const auto gridStorage = preview.grids;
        if (gridResult.grid == nullptr) return;
        PsdDepthGridResult composedGrid;
        compat::RetainedString composeError;
        if (ngComposePsdDepthTarget(preview, gridResult, composedGrid, composeError)) {
            gridResult = composedGrid;
            disposeCompositePreviewTexture(gridResult);
            return;
        }
        errorMessage = composeError.size() ? composeError
            : compat::RetainedString::fromStaticLiteral("PSD depth map composition failed");
    }

void PSDDepthMapWindow::refreshPreviewDepthsForLayer(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        const auto keepAlive = weak_from_this().lock();
        std::ptrdiff_t changedIndex = -1;
        auto changedLayers = preview.composedLayers;
        for (std::size_t i = 0; i < changedLayers.size(); ++i) {
            auto& layer = compat::ngArrayAt(changedLayers, i);
            if (layer.layerPath == layerPath && layer.targetGridUuid == targetGridUuid) {
                changedIndex = (std::ptrdiff_t)i;
                break;
            }
        }
        for (auto grids = preview.grids; auto& gridResult : grids) {
            if (gridResult.grid == nullptr) continue;
            bool affected = gridResultUsesLayer(gridResult, layerPath) ||
                layerTargetsGrid(layerPath, compat::ngRequireObject(gridResult.grid)->getUUID());
            if (!affected && changedIndex >= 0) {
                for (auto masks = gridResult.layerMasks; auto& layerMask : masks) {
                    auto candidates = preview.composedLayers;
                    for (std::size_t i = 0; i < candidates.size(); ++i) {
                        auto& candidate = compat::ngArrayAt(candidates, i);
                        if (candidate.layerPath != layerMask.layerPath) continue;
                        const auto candidateUuid = candidate.targetGridUuid;
                        if (candidateUuid != compat::ngRequireObject(gridResult.grid)->getUUID()) continue;
                        if ((std::ptrdiff_t)i > changedIndex && !candidate.depthEnabled) affected = true;
                        break;
                    }
                    if (affected) break;
                }
            }
            if (!affected) continue;
            refreshGridResultDepths(gridResult);
        }
    }

void PSDDepthMapWindow::invalidate3DAdjustLayerCaches(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        const auto keepAlive = weak_from_this().lock();
        const auto key = compat::RetainedStringSlice(layerCacheKey(layerPath, targetGridUuid));
        if (auto entries = depthMaskPreviewTextures; !entries.empty()) {
            for (auto [textureKey, texture] : entries) {
                if (texture != nullptr) texture->dispose();
            }
        }
        depthMaskPreviewTextures = {};
        if (auto texture = lookup(originalPreviewTextures, key)) {
            const auto receiver = *texture;
            if (receiver != nullptr) receiver->dispose();
            originalPreviewTextures.erase(key);
        }
        // Stacking is a document-space dependency between every layer.
        // Changing one layer invalidates all prepared depths and meshes.
        threeDAdjustSamples = {};
        threeDAdjustMeshes = {};
        threeDAdjustPreparedTransforms = {};
        threeDAdjustFrontIntersections = {};
        threeDAdjustDisplayDepthDirty = {};
        threeDAdjustSamplesPrepared = false;
        threeDAdjustPreviewDirty = true;
    }

void PSDDepthMapWindow::refreshAfter3DAdjustLayerChange(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        const auto keepAlive = weak_from_this().lock();
        invalidate3DAdjustLayerCaches(layerPath, targetGridUuid);
        if (preview.gpuCompositionRequested) {
            startPreviewComposition();
            pending3DAdjustLayerChanges = {};
            return;
        }
        refreshPreviewDepthsForLayer(layerPath, targetGridUuid);
    }

void PSDDepthMapWindow::ensureDialogActionScope() {
        const auto keepAlive = weak_from_this().lock();
        if (dialogActionScope == nullptr || !compat::ngRequireObject(dialogActionScope)->isActive()) {
            dialogActionScope = ngOpenActionStackScope();
        }
    }

void PSDDepthMapWindow::activateDialogCommandContext() {
        const auto keepAlive = weak_from_this().lock();
        detail::activePsdDepthMapWindow = std::static_pointer_cast<PSDDepthMapWindow>(shared_from_this());
        ngSetSelectedNodesProvider(&psdDepthDialogSelectedNodes);
        if (dialogCommandScope == nullptr || !dialogCommandScope->isActive()) {
            dialogCommandScope = ngPushCommandScope(
                ngCommandScope<PsdDepthDialogCommandScope>());
        }
    }

void PSDDepthMapWindow::deactivateDialogCommandContext() {
        const auto keepAlive = weak_from_this().lock();
        if (detail::activePsdDepthMapWindow.get() == this) {
            detail::activePsdDepthMapWindow = {};
            ngSetSelectedNodesProvider(nullptr);
        }
        if (dialogCommandScope != nullptr) {
            compat::ngRequireObject(dialogCommandScope)->close();
            dialogCommandScope = {};
        }
    }

void PSDDepthMapWindow::closeDialogActionScope() {
        const auto keepAlive = weak_from_this().lock();
        if (dialogActionScope != nullptr) {
            compat::ngRequireObject(dialogActionScope)->close();
            dialogActionScope = {};
        }
    }

PsdDepthImportSettings PSDDepthMapWindow::cloneDialogSettings(PsdDepthImportSettings source) {
        auto result = source;
        result.layerTargetGridUuidOverrides = {};
        source.layerTargetGridUuidOverrides.forEachRetained(
            [&result](compat::RetainedString key, compat::RetainedString value) {
                result.layerTargetGridUuidOverrides.setRetained(key, value);
            });
        result.ignoredLayerPaths = {};
        source.ignoredLayerPaths.forEachRetained([&result](compat::RetainedString key, bool value) {
            result.ignoredLayerPaths.setRetained(key, value);
        });
        result.disabledGridUuids = {};
        source.disabledGridUuids.forEachRetained([&result](compat::RetainedString key, bool value) {
            result.disabledGridUuids.setRetained(key, value);
        });
        result.disabledGridLayerKeys = {};
        source.disabledGridLayerKeys.forEachRetained([&result](compat::RetainedString key, bool value) {
            result.disabledGridLayerKeys.setRetained(key, value);
        });
        return result;
    }

PsdDepthDialogLayerState PSDDepthMapWindow::dialogLayerState(PsdDepthComposedLayer& layer) {
        PsdDepthDialogLayerState result;
        result.layerPath = layer.layerPath;
        result.targetGridUuid = layer.targetGridUuid;
        result.visible = layer.visible;
        result.enabled = layer.enabled;
        result.depthEnabled = layer.depthEnabled;
        result.invert = layer.invert;
        result.depthOffset = layer.depthOffset;
        result.depthScale = layer.depthScale;
        result.outlierPruneEnabled = layer.outlierPruneEnabled;
        result.puppetFitEnabled = layer.puppetFitEnabled;
        return result;
    }

bool PSDDepthMapWindow::sameDialogLayerState(PsdDepthDialogLayerState a, PsdDepthDialogLayerState b) {
        return a.layerPath == b.layerPath &&
            a.targetGridUuid == b.targetGridUuid &&
            a.visible == b.visible &&
            a.enabled == b.enabled &&
            a.depthEnabled == b.depthEnabled &&
            a.invert == b.invert &&
            a.depthOffset == b.depthOffset &&
            a.depthScale == b.depthScale &&
            a.outlierPruneEnabled == b.outlierPruneEnabled &&
            a.puppetFitEnabled == b.puppetFitEnabled;
    }

void PSDDepthMapWindow::applyDialogLayerStateFields(PsdDepthComposedLayer& layer, PsdDepthDialogLayerState state) {
        layer.visible = state.visible;
        layer.enabled = state.enabled;
        layer.depthEnabled = state.depthEnabled;
        layer.invert = state.invert;
        layer.depthOffset = state.depthOffset;
        layer.depthScale = state.depthScale;
        layer.outlierPruneEnabled = state.outlierPruneEnabled;
        layer.puppetFitEnabled = state.puppetFitEnabled;
    }

void PSDDepthMapWindow::stage3DAdjustLayerChange(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
        const auto keepAlive = weak_from_this().lock();
        // A layer remains pending until Apply, but every edit still has to
        // refresh its display mesh. Do this before deduplicating the Apply work.
        threeDAdjustDisplayDepthDirty[compat::RetainedStringSlice(layerCacheKey(layerPath, targetGridUuid))] = true;
        threeDAdjustPreviewDirty = true;
        const auto pendingChanges = pending3DAdjustLayerChanges;
        for (auto pending : pendingChanges) {
            if (pending.layerPath == layerPath && pending.targetGridUuid == targetGridUuid) return;
        }
        pending3DAdjustLayerChanges.append(PsdDepthPendingLayerChange{layerPath, targetGridUuid});
    }

bool PSDDepthMapWindow::compose3DAdjustChangesForApply() {
    const auto keepAlive = weak_from_this().lock();
    last3DAdjustApplyRecomposedGridCount = 0;
    if (pending3DAdjustLayerChanges.size() == 0) return true;

    struct UuidHash {
        std::size_t operator()(std::uint64_t value) const noexcept {
            if constexpr (sizeof(std::size_t) == 4)
                return static_cast<std::size_t>(value ^ (value >> 32));
            else return static_cast<std::size_t>(value);
        }
    };
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, bool,
        UuidHash, std::equal_to<std::uint64_t>> targetGridUuids;
    const auto pendingChanges = pending3DAdjustLayerChanges;
    for (auto change : pendingChanges) {
        if (change.targetGridUuid != 0) {
            targetGridUuids[change.targetGridUuid] = true;
            continue;
        }
        for (auto grids = preview.grids; auto& gridResult : grids) {
            if (gridResult.grid == nullptr || !gridResultUsesLayer(gridResult, change.layerPath)) continue;
            targetGridUuids[compat::ngRequireObject(gridResult.grid)->getUUID()] = true;
        }
    }

    auto compositionGrids = preview.grids;
    for (std::size_t i = 0; i < compositionGrids.size(); ++i) {
        auto& gridResult = compat::ngArrayAt(compositionGrids, i);
        if (gridResult.grid == nullptr ||
            !targetGridUuids.contains(compat::ngRequireObject(gridResult.grid)->getUUID())) continue;
        PsdDepthGridResult composedGrid;
        compat::RetainedString composeError;
        if (!ngComposePsdDepthTarget(preview, gridResult, composedGrid, composeError)) {
            errorMessage = composeError.size() ? composeError
                : compat::RetainedString::fromStaticLiteral("PSD depth map composition failed");
            return false;
        }
        compat::ngArrayAt(preview.grids, i) = composedGrid;
        last3DAdjustApplyRecomposedGridCount++;
    }
    if (last3DAdjustApplyRecomposedGridCount != targetGridUuids.size()) {
        errorMessage = compat::RetainedString::fromStaticLiteral("PSD depth map composition target was not found");
        return false;
    }
    errorMessage = {};
    pending3DAdjustLayerChanges = {};
    return true;
}


}
