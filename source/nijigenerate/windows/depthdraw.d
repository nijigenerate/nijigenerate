module nijigenerate.windows.depthdraw;

import bindbc.imgui;
import i18n;
import nijigenerate;
import nijigenerate.commands : Context;
import nijigenerate.core.actionstack : incActionInvalidateSavedState;
import nijigenerate.ext.nodes.exdepthmapped : DepthMappedNode;
import nijigenerate.io.depthimage : DepthImageChannel, DepthImageConvolution, ngDepthImageSampleRgbaWithOpacity,
    ngDepthImageSampleRgbaWithOpacityAndCoverage;
import nijigenerate.viewport.base : ngPresentTemporaryViewport;
import nijigenerate.viewport.depth.common.targetview : DepthTargetView;
import nijigenerate.viewport.depth.draw;
import nijigenerate.windows.base;
import nijigenerate.widgets;
import nijilive.core.nodes : Node;
import nijilive.core.nodes.deformable : Deformable;
import nijilive.math : vec2, vec4;
import std.exception : collectException;
import std.file : exists;
import std.format : format;
import std.array : join;
import std.path : absolutePath, baseName, buildNormalizedPath, buildPath, dirName, extension, isAbsolute,
    setExtension, stripExtension;
import std.string : toLower, toStringz;

struct DepthDrawLayerRangeDiagnostics {
    DepthDrawRange rawRange;
    DepthDrawRange finalRange;
}

struct DepthDrawLayerSamplingDiagnostics {
    size_t totalPixels;
    size_t sampledPixels;
    size_t missingPixels;
    bool usesCoverage;
    bool hasCoverage;
    DepthDrawRange sampledRange;
}

class DepthDrawWindow : Window {
private:
    string path;
    DepthDrawSession session;
    int documentWidth = 1;
    int documentHeight = 1;
    string errorMessage;
    string statusMessage;
    float fitGapBack = -1.0f;
    float fitGapFront = 1.0f;
    float fitMargin = 0.0f;
    string fitBackLayerId;
    string fitFrontLayerId;
    DepthDrawFitZDiagnostics lastFitZDiagnostics;
    bool xyUniformScaleLock;
    string layerStackFilter;
    DepthDrawLayerStackSortMode layerStackSortMode = DepthDrawLayerStackSortMode.SourceOrder;
    bool layerStackSortDescending;
    bool hasPendingGpuApplyJob;
    ulong pendingGpuApplyGridUuid;
    Context pendingGpuApplyContext;
    DepthDrawGpuTargetComposeJob pendingGpuApplyJob;
    DepthDrawSession pendingGpuApplySession;
    ulong pendingGpuApplyRevision;
    string pendingGpuApplySessionState;
    string lastPersistedSessionState;

    enum string[] ChannelNames = [
        "AverageRGB",
        "R",
        "G",
        "B",
        "Luminance",
    ];

    enum string[] ConvolutionNames = [
        "Nearest",
        "Box3x3",
        "Box5x5",
        "Gaussian3x3",
        "Gaussian5x5",
        "Median3x3",
        "Frontmost3x3",
        "Backmost3x3",
        "BoxCustom",
        "GaussianCustom",
        "MedianCustom",
        "FrontmostCustom",
        "BackmostCustom",
    ];

    enum string[] LayerStackSortNames = [
        "Source Order",
        "Name",
        "Target",
        "Depth Min",
        "Sampled",
        "Missing",
    ];

    enum string[] MergePolicyNames = [
        "Replace",
        "Frontmost",
        "Backmost",
        "Add",
        "Keep Existing",
    ];

    string mergePolicyDisplayName(DepthMergePolicy policy) {
        final switch (policy) {
            case DepthMergePolicy.Replace: return "Replace";
            case DepthMergePolicy.Frontmost: return "Frontmost";
            case DepthMergePolicy.Backmost: return "Backmost";
            case DepthMergePolicy.Add: return "Add";
            case DepthMergePolicy.KeepExistingWhereMissing: return "Keep Existing";
        }
    }

    string warningSummary(DepthDrawLayerStackRow row) {
        string[] warnings;
        if (row.warningMissingDepthPixels) warnings ~= _("Missing depth");
        if (row.warningMissingTarget) warnings ~= _("Missing target");
        if (row.warningMissingCoverage) warnings ~= _("Missing coverage");
        if (row.warningMostlyMissingSamples) warnings ~= _("Mostly missing");
        if (row.hasAmbiguousBindings) warnings ~= _("Ambiguous binding");
        return warnings.length ? warnings.join(", ") : "";
    }

    string sourceDiagnosticsText() {
        if (session is null) return "";
        auto diagnostics = ngDepthDrawSourceDiagnostics(session);
        return _("Source: %s layer(s), %s usable, %s binding(s), %s enabled, %s missing target, %s missing coverage, %s ambiguous").format(
            diagnostics.totalLayers,
            diagnostics.usableDepthLayers,
            diagnostics.totalBindings,
            diagnostics.enabledBindings,
            diagnostics.layersMissingTarget,
            diagnostics.layersMissingCoverage,
            diagnostics.layersWithAmbiguousBindings
        );
    }

    string normalizedSourceIdentity(string sourcePath) {
        if (sourcePath.length == 0) return null;
        return buildNormalizedPath(absolutePath(sourcePath));
    }

    void hydrateManifestLayerImages(DepthDrawSession loadedSession, string manifestPath) {
        if (loadedSession is null) return;
        foreach (ref layer; loadedSession.layers) {
            if (layer.sourcePath.length == 0) continue;
            auto resolvedSourcePath = layer.sourcePath.isAbsolute
                ? layer.sourcePath
                : buildPath(manifestPath.dirName, layer.sourcePath);
            if (!exists(resolvedSourcePath)) continue;
            if (resolvedSourcePath.extension.toLower != ".png") continue;

            DepthDrawLayer imageLayer;
            auto ex = collectException(imageLayer = ngLoadDepthDrawPngLayer(resolvedSourcePath, layer.id));
            if (ex !is null) {
                throw new Exception(_("Failed to decode DepthDraw layer image %s: %s").format(
                    resolvedSourcePath, ex.msg));
            }
            if ((layer.width > 0 && layer.width != imageLayer.width) ||
                (layer.height > 0 && layer.height != imageLayer.height)) {
                throw new Exception(_("DepthDraw layer image dimensions do not match the manifest for %s: expected %sx%s, decoded %sx%s").format(
                    resolvedSourcePath, layer.width, layer.height, imageLayer.width, imageLayer.height));
            }
            layer.rgba = imageLayer.rgba;
            layer.depthPixels = imageLayer.depthPixels;
            layer.width = imageLayer.width;
            layer.height = imageLayer.height;
            if (layer.bounds.width <= 0) layer.bounds.width = layer.width;
            if (layer.bounds.height <= 0) layer.bounds.height = layer.height;
            loadedSession.replayLayerCleanupOperations(layer.id);
        }
    }

    void updateDocumentSizeFromLayers(DepthDrawSession loadedSession, out int width, out int height) {
        width = 1;
        height = 1;
        if (loadedSession is null) return;
        foreach (layer; loadedSession.layers) {
            auto right = layer.bounds.left + (layer.bounds.width > 0 ? layer.bounds.width : layer.width);
            auto bottom = layer.bounds.top + (layer.bounds.height > 0 ? layer.bounds.height : layer.height);
            if (right > width) width = right;
            if (bottom > height) height = bottom;
        }
    }

    void updateManifestValidationStatus() {
        if (session is null) return;

        auto puppet = incActivePuppet();
        bool delegate(ulong) hasTargetGrid = null;
        if (puppet !is null) {
            hasTargetGrid = (ulong uuid) {
                return cast(DepthMappedNode)puppet.find!Node(cast(uint)uuid) !is null;
            };
        }

        auto validation = ngValidateDepthDrawSessionManifest(session, hasTargetGrid, path.dirName);
        if (validation.ok) {
            statusMessage = _("Loaded DepthDraw manifest");
            return;
        }

        auto targetMissing = validation.missingTargetLayerIds.length +
            (validation.missingSelectedGridUuid != 0 ? 1 : 0);
        auto selectionMissing = validation.missingSelectedLayerId.length ? 1 : 0;
        auto invalidReferences = validation.missingBindingLayerIds.length +
            validation.duplicateLayerIds.length +
            validation.duplicateBindingKeys.length;
        statusMessage = _("Loaded DepthDraw manifest with missing references: %s source(s), %s target(s), %s selection(s), %s invalid").format(
            validation.missingSourceLayerIds.length,
            targetMissing,
            selectionMissing,
            invalidReferences
        );
    }

    void loadSource(bool preserveState = false) {
        auto previousSession = preserveState ? session : null;
        errorMessage = null;
        statusMessage = null;

        auto loadedSession = new DepthDrawSession();
        int loadedDocumentWidth = 1;
        int loadedDocumentHeight = 1;
        string loadedStatus;

        auto ext = path.extension.toLower;
        if (ext == ".psd") {
            DepthDrawPsdLoadResult result;
            auto ex = collectException(result = ngLoadDepthDrawPsd(path));
            if (ex !is null) {
                errorMessage = ex.msg;
                return;
            }
            loadedSession = result.session is null ? new DepthDrawSession() : result.session;
            loadedDocumentWidth = result.documentWidth > 0 ? result.documentWidth : 1;
            loadedDocumentHeight = result.documentHeight > 0 ? result.documentHeight : 1;
            loadedStatus = _("Loaded PSD source");
        } else if (ext == ".png") {
            DepthDrawLayer layer;
            auto ex = collectException(layer = ngLoadDepthDrawPngLayer(path));
            if (ex !is null) {
                errorMessage = ex.msg;
                return;
            }
            loadedSession.layers ~= layer;
            loadedDocumentWidth = layer.width > 0 ? layer.width : 1;
            loadedDocumentHeight = layer.height > 0 ? layer.height : 1;
            loadedStatus = _("Loaded PNG source");
        } else if (ext == ".json") {
            auto ex = collectException(loadedSession = ngLoadDepthDrawManifest(path));
            if (ex !is null) {
                errorMessage = ex.msg;
                return;
            }
            ex = collectException(hydrateManifestLayerImages(loadedSession, path));
            if (ex !is null) {
                errorMessage = ex.msg;
                return;
            }
            if (loadedSession.documentWidth > 0 && loadedSession.documentHeight > 0) {
                loadedDocumentWidth = loadedSession.documentWidth;
                loadedDocumentHeight = loadedSession.documentHeight;
            } else {
                updateDocumentSizeFromLayers(loadedSession, loadedDocumentWidth, loadedDocumentHeight);
            }
        } else {
            errorMessage = _("DepthDraw supports PSD, PNG, and JSON manifest sources.");
            return;
        }

        loadedSession.sourceIdentity = normalizedSourceIdentity(path);
        loadedSession.documentWidth = loadedDocumentWidth;
        loadedSession.documentHeight = loadedDocumentHeight;
        auto normalLayers = loadedSession.layers.dup;
        ngDepthDrawAttachNormalCoverage(loadedSession, normalLayers);

        string reloadStatus;
        if (previousSession !is null) {
            auto reload = ngDepthDrawCarryReloadState(loadedSession, previousSession);
            reloadStatus = _("Reloaded source: %s layer(s), %s binding(s)").format(
                reload.matchedLayers,
                reload.preservedBindings
            );
        }

        clearPendingGpuApply();
        session = loadedSession;
        documentWidth = loadedDocumentWidth;
        documentHeight = loadedDocumentHeight;
        statusMessage = loadedStatus;
        if (ext == ".json") updateManifestValidationStatus();
        if (reloadStatus.length > 0) statusMessage = reloadStatus;
    }

    void restorePersistentSessionState() {
        auto puppet = incActivePuppet();
        if (puppet is null || session is null) return;
        auto persisted = ngGetPuppetDepthDrawSession(puppet);
        if (persisted is null) return;
        bool sameSource = persisted.sourceIdentity.length > 0 &&
            persisted.sourceIdentity == session.sourceIdentity;
        // Backward compatibility for projects saved before sourceIdentity was serialized.
        if (!sameSource && persisted.sourceIdentity.length == 0) foreach (layer; persisted.layers) {
            if (layer.sourcePath == path) { sameSource = true; break; }
        }
        if (!sameSource) return;
        ngDepthDrawCarryReloadState(session, persisted);
    }

    void persistSessionIfChanged() {
        if (session is null) return;
        auto state = ngDepthDrawSessionToPersistentJson(session);
        if (state == lastPersistedSessionState) return;
        auto puppet = incActivePuppet();
        if (puppet is null || !ngSetPuppetDepthDrawSession(puppet, session)) return;
        lastPersistedSessionState = state;
        incActionInvalidateSavedState();
    }

    void drawDisplayToggles() {
        if (session is null) return;
        auto display = session.display;
        bool changed;
        changed = ngCheckbox(__("Normal Image"), &display.showNormalImage) || changed;
        changed = ngCheckbox(__("Raw Depth"), &display.showRawDepth) || changed;
        changed = ngCheckbox(__("Coverage"), &display.showCoverage) || changed;
        changed = ngCheckbox(__("Composite"), &display.showComposite) || changed;
        changed = ngCheckbox(__("Layer Planes"), &display.showLayerPlanes) || changed;
        changed = ngCheckbox(__("Depth Ranges"), &display.showDepthRanges) || changed;
        changed = ngCheckbox(__("Missing Vertices"), &display.showMissingVertices) || changed;
        changed = ngCheckbox(__("Winning Layer"), &display.showWinningLayer) || changed;
        changed = ngCheckbox(__("GPU Preview"), &display.useGpuPreview) || changed;
        if (changed) session.updateDisplayOptions(display);
    }

    string targetDisplayName(ulong uuid) {
        auto puppet = incActivePuppet();
        if (puppet is null || uuid == 0) return null;
        auto node = puppet.find!Node(cast(uint)uuid);
        if (node is null || node.name.length == 0) return null;
        return node.name;
    }

    bool drawLayerStackSortCombo() {
        auto index = cast(size_t)layerStackSortMode;
        if (index >= LayerStackSortNames.length) index = 0;
        bool changed;
        if (igBeginCombo(__("Layer Sort"), LayerStackSortNames[index].toStringz)) {
            foreach (i, name; LayerStackSortNames) {
                auto selected = i == index;
                if (igSelectable(name.toStringz, selected)) {
                    layerStackSortMode = cast(DepthDrawLayerStackSortMode)i;
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    void drawLayerStackControls() {
        incInputText("Layer Filter", layerStackFilter);
        drawLayerStackSortCombo();
        ngCheckbox(__("Descending"), &layerStackSortDescending);
    }

    void drawLayerRows() {
        if (session is null) return;
        drawLayerStackControls();
        auto rows = displayLayerStackRows();
        if (igBeginTable("DepthDrawLayerRows", 9, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg)) {
            igTableSetupColumn(__("Layer"));
            igTableSetupColumn(__("Visible"));
            igTableSetupColumn(__("Enabled"));
            igTableSetupColumn(__("Depth"));
            igTableSetupColumn(__("Target"));
            igTableSetupColumn(__("Merge"));
            igTableSetupColumn(__("Sampled"));
            igTableSetupColumn(__("Range"));
            igTableSetupColumn(__("Warnings"));
            igTableHeadersRow();
            foreach (row; rows) {
                igTableNextRow();
                igTableSetColumnIndex(0);
                auto label = (row.displayName.length ? row.displayName : row.layerId);
                auto selected = session.selectedLayerId == row.layerId;
                if (igSelectable((label ~ "###depthDrawLayer" ~ row.layerId).toStringz, selected,
                        ImGuiSelectableFlags.SpanAllColumns)) {
                    selectLayerStackRow(row.layerId);
                }
                igTableSetColumnIndex(1);
                auto visible = row.visible;
                if (igCheckbox(("###depthDrawVisible" ~ row.layerId).toStringz, &visible)) {
                    updateLayerVisibility(row.layerId, visible, row.enabled);
                }
                igTableSetColumnIndex(2);
                auto enabled = row.enabled;
                if (igCheckbox(("###depthDrawEnabled" ~ row.layerId).toStringz, &enabled)) {
                    updateLayerVisibility(row.layerId, row.visible, enabled);
                }
                igTableSetColumnIndex(3);
                igText(row.hasDepthPixels ? __("Yes") : __("No"));
                igTableSetColumnIndex(4);
                auto targetLabel = row.targetDisplayName.length
                    ? row.targetDisplayName
                    : "%s".format(row.targetGridUuid);
                igText(row.hasBinding ? targetLabel.toStringz : __("Unbound"));
                igTableSetColumnIndex(5);
                igText(row.hasBinding ? mergePolicyDisplayName(row.mergePolicy).toStringz : "-");
                igTableSetColumnIndex(6);
                igText("%s/%s".format(row.sampledVertices, row.missingVertices).toStringz);
                igTableSetColumnIndex(7);
                auto rangeText = row.hasDepthRange
                    ? "%.3f .. %.3f".format(row.minDepth, row.maxDepth)
                    : "-";
                igText(rangeText.toStringz);
                igTableSetColumnIndex(8);
                auto warnings = warningSummary(row);
                if (warnings.length) {
                    incTextColored(ImVec4(1, 0.7f, 0.2f, 1), warnings);
                } else {
                    igText("-");
                }
            }
            igEndTable();
        }
    }

    void drawBindingRows() {
        if (session is null) return;
        incText(_("Bindings"));
        if (igBeginTable("DepthDrawBindingRows", 5, ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg)) {
            igTableSetupColumn(__("Layer"));
            igTableSetupColumn(__("Target"));
            igTableSetupColumn(__("Enabled"));
            igTableSetupColumn(__("Order"));
            igTableSetupColumn(__("Merge"));
            igTableHeadersRow();
            foreach (binding; session.bindings) {
                igTableNextRow();
                igTableSetColumnIndex(0);
                auto selected = session.selectedLayerId == binding.layerId &&
                    session.selectedGridUuid == binding.targetGridUuid;
                if (igSelectable((binding.layerId ~ "###depthDrawBinding" ~ binding.layerId ~
                        "%s".format(binding.targetGridUuid)).toStringz, selected, ImGuiSelectableFlags.SpanAllColumns)) {
                    selectLayer(binding.layerId);
                    selectTargetGrid(binding.targetGridUuid);
                }
                igTableSetColumnIndex(1);
                igText("%s".format(binding.targetGridUuid).toStringz);
                igTableSetColumnIndex(2);
                auto enabled = binding.enabled;
                auto bindingId = "%s%s".format(binding.layerId, binding.targetGridUuid);
                if (igCheckbox(("###depthDrawBindingEnabled" ~ bindingId).toStringz, &enabled)) {
                    updateBindingState(binding.layerId, binding.targetGridUuid, enabled, binding.order, binding.mergePolicy);
                }
                igTableSetColumnIndex(3);
                auto order = binding.order;
                igSetNextItemWidth(80);
                if (igInputInt(("###depthDrawBindingOrder" ~ bindingId).toStringz, &order)) {
                    updateBindingState(binding.layerId, binding.targetGridUuid, binding.enabled, order, binding.mergePolicy);
                }
                igTableSetColumnIndex(4);
                auto mergePolicy = binding.mergePolicy;
                igSetNextItemWidth(150);
                if (drawMergePolicyCombo(("###depthDrawBindingMerge" ~ bindingId).toStringz, mergePolicy)) {
                    updateBindingState(binding.layerId, binding.targetGridUuid, binding.enabled, binding.order, mergePolicy);
                }
            }
            igEndTable();
        }
    }

    bool drawMergePolicyCombo(const(char)* id, ref DepthMergePolicy mergePolicy) {
        auto index = cast(size_t)mergePolicy;
        if (index >= MergePolicyNames.length) index = 0;
        bool changed;
        if (igBeginCombo(id, MergePolicyNames[index].toStringz)) {
            foreach (i, name; MergePolicyNames) {
                auto selected = i == index;
                if (igSelectable(name.toStringz, selected)) {
                    mergePolicy = cast(DepthMergePolicy)i;
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    bool drawChannelCombo(ref DepthImageChannel channel) {
        auto index = cast(size_t)channel;
        if (index >= ChannelNames.length) index = 0;
        bool changed;
        if (igBeginCombo(__("Channel"), ChannelNames[index].toStringz)) {
            foreach (i, name; ChannelNames) {
                auto selected = i == index;
                if (igSelectable(name.toStringz, selected)) {
                    channel = cast(DepthImageChannel)i;
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    bool drawConvolutionCombo(ref DepthImageConvolution convolution) {
        auto index = cast(size_t)convolution;
        if (index >= ConvolutionNames.length) index = 0;
        bool changed;
        if (igBeginCombo(__("Sampling"), ConvolutionNames[index].toStringz)) {
            foreach (i, name; ConvolutionNames) {
                if (session !is null && session.display.useGpuPreview &&
                    !ngDepthDrawGpuLayerSampleSupportsConvolution(cast(int)i)) continue;
                auto selected = i == index;
                if (igSelectable(name.toStringz, selected)) {
                    convolution = cast(DepthImageConvolution)i;
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    DepthDrawBinding* selectedBinding() {
        if (session is null || session.selectedLayerId.length == 0 || session.selectedGridUuid == 0) return null;
        foreach (ref binding; session.bindings) {
            if (binding.layerId == session.selectedLayerId && binding.targetGridUuid == session.selectedGridUuid) {
                return &binding;
            }
        }
        return null;
    }

    DepthTargetView targetViewByGrid(ulong gridUuid) {
        if (gridUuid == 0) return null;
        auto puppet = incActivePuppet();
        if (puppet is null) return null;
        auto node = puppet.find!Node(cast(uint)gridUuid);
        auto deformable = cast(Deformable)node;
        if (deformable is null || cast(DepthMappedNode)deformable is null) return null;
        return new DepthTargetView(deformable);
    }

    DepthTargetView selectedTargetView() {
        if (session is null || session.selectedGridUuid == 0) return null;
        return targetViewByGrid(session.selectedGridUuid);
    }

    bool selectedComposeResult(out DepthDrawComposeResult result) {
        auto target = selectedTargetView();
        if (target is null) return false;
        result = ngComposeDepthDrawTarget(session, target, documentWidth, documentHeight);
        return result.targetGridUuid != 0 && result.depths.length > 0;
    }

    ptrdiff_t selectedLayerIndex() {
        if (session is null || session.selectedLayerId.length == 0) return -1;
        foreach (i, ref layer; session.layers) {
            if (layer.id == session.selectedLayerId) return cast(ptrdiff_t)i;
        }
        return -1;
    }

    DepthDrawRange transformedLayerRange(ref DepthDrawLayer layer) {
        auto rawRange = measureLayerRawRange(layer.id);
        if (!rawRange.valid) return rawRange;
        return ngDepthDrawRangeFromValues([
            layer.applyZTransform(rawRange.minDepth),
            layer.applyZTransform(rawRange.maxDepth),
        ]);
    }

    DepthDrawRange[] transformedLayerRanges() {
        DepthDrawRange[] ranges;
        if (session is null) return ranges;
        foreach (ref layer; session.layers) {
            ranges ~= transformedLayerRange(layer);
        }
        return ranges;
    }

    bool drawLayerIdCombo(string label, ref string layerId) {
        if (session is null) return false;
        auto current = layerId.length ? layerId : _("<none>");
        bool changed;
        if (igBeginCombo(label.toStringz, current.toStringz)) {
            foreach (layer; session.layers) {
                auto display = layer.displayName.length ? layer.displayName : layer.id;
                auto selected = layer.id == layerId;
                if (igSelectable((display ~ "###fitLayer" ~ label ~ layer.id).toStringz, selected)) {
                    layerId = layer.id;
                    changed = true;
                }
            }
            igEndCombo();
        }
        return changed;
    }

    void showFitZDiagnostics(DepthDrawFitZDiagnostics diagnostics) {
        if (!diagnostics.rawRangeValid) {
            incTextDisabled(_("Raw range: unavailable"));
        } else {
            incTextDisabled(_("Raw range: %.3f .. %.3f").format(diagnostics.rawMin, diagnostics.rawMax));
        }
        if (!diagnostics.gapValid) {
            incTextDisabled(_("Gap: unavailable"));
        } else {
            incTextDisabled(_("Gap: %.3f .. %.3f").format(diagnostics.gapBack, diagnostics.gapFront));
        }
        if (diagnostics.succeeded) {
            incTextDisabled(_("Fit Z: scale %.3f offset %.3f").format(diagnostics.zScale, diagnostics.zOffset));
        } else if (diagnostics.error.length) {
            incTextColored(ImVec4(1, 0.2f, 0.2f, 1), diagnostics.error);
        }
    }

    void showLayerRangeDiagnostics(ref DepthDrawLayer layer) {
        auto diagnostics = layerRangeDiagnostics(layer.id);
        if (diagnostics.rawRange.valid) {
            incTextDisabled(_("Raw Depth: %.3f .. %.3f").format(
                diagnostics.rawRange.minDepth,
                diagnostics.rawRange.maxDepth
            ));
        } else {
            incTextDisabled(_("Raw Depth: unavailable"));
        }
        if (diagnostics.finalRange.valid) {
            incTextDisabled(_("Final Depth: %.3f .. %.3f").format(
                diagnostics.finalRange.minDepth,
                diagnostics.finalRange.maxDepth
            ));
        } else {
            incTextDisabled(_("Final Depth: unavailable"));
        }
    }

    void showLayerSamplingDiagnostics(ref DepthDrawLayer layer) {
        auto diagnostics = layerSamplingDiagnostics(layer.id);
        if (diagnostics.totalPixels == 0) {
            incTextDisabled(_("Sampling: unavailable"));
            return;
        }
        incTextDisabled(_("Sampling: %s sampled, %s missing").format(
            diagnostics.sampledPixels,
            diagnostics.missingPixels
        ));
        if (diagnostics.sampledRange.valid) {
            incTextDisabled(_("Sampled Depth: %.3f .. %.3f").format(
                diagnostics.sampledRange.minDepth,
                diagnostics.sampledRange.maxDepth
            ));
        } else {
            incTextDisabled(_("Sampled Depth: unavailable"));
        }
        if (diagnostics.usesCoverage && !diagnostics.hasCoverage) {
            incTextColored(ImVec4(1, 0.7f, 0.2f, 1), _("Coverage is enabled but missing for this layer."));
        }
    }

    void drawFitZControls() {
        if (session is null || session.selectedLayerId.length == 0) return;
        auto layer = session.layerById(session.selectedLayerId);
        if (layer is null) return;

        incText(_("Fit Z to Gap"));
        igDragFloat(__("Gap Back"), &fitGapBack, 0.01f, -10.0f, 10.0f, "%.3f");
        igDragFloat(__("Gap Front"), &fitGapFront, 0.01f, -10.0f, 10.0f, "%.3f");
        igDragFloat(__("Margin"), &fitMargin, 0.001f, 0.0f, 10.0f, "%.3f");
        if (incButtonColored(__("Fit Manual Gap"), ImVec2(150, 0))) {
            applySelectedLayerFitZToManualGap(fitGapBack, fitGapFront, fitMargin);
        }
        igSameLine();
        if (incButtonColored(__("Fit Adjacent Gap"), ImVec2(150, 0))) {
            applySelectedLayerFitZToAdjacentGap(fitMargin);
        }
        igSameLine();
        if (incButtonColored(__("Fit Target Range"), ImVec2(150, 0))) {
            applySelectedLayerFitZToTargetRange(fitMargin);
        }
        drawLayerIdCombo(_("Back Layer"), fitBackLayerId);
        drawLayerIdCombo(_("Front Layer"), fitFrontLayerId);
        if (incButtonColored(__("Fit Selected Layers"), ImVec2(150, 0))) {
            applySelectedLayerFitZToSelectedLayerGap(fitBackLayerId, fitFrontLayerId, fitMargin);
        }
        showFitZDiagnostics(lastFitZDiagnostics);
    }

    void drawLayerInspector() {
        if (session is null || session.selectedLayerId.length == 0) return;
        auto layer = session.layerById(session.selectedLayerId);
        if (layer is null) return;

        incText(_("Layer Inspector"));
        incText((layer.displayName.length ? layer.displayName : layer.id));

        auto offsetX = layer.xyOffset.x;
        auto offsetY = layer.xyOffset.y;
        auto scaleX = layer.xyScale.x;
        auto scaleY = layer.xyScale.y;
        bool xyChanged;
        bool scaleXChanged;
        bool scaleYChanged;
        xyChanged = igDragFloat(__("Offset X"), &offsetX, 0.1f, -100000.0f, 100000.0f, "%.3f") || xyChanged;
        xyChanged = igDragFloat(__("Offset Y"), &offsetY, 0.1f, -100000.0f, 100000.0f, "%.3f") || xyChanged;
        xyChanged = ngCheckbox(__("Uniform Scale"), &xyUniformScaleLock) || xyChanged;
        scaleXChanged = igDragFloat(__("Scale X"), &scaleX, 0.01f, 0.001f, 1000.0f, "%.3f");
        scaleYChanged = igDragFloat(__("Scale Y"), &scaleY, 0.01f, 0.001f, 1000.0f, "%.3f");
        if (xyUniformScaleLock) {
            if (scaleXChanged) scaleY = scaleX;
            else if (scaleYChanged) scaleX = scaleY;
        }
        xyChanged = scaleXChanged || scaleYChanged || xyChanged;
        if (xyChanged) {
            updateLayerXYTransform(layer.id, vec2(offsetX, offsetY), vec2(scaleX, scaleY));
        }
        if (incButtonColored(__("Reset XY"), ImVec2(120, 0))) {
            updateLayerXYTransform(layer.id, vec2(0, 0), vec2(1, 1));
        }
        igSameLine();
        if (incButtonColored(__("Fit Source Bounds"), ImVec2(150, 0))) {
            fitSelectedLayerXYToSourceBounds();
        }
        igSameLine();
        if (incButtonColored(__("Fit Target Bounds"), ImVec2(150, 0))) {
            fitSelectedLayerXYToTargetBounds();
        }

        auto backDepth = layer.backDepth;
        auto frontDepth = layer.frontDepth;
        auto invert = layer.invert;
        auto zScale = layer.zScale;
        auto zOffset = layer.zOffset;
        bool zChanged;
        zChanged = igDragFloat(__("Back Depth"), &backDepth, 0.01f, -10.0f, 10.0f, "%.3f") || zChanged;
        zChanged = igDragFloat(__("Front Depth"), &frontDepth, 0.01f, -10.0f, 10.0f, "%.3f") || zChanged;
        zChanged = ngCheckbox(__("Invert Depth"), &invert) || zChanged;
        zChanged = igDragFloat(__("Z Scale"), &zScale, 0.01f, -100.0f, 100.0f, "%.3f") || zChanged;
        zChanged = igDragFloat(__("Z Offset"), &zOffset, 0.01f, -100.0f, 100.0f, "%.3f") || zChanged;
        if (zChanged) {
            updateLayerZTransform(layer.id, backDepth, frontDepth, invert, zScale, zOffset);
        }
        if (incButtonColored(__("Reset Z"), ImVec2(120, 0))) {
            updateLayerZTransform(layer.id, -1.0f, 1.0f, false, 1.0f, 0.0f);
        }
        showLayerRangeDiagnostics(*layer);

        auto channel = layer.channel;
        auto convolution = layer.convolution;
        auto customRadius = layer.customRadius;
        auto alphaThreshold = layer.alphaThreshold;
        bool samplingChanged;
        samplingChanged = drawChannelCombo(channel) || samplingChanged;
        samplingChanged = drawConvolutionCombo(convolution) || samplingChanged;
        samplingChanged = igInputInt(__("Custom Radius"), &customRadius, 1, 3) || samplingChanged;
        samplingChanged = igDragFloat(__("Alpha Threshold"), &alphaThreshold, 0.001f, 0.0f, 1.0f, "%.3f") ||
            samplingChanged;
        if (samplingChanged) {
            updateLayerSampling(layer.id, channel, convolution, customRadius, alphaThreshold);
        }
        showLayerSamplingDiagnostics(*layer);
        if (incButtonColored(__("Fill Alpha Depth Gaps"), ImVec2(180, 0))) {
            applySelectedLayerAlphaDepthGapFill();
        }
        igSameLine();
        if (incButtonColored(__("Repair Contour Depth"), ImVec2(180, 0))) {
            repairSelectedLayerContourDepth();
        }

        auto binding = selectedBinding();
        if (binding !is null) {
            bool useNormalLayerAlpha = binding.useNormalLayerAlpha;
            auto coverageThreshold = binding.coverageThreshold;
            bool bindingChanged;
            bindingChanged = ngCheckbox(__("Use Normal Layer Coverage"), &useNormalLayerAlpha) || bindingChanged;
            bindingChanged = igDragFloat(__("Coverage Threshold"), &coverageThreshold, 0.001f, 0.0f, 1.0f, "%.3f") ||
                bindingChanged;
            if (bindingChanged) {
                updateBindingSampling(binding.layerId, binding.targetGridUuid, useNormalLayerAlpha, coverageThreshold);
            }
        }

        drawFitZControls();
    }

protected:
    override void onClose() {
        clearPendingGpuApply();
    }

    override void onUpdate() {
        igSetNextWindowSize(ImVec2(840, 620), ImGuiCond.FirstUseEver);
        onBeginUpdate();
        if (!drewWindow) {
            onEndUpdate();
            return;
        }
        pollPendingGpuApply();

        incText(_("DepthDraw"));
        incTextWrapped(path);
        incText(_("Document: %sx%s").format(documentWidth, documentHeight));
        if (incButtonColored(__("Reload Source"), ImVec2(130, 0))) {
            reloadSourcePreservingState();
        }
        igSameLine();
        if (incButtonColored(__("Export PNG Depth"), ImVec2(150, 0))) {
            TFD_Filter[] filters = [
                { ["*.json"], "DepthDraw Manifest (*.json)" }
            ];
            auto manifestPath = incShowSaveDialog(filters, "depthdraw.json", _("Export DepthDraw PNG Layers..."));
            if (manifestPath.length) {
                auto outputDir = buildPath(manifestPath.dirName, manifestPath.baseName.stripExtension ~ "-layers");
                exportPngSession(outputDir, manifestPath);
            }
        }
        igSameLine();
        if (incButtonColored(__("Open Depth View"), ImVec2(150, 0))) {
            presentDepthDrawViewport();
        }
        igSameLine();
        if (incButtonColored(__("Apply Selected Target"), ImVec2(180, 0))) {
            applySelectedTargetDepthDraw();
        }
        if (errorMessage.length) incTextColored(ImVec4(1, 0.2f, 0.2f, 1), errorMessage);
        else if (statusMessage.length) incTextDisabled(statusMessage);

        if (session !is null) {
            incText(_("Layers: %s  Bindings: %s").format(session.layers.length, session.bindings.length));
            incTextDisabled(sourceDiagnosticsText);
            if (incButtonColored(__("Auto Bind"), ImVec2(120, 0))) {
                auto puppet = incActivePuppet();
                if (puppet is null) {
                    statusMessage = _("No active puppet");
                } else {
                    auto results = ngDepthDrawAutoBindSession(session, puppet);
                    session.markAllPreviewDirty();
                    statusMessage = _("Auto bind completed: %s layer(s)").format(results.length);
                }
            }

            drawDisplayToggles();
            drawLayerRows();
            drawBindingRows();
            drawLayerInspector();
        }

        persistSessionIfChanged();
        onEndUpdate();
    }

public:
    this(string path) {
        super(_("DepthDraw"));
        this.path = path;
        flags |= ImGuiWindowFlags.NoSavedSettings;
        loadSource();
        restorePersistentSessionState();
        if (session !is null) lastPersistedSessionState = ngDepthDrawSessionToPersistentJson(session);
    }

    DepthDrawSession depthDrawSession() {
        return session;
    }

    bool presentDepthDrawViewport() {
        if (session is null) {
            errorMessage = _("No DepthDraw session to view");
            return false;
        }
        auto viewport = new DepthDrawViewport(session);
        viewport.setDocumentSize(documentWidth, documentHeight);
        ngPresentTemporaryViewport(viewport);
        statusMessage = _("Opened DepthDraw viewport");
        errorMessage = null;
        return true;
    }

    DepthDrawApplySummary applySelectedTargetDepthDraw(Context ctx = null) {
        DepthDrawApplySummary summary;
        if (ctx is null) ctx = new Context();
        if (session is null) {
            errorMessage = _("No DepthDraw session to apply");
            return summary;
        }
        auto target = selectedTargetView();
        if (target is null) {
            errorMessage = _("No selected DepthDraw target to apply");
            return summary;
        }
        if (session.display.useGpuPreview) {
            return applySelectedTargetDepthDrawGpu(ctx, target);
        }

        auto result = ngComposeDepthDrawTarget(session, target, documentWidth, documentHeight);
        summary = ngApplyDepthDrawTargetResultWithSummary(ctx, target, result);
        if (!summary.succeeded) {
            errorMessage = _("DepthDraw apply failed");
            statusMessage = null;
            return summary;
        }

        errorMessage = null;
        statusMessage = _("Applied DepthDraw: %s target(s), %s vertex/vertices, %s missing sample(s)").format(
            summary.changedTargets,
            summary.changedVertices,
            summary.missingSamples
        );
        return summary;
    }

    DepthDrawApplySummary applySelectedTargetDepthDrawGpu(Context ctx, DepthTargetView target) {
        DepthDrawApplySummary summary;
        if (hasPendingGpuApplyJob) {
            statusMessage = _("DepthDraw GPU apply is already pending");
            errorMessage = null;
            pollPendingGpuApply();
            return summary;
        }

        string error;
        if (!ngSubmitDepthDrawGpuTargetCompose(session, target, documentWidth, documentHeight, pendingGpuApplyJob, error)) {
            errorMessage = error.length ? error : _("DepthDraw GPU apply submit failed");
            statusMessage = null;
            return summary;
        }
        hasPendingGpuApplyJob = true;
        pendingGpuApplyGridUuid = target.getTarget().uuid;
        pendingGpuApplyContext = ctx;
        pendingGpuApplySession = session;
        pendingGpuApplyRevision = session.targetPreviewRevision(pendingGpuApplyGridUuid);
        pendingGpuApplySessionState = ngDepthDrawSessionToManifest(session).toString();
        statusMessage = _("DepthDraw GPU apply submitted");
        errorMessage = null;
        return pollPendingGpuApply();
    }

    DepthDrawApplySummary pollPendingGpuApply() {
        DepthDrawApplySummary summary;
        if (!hasPendingGpuApplyJob) return summary;

        DepthDrawGpuTargetComposePollResult pollResult;
        string error;
        if (!ngPollDepthDrawGpuTargetCompose(pendingGpuApplyJob, pollResult, error)) {
            errorMessage = error.length ? error : _("DepthDraw GPU apply poll failed");
            statusMessage = null;
            clearPendingGpuApply();
            return summary;
        }
        if (!pollResult.ready) {
            statusMessage = _("DepthDraw GPU apply pending");
            return summary;
        }

        auto target = targetViewByGrid(pendingGpuApplyGridUuid);
        if (target is null) {
            errorMessage = _("DepthDraw GPU apply target was not found");
            statusMessage = null;
            clearPendingGpuApply();
            return summary;
        }
        if (session is null || pendingGpuApplySession !is session ||
            pendingGpuApplyRevision != session.targetPreviewRevision(pendingGpuApplyGridUuid) ||
            pendingGpuApplySessionState != ngDepthDrawSessionToManifest(session).toString()) {
            statusMessage = _("DepthDraw GPU apply was canceled because the session changed");
            errorMessage = null;
            clearPendingGpuApply();
            return summary;
        }
        auto currentPacket = ngBuildDepthDrawGpuComposePacket(session, target, documentWidth, documentHeight);
        if (currentPacket.vertices != pendingGpuApplyJob.packet.vertices ||
            currentPacket.documentPositions != pendingGpuApplyJob.packet.documentPositions ||
            currentPacket.baseDepths != pendingGpuApplyJob.packet.baseDepths) {
            statusMessage = _("DepthDraw GPU apply was canceled because the session changed");
            errorMessage = null;
            clearPendingGpuApply();
            return summary;
        }
        summary = ngApplyDepthDrawTargetResultWithSummary(pendingGpuApplyContext, target, pollResult.result);
        clearPendingGpuApply();
        if (!summary.succeeded) {
            errorMessage = _("DepthDraw GPU apply failed");
            statusMessage = null;
            return summary;
        }
        errorMessage = null;
        statusMessage = _("Applied DepthDraw GPU: %s target(s), %s vertex/vertices, %s missing sample(s)").format(
            summary.changedTargets,
            summary.changedVertices,
            summary.missingSamples
        );
        return summary;
    }

    void clearPendingGpuApply() {
        if (hasPendingGpuApplyJob) ngCancelDepthDrawGpuTargetCompose(pendingGpuApplyJob);
        hasPendingGpuApplyJob = false;
        pendingGpuApplyGridUuid = 0;
        pendingGpuApplyContext = null;
        pendingGpuApplyJob = DepthDrawGpuTargetComposeJob.init;
        pendingGpuApplySession = null;
        pendingGpuApplyRevision = 0;
        pendingGpuApplySessionState = null;
    }

    bool selectLayer(string layerId) {
        if (session is null) return false;
        auto selected = session.selectLayer(layerId);
        if (selected) statusMessage = _("Selected layer: %s").format(layerId);
        return selected;
    }

    bool selectTargetGrid(ulong gridUuid) {
        if (session is null) return false;
        auto selected = session.selectTargetGrid(gridUuid);
        if (selected) statusMessage = _("Selected target: %s").format(gridUuid);
        return selected;
    }

    bool selectLayerStackRow(string layerId) {
        if (session is null || !selectLayer(layerId)) return false;
        foreach (binding; session.bindings) {
            if (!binding.enabled || binding.layerId != layerId) continue;
            selectTargetGrid(binding.targetGridUuid);
            break;
        }
        return true;
    }

    bool reloadSourcePreservingState() {
        loadSource(true);
        return errorMessage.length == 0;
    }

    bool replaceSourcePreservingState(string nextPath) {
        auto previousPath = path;
        path = nextPath;
        loadSource(true);
        if (errorMessage.length > 0) path = previousPath;
        return errorMessage.length == 0;
    }

    DepthDrawPngExportResult exportPngSession(string outputDir, string manifestPath) {
        DepthDrawPngExportResult result;
        if (session is null) {
            errorMessage = _("No DepthDraw session to export");
            return result;
        }
        auto ex = collectException(result = ngExportDepthDrawPngSession(session, outputDir, manifestPath));
        if (ex !is null) {
            errorMessage = ex.msg;
            statusMessage = null;
            return result;
        }
        errorMessage = null;
        statusMessage = _("Exported %s DepthDraw layer(s): %s").format(result.exportedLayers, manifestPath);
        return result;
    }

    bool updateLayerXYTransform(string layerId, vec2 xyOffset, vec2 xyScale) {
        if (session is null) return false;
        auto updated = session.updateLayerXYTransform(layerId, xyOffset, xyScale);
        if (updated) statusMessage = _("Updated XY transform: %s").format(layerId);
        return updated;
    }

    bool updateSelectedLayerXYUniformScale(float scale) {
        if (session is null || session.selectedLayerId.length == 0) return false;
        auto layer = session.layerById(session.selectedLayerId);
        if (layer is null) return false;
        return updateLayerXYTransform(layer.id, layer.xyOffset, vec2(scale, scale));
    }

    bool fitSelectedLayerXYToSourceBounds() {
        if (session is null || session.selectedLayerId.length == 0) return false;
        auto layer = session.layerById(session.selectedLayerId);
        if (layer is null) return false;
        auto updated = updateLayerXYTransform(layer.id, vec2(0, 0), vec2(1, 1));
        if (updated) statusMessage = _("Fit XY to source bounds: %s").format(layer.id);
        return updated;
    }

    bool fitSelectedLayerXYToTargetBounds() {
        if (session is null || session.selectedLayerId.length == 0 || session.selectedGridUuid == 0) return false;
        auto layer = session.layerById(session.selectedLayerId);
        if (layer is null || layer.width <= 0 || layer.height <= 0) return false;
        auto puppet = incActivePuppet();
        if (puppet is null) return false;
        auto node = puppet.find!Node(cast(uint)session.selectedGridUuid);
        auto deformable = cast(Deformable)node;
        if (deformable is null || deformable.vertices.length == 0) return false;

        bool haveBounds;
        vec2 minPoint;
        vec2 maxPoint;
        foreach (vertex; deformable.vertices) {
            auto world = deformable.transform.matrix * vec4(vertex, 0, 1);
            auto documentPoint = vec2(
                world.x + cast(float)documentWidth / 2.0f,
                world.y + cast(float)documentHeight / 2.0f
            );
            if (!haveBounds) {
                minPoint = documentPoint;
                maxPoint = documentPoint;
                haveBounds = true;
            } else {
                if (documentPoint.x < minPoint.x) minPoint.x = documentPoint.x;
                if (documentPoint.y < minPoint.y) minPoint.y = documentPoint.y;
                if (documentPoint.x > maxPoint.x) maxPoint.x = documentPoint.x;
                if (documentPoint.y > maxPoint.y) maxPoint.y = documentPoint.y;
            }
        }
        if (!haveBounds) return false;

        auto targetSize = maxPoint - minPoint;
        if (targetSize.x <= 0.0f || targetSize.y <= 0.0f) return false;
        auto xyOffset = vec2(
            minPoint.x - cast(float)layer.bounds.left,
            minPoint.y - cast(float)layer.bounds.top
        );
        auto xyScale = vec2(
            targetSize.x / cast(float)layer.width,
            targetSize.y / cast(float)layer.height
        );
        auto updated = updateLayerXYTransform(layer.id, xyOffset, xyScale);
        if (updated) statusMessage = _("Fit XY to target bounds: %s").format(layer.id);
        return updated;
    }

    bool updateLayerZTransform(string layerId, float backDepth, float frontDepth, bool invert, float zScale, float zOffset) {
        if (session is null) return false;
        auto updated = session.updateLayerZTransform(layerId, backDepth, frontDepth, invert, zScale, zOffset);
        if (updated) statusMessage = _("Updated Z transform: %s").format(layerId);
        return updated;
    }

    bool updateLayerSampling(string layerId, DepthImageChannel channel, DepthImageConvolution convolution,
            int customRadius, float alphaThreshold) {
        if (session is null) return false;
        auto updated = session.updateLayerSampling(layerId, channel, convolution, customRadius, alphaThreshold);
        if (updated) statusMessage = _("Updated sampling: %s").format(layerId);
        return updated;
    }

    bool updateLayerVisibility(string layerId, bool visible, bool enabled) {
        if (session is null) return false;
        auto updated = session.updateLayerVisibility(layerId, visible, enabled);
        if (updated) statusMessage = _("Updated layer state: %s").format(layerId);
        return updated;
    }

    DepthDrawLayerAlphaDepthGapFillSummary applyLayerAlphaDepthGapFill(string layerId) {
        DepthDrawLayerAlphaDepthGapFillSummary summary;
        if (session is null) return summary;
        summary = session.applyLayerAlphaDepthGapFill(layerId);
        if (summary.succeeded) {
            statusMessage = _("Filled alpha-depth gaps: %s marked, %s filled, %s remaining").format(
                summary.detected.total,
                summary.filled.filled,
                summary.filled.remaining
            );
        }
        return summary;
    }

    DepthDrawLayerAlphaDepthGapFillSummary applySelectedLayerAlphaDepthGapFill() {
        if (session is null || session.selectedLayerId.length == 0) return DepthDrawLayerAlphaDepthGapFillSummary();
        return applyLayerAlphaDepthGapFill(session.selectedLayerId);
    }

    DepthDrawLayerContourRepairSummary repairLayerContourDepth(string layerId, int thickness = 2) {
        DepthDrawLayerContourRepairSummary summary;
        if (session is null) return summary;
        summary = session.repairLayerContourDepth(layerId, thickness);
        if (summary.succeeded) {
            statusMessage = _("Repaired contour depth: %s contour, %s filled").format(
                summary.contourPixels,
                summary.filledPixels
            );
        }
        return summary;
    }

    DepthDrawLayerContourRepairSummary repairSelectedLayerContourDepth(int thickness = 2) {
        if (session is null || session.selectedLayerId.length == 0) return DepthDrawLayerContourRepairSummary();
        return repairLayerContourDepth(session.selectedLayerId, thickness);
    }

    void updateLayerStackView(string filter, DepthDrawLayerStackSortMode sortMode, bool descending) {
        layerStackFilter = filter;
        layerStackSortMode = sortMode;
        layerStackSortDescending = descending;
    }

    DepthDrawLayerStackRow[] displayLayerStackRows() {
        if (session is null) return null;
        DepthDrawComposeResult composeResult;
        auto hasComposeResult = selectedComposeResult(composeResult);
        auto rows = hasComposeResult
            ? ngDepthDrawLayerStackRows(session, &composeResult, (ulong uuid) => targetDisplayName(uuid))
            : ngDepthDrawLayerStackRows(session, null, (ulong uuid) => targetDisplayName(uuid));
        return ngDepthDrawFilterAndSortLayerStackRows(rows, layerStackFilter, layerStackSortMode, layerStackSortDescending);
    }

    bool updateBindingSampling(string layerId, ulong targetGridUuid, bool useNormalLayerAlpha, float coverageThreshold) {
        if (session is null) return false;
        auto updated = session.updateBindingSampling(layerId, targetGridUuid, useNormalLayerAlpha, coverageThreshold);
        if (updated) statusMessage = _("Updated binding sampling: %s").format(layerId);
        return updated;
    }

    bool updateBindingState(string layerId, ulong targetGridUuid, bool enabled, int order, DepthMergePolicy mergePolicy) {
        if (session is null) return false;
        auto updated = session.updateBindingState(layerId, targetGridUuid, enabled, order, mergePolicy);
        if (updated) statusMessage = _("Updated binding state: %s").format(layerId);
        return updated;
    }

    DepthDrawRange measureLayerRawRange(string layerId) {
        DepthDrawRange empty;
        if (session is null) return empty;
        auto layer = session.layerById(layerId);
        if (layer is null || !layer.hasDepthPixels()) return empty;

        auto settings = layer.sampleSettings();
        settings.convolution = DepthImageConvolution.Nearest;
        float[] values;
        foreach (y; 0 .. layer.height) {
            foreach (x; 0 .. layer.width) {
                auto sample = ngDepthImageSampleRgbaWithOpacity(
                    layer.depthPixels,
                    layer.width,
                    layer.height,
                    cast(float)x,
                    cast(float)y,
                    layer.opacity,
                    settings
                );
                if (sample.valid) values ~= sample.value;
            }
        }
        return ngDepthDrawRangeFromValues(values);
    }

    DepthDrawRange selectedLayerRawRange() {
        if (session is null || session.selectedLayerId.length == 0) return DepthDrawRange();
        return measureLayerRawRange(session.selectedLayerId);
    }

    DepthDrawRange selectedLayerFinalRange() {
        if (session is null || session.selectedLayerId.length == 0) return DepthDrawRange();
        auto layer = session.layerById(session.selectedLayerId);
        if (layer is null) return DepthDrawRange();
        return transformedLayerRange(*layer);
    }

    DepthDrawLayerRangeDiagnostics layerRangeDiagnostics(string layerId) {
        DepthDrawLayerRangeDiagnostics diagnostics;
        diagnostics.rawRange = measureLayerRawRange(layerId);
        auto layer = session is null ? null : session.layerById(layerId);
        if (layer !is null && diagnostics.rawRange.valid) {
            diagnostics.finalRange = ngDepthDrawRangeFromValues([
                layer.applyZTransform(diagnostics.rawRange.minDepth),
                layer.applyZTransform(diagnostics.rawRange.maxDepth),
            ]);
        }
        return diagnostics;
    }

    DepthDrawLayerRangeDiagnostics selectedLayerRangeDiagnostics() {
        if (session is null || session.selectedLayerId.length == 0) return DepthDrawLayerRangeDiagnostics();
        return layerRangeDiagnostics(session.selectedLayerId);
    }

    DepthDrawLayerSamplingDiagnostics layerSamplingDiagnostics(string layerId) {
        DepthDrawLayerSamplingDiagnostics diagnostics;
        if (session is null) return diagnostics;
        auto layer = session.layerById(layerId);
        if (layer is null || !layer.hasDepthPixels()) return diagnostics;

        auto settings = layer.sampleSettings();
        auto binding = selectedBinding();
        if (binding !is null && binding.layerId == layerId) {
            if (settings.alphaThreshold < binding.coverageThreshold) {
                settings.alphaThreshold = binding.coverageThreshold;
            }
            diagnostics.usesCoverage = binding.useNormalLayerAlpha;
        }
        diagnostics.hasCoverage = layer.hasNormalCoverage();
        diagnostics.totalPixels = cast(size_t)(layer.width * layer.height);

        float[] values;
        foreach (y; 0 .. layer.height) {
            foreach (x; 0 .. layer.width) {
                auto sample = diagnostics.usesCoverage && diagnostics.hasCoverage
                    ? ngDepthImageSampleRgbaWithOpacityAndCoverage(
                        layer.depthPixels,
                        layer.width,
                        layer.height,
                        layer.normalCoverage,
                        layer.width,
                        layer.height,
                        layer.opacity,
                        1.0f,
                        cast(float)x,
                        cast(float)y,
                        settings)
                    : ngDepthImageSampleRgbaWithOpacity(
                        layer.depthPixels,
                        layer.width,
                        layer.height,
                        cast(float)x,
                        cast(float)y,
                        layer.opacity,
                        settings);
                if (sample.valid) {
                    diagnostics.sampledPixels++;
                    values ~= sample.value;
                } else {
                    diagnostics.missingPixels++;
                }
            }
        }
        diagnostics.sampledRange = ngDepthDrawRangeFromValues(values);
        return diagnostics;
    }

    DepthDrawLayerSamplingDiagnostics selectedLayerSamplingDiagnostics() {
        if (session is null || session.selectedLayerId.length == 0) return DepthDrawLayerSamplingDiagnostics();
        return layerSamplingDiagnostics(session.selectedLayerId);
    }

    DepthDrawRange targetDepthRange(ulong gridUuid) {
        DepthDrawRange empty;
        auto puppet = incActivePuppet();
        if (puppet is null || gridUuid == 0) return empty;
        auto node = puppet.find!Node(cast(uint)gridUuid);
        auto mapped = cast(DepthMappedNode)node;
        if (mapped is null) return empty;
        return ngDepthDrawRangeFromValues(mapped.copyDepths());
    }

    bool selectFitZBackLayer(string layerId) {
        if (session is null || session.layerById(layerId) is null) return false;
        fitBackLayerId = layerId;
        return true;
    }

    bool selectFitZFrontLayer(string layerId) {
        if (session is null || session.layerById(layerId) is null) return false;
        fitFrontLayerId = layerId;
        return true;
    }

    DepthDrawFitZResult applySelectedLayerFitZToManualGap(float gapBack, float gapFront, float margin = 0.0f) {
        DepthDrawFitZResult result;
        if (session is null || session.selectedLayerId.length == 0) {
            result.error = "no selected layer";
            lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(DepthDrawRange(), DepthDrawFitZGap(), result);
            return result;
        }
        auto layer = session.layerById(session.selectedLayerId);
        if (layer is null) {
            result.error = "selected layer is missing";
            lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(DepthDrawRange(), DepthDrawFitZGap(), result);
            return result;
        }
        auto rawRange = measureLayerRawRange(layer.id);
        DepthDrawFitZGap gap;
        gap.valid = true;
        gap.back = gapBack;
        gap.front = gapFront;
        result = ngDepthDrawFitZToGap(rawRange, gap, margin);
        lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(rawRange, gap, result);
        if (result.succeeded) {
            updateLayerZTransform(layer.id, layer.backDepth, layer.frontDepth, layer.invert, result.zScale, result.zOffset);
        } else {
            statusMessage = result.error;
        }
        return result;
    }

    DepthDrawFitZResult applySelectedLayerFitZToAdjacentGap(float margin = 0.0f) {
        DepthDrawFitZResult result;
        if (session is null || session.selectedLayerId.length == 0) {
            result.error = "no selected layer";
            lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(DepthDrawRange(), DepthDrawFitZGap(), result);
            return result;
        }
        auto index = selectedLayerIndex();
        auto layer = session.layerById(session.selectedLayerId);
        if (index < 0 || layer is null) {
            result.error = "selected layer is missing";
            lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(DepthDrawRange(), DepthDrawFitZGap(), result);
            return result;
        }
        auto rawRange = measureLayerRawRange(layer.id);
        auto gap = ngDepthDrawGapFromAdjacentRanges(transformedLayerRanges(), cast(size_t)index);
        result = ngDepthDrawFitZToGap(rawRange, gap, margin);
        lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(rawRange, gap, result);
        if (result.succeeded) {
            updateLayerZTransform(layer.id, layer.backDepth, layer.frontDepth, layer.invert, result.zScale, result.zOffset);
        } else {
            statusMessage = result.error;
        }
        return result;
    }

    DepthDrawFitZResult applySelectedLayerFitZToTargetRange(float margin = 0.0f) {
        DepthDrawFitZResult result;
        if (session is null || session.selectedLayerId.length == 0) {
            result.error = "no selected layer";
            lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(DepthDrawRange(), DepthDrawFitZGap(), result);
            return result;
        }
        auto layer = session.layerById(session.selectedLayerId);
        if (layer is null) {
            result.error = "selected layer is missing";
            lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(DepthDrawRange(), DepthDrawFitZGap(), result);
            return result;
        }
        auto rawRange = measureLayerRawRange(layer.id);
        auto gap = ngDepthDrawGapFromTargetRange(targetDepthRange(session.selectedGridUuid));
        result = ngDepthDrawFitZToGap(rawRange, gap, margin);
        lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(rawRange, gap, result);
        if (result.succeeded) {
            updateLayerZTransform(layer.id, layer.backDepth, layer.frontDepth, layer.invert, result.zScale, result.zOffset);
        } else {
            statusMessage = result.error;
        }
        return result;
    }

    DepthDrawFitZResult applySelectedLayerFitZToSelectedLayerGap(
        string backLayerId,
        string frontLayerId,
        float margin = 0.0f
    ) {
        DepthDrawFitZResult result;
        if (session is null || session.selectedLayerId.length == 0) {
            result.error = "no selected layer";
            lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(DepthDrawRange(), DepthDrawFitZGap(), result);
            return result;
        }
        auto layer = session.layerById(session.selectedLayerId);
        auto backLayer = session.layerById(backLayerId);
        auto frontLayer = session.layerById(frontLayerId);
        if (layer is null || backLayer is null || frontLayer is null) {
            result.error = "selected fit layer is missing";
            lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(DepthDrawRange(), DepthDrawFitZGap(), result);
            return result;
        }
        auto rawRange = measureLayerRawRange(layer.id);
        auto gap = ngDepthDrawGapFromSelectedLayerRanges(transformedLayerRange(*backLayer), transformedLayerRange(*frontLayer));
        result = ngDepthDrawFitZToGap(rawRange, gap, margin);
        lastFitZDiagnostics = ngDepthDrawFitZDiagnostics(rawRange, gap, result);
        if (result.succeeded) {
            updateLayerZTransform(layer.id, layer.backDepth, layer.frontDepth, layer.invert, result.zScale, result.zOffset);
        } else {
            statusMessage = result.error;
        }
        return result;
    }

    int loadedDocumentWidth() const {
        return documentWidth;
    }

    int loadedDocumentHeight() const {
        return documentHeight;
    }

    string loadError() const {
        return errorMessage;
    }

    string statusText() const {
        return statusMessage;
    }

    string diagnosticsText() {
        return sourceDiagnosticsText();
    }
}
