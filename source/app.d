/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
//import std.stdio;
import std.string;
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
import nijigenerate.core.i18n;
import nijigenerate.io;
import nijigenerate.io.autosave;
import nijigenerate.atlas.atlas : incInitAtlassing;
import nijigenerate.ext;
import nijigenerate.windows.flipconfig;
import nijilive;
import nijigenerate;
import nijigenerate.api.mcp : ngMcpProcessQueue, ngMcpLoadSettings;
import i18n;

version(D_X32) {
    pragma(msg, "nijigenerate does not support compilation on 32 bit platforms");
    static assert(0, "😎👉👉 no");
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
    try {
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
        ngMcpLoadSettings();

        // Open or create project
        if (incSettingsGet!bool("hasDoneQuickSetup", false) && args.length > 1) incOpenProject(args[1]);
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
        while(!incIsCloseRequested()) {
            incUpdate();
        }
        incSettingsSave();
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
            ngMcpProcessQueue();
            incMainMenu();

            incUpdatePanels();
            incUpdateWindows();
            incStatusUpdate();
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
        }
    incEndLoop();
}
