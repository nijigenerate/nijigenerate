/*
    Copyright © 2020-2023, Inochi2D Project
    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    
    Authors: Luna Nielsen
*/
module nijigenerate.panels.parameters;
import nijigenerate.viewport.model.deform;
import nijigenerate.panels;
import nijigenerate.ext.param;
import nijigenerate.widgets;
import nijigenerate.windows;
import nijigenerate.core.math.triangle;
import nijigenerate.core;
import nijigenerate.actions;
import nijigenerate.ext;
import nijigenerate.ext.param;
import nijigenerate.viewport.common.mesheditor;
import nijigenerate.viewport.common.mesh;
import nijigenerate.windows.flipconfig;
import nijigenerate.viewport.model.onionslice;
import nijigenerate.utils.transform;
import nijigenerate;
import std.string;
import nijilive;
import i18n;
import std.uni : toLower;
//import std.stdio;
import nijigenerate.utils;
import std.algorithm.searching : countUntil;
import std.algorithm.sorting : sort;
import std.algorithm.mutation : remove;

import nijigenerate.commands;
import nijigenerate.commands.binding.base;
import nijigenerate.commands.parameter.base;

private {
    void pushColorScheme(vec3 color) {
        float h, s, v;
        igColorConvertRGBtoHSV(color.r, color.g, color.b, &h, &s, &v);

        float maxS = lerp(1, 0.60, v);

        vec3 c = color;
        igColorConvertHSVtoRGB(
            h, 
            clamp(lerp(s, s-0.20, v), 0, maxS), 
            clamp(v-0.15, 0.15, 0.90), 
            &c.vector[0], &c.vector[1], &c.vector[2]
        );
        igPushStyleColor(ImGuiCol.FrameBg, ImVec4(c.r, c.g, c.b, 1));


        maxS = lerp(1, 0.60, v);
        igColorConvertHSVtoRGB(
            h, 
            lerp(
                clamp(s-0.25, 0, maxS),
                clamp(s+0.25, 0, maxS),
                s
            ),
            v <= 0.55 ?
                clamp(v+0.25, 0.45, 0.95) :
                clamp(v-(0.25*(1+v)), 0.30, 1),
            &c.vector[0], &c.vector[1], &c.vector[2]
        );
        igPushStyleColor(ImGuiCol.TextDisabled, ImVec4(c.r, c.g, c.b, 1));
    }

    void popColorScheme() {
        igPopStyleColor(2);
    }

    ParamDragDropData* dragDropData;
    void setTransparency(float alpha, float text) {
        ImGuiCol[] colIDs = [ImGuiCol.WindowBg, ImGuiCol.Text, ImGuiCol.FrameBg, ImGuiCol.Button, ImGuiCol.Border, ImGuiCol.PopupBg];
        foreach (id; colIDs) {
            ImVec4 style;
            style = *igGetStyleColorVec4(id);
            style.w = id == ImGuiCol.Text? text: alpha;
            igPushStyleColor(id, style);
        }
    }

    void resetTransparency() {
        igPopStyleColor(6);
    }

}

struct ParamDragDropData {
    Parameter param;
}

void incKeypointActions(Parameter param, ParameterBinding[] srcBindings, ParameterBinding[] targetBindings) {
    Context ctx = new Context();
    if (srcBindings !is null)
        ctx.bindings = srcBindings;
    if (targetBindings !is null)
        ctx.activeBindings = targetBindings;
    ctx.puppet = incActivePuppet();
    ctx.parameters = [param];
    if (incArmedParameter() !is null)
        ctx.armedParameters = [incArmedParameter()];
    ctx.keyPoint = cParamPoint;

    if (igMenuItem(__("Unset"), "", false, true)) {
        cmd!(BindingCommand.UnsetKeyFrame)(ctx);
    }
    if (igMenuItem(__("Set to current"), "", false, true)) {
        cmd!(BindingCommand.SetKeyFrame)(ctx);
    }
    if (igMenuItem(__("Reset"), "", false, true)) {
        cmd!(BindingCommand.ResetKeyFrame)(ctx);
    }
    if (igMenuItem(__("Invert"), "", false, true)) {
        cmd!(BindingCommand.InvertKeyFrame)(ctx);
    }
    if (igBeginMenu(__("Mirror"), true)) {
        if (igMenuItem(__("Horizontally"), "", false, true)) {
            cmd!(BindingCommand.MirrorKeyFrameHorizontally)(ctx);
        }
        if (igMenuItem(__("Vertically"), "", false, true)) {
            cmd!(BindingCommand.MirrorKeyFrameVertically)(ctx);
        }
        igEndMenu();
    }
    if (igMenuItem(__("Flip Deform"), "", false, true)) {
        cmd!(BindingCommand.FlipDeform)(ctx);
    }
    if (igMenuItem(__("Symmetrize Deform"), "", false, true)) {
        cmd!(BindingCommand.SymmetrizeDeform)(ctx);
    }

    if (param.isVec2) {
        if (igBeginMenu(__("Set from mirror"), true)) {
            if (igMenuItem(__("Horizontally"), "", false, true)) {
                cmd!(BindingCommand.SetFromHorizontalMirror)(ctx);
            }
            if (igMenuItem(__("Vertically"), "", false, true)) {
                cmd!(BindingCommand.SetFromVerticalMirror)(ctx);
            }
            if (igMenuItem(__("Diagonally"), "", false, true)) {
                cmd!(BindingCommand.SetFromDiagonalMirror)(ctx);
            }
            igEndMenu();
        }
    } else {
        if (igMenuItem(__("Set from mirror"), "", false, true)) {
            cmd!(BindingCommand.SetFrom1DMirror)(ctx);
        }
    }

    if (igMenuItem(__("Copy"), "", false, true)) {
        cmd!(BindingCommand.CopyBinding)(ctx);
    }

    if (igMenuItem(__("Paste"), "", false,  true)) {
        cmd!(BindingCommand.PasteBinding)(ctx);
    }

}

void incBindingMenuContents(Parameter param, ParameterBinding[BindTarget] cSelectedBindings) {
    Context ctx = new Context();
    ctx.activeBindings = cSelectedBindings.values;
    ctx.puppet = incActivePuppet();
    ctx.parameters = [param];
    if (incArmedParameter() !is null)
        ctx.armedParameters = [incArmedParameter()];
    ctx.keyPoint = cParamPoint;

    if (igMenuItem(__("Remove"), "", false, true)) {
        cmd!(BindingCommand.RemoveBinding)(ctx);
    }

    incKeypointActions(param, null, cSelectedBindings.values);

    if (igBeginMenu(__("Interpolation Mode"), true)) {
        if (igMenuItem(__("Nearest"), "", false, true)) {
            cmd!(BindingCommand.SetInterpolation)(ctx, InterpolateMode.Nearest);
        }
        if (igMenuItem(__("Linear"), "", false, true)) {
            cmd!(BindingCommand.SetInterpolation)(ctx, InterpolateMode.Linear);
        }
        if (igMenuItem(__("Cubic"), "", false, true)) {
            cmd!(BindingCommand.SetInterpolation)(ctx, InterpolateMode.Cubic);
        }
        igEndMenu();
    }

    bool haveCompatible = cCompatibleNodes.length > 0;
    if (igBeginMenu(__("Copy to"), haveCompatible)) {
        foreach(c; cCompatibleNodes) {
            if (Node cNode = cast(Node)c) {
                if (igMenuItem(cNode.name.toStringz, "", false, true)) {
                    copySelectionToNode(param, cNode);
                }
            }
        }
        igEndMenu();
    }
    if (igBeginMenu(__("Swap with"), haveCompatible)) {
        foreach(c; cCompatibleNodes) {
            if (Node cNode = cast(Node)c) {
                if (igMenuItem(cNode.name.toStringz, "", false, true)) {
                    swapSelectionWithNode(param, cNode);
                }
            }
        }
        igEndMenu();
    }

}

void incBindingList(Parameter param) {

    if (incBeginCategory(__("Bindings"),IncCategoryFlags.None, (float w, float h) {
        if (selectedOnly)
            igText("");
        else
            igTextDisabled("");
        if (igIsItemClicked()) {
            selectedOnly = !selectedOnly;
        }
        incTooltip(selectedOnly ? _("Show all nodes") : _("Show only selected nodes"));
        igSameLine();
    })) {
        refreshBindingList(param, selectedOnly);

        auto io = igGetIO();
        auto style = igGetStyle();
        ImS32 inactiveColor = igGetColorU32(style.Colors[ImGuiCol.TextDisabled]);

        igBeginChild("BindingList", ImVec2(0, 256), false);
            igPushStyleVar(ImGuiStyleVar.CellPadding, ImVec2(4, 1));
            igPushStyleVar(ImGuiStyleVar.IndentSpacing, 14);

            foreach(n; cAllBoundNodes) {
                ParameterBinding[] allBindings = cParamBindingEntriesAll[n];
                ParameterBinding[] *bindings = (n in cParamBindingEntries);

                // Figure out if node is selected ( == all bindings selected)
                bool nodeSelected = true;
                bool someSelected = false;
                foreach(binding; allBindings) {
                    if ((binding.getTarget() in cSelectedBindings) is null)
                        nodeSelected = false;
                    else
                        someSelected = true;
                }

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags.DefaultOpen | ImGuiTreeNodeFlags.OpenOnArrow;
                if (nodeSelected)
                    flags |= ImGuiTreeNodeFlags.Selected;

                if (bindings is null) igPushStyleColor(ImGuiCol.Text, inactiveColor);
                Node node = cast(Node)n;
                string nodeName = node? (incTypeIdToIcon(node.typeId) ~ " " ~ node.name): n.name;
                if (igTreeNodeEx(cast(void*)n.uuid, flags, nodeName.toStringz)) {
                    if (bindings is null) igPopStyleColor();
                    if (igBeginPopup("###BindingPopup")) {
                        incBindingMenuContents(param, cSelectedBindings);
                        igEndPopup();
                    }
                    if (igIsItemClicked(ImGuiMouseButton.Right)) {
                        if (!someSelected) {
                            cSelectedBindings.clear();
                            foreach(binding; allBindings) {
                                cSelectedBindings[binding.getTarget()] = binding;
                            }
                        }
                        cCompatibleNodes = getCompatibleNodes();
                        igOpenPopup("###BindingPopup");
                    }

                    // Node selection logic
                    if (igIsItemClicked(ImGuiMouseButton.Left) && !igIsItemToggledOpen()) {
                        
                        // Select the node you've clicked in the bindings list
                        if (incNodeInSelection(node)) {
                            incFocusCamera(node);
                        } else incSelectNode(node);
                        
                        if (!io.KeyCtrl) {
                            cSelectedBindings.clear();
                            nodeSelected = false;
                        }
                        foreach(binding; allBindings) {
                            if (nodeSelected) cSelectedBindings.remove(binding.getTarget());
                            else cSelectedBindings[binding.getTarget()] = binding;
                        }
                    }

                    // Iterate over bindings
                    foreach(binding; allBindings) {
                        ImGuiTreeNodeFlags flags2 =
                            ImGuiTreeNodeFlags.DefaultOpen | ImGuiTreeNodeFlags.OpenOnArrow |
                            ImGuiTreeNodeFlags.Leaf | ImGuiTreeNodeFlags.NoTreePushOnOpen;

                        bool selected = cast(bool)(binding.getTarget() in cSelectedBindings);
                        if (selected) flags2 |= ImGuiTreeNodeFlags.Selected;

                        // Style as inactive if not set at this keypoint
                        if (!binding.isSet(cParamPoint))
                            igPushStyleColor(ImGuiCol.Text, inactiveColor);


                        // Binding entry
                        auto value = cast(ValueParameterBinding)binding;
                        string label;
                        if (value && binding.isSet(cParamPoint)) {
                            label = format("%s (%.02f)", binding.getName(), value.getValue(cParamPoint));
                        } else {
                            label = binding.getName();
                        }

                        // NOTE: This is a leaf node so it should NOT be popped.
                        const(char)* bid = binding.getName().toStringz;
                        igTreeNodeEx(bid, flags2, label.toStringz);
                            if (!binding.isSet(cParamPoint)) igPopStyleColor();

                            // Binding selection logic
                            if (igIsItemClicked(ImGuiMouseButton.Right)) {
                                if (!selected) {
                                    cSelectedBindings.clear();
                                    cSelectedBindings[binding.getTarget()] = binding;
                                }
                                cCompatibleNodes = getCompatibleNodes();
                                igOpenPopup("###BindingPopup");
                            }
                            if (igIsItemClicked(ImGuiMouseButton.Left)) {
                                if (!io.KeyCtrl) {
                                    cSelectedBindings.clear();
                                    selected = false;
                                }
                                if (selected) cSelectedBindings.remove(binding.getTarget());
                                else cSelectedBindings[binding.getTarget()] = binding;
                            }
                        
                    }
                    igTreePop();
                } else if (bindings is null) igPopStyleColor();

            }

            igPopStyleVar();
            igPopStyleVar();
        igEndChild();
    }
    incEndCategory();
}

/**
    Generates a parameter view
*/
void incParameterViewEditButtons(bool armedParam, bool horizontal)(size_t idx, Parameter param, ref Parameter[] paramArr, bool childVisible = true) {
    if (childVisible || armedParam) {
        Context ctx = new Context();
        ctx.puppet = incActivePuppet();
        ctx.parameters = [param];

        if (incEditMode == EditMode.ModelEdit) {
            setTransparency(1.0, 1.0);
            if (igBeginPopup("###EditParam")) {

                if (igMenuItem(__("Edit Properties"), "", false, true)) {
                    incPushWindowList(new ParamEditorWindow(param, ParamEditorTab.Properties));
                }
                
                if (igMenuItem(__("Edit Axes Points"), "", false, true)) {
                    incPushWindowList(new ParamEditorWindow(param, ParamEditorTab.Axes));
                }
                
                if (igMenuItem(__("Split"), "", false, true)) {
                    incPushWindowList(new ParamSplitWindow(idx, param));
                }

                if (!param.isVec2 && igMenuItem(__("To 2D"), "", false, true)) {
                    cmd!(ParameditCommand.ConvertTo2DParam)(ctx);
                }

                if (param.isVec2) {
                    if (igMenuItem(__("Flip X"), "", false, true)) {
                        cmd!(ParameditCommand.FlipX)(ctx);
                    }
                    if (igMenuItem(__("Flip Y"), "", false, true)) {
                        cmd!(ParameditCommand.FlipY)(ctx);
                    }
                } else {
                    if (igMenuItem(__("Flip"), "", false, true)) {
                        cmd!(ParameditCommand.Flip1D)(ctx);
                    }
                }
                if (igBeginMenu(__("Mirror"), true)) {
                    if (igMenuItem(__("Horizontally"), "", false, true)) {
                        cmd!(ParameditCommand.MirrorHorizontally)(ctx);
                    }
                    if (igMenuItem(__("Vertically"), "", false, true)) {
                        cmd!(ParameditCommand.MirrorVertically)(ctx);
                    }
                    igEndMenu();
                }
                if (igBeginMenu(__("Mirrored Autofill"), true)) {
                    if (igMenuItem("", "", false, true)) {
                        cmd!(ParameditCommand.MirroredAutoFillDir1)(ctx);
                    }
                    if (igMenuItem("", "", false, true)) {
                        cmd!(ParameditCommand.MirroredAutoFillDir2)(ctx);
                    }
                    if (param.isVec2) {
                        if (igMenuItem("", "", false, true)) {
                            cmd!(ParameditCommand.MirroredAutoFillDir3)(ctx);
                        }
                        if (igMenuItem("", "", false, true)) {
                            cmd!(ParameditCommand.MirroredAutoFillDir4)(ctx);
                        }
                    }
                    igEndMenu();
                }

                igNewLine();
                igSeparator();

                if (igMenuItem(__("Copy"), "", false, true)) {
                    cmd!(ParameditCommand.CopyParameter)(ctx);
                }
                if (igMenuItem(__("Paste"), "", false, true)) {
                    cmd!(ParameditCommand.PasteParameter)(ctx);
                }
                if (igMenuItem(__("Paste and Horizontal Flip"), "", false, true)) {
                    cmd!(ParameditCommand.PasteParameterWithFlip)(ctx);
                }

                if (igMenuItem(__("Duplicate"), "", false, true)) {
                    cmd!(ParameditCommand.DuplicateParameter)(ctx);
                }

                if (igMenuItem(__("Duplicate and Horizontal Flip"), "", false, true)) {
                    cmd!(ParameditCommand.DuplicateParameterWithFlip)(ctx);
                }

                if (igMenuItem(__("Delete"), "", false, true)) {
                    cmd!(ParameditCommand.DeleteParameter)(ctx);
                }

                igNewLine();
                igSeparator();

                void listParams(int fromAxis) {
                    foreach (p; incActivePuppet().parameters) {
                        if (param == p) continue;
                        if (p.isVec2) {
                            if (igBeginMenu(p.name.toStringz, true)) {
                                if (igMenuItem(__("X"))) {
                                    cmd!(ParameditCommand.LinkTo)(ctx, p, fromAxis, 0);
                                }
                                if (igMenuItem(__("Y"))) {
                                    cmd!(ParameditCommand.LinkTo)(ctx, p, fromAxis, 1);
                                }
                                igEndMenu();
                            } 
                        } else if (igMenuItem(p.name.toStringz, null, false, true)) {
                            cmd!(ParameditCommand.LinkTo)(ctx, p, fromAxis, 0);
                        }
                    }
                }

                if (param.isVec2) {
                    if (igBeginMenu(__("Link X to..."), true)) {
                        listParams(0);
                        igEndMenu();
                    }

                    if (igBeginMenu(__("Link Y to .."), true)) {
                        listParams(1);
                        igEndMenu();
                    }
                } else {
                    if (igBeginMenu(__("Link parameter to..."), true)) {
                        listParams(0);
                        igEndMenu();
                    }
                }

                igNewLine();
                igSeparator();

                // Sets the default value of the param
                if (igMenuItem(__("Set Starting Position"), "", false, true)) {
                    cmd!(ParameditCommand.SetStartingKeyFrame)(ctx);
                }
                igEndPopup();
            }
            resetTransparency();
            
            if (incButtonColored("", ImVec2(24, 24))) {
                igOpenPopup("###EditParam");
            }
            
            if (horizontal) {
                igSameLine();
            }
            
            bool isArmed = incArmedParameter() == param;
            if (incButtonColored(isArmed ? "" : "", ImVec2(24, 24), isArmed ? ImVec4(1f, 0f, 0f, 1f) : colorUndefined)) {
                cmd!(ParameditCommand.ToggleParameterArm)(ctx);
            }

            // Arms the parameter for recording values.
            incTooltip(_("Arm Parameter"));
        }

        if (incEditMode == EditMode.AnimEdit) {
            if (horizontal) {
                igSameLine();
            }
            igBeginDisabled(incAnimationGet() is null);
                if (incButtonColored("", ImVec2(24, 24))) {
                    cmd!(AnimeditCommand.AddKeyFrame)(ctx);
                }
                incTooltip(_("Add Keyframe"));
            igEndDisabled();
            
        }
    }
}

void incParameterView(bool armedParam=false, bool showCategory = true, bool fixedWidth = false)(size_t idx, Parameter param, string* grabParam, bool canGroup, ref Parameter[] paramArr, vec3 groupColor = vec3.init) {
    igPushID(cast(void*)param);
    scope(exit) igPopID();

    
    bool open = true;
    if (showCategory) {
        if (!groupColor.isFinite) open = incBeginCategory(param.name.toStringz);
        else open = incBeginCategory(param.name.toStringz, ImVec4(groupColor.r, groupColor.g, groupColor.b, 1));
    }

    if(igBeginDragDropSource(ImGuiDragDropFlags.SourceAllowNullID)) {
        if (!dragDropData) dragDropData = new ParamDragDropData;
        
        dragDropData.param = param;

        igSetDragDropPayload("_PARAMETER", cast(void*)&dragDropData, (&dragDropData).sizeof, ImGuiCond.Always);
        incText(dragDropData.param.name);
        igEndDragDropSource();
    }

    if (canGroup) {
        incBeginDragDropFake();
            auto peek = igAcceptDragDropPayload("_PARAMETER", ImGuiDragDropFlags.AcceptPeekOnly | ImGuiDragDropFlags.SourceAllowNullID);
            if(peek && peek.Data && (*cast(ParamDragDropData**)peek.Data).param != param) {
                if (igBeginDragDropTarget()) {
                    auto payload = igAcceptDragDropPayload("_PARAMETER");
                    
                    if (payload !is null) {
                        ParamDragDropData* payloadParam = *cast(ParamDragDropData**)payload.Data;

                        auto group = incCreateParamGroup(cast(int)idx);
                        incMoveParameter(param, group);
                        incMoveParameter(payloadParam.param, group);
                    }
                    igEndDragDropTarget();
                }
            }
        incEndDragDropFake();
    }

    if (open) {
        // Push color scheme
        if (groupColor.isFinite) pushColorScheme(groupColor);

        float reqSpace = param.isVec2 ? 144 : 52;

        // Parameter Control
        ImVec2 avail = incAvailableSpace();

        // We want to always show armed parameters but also make sure the child is begun.
        bool childVisible = true;
        float width = fixedWidth? 156: avail.x-24;
        float height = fixedWidth? (param.isVec2? 132: 52): reqSpace - 24;
        if (showCategory) {
            childVisible = igBeginChild("###PARAM", ImVec2(width, reqSpace));
        }
        if (childVisible || armedParam) {

            // Popup for rightclicking the controller
            if (igBeginPopup("###ControlPopup")) {
                if (incArmedParameter() == param) {
                    incKeypointActions(param, param.bindings, null);
                }
                igEndPopup();
            }

            if (param.isVec2) incText("%.2f %.2f".format(param.value.x, param.value.y));
            else incText("%.2f".format(param.value.x));

            if (incController("###CONTROLLER", param, ImVec2(width, height), incArmedParameter() == param, *grabParam)) {
                if (incArmedParameter() == param) {
                    auto onion = OnionSlice.singleton;
                    onion.capture(cParamPoint);

                    incViewportNodeDeformNotifyParamValueChanged();
                    paramPointChanged(param);
                }
                if (igIsMouseDown(ImGuiMouseButton.Left)) {
                    if (*grabParam == null)
                        *grabParam = param.name;
                } else {
                    *grabParam = "";
                }
            }
            if (igIsItemClicked(ImGuiMouseButton.Right)) {
                if (incArmedParameter() == param) incViewportNodeDeformNotifyParamValueChanged();
                refreshBindingList(param);
                igOpenPopup("###ControlPopup");
            }
        }
        if (showCategory) {
            igEndChild();
        }

            igSameLine(0, 0);

            // Parameter Setting Buttons
            if (showCategory) {
                childVisible = igBeginChild("###SETTING", ImVec2(24, reqSpace), false);
                incParameterViewEditButtons!(armedParam, false)(idx, param, paramArr, childVisible);
                igEndChild();
            }
        if (showCategory) {
            if (incArmedParameter() == param) {
                incBindingList(param);
            }
        }
        if (groupColor.isFinite) popColorScheme();
    }
    if (showCategory)
        incEndCategory();
}

bool incParameterGropuMenuContents(ExParameterGroup group) {
    Context ctx = new Context();
    ctx.puppet = incActivePuppet();
    ctx.parameters = [group];

    bool result = false;
    if (igMenuItem(__("Rename"))) {
        incPushWindow(new RenameWindow(group.name_));
    }

    if (igBeginMenu(__("Colors"))) {
        auto flags = ImGuiColorEditFlags.NoLabel | ImGuiColorEditFlags.NoTooltip;
        ImVec2 swatchSize = ImVec2(24, 24);

        // COLOR SWATCHES
        if (igColorButton("NONE", ImVec4(0, 0, 0, 0), flags | ImGuiColorEditFlags.AlphaPreview, swatchSize)) group.color = vec3(float.nan, float.nan, float.nan);
        igSameLine(0, 4);
        if (igColorButton("RED", ImVec4(1, 0, 0, 1), flags, swatchSize)) group.color = vec3(0.25, 0.15, 0.15);
        igSameLine(0, 4);
        if (igColorButton("GREEN", ImVec4(0, 1, 0, 1), flags, swatchSize)) group.color = vec3(0.15, 0.25, 0.15);
        igSameLine(0, 4);
        if (igColorButton("BLUE", ImVec4(0, 0, 1, 1), flags, swatchSize)) group.color = vec3(0.15, 0.15, 0.25);
        igSameLine(0, 4);
        if (igColorButton("PURPLE", ImVec4(1, 0, 1, 1), flags, swatchSize)) group.color = vec3(0.25, 0.15, 0.25);
        igSameLine(0, 4);
        if (igColorButton("CYAN", ImVec4(0, 1, 1, 1), flags, swatchSize)) group.color = vec3(0.15, 0.25, 0.25);
        igSameLine(0, 4);
        if (igColorButton("YELLOW", ImVec4(1, 1, 0, 1), flags, swatchSize)) group.color = vec3(0.25, 0.25, 0.15);
        igSameLine(0, 4);
        if (igColorButton("WHITE", ImVec4(1, 1, 1, 1), flags, swatchSize)) group.color = vec3(0.25, 0.25, 0.25);
        
        igSpacing();

        // CUSTOM COLOR PICKER
        // Allows user to select a custom color for parameter group.
        igColorPicker3(__("Custom Color"), &group.color.vector, ImGuiColorEditFlags.InputRGB | ImGuiColorEditFlags.DisplayHSV);
        igEndMenu();
    }

    if (igMenuItem(__("Delete"))) {
        cmd!(GroupCommand.DeleteParamGroup)(ctx);
        // End early.
        result = true;
    }
    return result;
}

void incParameterMenuContents(Parameter[] parameters) {
    Context ctx = new Context();
    ctx.puppet = incActivePuppet();
    ctx.parameters = parameters;

    if (igMenuItem(__("Add 1D Parameter (0..1)"), "", false, true)) {
        cmd!(ParamCommand.Add1DParameter)(ctx, 0, 1);
    }
    if (igMenuItem(__("Add 1D Parameter (-1..1)"), "", false, true)) {
        cmd!(ParamCommand.Add1DParameter)(ctx, -1, 1);
    }
    if (igMenuItem(__("Add 2D Parameter (0..1)"), "", false, true)) {
        cmd!(ParamCommand.Add2DParameter)(ctx, 0, 1);
    }
    if (igMenuItem(__("Add 2D Parameter (-1..+1)"), "", false, true)) {
        cmd!(ParamCommand.Add2DParameter)(ctx, -1, 1);
    }
    if (igMenuItem(__("Add Mouth Shape"), "", false, true)) {
        cmd!(ParamCommand.AddMouthParameter)(ctx);
    }
}

/**
    The logger frame
*/
class ParametersPanel : Panel {
private:
    string filter;
    string grabParam = "";
protected:
    override
    void onUpdate() {
        if (incEditMode == EditMode.VertexEdit) {
            incLabelOver(_("In vertex edit mode..."), ImVec2(0, 0), true);
            return;
        }

        auto parameters = incActivePuppet().parameters;
        auto exPuppet = cast(ExPuppet)incActivePuppet();
        auto groups = (exPuppet !is null)? exPuppet.groups: [];

        if (igBeginPopup("###AddParameter")) {
            incParameterMenuContents(parameters);
            igEndPopup();
        }
        if (igBeginChild("###FILTER", ImVec2(0, 32))) {
            if (incInputText("Filter", filter)) {
                filter = filter.toLower;
            }
            incTooltip(_("Filter, search for specific parameters"));
        }
        igEndChild();

        if (igBeginChild("ParametersList", ImVec2(0, -36))) {
            
            // Always render the currently armed parameter on top
            if (incArmedParameter()) {
                incParameterView!true(incArmedParameterIdx(), incArmedParameter(), &grabParam, false, parameters);
            }

            // Render other parameters
            void displayParameters(Parameter[] targetParams, bool hideChildren) {
                foreach(i, ref param; targetParams) {
                    if (incArmedParameter() == param) continue;
                    if (hideChildren && (cast(ExParameter)param) && (cast(ExParameter)param).parent) continue;
                    import std.algorithm.searching : canFind;
                    ExParameterGroup group = cast(ExParameterGroup)param;
                    bool found = filter.length == 0 || param.indexableName.canFind(filter);
                    if (group) {
                        foreach (ix, ref child; group.children) {
                            if (incArmedParameter() == child) continue;
                            if (child.indexableName.canFind(filter))
                                found = true;
                        }
                    }
                    if (found) {
                        if (group) {
                            igPushID(group.uuid);

                                bool open;
                                if (group.color.isFinite) open = incBeginCategory(group.name.toStringz, ImVec4(group.color.r, group.color.g, group.color.b, 1));
                                else open = incBeginCategory(group.name.toStringz);
                                
                                if (igIsItemClicked(ImGuiMouseButton.Right)) {
                                    igOpenPopup("###CategorySettings");
                                }

                                // Popup
                                if (igBeginPopup("###CategorySettings")) {
                                    bool deleted = incParameterGropuMenuContents(group);
                                    igEndPopup();
                                    if (deleted) {
                                        incEndCategory();
                                        igPopID();
                                        continue;
                                    }
                                }

                                // Allow drag/drop in to the category
                                if (igBeginDragDropTarget()) {
                                    auto payload = igAcceptDragDropPayload("_PARAMETER");
                                    
                                    if (payload !is null) {
                                        ParamDragDropData* payloadParam = *cast(ParamDragDropData**)payload.Data;
                                        incMoveParameter(payloadParam.param, group);
                                    }
                                    igEndDragDropTarget();
                                }

                                // Render children if open
                                if (open) {
                                    foreach(ix, ref child; group.children) {

                                        // Skip armed param
                                        if (incArmedParameter() == child) continue;
                                        if (child.indexableName.canFind(filter)) {
                                            // Otherwise render it
                                            incParameterView(ix, child, &grabParam, false, group.children, group.color);
                                        }
                                    }
                                }
                                incEndCategory();
                            igPopID();
                        } else {
                            incParameterView(i, param, &grabParam, true, incActivePuppet().parameters);
                        }
                    }
                }
            }
            displayParameters(cast(Parameter[])groups, false);
            displayParameters(parameters, true);
        }
        igEndChild();
        
        // Allow drag/drop out of categories
        if (igBeginDragDropTarget()) {
            auto payload = igAcceptDragDropPayload("_PARAMETER");
            
            if (payload !is null) {
                ParamDragDropData* payloadParam = *cast(ParamDragDropData**)payload.Data;
                incMoveParameter(payloadParam.param, null);
            }
            igEndDragDropTarget();
        }

        // Right align add button
        ImVec2 avail = incAvailableSpace();
        incDummy(ImVec2(avail.x-32, 32));
        igSameLine(0, 0);

        // Add button
        if (incButtonColored("", ImVec2(32, 32))) {
            igOpenPopup("###AddParameter");
        }
        incTooltip(_("Add Parameter"));
    }

public:
    this() {
        super("Parameters", _("Parameters"), true);
    }
}

vec2u incParamPoint() {
    return cParamPoint;
}

/**
    Generate logger frame
*/
mixin incPanel!ParametersPanel;
