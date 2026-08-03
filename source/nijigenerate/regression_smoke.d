module nijigenerate.regression_smoke;

version (RegressionSmoke):

import std.algorithm.comparison : min;
import std.conv : to;
import std.file : exists, tempDir;
import std.path : buildPath;
import std.process : environment;
import std.string : join, startsWith;

import nijigenerate.actions : Action;
import nijigenerate.commands : Context;
import nijigenerate.commands.depth.bone : ngFlushDepthBoneDirty;
import nijigenerate.commands.depth.map : PsdDepthComposedView, ngApplyPsdDepthImportResult,
    ngComposePsdDepthImportResult;
import nijigenerate.commands.vertex.define_mesh : DefineGridCommand;
import nijigenerate.core;
import nijigenerate.core.actionstack;
import nijigenerate.ext.nodes.exdepthbone : ExDepthBone, ExDepthRigRoot, ExDepthTargetKind;
import nijigenerate.ext.nodes.exgriddeformer : ExGridDeformer;
import nijigenerate.ext.nodes.expathdeformer : ExPathDeformer;
import nijigenerate.io.depthmap_psd : PsdDepthImportResult;
import nijigenerate.io.save : incCloseProjectAsk, incSetSaveProjectOnClose;
import nijigenerate.panels;
import nijigenerate.panels.resource;
import nijigenerate.project : EditMode, incActivePuppet, incSelectNode, incSetEditMode;
import nijigenerate.widgets.modal : incModalAdd;
import nijigenerate.windows;
import nijigenerate.windows.autosave : RestoreSaveWindow;
import nijigenerate.windows.inpexport : ExportWindow;
import nijigenerate.viewport.depth.camera : DepthBrushSettings, DepthCamera3D, DepthToolMode;
import nijigenerate.viewport.depth.draw : DepthDrawGpuComposePacket, DepthDrawGpuComposeReadback,
    DepthDrawGpuDispatchPollResult, DepthDrawGpuLayerReadback, DepthDrawViewport, ngClearDepthDrawGpuTestHooks,
    ngDepthDrawAutoBindSession, ngLoadDepthDrawPngLayer, ngSetDepthDrawGpuTestHooks;
import nijigenerate.viewport.depth.renderer : DepthTargetRenderer;
import nijigenerate.viewport.depth.tools.operation : DepthAttachedPointOperation, DepthPlaneOperation, DepthRingOperation;
import nijigenerate.viewport.depth.viewport : DepthEditViewport;
import nijigenerate.windows.settings : SettingsWindow;
import nijilive;
import nijilive.core.meshdata : MeshData;
import nijilive.core.param : Parameter;
import nijilive.core.texture : Texture;
import nijilive.math : Vec2Array, vec2;

private string g_RegressionSmokeFailureMessage;
private uint regressionSmokeDepthDrawGpuNextJobId = 1;
private uint regressionSmokeDepthDrawGpuSubmitCount;
private uint regressionSmokeDepthDrawGpuPollCount;
private DepthDrawGpuComposeReadback[uint] regressionSmokeDepthDrawGpuReadbacks;

private bool regressionSmokeDepthDrawGpuSupported() {
    return true;
}

private bool regressionSmokeDepthDrawGpuSubmit(ref DepthDrawGpuComposePacket packet, out uint jobId, out string error) {
    jobId = regressionSmokeDepthDrawGpuNextJobId++;
    error = null;
    regressionSmokeDepthDrawGpuSubmitCount++;

    DepthDrawGpuComposeReadback readback;
    readback.targetGridUuid = packet.targetGridUuid;
    readback.depths.length = packet.vertices.length;
    readback.winningLayerIndices.length = packet.vertices.length;
    foreach (i; 0 .. packet.vertices.length) {
        readback.depths[i] = 0.625f;
        readback.winningLayerIndices[i] = packet.layers.length > 0 ? 0 : -1;
    }
    readback.layers.length = packet.layers.length;
    foreach (layerIndex; 0 .. packet.layers.length) {
        ubyte[] validSamples;
        float[] sampleDepths;
        validSamples.length = packet.vertices.length;
        sampleDepths.length = packet.vertices.length;
        foreach (i; 0 .. packet.vertices.length) {
            validSamples[i] = 1;
            sampleDepths[i] = 0.625f;
        }
        readback.layers[layerIndex] = DepthDrawGpuLayerReadback(cast(uint)layerIndex, validSamples, sampleDepths);
    }
    regressionSmokeDepthDrawGpuReadbacks[jobId] = readback;
    return true;
}

private bool regressionSmokeDepthDrawGpuPoll(uint jobId, out DepthDrawGpuDispatchPollResult result, out string error) {
    result = DepthDrawGpuDispatchPollResult.init;
    error = null;
    auto readback = jobId in regressionSmokeDepthDrawGpuReadbacks;
    if (readback is null) {
        error = "missing regression smoke DepthDraw GPU readback";
        return false;
    }
    result.ready = true;
    result.readback = *readback;
    regressionSmokeDepthDrawGpuReadbacks.remove(jobId);
    regressionSmokeDepthDrawGpuPollCount++;
    return true;
}

private void resetRegressionSmokeDepthDrawGpuHooks() {
    regressionSmokeDepthDrawGpuNextJobId = 1;
    regressionSmokeDepthDrawGpuSubmitCount = 0;
    regressionSmokeDepthDrawGpuPollCount = 0;
    regressionSmokeDepthDrawGpuReadbacks = null;
    ngSetDepthDrawGpuTestHooks(
        &regressionSmokeDepthDrawGpuSupported,
        &regressionSmokeDepthDrawGpuSubmit,
        &regressionSmokeDepthDrawGpuPoll
    );
}

struct RegressionSmokeOptions {
    bool enabled;
    bool computerUse;
    string scenario;
    int frames = 6;
    int frameDelayMs = 0;
}

RegressionSmokeOptions ngParseRegressionSmokeOptions(string[] args) {
    RegressionSmokeOptions options;
    options.enabled = args.length >= 2 && args[1] == "--regression-smoke";
    options.scenario = options.enabled && args.length >= 3 ? args[2] : "";

    foreach (i, arg; args) {
        if (arg == "--regression-computer-use")
            options.computerUse = true;
        if (arg == "--regression-frames" && i + 1 < args.length)
            options.frames = args[i + 1].to!int;
        if (arg == "--regression-frame-delay-ms" && i + 1 < args.length)
            options.frameDelayMs = args[i + 1].to!int;
    }
    if (options.frames < 1)
        options.frames = 1;
    if (options.frameDelayMs < 0)
        options.frameDelayMs = 0;
    return options;
}

void ngRegressionSmokeFail(string message) {
    g_RegressionSmokeFailureMessage = message;
}

bool ngRegressionSmokeFailed() {
    return g_RegressionSmokeFailureMessage.length != 0;
}

string ngRegressionSmokeFailureMessage() {
    return g_RegressionSmokeFailureMessage;
}

private final class RegressionSmokeDirtyAction : Action {
    void rollback() {}
    void redo() {}
    string describe() { return "Regression smoke dirty marker"; }
    string describeUndo() { return "Regression smoke dirty marker"; }
    string getName() { return "RegressionSmokeDirtyAction"; }
    bool merge(Action other) { return false; }
    bool canMerge(Action other) { return false; }
}

void ngSetupRegressionSmokeScenario(string scenario) {
    void showPanels(string[] names...) {
        foreach (name; names) {
            auto panel = incFindPanelByName(name);
            if (panel !is null)
                panel.visible = true;
        }
    }

    void showAllPanels() {
        foreach (panel; incPanels)
            panel.visible = true;
    }

    void ensureAnimMode() {
        incSetEditMode(EditMode.AnimEdit);
    }

    void ensureModelMode() {
        incSetEditMode(EditMode.ModelEdit);
    }

    void ensureDepthMode() {
        incSetEditMode(EditMode.DepthEdit, false);
    }

    void ensureVertexMode() {
        incSetEditMode(EditMode.VertexEdit);
    }

    string localDepthDrawDataPath(string filename, string fallback) {
        auto userProfile = environment.get("USERPROFILE", null);
        if (userProfile.length) {
            auto candidate = buildPath(userProfile, "src", "depth-draw", "data", filename);
            if (candidate.exists)
                return candidate;
        }
        return fallback;
    }

    string writeSmokeDepthPng(string filename, ubyte baseGray) {
        auto path = buildPath(tempDir(), filename);
        immutable width = 32;
        immutable height = 32;
        ubyte[] pixels;
        pixels.length = width * height * 4;
        foreach (i; 0 .. width * height) {
            auto offset = i * 4;
            auto gray = cast(ubyte)min(255, cast(int)baseGray + cast(int)(i % 4) * 8);
            pixels[offset + 0] = gray;
            pixels[offset + 1] = gray;
            pixels[offset + 2] = gray;
            pixels[offset + 3] = 255;
        }
        auto texture = ShallowTexture(pixels, width, height, 4);
        texture.save(path);
        return path;
    }

    ExGridDeformer createSmokeDepthGridWithAxes(string name, float[] xs, float[] ys) {
        auto grid = new ExGridDeformer(incActivePuppet().root);
        grid.name = name;
        auto ctx = new Context();
        ctx.nodes = [cast(Node)grid];
        auto result = (new DefineGridCommand(xs, ys)).run(ctx);
        if (!result.succeeded)
            ngRegressionSmokeFail("Regression smoke failed to define target grid: " ~ result.message);
        return grid;
    }

    ExGridDeformer createSmokeDepthGrid(string name) {
        return createSmokeDepthGridWithAxes(name, [-1.0f, 0.0f, 1.0f], [-1.0f, 1.0f]);
    }

    PsdDepthImportResult combinePsdDepthImportPreviews(PsdDepthImportResult[] previews, ulong[] targetGridUuids) {
        PsdDepthImportResult result;
        foreach (previewIndex, preview; previews) {
            if (previewIndex >= targetGridUuids.length) break;
            auto targetGridUuid = targetGridUuids[previewIndex];
            auto prefix = "/combined-" ~ previewIndex.to!string;
            string[string] renamedPaths;
            bool[string] usedPaths;
            foreach (grid; preview.grids) {
                if (grid.grid is null || grid.grid.uuid != targetGridUuid) continue;
                auto renamed = grid;
                renamed.layerMasks = grid.layerMasks.dup;
                foreach (ref layerMask; renamed.layerMasks) {
                    auto oldPath = layerMask.layerPath;
                    auto newPath = prefix ~ oldPath;
                    renamedPaths[oldPath] = newPath;
                    usedPaths[oldPath] = true;
                    layerMask.layerPath = newPath;
                }
                result.grids ~= renamed;
            }
            foreach (layer; preview.composedLayers) {
                if ((layer.layerPath in usedPaths) is null) continue;
                auto renamed = layer;
                auto newPath = layer.layerPath in renamedPaths;
                if (newPath is null) continue;
                renamed.id = *newPath;
                renamed.layerPath = *newPath;
                result.composedLayers ~= renamed;
            }
            foreach (mapping; preview.mappings) {
                if (mapping.targetGridUuid != targetGridUuid || (mapping.layerPath in usedPaths) is null) continue;
                auto renamed = mapping;
                if (auto newPath = mapping.layerPath in renamedPaths) renamed.layerPath = *newPath;
                result.mappings ~= renamed;
            }
            result.sourceDepthLayerCount += preview.sourceDepthLayerCount;
            result.composedLayerCount += preview.composedLayerCount;
            result.compositionDiagnostics ~= preview.compositionDiagnostics;
            result.matchedLayers += preview.matchedLayers;
            result.unmatchedLayers += preview.unmatchedLayers;
            result.ambiguousLayers += preview.ambiguousLayers;
            result.skippedGrids += preview.skippedGrids;
            result.gpuCompositionRequested = result.gpuCompositionRequested || preview.gpuCompositionRequested;
        }
        return result;
    }

    void attachSmokeCoveragePart(ExGridDeformer grid, string name, float extent = 1.0f) {
        MeshData data;
        data.vertices = Vec2Array([
            vec2(-extent, -extent),
            vec2(extent, -extent),
            vec2(-extent, extent),
            vec2(extent, extent),
        ]);
        data.uvs = Vec2Array([
            vec2(0.0f, 0.0f),
            vec2(1.0f, 0.0f),
            vec2(0.0f, 1.0f),
            vec2(1.0f, 1.0f),
        ]);
        data.indices = [cast(ushort)0, 1, 3, 0, 3, 2];
        data.origin = vec2(0.0f, 0.0f);
        auto texture = new Texture(cast(ubyte[])[255, 255, 255, 255], 1, 1, 4, 4, false, false);
        auto part = new Part(data, [texture], inCreateUUID(), grid);
        part.name = name;
    }

    bool isPanelScenario =
        scenario.startsWith("panels.") ||
        scenario == "viewport.panels" ||
        scenario == "node.visibility-lock" ||
        scenario == "inspectors.commit-boundaries" ||
        scenario == "undo.ui-commit-boundaries";
    bool isViewportScenario = scenario.startsWith("viewport.");
    bool isWidgetScenario = scenario.startsWith("widgets.");

    if (isPanelScenario || isViewportScenario || isWidgetScenario) {
        showAllPanels();
        if (scenario == "panels.timeline" || scenario == "animation.timeline-ui" ||
            scenario == "animation.playback" || scenario == "animation.keyframe-copy-paste" ||
            scenario == "widgets.timeline" || scenario == "viewport.animation-mode")
            ensureAnimMode();
        else if (scenario.startsWith("depth.") || scenario == "viewport.depth-mode" ||
            scenario == "depthbone.refresh-queue")
            ensureDepthMode();
        else if (scenario.startsWith("mesh.") || scenario.startsWith("deform.") ||
            scenario == "viewport.model-mode")
            ensureModelMode();
        return;
    }

    if (scenario == "tools.command-browser")
        incPushWindow(new CommandBrowserWindow());
    else if (scenario == "tools.texture-viewer")
        incPushWindow(new TextureViewerWindow(incGetLogo()));
    else if (scenario == "tools.export-dialogs" || scenario == "windows.export-import" ||
        scenario == "project.file-dialogs")
        incPushWindow(new ExportWindow("regression-smoke.inp"));
    else if (scenario == "tools.ai-agent" || scenario == "api.agent-panel")
        showPanels("AI Agent");
    else if (scenario == "tools.shell")
        showPanels("Shell");
    else if (scenario == "windows.welcome-about") {
        incPushWindow(new WelcomeWindow());
        incPushWindow(new AboutWindow());
    } else if (scenario == "windows.automesh-batch")
        incModalAdd(new AutoMeshBatchWindow());
    else if (scenario == "windows.settings" || scenario == "settings.window")
        incPushWindow(new SettingsWindow());
    else if (scenario == "windows.rename") {
        static string renameTarget = "Regression";
        incPushWindow(new RenameWindow(renameTarget));
    } else if (scenario == "windows.flip-config")
        incPushWindow(new FlipPairWindow());
    else if (scenario == "windows.parameter-editors") {
        auto param = new Parameter("Regression Smoke", true);
        incActivePuppet().parameters ~= param;
        incPushWindow(new ParamEditorWindow(param));
    } else if (scenario == "windows.parameter-split") {
        auto param = new Parameter("Regression Smoke Split", true);
        incActivePuppet().parameters ~= param;
        incPushWindow(new ParamSplitWindow(0, param));
    } else if (scenario == "windows.autosave")
        incPushWindow(new RestoreSaveWindow("regression-smoke.inx"));
    else if (scenario == "render.backend-gl-sdl" || scenario == "platform.input-window") {
        showPanels("Viewport");
    } else if (scenario == "render.postprocess" || scenario == "render.onion-slice" ||
        scenario == "viewport.driver-postprocess") {
        showPanels("Viewport");
    } else if (scenario == "project.close-dirty-prompts") {
        incSetSaveProjectOnClose("Ask");
        incActionPush(new RegressionSmokeDirtyAction());
        incCloseProjectAsk();
    } else if (scenario == "project.depthdraw-live-ui-smoke") {
        ensureDepthMode();
        showPanels("Viewport", "Tool Settings", "Inspector");
        auto grid = createSmokeDepthGrid("nijigenerate-depthdraw-smoke");
        if (ngRegressionSmokeFailed()) return;
        auto depthDrawWindow = new DepthDrawWindow(writeSmokeDepthPng("nijigenerate-depthdraw-smoke-back.png", 32));
        if (depthDrawWindow.loadError.length) {
            ngRegressionSmokeFail("DepthDraw smoke failed to load source: " ~ depthDrawWindow.loadError);
            return;
        }
        if (depthDrawWindow.depthDrawSession() is null || depthDrawWindow.depthDrawSession().layers.length == 0) {
            ngRegressionSmokeFail("DepthDraw smoke loaded no layers");
            return;
        }
        depthDrawWindow.depthDrawSession().layers[0].layerPath = "/" ~ grid.name;
        depthDrawWindow.depthDrawSession().layers[0].displayName = grid.name;
        auto frontLayer = ngLoadDepthDrawPngLayer(
            writeSmokeDepthPng("nijigenerate-depthdraw-smoke-front.png", 144),
            "nijigenerate-depthdraw-smoke-front"
        );
        frontLayer.layerPath = "/" ~ grid.name;
        frontLayer.displayName = grid.name;
        depthDrawWindow.depthDrawSession().layers ~= frontLayer;
        auto bindings = ngDepthDrawAutoBindSession(depthDrawWindow.depthDrawSession(), incActivePuppet());
        if (bindings.length < 2 || depthDrawWindow.depthDrawSession().bindings.length < 2) {
            ngRegressionSmokeFail("DepthDraw smoke failed to auto-bind generated layers to target grid");
            return;
        }
        depthDrawWindow.selectLayer(depthDrawWindow.depthDrawSession().layers[0].id);
        depthDrawWindow.selectTargetGrid(grid.uuid);
        auto rows = depthDrawWindow.displayLayerStackRows();
        size_t boundRows;
        size_t sampledRows;
        foreach (row; rows) {
            if (row.targetGridUuid != grid.uuid) continue;
            boundRows++;
            if (row.sampledVertices > 0 && row.hasDepthRange) sampledRows++;
        }
        if (rows.length < 2 || boundRows < 2 || sampledRows < 2) {
            ngRegressionSmokeFail("DepthDraw smoke failed to build two sampled layer-stack rows for the target grid");
            return;
        }
        auto smokeViewport = new DepthDrawViewport(depthDrawWindow.depthDrawSession());
        smokeViewport.setDocumentSize(3, 2);
        smokeViewport.selectionChanged([cast(Node)grid]);
        auto preview = smokeViewport.composePreview(grid.uuid);
        if (preview.depths.length == 0 || preview.sampledVertices == 0 || preview.layerStats.length < 2) {
            ngRegressionSmokeFail("DepthDraw smoke failed to compose viewport preview for the target grid");
            return;
        }
        auto gpuDisplay = depthDrawWindow.depthDrawSession().display;
        gpuDisplay.useGpuPreview = true;
        if (!depthDrawWindow.depthDrawSession().updateDisplayOptions(gpuDisplay) ||
            !depthDrawWindow.depthDrawSession().display.useGpuPreview ||
            !depthDrawWindow.depthDrawSession().isTargetPreviewDirty(grid.uuid) ||
            smokeViewport.composeSelectedPreviewForUpdate()) {
            ngRegressionSmokeFail("DepthDraw smoke failed to route GPU preview display option through session");
            return;
        }
        gpuDisplay.useGpuPreview = false;
        if (!depthDrawWindow.depthDrawSession().updateDisplayOptions(gpuDisplay)) {
            ngRegressionSmokeFail("DepthDraw smoke failed to restore CPU preview display option");
            return;
        }
        auto geometryStats = smokeViewport.collectRenderGeometry().stats();
        if (geometryStats.targetMeshes == 0 ||
            geometryStats.targetLines == 0 ||
            geometryStats.layerPlaneLines < 8 ||
            geometryStats.depthRangeLines < 8 ||
            geometryStats.selectedLayerLines == 0) {
            ngRegressionSmokeFail("DepthDraw smoke failed to build viewport relationship render geometry");
            return;
        }
        if (!depthDrawWindow.presentDepthDrawViewport()) {
            ngRegressionSmokeFail("DepthDraw smoke failed to open viewport: " ~ depthDrawWindow.loadError);
            return;
        }
        incPushWindow(depthDrawWindow);
    } else if (scenario == "project.psd-depth-3d-adjust-consecutive-smoke") {
        ensureDepthMode();
        auto grid = createSmokeDepthGridWithAxes(
            "PsdDepthConsecutiveSmoke:G",
            [0.0f, 16.0f, 31.0f],
            [0.0f, 16.0f, 31.0f]
        );
        if (ngRegressionSmokeFailed()) return;
        auto window = new PSDDepthMapWindow(writeSmokeDepthPng(
            "nijigenerate-psd-depth-consecutive-smoke.png", 96));
        window.rebuildPreviewForRegressionSmoke();
        if (window.loadErrorForRegressionSmoke.length ||
            !window.remapAnyLayerToSampledGridForRegressionSmoke(grid.name, grid.uuid) ||
            !window.has3DAdjustGeometryForRegressionSmoke(grid.name)) {
            ngRegressionSmokeFail("PSD depth consecutive-adjust smoke failed to prepare mapped geometry: " ~
                window.loadErrorForRegressionSmoke ~ " :: " ~ window.previewSummaryForRegressionSmoke());
            return;
        }
        if (!window.setFirstMappedLayerZTransformForRegressionSmoke(grid.name, 1.25f, 0.75f) ||
            !window.hasRealtime3DAdjustTransformForRegressionSmoke(grid.name) ||
            !window.setFirstMappedLayerZTransformForRegressionSmoke(grid.name, 0.8f, -0.25f) ||
            !window.hasRealtime3DAdjustTransformForRegressionSmoke(grid.name)) {
            ngRegressionSmokeFail("PSD depth consecutive-adjust smoke failed to refresh every edit before Apply: " ~
                window.previewSummaryForRegressionSmoke());
            return;
        }
        string applyMessage;
        if (!window.applyForRegressionSmoke(applyMessage) ||
            window.hasPending3DAdjustLayerChangesForRegressionSmoke() ||
            window.last3DAdjustApplyRecomposedGridCountForRegressionSmoke() != 1 ||
            !window.hasAppliedDepthsForRegressionSmoke(grid.name)) {
            ngRegressionSmokeFail("PSD depth consecutive-adjust smoke failed to apply the final edit: " ~
                applyMessage ~ " :: " ~ window.previewSummaryForRegressionSmoke());
            return;
        }
    } else if (scenario == "project.psd-depth-map-import-ui-smoke" || scenario == "windows.psd-depth-map") {
        ensureDepthMode();
        showPanels("Viewport", "Tool Settings", "Inspector");
        float[] psdAxes;
        for (float value = -600.0f; value <= 600.0f; value += 40.0f) psdAxes ~= value;
        auto bodyGrid = createSmokeDepthGridWithAxes(
            "Body:G",
            psdAxes,
            psdAxes
        );
        if (ngRegressionSmokeFailed()) return;
        attachSmokeCoveragePart(bodyGrid, "bottomwear-back", 300.0f);
        auto psdDepthWindow = new PSDDepthMapWindow(localDepthDrawDataPath(
            "Midori-20260621-color-psd-depth.psd",
            "regression-smoke-depth.psd"
        ));
        psdDepthWindow.rebuildPreviewForRegressionSmoke();
        if (psdDepthWindow.loadErrorForRegressionSmoke.length) {
            ngRegressionSmokeFail("PSD depth import smoke failed to build preview: " ~ psdDepthWindow.loadErrorForRegressionSmoke);
            return;
        }
        if (!psdDepthWindow.hasSampledPreviewGridForRegressionSmoke(bodyGrid.name)) {
            auto bodyRemapped = psdDepthWindow.remapLayerNameToGridForRegressionSmoke("bottomwear-back", bodyGrid.uuid) &&
                psdDepthWindow.hasSampledPreviewGridForRegressionSmoke(bodyGrid.name);
            auto anyRemapped = bodyRemapped ||
                psdDepthWindow.remapAnyLayerToSampledGridForRegressionSmoke(bodyGrid.name, bodyGrid.uuid);
            if (!anyRemapped) {
                ngRegressionSmokeFail("PSD depth import smoke failed to remap body layer to target grid: " ~
                    psdDepthWindow.diagnosticsForRegressionSmoke().join(" | ") ~ " :: " ~
                    psdDepthWindow.previewSummaryForRegressionSmoke());
                return;
            }
        }
        if (psdDepthWindow.previewGridCountForRegressionSmoke() == 0 ||
            !psdDepthWindow.hasSampledPreviewGridForRegressionSmoke(bodyGrid.name)) {
            ngRegressionSmokeFail("PSD depth import smoke failed to build sampled preview grid");
            return;
        }
        if (!psdDepthWindow.has3DAdjustGeometryForRegressionSmoke(bodyGrid.name)) {
            ngRegressionSmokeFail("PSD depth import smoke failed to build 3D Adjust relationship geometry");
            return;
        }
        if (!psdDepthWindow.hasFrontOnlyIntersectionConstraintForRegressionSmoke()) {
            ngRegressionSmokeFail("PSD depth import smoke failed to constrain adjusted layers to intersecting front layers only: " ~
                psdDepthWindow.frontOnlyIntersectionConstraintDiagnosticsForRegressionSmoke());
            return;
        }
        auto pngGrid = createSmokeDepthGridWithAxes(
            "PngDepthSmoke:G",
            [-1.0f, 0.0f, 1.0f],
            [-1.0f, 0.0f, 1.0f]
        );
        attachSmokeCoveragePart(pngGrid, "png-depth-smoke", 1.0f);
        auto pngWindow = new PSDDepthMapWindow(writeSmokeDepthPng("nijigenerate-psd-depth-import-smoke.png", 192));
        pngWindow.rebuildPreviewForRegressionSmoke();
        if (pngWindow.loadErrorForRegressionSmoke.length) {
            ngRegressionSmokeFail("PSD depth import smoke failed to load PNG source: " ~ pngWindow.loadErrorForRegressionSmoke);
            return;
        }
        if (!pngWindow.remapAnyLayerToSampledGridForRegressionSmoke(pngGrid.name, pngGrid.uuid) ||
            !pngWindow.hasSampledPreviewGridForRegressionSmoke(pngGrid.name) ||
            !pngWindow.has3DAdjustGeometryForRegressionSmoke(pngGrid.name)) {
            ngRegressionSmokeFail("PSD depth import smoke failed to route PNG through Source / Mapping and 3D Adjust: " ~
                pngWindow.diagnosticsForRegressionSmoke().join(" | ") ~ " :: " ~
                pngWindow.previewSummaryForRegressionSmoke());
            return;
        }
        auto pngDepthsBeforeAdjust = pngWindow.previewDepthsForRegressionSmoke(pngGrid.name);
        if (!pngWindow.setFirstMappedLayerZTransformForRegressionSmoke(pngGrid.name, 1.25f, 0.75f) ||
            !pngWindow.hasPending3DAdjustLayerChangesForRegressionSmoke() ||
            !pngWindow.hasRealtime3DAdjustTransformForRegressionSmoke(pngGrid.name) ||
            pngWindow.previewDepthsForRegressionSmoke(pngGrid.name) != pngDepthsBeforeAdjust) {
            ngRegressionSmokeFail("PSD depth import smoke failed to update 3D Adjust in real time without composing targets: " ~
                pngWindow.previewSummaryForRegressionSmoke());
            return;
        }
        if (!pngWindow.setFirstMappedLayerZTransformForRegressionSmoke(pngGrid.name, 0.8f, -0.25f) ||
            !pngWindow.hasRealtime3DAdjustTransformForRegressionSmoke(pngGrid.name)) {
            ngRegressionSmokeFail("PSD depth import smoke failed to update consecutive 3D Adjust edits before Apply: " ~
                pngWindow.previewSummaryForRegressionSmoke());
            return;
        }
        string pngApplyMessage;
        if (!pngWindow.applyForRegressionSmoke(pngApplyMessage) ||
            pngWindow.hasPending3DAdjustLayerChangesForRegressionSmoke() ||
            pngWindow.last3DAdjustApplyRecomposedGridCountForRegressionSmoke() != 1 ||
            !pngWindow.hasAppliedDepthsForRegressionSmoke(pngGrid.name) ||
            pngWindow.previewDepthsForRegressionSmoke(pngGrid.name) == pngDepthsBeforeAdjust) {
            ngRegressionSmokeFail("PSD depth import smoke failed to flush and apply deferred 3D Adjust Z Scale/Offset: " ~
                pngApplyMessage ~ " :: " ~ pngWindow.previewSummaryForRegressionSmoke());
            return;
        }

        auto pngGpuGrid = createSmokeDepthGridWithAxes(
            "PngGpuDepthSmoke:G",
            [-1.0f, 0.0f, 1.0f],
            [-1.0f, 0.0f, 1.0f]
        );
        attachSmokeCoveragePart(pngGpuGrid, "png-gpu-depth-smoke", 1.0f);
        auto pngGpuWindow = new PSDDepthMapWindow(writeSmokeDepthPng("nijigenerate-psd-depth-import-gpu-smoke.png", 208));
        pngGpuWindow.rebuildPreviewForRegressionSmoke();
        if (pngGpuWindow.loadErrorForRegressionSmoke.length) {
            ngRegressionSmokeFail("PSD depth import smoke failed to load GPU PNG source: " ~ pngGpuWindow.loadErrorForRegressionSmoke);
            return;
        }
        if (!pngGpuWindow.remapAnyLayerToSampledGridForRegressionSmoke(pngGpuGrid.name, pngGpuGrid.uuid)) {
            ngRegressionSmokeFail("PSD depth import smoke failed to map GPU PNG source");
            return;
        }
        resetRegressionSmokeDepthDrawGpuHooks();
        pngGpuWindow.setGpuCompositionForRegressionSmoke(true);
        auto gpuSubmitCountBeforeApply = regressionSmokeDepthDrawGpuSubmitCount;
        auto gpuPollCountBeforeApply = regressionSmokeDepthDrawGpuPollCount;
        string pngGpuApplyMessage;
        auto gpuApplied = pngGpuWindow.applyForRegressionSmoke(pngGpuApplyMessage);
        ngClearDepthDrawGpuTestHooks();
        if (!gpuApplied ||
            gpuSubmitCountBeforeApply == 0 ||
            gpuPollCountBeforeApply == 0 ||
            regressionSmokeDepthDrawGpuSubmitCount != gpuSubmitCountBeforeApply ||
            regressionSmokeDepthDrawGpuPollCount != gpuPollCountBeforeApply ||
            pngGpuGrid.copyDepths().length == 0 ||
            pngGpuGrid.copyDepths()[0] != 0.625f) {
            ngRegressionSmokeFail("PSD depth import smoke failed to apply PNG source through GPU submit/poll/readback: " ~ pngGpuApplyMessage);
            return;
        }

        auto multiGridA = createSmokeDepthGrid("PngMultiSmokeA:G");
        auto multiGridB = createSmokeDepthGrid("PngMultiSmokeB:G");
        attachSmokeCoveragePart(multiGridA, "png-multi-smoke-a", 1.0f);
        attachSmokeCoveragePart(multiGridB, "png-multi-smoke-b", 1.0f);
        multiGridA.replaceDepths([0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f]);
        multiGridB.replaceDepths([0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f]);
        auto multiWindowA = new PSDDepthMapWindow(writeSmokeDepthPng("nijigenerate-psd-depth-multi-a.png", 80));
        auto multiWindowB = new PSDDepthMapWindow(writeSmokeDepthPng("nijigenerate-psd-depth-multi-b.png", 176));
        multiWindowA.rebuildPreviewForRegressionSmoke();
        multiWindowB.rebuildPreviewForRegressionSmoke();
        if (multiWindowA.loadErrorForRegressionSmoke.length || multiWindowB.loadErrorForRegressionSmoke.length) {
            ngRegressionSmokeFail("PSD depth import smoke failed to load multi-binding PNG sources");
            return;
        }
        if (!multiWindowA.remapAnyLayerToSampledGridForRegressionSmoke(multiGridA.name, multiGridA.uuid) ||
            !multiWindowB.remapAnyLayerToSampledGridForRegressionSmoke(multiGridB.name, multiGridB.uuid)) {
            ngRegressionSmokeFail("PSD depth import smoke failed to map multiple PNG bindings");
            return;
        }
        auto combinedImport = combinePsdDepthImportPreviews([
            multiWindowA.previewForRegressionSmoke(),
            multiWindowB.previewForRegressionSmoke(),
        ], [multiGridA.uuid, multiGridB.uuid]);
        incActionClearHistory();
        PsdDepthComposedView combinedComposed;
        string combinedComposeError;
        if (!ngComposePsdDepthImportResult(combinedImport, combinedComposed, combinedComposeError)) {
            ngRegressionSmokeFail("PSD depth import smoke failed to compose multiple mapped targets: " ~ combinedComposeError);
            return;
        }
        auto multiApply = ngApplyPsdDepthImportResult(combinedComposed);
        if (!multiApply.succeeded ||
            multiGridA.copyDepths() == [0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f] ||
            multiGridB.copyDepths() == [0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f]) {
            ngRegressionSmokeFail("PSD depth import smoke failed to apply multiple mapped targets: " ~ multiApply.message ~
                " a=" ~ multiGridA.copyDepths().to!string ~ " b=" ~ multiGridB.copyDepths().to!string ~
                " composedA=" ~ combinedImport.grids[0].depths.to!string ~
                " composedB=" ~ combinedImport.grids[1].depths.to!string);
            return;
        }
        incActionUndo();
        if (multiGridA.copyDepths() != [0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f] ||
            multiGridB.copyDepths() != [0.6f, 0.5f, 0.4f, 0.3f, 0.2f, 0.1f]) {
            ngRegressionSmokeFail("PSD depth import smoke failed to undo multiple mapped targets");
            return;
        }
        incActionRedo();
        if (multiGridA.copyDepths()[0] == 0.1f || multiGridB.copyDepths()[0] == 0.6f) {
            ngRegressionSmokeFail("PSD depth import smoke failed to redo multiple mapped targets");
            return;
        }
        incPushWindow(psdDepthWindow);
    } else if (scenario == "project.export-video" || scenario.startsWith("io.video-")) {
        incPushWindow(new VideoExportWindow("regression-smoke.mp4"));
    } else if (scenario == "io.image-export") {
        incPushWindow(new ImageExportWindow("regression-smoke.png"));
    } else if (scenario.startsWith("project.export-") || scenario == "render.blend-modes") {
        showPanels("Viewport");
    } else if (scenario == "automesh.async-shortcut") {
        ensureVertexMode();
        incModalAdd(new AutoMeshBatchWindow());
    } else if (scenario == "simplephysics.runtime") {
        incActivePuppet().enableDrivers = true;
        showPanels("Viewport");
    } else if (scenario.startsWith("depth.") || scenario == "depthbone.refresh-queue") {
        ensureDepthMode();
        showPanels("Viewport", "Tool Settings", "Inspector");
        if (scenario == "depth.bonesource-rotation-grid-ui" ||
            scenario == "depth.bonesource-rotation-path-ui") {
            auto root = new ExDepthRigRoot(incActivePuppet().root);
            root.name = "BoneSource Rotation Root";
            auto bone = new ExDepthBone(root);
            bone.name = "BoneSource Rotation Bone";
            bone.boneId = "RotationBone";
            Node target;
            if (scenario == "depth.bonesource-rotation-grid-ui") {
                auto grid = createSmokeDepthGrid("BoneSource Rotation Grid");
                if (ngRegressionSmokeFailed()) return;
                root.addBoneSource(grid, ExDepthTargetKind.Grid, bone);
                target = grid;
            } else {
                auto path = new ExPathDeformer(incActivePuppet().root);
                path.name = "BoneSource Rotation Path";
                path.rebuffer(Vec2Array([
                    vec2(-20.0f, 0.0f),
                    vec2(0.0f, 50.0f),
                    vec2(20.0f, 100.0f),
                ]));
                path.replaceDepths([0.25f, 0.5f, 0.75f]);
                root.addBoneSource(path, ExDepthTargetKind.Path, bone);
                target = path;
            }
            incActivePuppet().rescanNodes();
            incSelectNode(target);
        } else if (scenario == "depth.edit-live-ui-smoke") {
            auto grid = createSmokeDepthGridWithAxes(
                "DepthEditSmoke:G",
                [-2.0f, -1.0f, 0.0f, 1.0f, 2.0f],
                [-2.0f, -1.0f, 0.0f, 1.0f, 2.0f]);
            if (ngRegressionSmokeFailed()) return;
            auto viewport = new DepthEditViewport();
            viewport.selectionChanged([cast(Node)grid]);
            viewport.present();
            scope(exit) viewport.withdraw();
            auto editor = viewport.getEditor();
            if (editor is null ||
                editor.getTargets().length != 1 ||
                editor.depthViewSession() is null ||
                editor.depthViewSession().targetByGrid(grid.uuid) is null) {
                ngRegressionSmokeFail("DepthEdit smoke failed to initialize shared target view");
                return;
            }
            auto activeEditor = editor.getEditorFor(grid);
            auto targetView = editor.depthViewSession().targetByGrid(grid.uuid);
            if (activeEditor is null || targetView is null || editor.targetViewFor(activeEditor) !is targetView) {
                ngRegressionSmokeFail("DepthEdit smoke failed to bind editor wrapper to shared target view");
                return;
            }
            foreach (mode; [
                DepthToolMode.DirectDepth,
                DepthToolMode.AttachedPoint,
                DepthToolMode.Ring,
                DepthToolMode.Plane,
            ]) {
                viewport.setToolMode(mode);
                auto tool = viewport.activeTool();
                if (viewport.activeToolMode() != mode || tool is null || tool.mode() != mode) {
                    ngRegressionSmokeFail("DepthEdit smoke failed to route tool mode through viewport");
                    return;
                }
            }
            activeEditor.setDepth(0, 0.75f);
            if (targetView.getDepth(0) != 0.75f || activeEditor.copyEditorDepths()[0] != 0.75f) {
                ngRegressionSmokeFail("DepthEdit smoke failed to route direct depth edit through shared target view");
                return;
            }
            DepthBrushSettings brush;
            brush.amount = 0.35f;
            brush.radiusY = 1.4f;
            brush.hardness = 1.0f;
            auto baselineDepths = activeEditor.copyEditorDepths();
            if (!editor.commitOperationAdd(activeEditor, new DepthAttachedPointOperation(12, 0.25f)) ||
                !editor.commitOperationAdd(activeEditor, new DepthRingOperation(vec2(-2.0f, 0.0f), vec2(2.0f, 0.0f), brush)) ||
                !editor.commitOperationAdd(activeEditor, new DepthPlaneOperation(vec2(0.0f, 0.0f), 2.0f, 2.0f, brush))) {
                ngRegressionSmokeFail("DepthEdit smoke failed to add attached, ring, and plane operations");
                return;
            }
            auto operations = editor.copyOperations(activeEditor);
            auto operatedDepths = activeEditor.copyEditorDepths();
            bool depthChanged;
            foreach (i, value; operatedDepths) {
                if (i < baselineDepths.length && value != baselineDepths[i]) {
                    depthChanged = true;
                    break;
                }
            }
            if (operations.length != 3 || !depthChanged || targetView.copyWorkingDepths() != operatedDepths) {
                ngRegressionSmokeFail("DepthEdit smoke failed to apply operation edits through shared target view");
                return;
            }
            DepthCamera3D depthCamera;
            auto renderer = new DepthTargetRenderer();
            auto mesh = renderer.buildMesh(targetView, depthCamera);
            auto lines = renderer.buildGridLines(targetView, depthCamera);
            if (mesh.positions.length != targetView.getVertices().length ||
                mesh.indices.length == 0 ||
                lines.length == 0) {
                ngRegressionSmokeFail("DepthEdit smoke failed to build shared target render geometry");
                return;
            }
        }
    } else if (scenario.startsWith("mesh.") || scenario.startsWith("deform.")) {
        ensureModelMode();
        showPanels("Viewport", "Tool Settings", "Inspector");
    }
}
