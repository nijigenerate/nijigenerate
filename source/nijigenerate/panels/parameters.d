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
    ctx.bindings = (targetBindings !is null)? targetBindings: srcBindings;
    ctx.puppet = incActivePuppet();
    ctx.parameters = [param];
    ctx.keyPoint = cParamPoint;

    if (igMenuItem(__("Unset"), "", false, true)) {
        bindingCommands[BindingCommand.UnsetKeyFrame].run(ctx);
    }
    if (igMenuItem(__("Set to current"), "", false, true)) {
        bindingCommands[BindingCommand.SetKeyFrame].run(ctx);
    }
    if (igMenuItem(__("Reset"), "", false, true)) {
        bindingCommands[BindingCommand.ResetKeyFrame].run(ctx);
    }
    if (igMenuItem(__("Invert"), "", false, true)) {
        bindingCommands[BindingCommand.InvertKeyFrame].run(ctx);
    }
    if (igBeginMenu(__("Mirror"), true)) {
        if (igMenuItem(__("Horizontally"), "", false, true)) {
            bindingCommands[BindingCommand.MirrorKeyFrameHorizontally].run(ctx);
        }
        if (igMenuItem(__("Vertically"), "", false, true)) {
            bindingCommands[BindingCommand.MirrorKeyFrameVertically].run(ctx);
        }
        igEndMenu();
    }
    if (igMenuItem(__("Flip Deform"), "", false, true)) {
        bindingCommands[BindingCommand.FlipDeform].run(ctx);
    }
    if (igMenuItem(__("Symmetrize Deform"), "", false, true)) {
        bindingCommands[BindingCommand.SymmetrizeDeform].run(ctx);
    }

    if (param.isVec2) {
        if (igBeginMenu(__("Set from mirror"), true)) {
            if (igMenuItem(__("Horizontally"), "", false, true)) {
                auto cmds = cast(SetFromHorizontalMirrorCommand)bindingCommands[BindingCommand.SetFromHorizontalMirror];
                cmds.targetBindingsNull = targetBindings is null;
                cmds.run(ctx);
            }
            if (igMenuItem(__("Vertically"), "", false, true)) {
                auto cmds = cast(SetFromVerticalMirrorCommand)bindingCommands[BindingCommand.SetFromVerticalMirror];
                cmds.targetBindingsNull = targetBindings is null;
                cmds.run(ctx);
            }
            if (igMenuItem(__("Diagonally"), "", false, true)) {
                auto cmds = cast(SetFromDiagonalMirrorCommand)bindingCommands[BindingCommand.SetFromDiagonalMirror];
                cmds.targetBindingsNull = targetBindings is null;
                cmds.run(ctx);
            }
            igEndMenu();
        }
    } else {
        if (igMenuItem(__("Set from mirror"), "", false, true)) {
            auto cmds = cast(SetFrom1DMirrorCommand)bindingCommands[BindingCommand.SetFrom1DMirror];
            cmds.targetBindingsNull = targetBindings is null;
            cmds.run(ctx);
        }
    }

    if (igMenuItem(__("Copy"), "", false, true)) {
        bindingCommands[BindingCommand.CopyBinding].run(ctx);
    }

    if (igMenuItem(__("Paste"), "", false,  true)) {
        bindingCommands[BindingCommand.PasteBinding].run(ctx);
    }

}

void incBindingMenuContents(Parameter param, ParameterBinding[BindTarget] cSelectedBindings) {
    Context ctx = new Context();
    ctx.bindings = cSelectedBindings.values;
    ctx.puppet = incActivePuppet();
    ctx.parameters = [param];
    ctx.keyPoint = cParamPoint;

    if (igMenuItem(__("Remove"), "", false, true)) {
        bindingCommands[BindingCommand.RemoveBinding].run(ctx);
    }

    incKeypointActions(param, null, cSelectedBindings.values);

    if (igBeginMenu(__("Interpolation Mode"), true)) {
        if (igMenuItem(__("Nearest"), "", false, true)) {
            auto cmds = cast(SetInterpolationCommand)bindingCommands[BindingCommand.SetInterpolation];
            cmds.mode = InterpolateMode.Nearest;
            cmds.run(ctx);
        }
        if (igMenuItem(__("Linear"), "", false, true)) {
            auto cmds = cast(SetInterpolationCommand)bindingCommands[BindingCommand.SetInterpolation];
            cmds.mode = InterpolateMode.Linear;
            cmds.run(ctx);
        }
        if (igMenuItem(__("Cubic"), "", false, true)) {
            auto cmds = cast(SetInterpolationCommand)bindingCommands[BindingCommand.SetInterpolation];
            cmds.mode = InterpolateMode.Cubic;
            cmds.run(ctx);
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
                    incPushWindowList(new ParamPropWindow(param));
                }
                
                if (igMenuItem(__("Edit Axes Points"), "", false, true)) {
                    incPushWindowList(new ParamAxesWindow(param));
                }
                
                if (igMenuItem(__("Split"), "", false, true)) {
                    incPushWindowList(new ParamSplitWindow(idx, param));
                }

                if (!param.isVec2 && igMenuItem(__("To 2D"), "", false, true)) {
                    paramEditCommands[ParameditCommand.ConvertTo2DParam].run(ctx);
                }

                if (param.isVec2) {
                    if (igMenuItem(__("Flip X"), "", false, true)) {
                        paramEditCommands[ParameditCommand.FlipX].run(ctx);
                    }
                    if (igMenuItem(__("Flip Y"), "", false, true)) {
                        paramEditCommands[ParameditCommand.FlipY].run(ctx);
                    }
                } else {
                    if (igMenuItem(__("Flip"), "", false, true)) {
                        paramEditCommands[ParameditCommand.Flip1D].run(ctx);
                    }
                }
                if (igBeginMenu(__("Mirror"), true)) {
                    if (igMenuItem(__("Horizontally"), "", false, true)) {
                        paramEditCommands[ParameditCommand.MirrorHorizontally].run(ctx);
                    }
                    if (igMenuItem(__("Vertically"), "", false, true)) {
                        paramEditCommands[ParameditCommand.MirrorVertically].run(ctx);
                    }
                    igEndMenu();
                }
                if (igBeginMenu(__("Mirrored Autofill"), true)) {
                    if (igMenuItem("", "", false, true)) {
                        paramEditCommands[ParameditCommand.MirroredAutoFillDir1].run(ctx);
                    }
                    if (igMenuItem("", "", false, true)) {
                        paramEditCommands[ParameditCommand.MirroredAutoFillDir2].run(ctx);
                    }
                    if (param.isVec2) {
                        if (igMenuItem("", "", false, true)) {
                            paramEditCommands[ParameditCommand.MirroredAutoFillDir3].run(ctx);
                        }
                        if (igMenuItem("", "", false, true)) {
                            paramEditCommands[ParameditCommand.MirroredAutoFillDir4].run(ctx);
                        }
                    }
                    igEndMenu();
                }

                igNewLine();
                igSeparator();

                if (igMenuItem(__("Copy"), "", false, true)) {
                    paramEditCommands[ParameditCommand.CopyParameter].run(ctx);
                }
                if (igMenuItem(__("Paste"), "", false, true)) {
                    paramEditCommands[ParameditCommand.PasteParameter].run(ctx);
                }
                if (igMenuItem(__("Paste and Horizontal Flip"), "", false, true)) {
                    paramEditCommands[ParameditCommand.PasteParameterWithFlip].run(ctx);
                }

                if (igMenuItem(__("Duplicate"), "", false, true)) {
                    paramEditCommands[ParameditCommand.DuplicateParameter].run(ctx);
                }

                if (igMenuItem(__("Duplicate and Horizontal Flip"), "", false, true)) {
                    paramEditCommands[ParameditCommand.DuplicateParameterWithFlip].run(ctx);
                }

                if (igMenuItem(__("Delete"), "", false, true)) {
                    paramEditCommands[ParameditCommand.DeleteParameter].run(ctx);
                }

                igNewLine();
                igSeparator();

                void listParams(int fromAxis) {
                    foreach (p; incActivePuppet().parameters) {
                        if (param == p) continue;
                        if (p.isVec2) {
                            if (igBeginMenu(p.name.toStringz, true)) {
                                if (igMenuItem(__("X"))) {
                                    auto cmds = cast(LinkToCommand)paramEditCommands[ParameditCommand.LinkTo];
                                    cmds.toParam = p;
                                    cmds.fromAxis = fromAxis;
                                    cmds.toAxis = 0;
                                    cmds.run(ctx);
                                }
                                if (igMenuItem(__("Y"))) {
                                    auto cmds = cast(LinkToCommand)paramEditCommands[ParameditCommand.LinkTo];
                                    cmds.toParam = p;
                                    cmds.fromAxis = fromAxis;
                                    cmds.toAxis = 1;
                                    cmds.run(ctx);
                                }
                                igEndMenu();
                            } 
                        } else if (igMenuItem(p.name.toStringz, null, false, true)) {
                            auto cmds = cast(LinkToCommand)paramEditCommands[ParameditCommand.LinkTo];
                            cmds.toParam = p;
                            cmds.fromAxis = fromAxis;
                            cmds.toAxis = 0;
                            cmds.run(ctx);
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
                    paramEditCommands[ParameditCommand.SetStartingKeyFrame].run(ctx);
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
                paramEditCommands[ParameditCommand.ToggleParameterArm].run(ctx);
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
                    animEditCommands[AnimeditCommand.AddKeyFrame].run(ctx);
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
        foreach(child; group.children) {
            auto exChild = cast(ExParameter)child;
            exChild.setParent(null);
        }
        (cast(ExPuppet)incActivePuppet()).removeGroup(group);
        
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
        auto cmd = cast(Add1DParameterCommand)paramCommands[ParamCommand.Add1DParameter];
        cmd.min = 0;
        cmd.max = 1;
        cmd.run(ctx);
    }
    if (igMenuItem(__("Add 1D Parameter (-1..1)"), "", false, true)) {
        auto cmd = cast(Add1DParameterCommand)paramCommands[ParamCommand.Add1DParameter];
        cmd.min = -1;
        cmd.max = 1;
        cmd.run(ctx);
    }
    if (igMenuItem(__("Add 2D Parameter (0..1)"), "", false, true)) {
        auto cmd = cast(Add2DParameterCommand)paramCommands[ParamCommand.Add2DParameter];
        cmd.min = 0;
        cmd.max = 1;
        cmd.run(ctx);
    }
    if (igMenuItem(__("Add 2D Parameter (-1..+1)"), "", false, true)) {
        auto cmd = cast(Add2DParameterCommand)paramCommands[ParamCommand.Add2DParameter];
        cmd.min = -1;
        cmd.max = 1;
        cmd.run(ctx);
    }
    if (igMenuItem(__("Add Mouth Shape"), "", false, true)) {
        paramCommands[ParamCommand.AddMouthParameter].run(ctx);
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
        super("Parameters", _("Parameters"), false);
    }
}

vec2u incParamPoint() {
    return cParamPoint;
}

/**
    Generate logger frame
*/
mixin incPanel!ParametersPanel;
