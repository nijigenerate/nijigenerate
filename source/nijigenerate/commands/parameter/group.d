module nijigenerate.commands.parameter.group;

import nijigenerate.commands.base;
import nijigenerate.commands.parameter.base;
import nijilive;
import nijigenerate.ext;
import nijigenerate.widgets;
import nijigenerate.windows;
import nijigenerate.core;
import nijigenerate.project;
import i18n;


void incMoveParameter(Parameter from, ExParameterGroup to = null, int index = 0) {
    (cast(ExParameter)from).setParent(to);
}

ExParameterGroup incCreateParamGroup(int index = 0) {
    import std.array : insertInPlace;

    if (index < 0) index = 0;
    else if (index > incActivePuppet().parameters.length) index = cast(int)incActivePuppet().parameters.length-1;

    auto group = new ExParameterGroup(_("New Parameter Group"));
    (cast(ExPuppet)incActivePuppet()).addGroup(group);
    return group;
}



bool incParameterGropuMenuContents(ExParameterGroup group) {
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
