module nijigenerate.commands.puppet.base;

import nijigenerate.windows;
import nijigenerate.widgets;
import nijigenerate.panels;
import nijigenerate.core;
import nijigenerate.core.input;
import nijigenerate.utils.link;
import nijigenerate.config;
import nijigenerate.io.autosave;
import nijigenerate;
import nijilive;
import nijilive.core.dbg;
import tinyfiledialogs;
import i18n;
import nijigenerate.ext;

import std.string;
//import std.stdio;
import std.path;

    bool dbgShowStyleEditor;
    bool dbgShowDebugger;
    bool dbgShowMetrics;
    bool dbgShowStackTool;

    void fileNew() {
        incNewProject();
    }

    void fileOpen() {
        const TFD_Filter[] filters = [
            { ["*.inx"], "nijigenerate Project (*.inx)" }
        ];

        string file = incShowOpenDialog(filters, _("Open..."));
        if (file) incOpenProject(file);
    }

    void fileSave() {
        incPopWelcomeWindow();

        // If a projeect path is set then the user has opened or saved
        // an existing file, we should just override that
        if (incProjectPath.length > 0) {
            // TODO: do backups on every save?

            incSaveProject(incProjectPath);
        } else {
            const TFD_Filter[] filters = [
                { ["*.inx"], "nijigenerate Project (*.inx)" }
            ];

            string file = incShowSaveDialog(filters, "", _("Save..."));
            if (file) incSaveProject(file);
        }
    }

    void fileSaveAs() {
        incPopWelcomeWindow();
        const TFD_Filter[] filters = [
            { ["*.inx"], "nijigenerate Project (*.inx)" }
        ];

        string fname = incProjectPath().length > 0 ? incProjectPath : "";
        string file = incShowSaveDialog(filters, fname, _("Save As..."));
        if (file) incSaveProject(file);
    }