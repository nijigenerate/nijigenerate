module nijigenerate.regression_smoke;

version (RegressionSmoke):

import std.conv : to;
import std.string : startsWith;

import nijigenerate.actions : Action;
import nijigenerate.commands.depth.bone : ngFlushDepthBoneDirty;
import nijigenerate.core;
import nijigenerate.core.actionstack;
import nijigenerate.io.save : incCloseProjectAsk, incSetSaveProjectOnClose;
import nijigenerate.panels;
import nijigenerate.panels.resource;
import nijigenerate.project : EditMode, incActivePuppet, incSetEditMode;
import nijigenerate.widgets.modal : incModalAdd;
import nijigenerate.windows;
import nijigenerate.windows.autosave : RestoreSaveWindow;
import nijigenerate.windows.inpexport : ExportWindow;
import nijigenerate.windows.settings : SettingsWindow;
import nijilive;
import nijilive.core.param : Parameter;

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
    } else if (scenario.startsWith("mesh.") || scenario.startsWith("deform.")) {
        ensureModelMode();
        showPanels("Viewport", "Tool Settings", "Inspector");
    }
}
