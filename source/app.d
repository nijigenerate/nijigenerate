/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
//import std.stdio;
import std.string;
import std.conv : to;
import core.thread : Thread;
import core.time : msecs;
import nijigenerate.core;
import nijigenerate.core.settings;
import nijigenerate.utils.crashdump;
import nijigenerate.panels;
import nijigenerate.panels.resource;
import nijigenerate.windows;
import nijigenerate.windows.autosave : RestoreSaveWindow;
import nijigenerate.windows.inpexport : ExportWindow;
import nijigenerate.windows.settings : SettingsWindow;
import nijigenerate.widgets;
import nijigenerate.widgets.modal : incModalAdd;
import nijigenerate.widgets.mainmenu;
import nijigenerate.actions : Action;
import nijigenerate.core.actionstack;
import nijigenerate.core.shortcut;               // package re-exports base
import nijigenerate.core.shortcut.base : ngLoadShortcutsFromSettings; // load persisted shortcuts
import nijigenerate.core.shortcut.defaults : ngRegisterDefaultShortcuts;
import nijigenerate.commands : ngInitAllCommands; // explicit commands init to avoid ctor cycles
import nijigenerate.commands.depth.bone : ngFlushDepthBoneDirty;
import nijigenerate.core.i18n;
import nijigenerate.io;
import nijigenerate.io.save : incCloseProjectAsk, incSetSaveProjectOnClose;
import nijigenerate.io.autosave;
import nijigenerate.atlas.atlas : incInitAtlassing;
import nijigenerate.ext;
import nijigenerate.windows.flipconfig;
import nijilive;
import nijilive.core.param : Parameter;
import nijilive.core.nodes.common : nlApplyBlendingCapabilities;
import nijigenerate;
version(HaveMCP) import nijigenerate.api.mcp : ngMcpProcessQueue, ngMcpLoadSettings, ngMcpStop;
import nijigenerate.panels.agent : ngAcpStopAll;
import i18n;

version(D_X32) {
    pragma(msg, "nijigenerate does not support compilation on 32 bit platforms");
    static assert(0, "nijigenerate does not support 32-bit builds.");
}

version(Windows) {
    debug {

    } else {
        version(InLite) {   
            // Sorry to the programming gods for this crime
            // phobos will crash in lite mode if this isn't here.
        } else {
            version (LDC) {
                pragma(linkerDirective, "/SUBSYSTEM:WINDOWS");
                static if (__VERSION__ >= 2091)
                    pragma(linkerDirective, "/ENTRY:wmainCRTStartup");
                else
                    pragma(linkerDirective, "/ENTRY:mainCRTStartup");
            }
        }
    }
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

private void incSetupRegressionSmokeScenario(string scenario) {
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

int main(string[] args)
{
    try {
        bool regressionSmoke = args.length >= 2 && args[1] == "--regression-smoke";
        bool regressionComputerUse = false;
        string regressionScenario = regressionSmoke && args.length >= 3 ? args[2] : "";
        int regressionFrames = 6;
        int regressionFrameDelayMs = 0;
        foreach (i, arg; args) {
            if (arg == "--regression-computer-use")
                regressionComputerUse = true;
            if (arg == "--regression-frames" && i + 1 < args.length)
                regressionFrames = args[i + 1].to!int;
            if (arg == "--regression-frame-delay-ms" && i + 1 < args.length)
                regressionFrameDelayMs = args[i + 1].to!int;
        }
        if (regressionFrames < 1)
            regressionFrames = 1;
        if (regressionFrameDelayMs < 0)
            regressionFrameDelayMs = 0;

        installNativeCrashDumpHandler();
        bool backgroundServicesStopped = false;
        void stopBackgroundServices() {
            if (backgroundServicesStopped) return;
            backgroundServicesStopped = true;
            version(HaveMCP) ngMcpStop();
            ngAcpStopAll();
        }
        scope(exit) stopBackgroundServices();
        incSettingsLoad();
        incLocaleInit();
        if (incSettingsCanGet("lang")) {
            string lang = incSettingsGet!string("lang");
            auto entry = incLocaleGetEntryFor(lang);
            if (entry !is null) {
                i18nLoadLanguage(entry.file);
            }
        }

        inSetUpdateBounds(true);

        // Initialize Window and nijilive
        incInitPanels();
        incActionInit();
        incOpenWindow();
        if (regressionSmoke)
            SDL_GL_SetSwapInterval(0);
        bool tripleBufferFallback = incSettingsGet!bool("TripleBufferFallback", nlIsTripleBufferFallbackEnabled());
        nlSetTripleBufferFallback(tripleBufferFallback);
        nlApplyBlendingCapabilities();

        // Initialize node overrides
        incInitExt();

        incInitFlipConfig();

        ngInitResourcePanel();

        // Initialize video exporting
        incInitVideoExport();
        
        // Initialize atlassing
        incInitAtlassing();

        // Initialize default post processing shader
        inPostProcessingAddBasicLighting();

        // Initialize command registries explicitly (avoid module ctor cycles)
        ngInitAllCommands();

        // Register default shortcuts, then load user overrides from settings
        ngRegisterDefaultShortcuts();
        ngLoadShortcutsFromSettings();

        // Start/stop MCP HTTP server based on persisted settings (single read)
        version(HaveMCP) {
            if (!regressionSmoke)
                ngMcpLoadSettings();
        }

        // Open or create project
        if (regressionSmoke) {
            incNewProject();
            incSetupRegressionSmokeScenario(regressionScenario);
        } else if (incSettingsGet!bool("hasDoneQuickSetup", false) && args.length > 1) incOpenProject(args[1]);
        else {
            incNewProject();

            // TODO: Replace with first-time welcome screen
            incPushWindow(new WelcomeWindow());
        }

        version(InNightly) incModalAdd(
            new Nagscreen(
                _("Warning!"), 
                _("You're running a nightly build of nijigenerate!\nnijigenerate may crash unexpectedly and you will likely encounter bugs.\nMake sure to save and back up your work often!"),
                5
            )
        );
        
        // Update loop
        int regressionFrame;
        while(!incIsCloseRequested()) {
            if (regressionSmoke && !regressionComputerUse)
                incUpdateNoEv();
            else
                incUpdate();
            if (regressionSmoke && ++regressionFrame >= regressionFrames)
                break;
            if (regressionSmoke && regressionFrameDelayMs > 0)
                Thread.sleep(regressionFrameDelayMs.msecs);
        }
        if (regressionSmoke) {
            stopBackgroundServices();
            return 0;
        }
        incSettingsSave();
        stopBackgroundServices();
        incFinalize();
    } catch(Throwable ex) {
        debug {
            version(Windows) {
                crashdump(ex);
            } else {
                throw ex;
            }
        } else {
            crashdump(ex);
        }
    }
    return 0;
}

/**
    Update
*/
void incUpdate() {

    // Update nijilive
    incAnimationUpdate();
    inUpdate();

    incCheckAutosave();

    // Begin IMGUI loop
    incBeginLoop();
        if (incShouldProcess()) {

            incHandleShortcuts();
            // Process any queued MCP commands on the main thread
            version(HaveMCP) ngMcpProcessQueue();
            incMainMenu();

            incUpdatePanels();
            incUpdateWindows();
            incStatusUpdate();
            ngFlushDepthBoneDirty();
        }
    incEndLoop();
}

/**
    Update without any event polling
*/
void incUpdateNoEv() {

    // Update nijilive
    incAnimationUpdate();
    inUpdate();
    
    // Begin IMGUI loop
    incBeginLoopNoEv();
        if (incShouldProcess()) {

            incHandleShortcuts();
            incMainMenu();

            incUpdatePanels();
            incUpdateWindows();
            incStatusUpdate();
            ngFlushDepthBoneDirty();
        }
    incEndLoop();
}
