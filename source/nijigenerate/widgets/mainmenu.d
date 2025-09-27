/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.widgets.mainmenu;

import nijigenerate.commands.puppet.base;
import nijigenerate.commands;

import nijigenerate.windows;
import nijigenerate.widgets;
import nijigenerate.panels;
import nijigenerate.core;
import nijigenerate.core.input;
import nijigenerate.utils.link;
import nijigenerate.config;
import nijigenerate.io.autosave;
import nijigenerate.io.save;
import nijigenerate;
import nijilive;
import nijilive.core.dbg;
import nijigenerate.viewport.common.mesheditor.brushstate;

import i18n;
import nijigenerate.ext;
import nijigenerate.core.logo;

import std.string;
import std.format;
//import std.stdio;
import std.path;

void incMainMenu() {
    auto io = igGetIO();
        Context ctx = new Context();
        ctx.puppet = incActivePuppet();
        if (incSelectedNodes().length > 0)
            ctx.nodes = incSelectedNodes();
    

        if (!incSettingsGet("hasDoneQuickSetup", false)) igBeginDisabled();

        if(igBeginMainMenuBar()) {
                
            ImVec2 pos;
            igGetCursorPos(&pos);
            igSetCursorPos(ImVec2(pos.x-(igGetStyle().WindowPadding.x/2), pos.y));

            ImVec2 avail;
            igGetContentRegionAvail(&avail);
                igImage(
                    cast(void*)incGetLogoI2D().getTextureId(), 
                    ImVec2(avail.y*2, avail.y*2), 
                    ImVec2(0, 0), ImVec2(1, 1), 
                    ImVec4(1, 1, 1, 1), 
                    ImVec4(0, 0, 0, 0)
                );
                
                igSeparator();

                if (igBeginMenu(__("File"), true)) {
                    ngMenuItemFor!(FileCommand.NewFile)(ctx);

                    ngMenuItemFor!(FileCommand.ShowOpenFileDialog)(ctx);

                    string[] prevProjects = incGetPrevProjects();
                    AutosaveRecord[] prevAutosaves = incGetPrevAutosaves();
                    if (igBeginMenu(__("Recent"), prevProjects.length > 0)) {
                        import std.path : baseName;
                        if (igBeginMenu(__("Autosaves"), prevAutosaves.length > 0)) {
                            foreach(saveRecord; prevAutosaves) {
                                auto autosavePath = saveRecord.autosavePath.baseName.toStringz;
                                if (igMenuItem(autosavePath, "", false, true)) {
                                    incOpenProject(
                                        saveRecord.mainsavePath,
                                        saveRecord.autosavePath
                                    );
                                }
                                incTooltip(saveRecord.autosavePath);
                            }
                            igEndMenu();
                        }

                        foreach(project; incGetPrevProjects) {
                            if (igMenuItem(project.baseName.toStringz, "", false, true)) {
                                incOpenProject(project);
                            }
                            incTooltip(project);
                        }
                        igEndMenu();
                    }
                    
                    ngMenuItemFor!(FileCommand.ShowSaveFileDialog)(ctx);
                    
                    ngMenuItemFor!(FileCommand.ShowSaveFileAsDialog)(ctx);

                    if (igBeginMenu(__("Import"), true)) {
                        ngMenuItemFor!(FileCommand.ShowImportPSDDialog)(ctx);
                        incTooltip(_("Import a standard Photoshop PSD file."));
                        ngMenuItemFor!(FileCommand.ShowImportKRADialog)(ctx);
                        incTooltip(_("Import a standard Krita KRA file."));

                        ngMenuItemFor!(FileCommand.ShowImportINPDialog)(ctx);
                        incTooltip(_("Import existing puppet file, editing options limited"));

                        ngMenuItemFor!(FileCommand.ShowImportImageFolderDialog)(ctx);
                        incTooltip(_("Supports PNGs, TGAs and JPEGs."));
                        igEndMenu();
                    }
                    if (igBeginMenu(__("Merge"), true)) {
                        ngMenuItemFor!(FileCommand.ShowMergePSDDialog)(ctx);
                        incTooltip(_("Merge layers from Photoshop document"));

                        ngMenuItemFor!(FileCommand.ShowMergeKRADialog)(ctx);
                        incTooltip(_("Merge layers from Krita document"));

                        ngMenuItemFor!(FileCommand.ShowMergeImageFileDialog)(ctx);
                        incTooltip(_("Merges (adds) selected image files to project"));

                        ngMenuItemFor!(FileCommand.ShowMergeINPDialog)(ctx);
                        incTooltip(_("Merge another nijigenerate project in to this one"));
                        
                        igEndMenu();
                    }

                    if (igBeginMenu(__("Export"), true)) {
                        ngMenuItemFor!(FileCommand.ShowExportToINPDialog)(ctx);
                        if (igBeginMenu(__("Image"), true)) {
                            ngMenuItemFor!(FileCommand.ShowExportToPNGDialog)(ctx);

                            ngMenuItemFor!(FileCommand.ShowExportToJpegDialog)(ctx);

                            ngMenuItemFor!(FileCommand.ShowExportToTGADialog)(ctx);

                            igEndMenu();
                        }
                        ngMenuItemFor!(FileCommand.ShowExportToVideoDialog)(ctx, false, incVideoCanExport());
                        igEndMenu();
                    }

                    // Close Project option
                    ngMenuItemFor!(FileCommand.CloseProject)(ctx);

                    // Quit option
                    if (igMenuItem(__("Quit"), "Alt+F4", false, true)) incExitSaveAsk();
                    igEndMenu();
                }
                
                if (igBeginMenu(__("Edit"), true)) {
                    ngMenuItemFor!(EditCommand.Undo)(ctx, false, incActionCanUndo());
                    ngMenuItemFor!(EditCommand.Redo)(ctx, false, incActionCanRedo());
                    
                    igSeparator();
                    // Enable via Command.runnable; no hard-coded enabled flags
                    ngMenuItemFor!(NodeCommand.CutNode)(ctx);
                    ngMenuItemFor!(NodeCommand.CopyNode)(ctx);
                    ngMenuItemFor!(NodeCommand.PasteNode)(ctx);

                    igSeparator();
                    ngMenuItemFor!(EditCommand.ShowSettingsWindow)(ctx);
                    
                    debug {
                        igSpacing();
                        igSpacing();

                        igTextColored(ImVec4(0.7, 0.5, 0.5, 1), __("ImGui Debugging"));

                        igSeparator();
                        if(igMenuItem(__("Style Editor"), "", dbgShowStyleEditor, true)) dbgShowStyleEditor = !dbgShowStyleEditor;
                        if(igMenuItem(__("ImGui Debugger"), "", dbgShowDebugger, true)) dbgShowDebugger = !dbgShowDebugger;
                        if(igMenuItem(__("ImGui Metrics"), "", dbgShowMetrics, true)) dbgShowMetrics = !dbgShowMetrics;
                        if(igMenuItem(__("ImGui Stack Tool"), "", dbgShowStackTool, true)) dbgShowStackTool = !dbgShowStackTool;
                    }
                    igEndMenu();
                }

                if (igBeginMenu(__("View"), true)) {
                    ngMenuItemFor!(ViewCommand.SetDefaultLayout)(ctx);
                    igSeparator();

                    // Spacing
                    igSpacing();
                    igSpacing();

                    igTextColored(ImVec4(0.7, 0.5, 0.5, 1), __("Panels"));
                    igSeparator();

                    foreach(panel; incPanels) {
                        // Skip panels that'll always be visible
                        if (panel.alwaysVisible) continue;
                        bool enabled = panel.isActive();
                        // Use dynamic command instance
                        auto cmdInst = ensureTogglePanelCommand(panel);
                        auto lbl = cmdInst.label();
                        // Show shortcut hint if any
                        import nijigenerate.core.shortcut : ngShortcutFor;
                        auto sc = ngShortcutFor(cmdInst);
                        const(char)* pShortcut = sc.length ? sc.toStringz : null;
                        if (igMenuItem(lbl.toStringz, pShortcut, panel.visible, enabled)) {
                            cmdInst.run(ctx);
                        }
                        if (!enabled) {
                            incTooltip(_("Panel is not visible in current edit mode."));
                        }
                    }

                    // Spacing
                    igSpacing();
                    igSpacing();

                    igTextColored(ImVec4(0.7, 0.5, 0.5, 1), __("Configuration"));

                    // Opens the directory where configuration resides in the user's file browser.
                    if (igMenuItem(__("Open Configuration Folder"), null, false, true)) {
                        incOpenLink(incGetAppConfigPath());
                    }

                    // Spacing
                    igSpacing();
                    igSpacing();
                    
                    
                    igTextColored(ImVec4(0.7, 0.5, 0.5, 1), __("Extras"));

                    igSeparator();
                    ngMenuItemFor!(ViewCommand.ShowSaveScreenshotDialog)(ctx);
                    incTooltip(_("Saves screenshot as PNG of the editor framebuffer."));
                    ngMenuItemFor!(ViewCommand.ShowStatusForNerds)(ctx, incShowStatsForNerds, true);


                    igEndMenu();
                }

                if (igBeginMenu(__("Tools"), true)) {

                    igTextColored(ImVec4(0.7, 0.5, 0.5, 1), __("Puppet Data"));
                    igSeparator();

                    // Opens the directory where configuration resides in the user's file browser.
                    ngMenuItemFor!(ToolCommand.ShowImportSessionDataDialog)(ctx);
                    incTooltip(_("Imports tracking data from an exported nijilive model which has been set up in Inochi Session."));
                    

                    igTextColored(ImVec4(0.7, 0.5, 0.5, 1), __("Puppet Texturing"));
                    igSeparator();

                    // Premultiply textures, causing every pixel value in every texture to
                    // be multiplied by their Alpha (transparency) component
                    ngMenuItemFor!(ToolCommand.PremultTexture)(ctx);
                    incTooltip(_("Premultiplies textures by their alpha component.\n\nOnly use this if your textures look garbled after importing files from an older version of nijigenerate."));
                    
                    ngMenuItemFor!(ToolCommand.RebleedTexture)(ctx);
                    incTooltip(_("Causes color to bleed out in to fully transparent pixels, this solves outlines on straight alpha compositing.\n\nOnly use this if your game engine can't use premultiplied alpha."));

                    ngMenuItemFor!(ToolCommand.RegenerateMipmaps)(ctx);
                    incTooltip(_("Regenerates the puppet's mipmaps."));

                    ngMenuItemFor!(ToolCommand.GenerateFakeLayerName)(ctx);
                    incTooltip(_("Generates fake layer info based on node names"));

                    // Spacing
                    igSpacing();
                    igSpacing();

                    igTextColored(ImVec4(0.7, 0.5, 0.5, 1), __("Puppet Recovery"));
                    igSeparator();

                    // FULL REPAIR
                    ngMenuItemFor!(ToolCommand.AttemptRepairPuppet)(ctx);
                    incTooltip(_("Attempts all the recovery and repair methods below on the currently loaded model"));

                    // REGEN NODE IDs
                    ngMenuItemFor!(ToolCommand.RegenerateNodeIDs)(ctx);
                    incTooltip(_("Regenerates all the unique IDs for the model"));

                    // Spacing
                    igSpacing();
                    igSpacing();
                    igSeparator();
                    ngMenuItemFor!(ToolCommand.AttemptRepairPuppet)(ctx);
                    incTooltip(_("Attempts to verify and repair INP files"));

                    igEndMenu();
                }

                if (igBeginMenu(__("Help"), true)) {

                    if(igMenuItem(__("Online Documentation"), "", false, true)) {
                        incOpenLink("https://github.com/nijigenerate/nijigenerate/wiki");
                    }
                    
                    if(igMenuItem(__("nijilive Documentation"), "", false, true)) {
                        incOpenLink("https://github.com/nijigenerate/nijilive/wiki");
                    }
                    igSpacing();
                    igSeparator();
                    igSpacing();
                    

                    if (igMenuItem(__("Report a Bug"))) {
                        incOpenLink(INC_BUG_REPORT_URI);
                    }
                    if (igMenuItem(__("Request a Feature"))) {
                        incOpenLink(INC_FEATURE_REQ_URI);
                    }
                    igSpacing();
                    igSeparator();
                    igSpacing();


                    if(igMenuItem(__("About"), "", false, true)) {
                        incPushWindow(new AboutWindow);
                    }
                    igEndMenu();
                }
        }
        ImVec2 avail;
        igGetContentRegionAvail(&avail);
        float tabBarWidth;
        tabBarWidth = clamp(avail.x-128*2, 0, int.max);

        // We need to pre-calculate the size of the right adjusted section
        // This code is very ugly because imgui doesn't really exactly understand this
        // stuff natively.
        ImVec2 secondSectionLength = ImVec2(0, 0);
        bool teacherDiffActive = incBrushHasTeacherPart();
        string statsPlaceholder = teacherDiffActive ? "1000ms | Diff 0.000" : "1000ms";
        if (incShowStatsForNerds) { // Extra padding I guess
            secondSectionLength.x += igGetStyle().ItemSpacing.x;
            secondSectionLength.x += incMeasureString(statsPlaceholder).x;
        }
        igDummy(ImVec2(tabBarWidth - secondSectionLength.x, 0));
        if (incShowStatsForNerds) {
            string fpsText = "%.0fms".format(1000f/io.Framerate);
            string statsText = fpsText;
            if (teacherDiffActive) {
                string diffValue;
                if (ngDifferenceAggregationResultValid) {
                    double sumTotals = 0;
                    double sumWeights = 0;
                    foreach (i; 0 .. ngDifferenceAggregationResult.tileTotals.length) {
                        sumTotals += ngDifferenceAggregationResult.tileTotals[i];
                        sumWeights += ngDifferenceAggregationResult.tileCounts[i];
                    }
                    if (sumWeights > 0) {
                        diffValue = "%.3f".format(sumTotals / sumWeights);
                    } else if (ngDifferenceAggregationResult.alpha > 0) {
                        diffValue = "%.3f".format(ngDifferenceAggregationResult.total / ngDifferenceAggregationResult.alpha);
                    } else {
                        diffValue = "--";
                    }
                } else {
                    diffValue = "--";
                }
                statsText ~= " | Diff %s".format(diffValue);
            }
            float textAreaDummyWidth = incMeasureString(statsPlaceholder).x - incMeasureString(statsText).x;
            if (textAreaDummyWidth > 0) incDummy(ImVec2(textAreaDummyWidth, 0));
            incText(statsText);
        }
        igSetNextItemWidth (avail.x - tabBarWidth);
        igBeginTabBar("###ModeTab");
            if(incEditMode != EditMode.VertexEdit) {
                auto mode = incEditMode; // snapshot for this frame

                // Use a persistent tab selection to avoid per-frame toggling.
                // Only request SetSelected when mode changed externally (e.g., shortcut).
                static EditMode tabSelected = EditMode.ModelEdit;
                ImGuiTabItemFlags modelFlags = ImGuiTabItemFlags.None;
                ImGuiTabItemFlags animFlags  = ImGuiTabItemFlags.None;
                if (tabSelected != mode) {
                    if (mode == EditMode.ModelEdit) modelFlags = ImGuiTabItemFlags.SetSelected;
                    else if (mode == EditMode.AnimEdit) animFlags = ImGuiTabItemFlags.SetSelected;
                    tabSelected = mode;
                }

                // Render tabs
                if (igBeginTabItem(("" ~ _("Edit Puppet")).toStringz, null, modelFlags)) {
                    // If user clicked this tab this frame, switch mode once.
                    if (mode != EditMode.ModelEdit) {
                        incSetEditMode(EditMode.ModelEdit);
                        tabSelected = EditMode.ModelEdit;
                    }
                    igEndTabItem();
                }
                incTooltip(_("Edit Puppet"));

                if (igBeginTabItem(("" ~ _("Edit Animation")).toStringz, null, animFlags)) {
                    if (mode != EditMode.AnimEdit) {
                        incSetEditMode(EditMode.AnimEdit);
                        tabSelected = EditMode.AnimEdit;
                    }
                    igEndTabItem();
                }
                incTooltip(_("Edit Animation"));
            }
        igEndTabBar();
        igEndMainMenuBar();

        // For quick-setup stuff
        if (!incSettingsGet("hasDoneQuickSetup", false)) igEndDisabled();

    // ImGui Debug Stuff
    if (dbgShowStyleEditor) igShowStyleEditor(igGetStyle());
    if (dbgShowDebugger) igShowAboutWindow(&dbgShowDebugger);
    if (dbgShowStackTool) igShowStackToolWindow();
    if (dbgShowMetrics) igShowMetricsWindow();
}
