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
import nijigenerate.core;
import nijigenerate.core.i18n;
import nijigenerate.io;
import nijigenerate.io.autosave;
import std.string;
import nijigenerate.utils.link;
import i18n;
import inmath;

bool incIsSettingsOpen;

enum SettingsPane : string {
    LookAndFeel = "Look and Feel",
    Viewport = "Viewport",
    Accessibility = "Accessbility",
    FileHandling = "File Handling"
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
        incSettingsSave();
        incIsSettingsOpen = false;
    }

public:
    this() {
        super(_("Settings"));
        targetUIScale = incGetUIScale();
        tmpUIScale = cast(int)(incGetUIScale()*100);
    }
}
