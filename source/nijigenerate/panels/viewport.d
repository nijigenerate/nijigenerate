/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.viewport;
import nijigenerate.viewport.model.onionslice;
import nijigenerate.viewport;
import nijigenerate.windows;
import nijigenerate.widgets;
import nijigenerate.widgets.viewport;
import nijigenerate.core;
import nijigenerate.core.window : incViewportGetBackgroundColor;
import nijigenerate.core.colorbleed;
import nijigenerate.panels;
import nijigenerate.actions;
import nijigenerate;
import nijilive;
import nijilive.core.dbg;
import bindbc.imgui;
import std.string;
import i18n;
import nijigenerate.commands;

/**
    A viewport
*/
class ViewportPanel : Panel {
private:
    ImVec2 lastSize;
    bool actingInViewport;


    ImVec2 priorWindowPadding;

protected:
    override
    void onBeginUpdate() {
        
        ImGuiWindowClass wmclass;
        wmclass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlagsI.NoTabBar;
        igSetNextWindowClass(&wmclass);
        priorWindowPadding = igGetStyle().WindowPadding;
        igPushStyleVar(ImGuiStyleVar.WindowPadding, ImVec2(0, 2));
        igSetNextWindowDockID(incGetViewportDockSpace(), ImGuiCond.Always);

        flags |= ImGuiWindowFlags.NoScrollbar | ImGuiWindowFlags.NoScrollWithMouse;
        super.onBeginUpdate();
    }

    override void onEndUpdate() {
        super.onEndUpdate();
        igPopStyleVar();
    }

    override
    void onUpdate() {

        auto io = igGetIO();
        auto camera = inGetCamera();
        auto drawList = igGetWindowDrawList();
        auto window = igGetCurrentWindow();

        // Draw viewport itself
        ImVec2 currSize;
        igGetContentRegionAvail(&currSize);

        // We do not want the viewport to be NaN
        // That will crash the app
        if (currSize.x.isNaN || currSize.y.isNaN) {
            currSize = ImVec2(0, 0);
        }

        // Resize nijilive viewport according to frame
        // Also viewport of 0 is too small, minimum 128.
        currSize = ImVec2(clamp(currSize.x, 128, float.max), clamp(currSize.y, 128, float.max));
        
        foreach(btn; 0..cast(int)ImGuiMouseButton.COUNT) {
            if (!incStartedDrag(btn)) {
                if (io.MouseDown[btn]) {
                    if (igIsWindowHovered(ImGuiHoveredFlags.ChildWindows)) {
                        incBeginDragInViewport(btn);
                    }
                    incBeginDrag(btn);
                }
            }

            if (incStartedDrag(btn) && !io.MouseDown[btn]) {
                incEndDrag(btn);
                incEndDragInViewport(btn);
            }
        }
        if (igBeginChild("##ViewportView", ImVec2(0, -38), false, flags)) {
            MainViewport viewport = incViewport;
            igGetContentRegionAvail(&currSize);
            currSize = ImVec2(
                clamp(currSize.x, 128, float.max), 
                clamp(currSize.y, 128, float.max)
            );

            if (currSize != lastSize) {
                inSetViewport(cast(int)(currSize.x*incGetUIScale()), cast(int)(currSize.y*incGetUIScale()));
            }

            viewport.poll();

            // Ignore events within child windows *unless* drag started within
            // viewport.
            ImGuiHoveredFlags winFlags = ImGuiHoveredFlags.None;
            if (actingInViewport) winFlags |= ImGuiHoveredFlags.ChildWindows | ImGuiHoveredFlags.AllowWhenBlockedByActiveItem;
            if (igIsWindowHovered(winFlags)) {
                actingInViewport = igIsMouseDown(ImGuiMouseButton.Left) ||
                    igIsMouseDown(ImGuiMouseButton.Middle) ||
                    igIsMouseDown(ImGuiMouseButton.Right);
                viewport.update();
            } else if (incViewport.alwaysUpdate()) {
                viewport.update(true);
            }

            auto style = igGetStyle();
            if (incShouldMirrorViewport) {
                camera.scale.x *= -1;
                viewport.draw();
                camera.scale.x *= -1;
            } else {
                viewport.draw();
            }

            int width, height;
            inGetViewport(width, height);

            ImVec4 color = incViewportGetBackgroundColor();

            ImRect rect = ImRect(
                ImVec2(
                    window.InnerRect.Max.x-1,
                    window.InnerRect.Max.y,
                ),
                ImVec2(
                    window.InnerRect.Min.x+1,
                    window.InnerRect.Max.y+currSize.y,
                ),
            );

            // Render background color
            ImDrawList_AddRectFilled(drawList,
                rect.Min,
                rect.Max,
                igGetColorU32(color),
            );

            // Render our viewport
            ImDrawList_AddImage(
                drawList,
                cast(void*)inGetRenderImage(),
                rect.Min,
                rect.Max,
                ImVec2((0.5/width), 1-(0.5/height)), 
                ImVec2(1-(0.5/width), (0.5/height)), 
                0xFFFFFFFF,
            );
            igItemAdd(rect, igGetID("###VIEWPORT_DISP"));
            
            // Popup right click menu
            igPushStyleVar(ImGuiStyleVar.WindowPadding, priorWindowPadding);
            if (viewport.hasMenu()) {
                static ImVec2 downPos;
                ImVec2 currPos;
                if (igIsItemHovered()) {
                    if (igIsItemClicked(ImGuiMouseButton.Right)) {
                        igGetMousePos(&downPos);
                    }

                    if (!igIsPopupOpen("ViewportMenu") && igIsMouseReleased(ImGuiMouseButton.Right)) {
                        igGetMousePos(&currPos);
                        float dist = sqrt(((downPos.x-currPos.x)^^2)+((downPos.y-currPos.y)^^2));
                        
                        if (dist < 16) {
                            viewport.menuOpening();
                            igOpenPopup("ViewportMenu");
                        }
                    }
                }

                if (igBeginPopup("ViewportMenu")) {
                    viewport.menu();
                    igEndPopup();
                }
            }
            igPopStyleVar();

            //igPushStyleVar(ImGuiStyleVar.FrameBorderSize, 0);
                incBeginViewportToolArea("ToolArea", ImGuiDir.Left);
                    igPushStyleVar_Vec2(ImGuiStyleVar.FramePadding, ImVec2(6, 6));
                        viewport.drawTools();
                    igPopStyleVar();
                incEndViewportToolArea();

                incBeginViewportToolArea("OptionsArea", ImGuiDir.Right);
                    igPushStyleVar_Vec2(ImGuiStyleVar.FramePadding, ImVec2(6, 6));
                        viewport.drawOptions();
                    igPopStyleVar();
                incEndViewportToolArea();

                incBeginViewportToolArea("ConfirmArea", ImGuiDir.Left, ImGuiDir.Down, false);
                    viewport.drawConfirmBar();
                incEndViewportToolArea();
                if (incEditMode == EditMode.ModelEdit)
                    incViewportTransformHandle();
            //igPopStyleVar();

            lastSize = currSize;
            igEndChild();
        }

        // Draw line in a better way
        ImDrawList_AddLine(drawList, 
            ImVec2(
                window.InnerRect.Max.x-1,
                window.InnerRect.Max.y+currSize.y,
            ),
            ImVec2(
                window.InnerRect.Min.x+1,
                window.InnerRect.Max.y+currSize.y,
            ), 
            igColorConvertFloat4ToU32(*igGetStyleColorVec4(ImGuiCol.Separator)), 
            2
        );

        // FILE DRAG & DROP
        if (igBeginDragDropTarget()) {
            const(ImGuiPayload)* payload = igAcceptDragDropPayload("__PARTS_DROP");
            if (payload !is null) {
                string[] files = *cast(string[]*)payload.Data;
                import std.path : baseName, extension;
                import std.uni : toLower;
                mainLoop: foreach(file; files) {
                    string fname = file.baseName;

                    switch(fname.extension.toLower) {
                    case ".png", ".tga", ".jpeg", ".jpg":
                        incCreatePartsFromFiles([file]);
                        break;

                    // Allow dragging PSD in to main window
                    case ".psd":
                        incAskImportPSD(file);
                        break mainLoop;

                    // Allow dragging KRA in to main window
                    case ".kra":
                        incAskImportKRA(file);
                        break mainLoop;

                    case ".inx":
                        incOpenProject(file);
                        break mainLoop;

                    default:
                        incDialog(__("Error"), _("%s is not supported").format(fname)); 
                        break;
                    }
                }

                // Finish the file drag
                incFinishFileDrag();
            }

            igEndDragDropTarget();
        }

        // BOTTOM VIEWPORT CONTROLS
        igGetContentRegionAvail(&currSize);
        if (igBeginChild("##ViewportControls", ImVec2(0, currSize.y), false, flags.NoScrollbar)) {
            igSetCursorPosY(igGetCursorPosY()+4);
            igPushItemWidth(72);
                igSpacing();
                igSameLine(0, 8);
                if (igSliderFloat(
                    "##Zoom", 
                    &incViewportZoom, 
                    incVIEWPORT_ZOOM_MIN, 
                    incVIEWPORT_ZOOM_MAX, 
                    "%s%%\0".format(cast(int)(incViewportZoom*100)).ptr, 
                    ImGuiSliderFlags.NoRoundToFormat)
                ) {
                    camera.scale = vec2(incViewportZoom);
                    incViewportTargetZoom = incViewportZoom;
                }
                if (incViewportTargetZoom != 1) {
                    igSameLine(0, 8);
                    if (incButtonColored("Reset Zoom", ImVec2(0, 0), ImVec4.init)) {
                        auto ctx = new Context;
                        if (incActivePuppet() !is null) ctx.puppet = incActivePuppet();
                        cmd!(ViewportCommand.ResetViewportZoom)(ctx);
                    }
                }

                igSameLine(0, 8);
                igSeparatorEx(ImGuiSeparatorFlags.Vertical);

                igSameLine(0, 8);
                incText("x = %.2f y = %.2f".format(incViewportTargetPosition.x, incViewportTargetPosition.y));
                if (incViewportTargetPosition != vec2(0)) {
                    igSameLine(0, 8);
                    if (incButtonColored("Reset Pos", ImVec2(0, 0), ImVec4.init)) {
                        auto ctx = new Context;
                        if (incActivePuppet() !is null) ctx.puppet = incActivePuppet();
                        cmd!(ViewportCommand.ResetViewportPosition)(ctx);
                    }
                }


            igPopItemWidth();
        }
        igEndChild();

        igGetContentRegionAvail(&currSize);
        igSameLine();
        // if add new buttons, please increase the offset
        igDummy(ImVec2(currSize.x-currSize.x-(32*10), 0));
        igSameLine();

        if (igBeginChild("##ModelControl", ImVec2(0, currSize.y), false, flags.NoScrollbar)) {

            if (incButtonColored("Mirror", ImVec2(32, 0), incShouldMirrorViewport ? ImVec4.init : ImVec4(0.6f, 0.6f, 0.6f, 1f))) {
                auto ctx = new Context;
                if (incActivePuppet() !is null) ctx.puppet = incActivePuppet();
                cmd!(ViewportCommand.ToggleMirrorView)(ctx);
            }
            incTooltip(_("Mirror View"));

            igSameLine(0, 0);

            auto onion = OnionSlice.singleton;
            if (incButtonColored("Onion", ImVec2(32, 0), onion.enabled ? ImVec4.init : ImVec4(0.6f, 0.6f, 0.6f, 1f))) {
                auto ctx = new Context;
                cmd!(ViewportCommand.ToggleOnionSlice)(ctx);
            }
            incTooltip(_("Onion slice"));

            igSameLine();

            if (incButtonColored("Physics", ImVec2(32, 0), incActivePuppet().enableDrivers ? ImVec4.init : ImVec4(0.6f, 0.6f, 0.6f, 1f))) {
                auto ctx = new Context;
                if (incActivePuppet() !is null) ctx.puppet = incActivePuppet();
                cmd!(ViewportCommand.TogglePhysics)(ctx);
            }
            incTooltip(_("Enable physics"));

            igSameLine(0, 0);

            if (incButtonColored("PostFX", ImVec2(32, 0), incShouldPostProcess ? ImVec4.init : ImVec4(0.6f, 0.6f, 0.6f, 1f))) {
                auto ctx = new Context;
                if (incActivePuppet() !is null) ctx.puppet = incActivePuppet();
                cmd!(ViewportCommand.TogglePostProcess)(ctx);
            }
            incTooltip(_("Enable post processing"));

            igSameLine();

            if (incButtonColored("Reset Phys", ImVec2(32, 0), ImVec4.init)) {
                auto ctx = new Context;
                if (incActivePuppet() !is null) ctx.puppet = incActivePuppet();
                cmd!(ViewportCommand.ResetPhysics)(ctx);
            }
            incTooltip(_("Reset physics"));

            igSameLine(0, 0);

            if (incButtonColored("Reset Params", ImVec2(32, 0), ImVec4.init)) {
                auto ctx = new Context;
                if (incActivePuppet() !is null) ctx.puppet = incActivePuppet();
                cmd!(ViewportCommand.ResetParameters)(ctx);
            }
            incTooltip(_("Reset parameters"));
            
            igSameLine();

            if (incButtonColored("Flip Pair", ImVec2(32, 0), ImVec4.init)) {
                auto ctx = new Context;
                if (incActivePuppet() !is null) ctx.puppet = incActivePuppet();
                cmd!(ViewportCommand.OpenFlipPairWindow)(ctx);
            }
            incTooltip(_("Configure Flip Pairings"));
            
            igSameLine(0, 0);

            if (incButtonColored("Automesh", ImVec2(32, 0), ImVec4.init)) {
                auto ctx = new Context;
                if (incActivePuppet() !is null) ctx.puppet = incActivePuppet();
                cmd!(ViewportCommand.OpenAutomeshBatching)(ctx);
            }
            incTooltip(_("Automesh Batching"));

        }
        igEndChild();

        // Handle smooth move
        incViewportZoom = dampen(incViewportZoom, incViewportTargetZoom, deltaTime);
        camera.scale = vec2(incViewportZoom, incViewportZoom);
        camera.position = vec2(dampen(camera.position, incViewportTargetPosition, deltaTime, 1.5));
    }

public:
    this() {
        super("Viewport", _("Viewport"), true);
        this.alwaysVisible = true;
    }

}

mixin incPanel!ViewportPanel;

