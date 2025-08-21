/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/

module nijigenerate.windows.settings;
import nijigenerate.viewport.base;
import nijigenerate.windows.base;
import nijigenerate.widgets;
import nijigenerate.core.settings;
import nijigenerate.core.window;
import nijigenerate.core.dpi;
import nijigenerate.core.actionstack;
import nijigenerate.core.i18n;
import nijigenerate.core.shortcut; // shortcut customization
import nijigenerate.io;
import nijigenerate.io.autosave;
import std.string;
import nijigenerate.utils.link;
import i18n;
import inmath;
import nijigenerate.commands; // list command maps per category

bool incIsSettingsOpen;

enum SettingsPane : string {
    LookAndFeel = "Look and Feel",
    Viewport = "Viewport",
    Accessibility = "Accessbility",
    FileHandling = "File Handling",
    Shotcuts = "Shotcuts",
}

/**
    Settings window
*/
class SettingsWindow : Window {
private:
    bool generalTabOpen = true;
    bool otherTabOpen = true;
    bool changesRequiresRestart;

    int tmpUIScale;
    float targetUIScale;

    SettingsPane settingsPane = SettingsPane.LookAndFeel;

    // Shortcut capture state
    bool capturingShortcut = false;
    Command capturingCommand;
    bool capturingRepeat = false;
    string capturingPreview;

    void beginSection(const(char)* title) {
        incBeginCategory(title, IncCategoryFlags.NoCollapse);
        incDummy(ImVec2(0, 4));
    }
    
    void endSection() {
        incDummy(ImVec2(0, 4));
        incEndCategory();
    }
protected:
    override
    void onBeginUpdate() {
        flags |= ImGuiWindowFlags.NoSavedSettings;
        incIsSettingsOpen = true;
        
        ImVec2 wpos = ImVec2(
            igGetMainViewport().Pos.x+(igGetMainViewport().Size.x/2),
            igGetMainViewport().Pos.y+(igGetMainViewport().Size.y/2),
        );

        ImVec2 uiSize = ImVec2(
            640, 
            480
        );

        igSetNextWindowPos(wpos, ImGuiCond.Appearing, ImVec2(0.5, 0.5));
        igSetNextWindowSize(uiSize, ImGuiCond.Appearing);
        igSetNextWindowSizeConstraints(uiSize, ImVec2(float.max, float.max));
        super.onBeginUpdate();
    }

    override
    void onUpdate() {
        float availX = incAvailableSpace().x;

        // Sidebar
        if (igBeginChild("SettingsSidebar", ImVec2(availX/3.5, -28), true)) {
            igPushTextWrapPos(128);
                if (igSelectable(__("Look and Feel"), settingsPane == SettingsPane.LookAndFeel)) {
                    settingsPane = SettingsPane.LookAndFeel;
                }
                
                if (igSelectable(__("Viewport"), settingsPane == SettingsPane.Viewport)) {
                    settingsPane = SettingsPane.Viewport;
                }
                
                if (igSelectable(__("Accessbility"), settingsPane == SettingsPane.Accessibility)) {
                    settingsPane = SettingsPane.Accessibility;
                }

                if (igSelectable(__("File Handling"), settingsPane == SettingsPane.FileHandling)) {
                    settingsPane = SettingsPane.FileHandling;
                }

                if (igSelectable(__("Shotcuts"), settingsPane == SettingsPane.Shotcuts)) {
                    settingsPane = SettingsPane.Shotcuts;
                }
            igPopTextWrapPos();
        }
        igEndChild();
        
        // Nice spacing
        igSameLine(0, 4);

        // Contents
        if (igBeginChild("SettingsContent", ImVec2(0, -28), true)) {
            availX = incAvailableSpace().x;

            // Start settings panel elements
            igPushItemWidth(availX/2);
                switch(settingsPane) {
                    case SettingsPane.LookAndFeel:

                        beginSection(__("Look and Feel"));
                            if(igBeginCombo(__("Color Theme"), incGetDarkMode() ? __("Dark") : __("Light"))) {
                                if (igSelectable(__("Dark"), incGetDarkMode())) incSetDarkMode(true);
                                if (igSelectable(__("Light"), !incGetDarkMode())) incSetDarkMode(false);

                                igEndCombo();
                            }
                            
                            import std.string : toStringz;
                            if(igBeginCombo(__("Language"), incLocaleCurrentName().toStringz)) {
                                if (igSelectable("English")) {
                                    incLocaleSet(null);
                                    changesRequiresRestart = true;
                                }
                                foreach(entry; incLocaleGetEntries()) {
                                    if (igSelectable(entry.humanNameC)) {
                                        incLocaleSet(entry.code);
                                        changesRequiresRestart = true;
                                    }
                                }
                                igEndCombo();
                            }

                            version (UseUIScaling) {
                                version(OSX) {

                                    // macOS follows Retina scaling.
                                } else {
                                    if (igInputInt(__("UI Scale"), &tmpUIScale, 25, 50, ImGuiInputTextFlags.EnterReturnsTrue)) {
                                        tmpUIScale = clamp(tmpUIScale, 100, 200);
                                        incSetUIScale(cast(float)tmpUIScale/100.0);
                                    }
                                }
                            }
                        endSection();

                        beginSection(__("Undo History"));
                            int maxHistory = cast(int)incActionGetUndoHistoryLength();
                            if (igDragInt(__("Max Undo History"), &maxHistory, 1, 1, 1000, "%d")) {
                                incActionSetUndoHistoryLength(maxHistory);
                            }
                        endSection();

                        version(linux) {
                            beginSection(__("Linux Tweaks"));
                                bool disableCompositor = incSettingsGet!bool("DisableCompositor");
                                if (ngCheckbox(__("Disable Compositor"), &disableCompositor)) {
                                    incSettingsSet("DisableCompositor", disableCompositor);
                                }
                            endSection();
                        }
                        break;
                    case SettingsPane.Shotcuts:
                        drawShotcutsPane();
                        break;
                    case SettingsPane.Accessibility:
                        beginSection(__("Accessibility"));
                            bool disableCompositor = incSettingsGet!bool("useOpenDyslexic");
                            if (ngCheckbox(__("Use OpenDyslexic Font"), &disableCompositor)) {
                                incSettingsSet("useOpenDyslexic", disableCompositor);
                                changesRequiresRestart = true;
                            }
                            incTooltip(_("Use the OpenDyslexic font for Latin text characters."));
                        endSection();
                        break;
                    case SettingsPane.FileHandling:
                        beginSection(__("Autosaves"));
                            bool autosaveEnabled = incGetAutosaveEnabled();
                            if (ngCheckbox(__("Enable Autosaves"), &autosaveEnabled)) {
                                incSetAutosaveEnabled(autosaveEnabled);
                            }

                            int autosaveFreq = incGetAutosaveInterval();
                            if (igInputInt(__("Save Interval (Minutes)"), &autosaveFreq, 5, 15, ImGuiInputTextFlags.EnterReturnsTrue)) {
                                incSetAutosaveInterval(autosaveFreq);
                            }

                            int saveFileLimit = incGetAutosaveFileLimit();
                            if (igInputInt(__("Maximum Autosaves"), &saveFileLimit, 1, 5, ImGuiInputTextFlags.EnterReturnsTrue)) {
                                incSetAutosaveFileLimit(saveFileLimit);
                            }
                        endSection();

                        beginSection(__("Preserve Imported File Folder Structure"));
                            string[string] configShowing = [
                                "Ask": "Always Ask",
                                "Preserve": "Preserve",
                                "NotPreserve": "Not Preserve"
                            ];

                            string selected = configShowing[incGetKeepLayerFolder()];
                            if(igBeginCombo(__("Preserve Folder Structure"), __(selected))) {
                                if (igSelectable(__("Always Ask"), incSettingsGet!string("KeepLayerFolder") == "Ask")) incSetKeepLayerFolder("Ask");
                                if (igSelectable(__("Preserve"), incSettingsGet!string("KeepLayerFolder") == "Preserve")) incSetKeepLayerFolder("Preserve");
                                if (igSelectable(__("Not Preserve"), incSettingsGet!string("KeepLayerFolder") == "NotPreserve")) incSetKeepLayerFolder("NotPreserve");

                                igEndCombo();
                            }

                        endSection();
                        break;
                    case SettingsPane.Viewport:
                        beginSection(__("Viewport"));
                            string[string] configShowing = [
                                "ScreenCenter": "To Screen Center",
                                "MousePosition": "To Mouse Position"
                            ];

                            string selected = configShowing[incGetViewportZoomMode()];
                            if(igBeginCombo(__("Zoom Mode"), __(selected))) {
                                if (igSelectable(__("To Screen Center"), incSettingsGet!string("ViewportZoomMode") == "ScreenCenter")) incSetViewportZoomMode("ScreenCenter");
                                if (igSelectable(__("To Mouse Position"), incSettingsGet!string("ViewportZoomMode") == "MousePosition")) incSetViewportZoomMode("MousePosition");

                                igEndCombo();
                            }

                            float zoomSpeed = cast(float)incGetViewportZoomSpeed();
                            if (igDragFloat(__("Zoom Speed"), &zoomSpeed, 0.1, 1, 50, "%f")) {
                                incSetViewportZoomSpeed(zoomSpeed);
                            }
                        endSection();
                        break;
                    default:
                        incLabelOver(_("No settings for this category."), ImVec2(0, 0), true);
                        break;
                }
            igPopItemWidth();
        }
        igEndChild();

        // Bottom buttons
        if (igBeginChild("SettingsButtons", ImVec2(0, 0), false, ImGuiWindowFlags.NoScrollbar)) {
            if (changesRequiresRestart) {
                igPushTextWrapPos(256+128);
                    incTextColored(
                        ImVec4(0.8, 0.2, 0.2, 1), 
                        _("nijigenerate needs to be restarted for some changes to take effect.")
                    );
                igPopTextWrapPos();
                igSameLine(0, 0);
            }
            incDummy(ImVec2(-64, 0));
            igSameLine(0, 0);

            if (incButtonColored(__("Done"), ImVec2(64, 24))) {
                this.close();
            }
        }
        igEndChild();
    }

    override
    void onClose() {
        // Persist current shortcut registry to settings before saving
        import nijigenerate.core.shortcut.base : ngSaveShortcutsToSettings;
        ngSaveShortcutsToSettings();
        incSettingsSave();
        incIsSettingsOpen = false;
    }

public:
    this() {
        super(_("Settings"));
        targetUIScale = incGetUIScale();
        tmpUIScale = cast(int)(incGetUIScale()*100);
        // Load persisted shortcuts when opening settings
        import nijigenerate.core.shortcut : ngLoadShortcutsFromSettings;
        ngLoadShortcutsFromSettings();
    }

protected:
    // Render a simple 2-column table for a commands AA (enum => Command)
    void renderCommandTable(alias CmdsAA)(const(char)* title)
    {
        // Collapsible category for each command group
        if (incBeginCategory(title)) {
            enum ImGuiTableFlags flags = ImGuiTableFlags.Borders | ImGuiTableFlags.RowBg | ImGuiTableFlags.SizingStretchSame;
            if (igBeginTable(title, 3, flags, ImVec2(0, 0), 0.0)) {
                igTableSetupColumn(__("Action"), ImGuiTableColumnFlags.None, 0.6, 0);
                igTableSetupColumn(__("Shortcut"), ImGuiTableColumnFlags.None, 0.2, 1);
                igTableSetupColumn(__("Edit"), ImGuiTableColumnFlags.None, 0.2, 2);
                igTableHeadersRow();

                foreach (k, cmd; CmdsAA) {
                    igTableNextRow(ImGuiTableRowFlags.None, 0.0);
                    igTableSetColumnIndex(0);
                    auto lbl = cmd.label();
                    incText(lbl);

                    igTableSetColumnIndex(1);
                    import nijigenerate.core.shortcut : ngShortcutFor;
                    import nijigenerate.core.input : ngModifierLabelCtrl, ngModifierLabelSuper;
                    auto sc = ngShortcutFor(cmd);
                    // On macOS, if swap is enabled, show swapped labels for stored shortcuts
                    version (OSX) {
                        if (incSettingsGet!bool("MacSwapCmdCtrl", false) && sc.length) {
                            import std.string : replace;
                            enum placeholder = "__CTRL_TMP__";
                            auto lblCtrl = ngModifierLabelCtrl();
                            auto lblSuper = ngModifierLabelSuper();
                            sc = sc.replace(lblCtrl, placeholder)
                                   .replace(lblSuper, lblCtrl)
                                   .replace(placeholder, lblSuper);
                        }
                    }
                    if (capturingShortcut && (capturingCommand is cmd)) {
                        // Live preview while capturing
                        incTextColored(ImVec4(0.9, 0.7, 0.2, 1), capturingPreview.length ? capturingPreview : _("Press keys..."));
                    } else {
                        incText(sc.length ? sc : _("<none>"));
                    }

                    igTableSetColumnIndex(2);
                    auto setLbl = __("Set");
                    import std.conv : to;
                    auto _idStr = to!string(k);
                    igPushID(_idStr.toStringz);
                    if (incButtonColored(setLbl, ImVec2(0, 0))) {
                        capturingShortcut = true;
                        capturingCommand = cmd;
                        capturingPreview = "";
                        capturingRepeat = false;
                    }
                    igSameLine(0, 4);
                    auto clrLbl = __("Clear");
                    if (incButtonColored(clrLbl, ImVec2(0, 0))) {
                        ngClearShortcutFor(cmd);
                        import nijigenerate.core.shortcut : ngSaveShortcutsToSettings;
                        ngSaveShortcutsToSettings();
                    }
                    igPopID();
                }
                igEndTable();
            }
        }
        incEndCategory();
    }

    void captureShortcutUI()
    {
        if (!capturingShortcut) return;

        // Capture input state and form a shortcut string from pressed key + modifiers
        auto io = igGetIO();

        // UI: small inline capture toolbar
        igSeparator();
        incTextColored(ImVec4(0.9, 0.7, 0.2, 1), _("Capturing shortcut..."));
        igSameLine(0, 8);
        if (ngCheckbox(__("Repeat"), &capturingRepeat)) {
            // no-op, state toggled
        }
        igSameLine(0, 8);
        if (incButtonColored(__("Cancel"), ImVec2(0, 0))) {
            capturingShortcut = false;
            capturingCommand = null;
            capturingPreview = "";
        }

        // Recognized keys to bind
        static immutable string[] keys = [
            "A","B","C","D","E","F","G","H","I","J","K","L","M",
            "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
            "0","1","2","3","4","5","6","7","8","9",
            "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
            "Left","Right","Up","Down"
        ];

        import nijigenerate.core.input : incKeyScancode, ngFormatShortcut;

        string pressedKey;
        foreach (k; keys) {
            auto sc = incKeyScancode(k);
            if (sc != ImGuiKey.None && igIsKeyPressed(sc, false)) {
                pressedKey = k;
                break;
            }
        }

        // Update preview text even when no confirm yet
        string preview = ngFormatShortcut(io.KeyCtrl, io.KeyAlt, io.KeyShift, io.KeySuper, pressedKey);
        capturingPreview = preview;

        // Confirm on Enter or on key press with any modifier
        if (pressedKey.length) {
            if (capturingCommand !is null) {
                // Commit shortcut
                ngClearShortcutFor(capturingCommand);
                ngRegisterShortcut(preview, capturingCommand, capturingRepeat);
                import nijigenerate.core.shortcut : ngSaveShortcutsToSettings;
                ngSaveShortcutsToSettings();
            }
            capturingShortcut = false;
            capturingCommand = null;
            capturingPreview = "";
        }
        igSeparator();
    }

    void drawShotcutsPane() {
        beginSection(__("Shotcuts"));
            incText(_("Assign shortcuts to commands. Click Set and press keys. Use Clear to remove a binding."));

            // macOS-specific option: swap Command and Control in shortcuts
            version (OSX) {
                bool swapCmdCtrl = incSettingsGet!bool("MacSwapCmdCtrl", false);
                if (ngCheckbox(__("Swap Command and Control"), &swapCmdCtrl)) {
                    // Persist and take effect immediately for detection/preview
                    incSettingsSet("MacSwapCmdCtrl", swapCmdCtrl);
                }
                incTooltip(_("When enabled, Command () and Control () are treated as swapped for shortcuts."));
                incDummy(ImVec2(0, 4));
            }
        endSection();

        // Inline capture UI (if active)
        captureShortcutUI();

        // Group by command categories in a scrollable child so header stays visible
        if (igBeginChild("ShortcutsTables", ImVec2(0, 0), true)) {
            renderCommandTable!(nijigenerate.commands.puppet.file.commands)(__("File"));
            renderCommandTable!(nijigenerate.commands.puppet.edit.commands)(__("Edit"));
            renderCommandTable!(nijigenerate.commands.puppet.view.commands)(__("View"));
            renderCommandTable!(nijigenerate.commands.puppet.tool.commands)(__("Tools"));
            renderCommandTable!(nijigenerate.commands.viewport.control.commands)(__("Viewport"));
            renderCommandTable!(nijigenerate.commands.viewport.palette.commands)(__("Palette"));
            renderCommandTable!(nijigenerate.commands.node.node.commands)(__("Node"));
            renderCommandTable!(nijigenerate.commands.inspector.apply_node.commands)(__("Inspector"));
            // Node add/insert (dynamic by node type)
            renderCommandTable!(nijigenerate.commands.node.dynamic.addNodeCommands)(__("Add Node"));
            renderCommandTable!(nijigenerate.commands.node.dynamic.insertNodeCommands)(__("Insert Node"));
            renderCommandTable!(nijigenerate.commands.binding.binding.commands)(__("Binding"));
            renderCommandTable!(nijigenerate.commands.parameter.param.commands)(__("Parameter"));
            renderCommandTable!(nijigenerate.commands.parameter.paramedit.commands)(__("Parameter Edit"));
            renderCommandTable!(nijigenerate.commands.parameter.animedit.commands)(__("Animation Edit"));
            renderCommandTable!(nijigenerate.commands.parameter.group.commands)(__("Parameter Group"));

            // Mesh editor tool modes (dynamically generated per mode)
            renderCommandTable!(nijigenerate.commands.mesheditor.tool.selectToolModeCommands)(__("Mesh Editor Tools"));

            // Panels (dynamically generated per panel)
            renderCommandTable!(nijigenerate.commands.view.panel.togglePanelCommands)(__("Panels"));

            // Convert Node To (dynamic per destination type; availability depends on selection)
            renderCommandTable!(nijigenerate.commands.node.dynamic.convertNodeCommands)(__("Convert Node"));
        }
        igEndChild();
    }
}
