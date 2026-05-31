/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
//import std.stdio;
import std.string;
version(RegressionSmoke) {
import core.thread : Thread;
import core.time : msecs;
}
import nijigenerate.core;
import nijigenerate.core.settings;
import nijigenerate.utils.crashdump;
import nijigenerate.panels;
import nijigenerate.panels.resource;
import nijigenerate.windows;
import nijigenerate.widgets;
import nijigenerate.widgets.mainmenu;
import nijigenerate.core.actionstack;
import nijigenerate.core.shortcut;               // package re-exports base
import nijigenerate.core.shortcut.base : ngLoadShortcutsFromSettings; // load persisted shortcuts
import nijigenerate.core.shortcut.defaults : ngRegisterDefaultShortcuts;
import nijigenerate.commands : ngInitAllCommands; // explicit commands init to avoid ctor cycles
import nijigenerate.commands.depth.bone : ngFlushDepthBoneDirty;
import nijigenerate.core.i18n;
import nijigenerate.io;
import nijigenerate.io.autosave;
import nijigenerate.atlas.atlas : incInitAtlassing;
import nijigenerate.ext;
import nijigenerate.windows.flipconfig;
import nijilive;
import nijilive.core.nodes.common : nlApplyBlendingCapabilities;
import nijigenerate;
version(RegressionSmoke) import nijigenerate.regression_smoke : ngParseRegressionSmokeOptions, ngSetupRegressionSmokeScenario;
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

int main(string[] args)
{
    if (args.length >= 3 && args[1] == "--crash-notify") {
        incSettingsLoad();
        incLocaleInitFromSettings();
        notifyCrashUser(args[2]);
        return 0;
    }

    try {
        version(RegressionSmoke) {
            auto regressionSmoke = ngParseRegressionSmokeOptions(args);
        } else {
            enum regressionSmoke = false;
        }

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
        incLocaleInitFromSettings();

        inSetUpdateBounds(true);

        // Initialize Window and nijilive
        incInitPanels();
        incActionInit();
        incOpenWindow();
        version(RegressionSmoke) if (regressionSmoke.enabled)
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
            version(RegressionSmoke) {
                if (!regressionSmoke.enabled)
                    ngMcpLoadSettings();
            } else {
                ngMcpLoadSettings();
            }
        }

        // Open or create project
        version(RegressionSmoke) {
            if (regressionSmoke.enabled) {
                incNewProject();
                ngSetupRegressionSmokeScenario(regressionSmoke.scenario);
            } else if (incSettingsGet!bool("hasDoneQuickSetup", false) && args.length > 1) incOpenProject(args[1]);
            else {
                incNewProject();

                // TODO: Replace with first-time welcome screen
                incPushWindow(new WelcomeWindow());
            }
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
        version(RegressionSmoke) int regressionFrame;
        while(!incIsCloseRequested()) {
            version(RegressionSmoke) {
                if (regressionSmoke.enabled && !regressionSmoke.computerUse)
                    incUpdateNoEv();
                else
                    incUpdate();
                if (regressionSmoke.enabled && ++regressionFrame >= regressionSmoke.frames)
                    break;
                if (regressionSmoke.enabled && regressionSmoke.frameDelayMs > 0)
                    Thread.sleep(regressionSmoke.frameDelayMs.msecs);
            } else {
                incUpdate();
            }
        }
        version(RegressionSmoke) if (regressionSmoke.enabled) {
            stopBackgroundServices();
            return 0;
        }
        incSettingsSave();
        stopBackgroundServices();
        incFinalize();
    } catch(Throwable ex) {
        debug {
            crashdump(ex);
            version(Windows) {
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
