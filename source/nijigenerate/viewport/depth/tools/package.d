/*
    Depth edit tool registry.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools;

public import nijigenerate.viewport.depth.tools.attachedpoint;
public import nijigenerate.viewport.depth.tools.base;
public import nijigenerate.viewport.depth.camera;
public import nijigenerate.viewport.depth.tools.directdepth;
public import nijigenerate.viewport.depth.tools.landmark;
public import nijigenerate.viewport.depth.tools.operation;
public import nijigenerate.viewport.depth.tools.plane;
public import nijigenerate.viewport.depth.tools.ring;
public import nijigenerate.viewport.depth.tools.select;

private DepthEditTool[] depthTools;

void ngRegisterDepthEditTool(DepthEditTool tool) {
    foreach (registered; depthTools) {
        if (registered.mode == tool.mode) return;
    }
    depthTools ~= tool;
}

DepthEditTool[] ngDepthEditTools() {
    if (depthTools.length == 0) {
        ngRegisterDepthEditTool(new DepthSelectTool);
        ngRegisterDepthEditTool(new DepthDirectDepthTool);
        ngRegisterDepthEditTool(new DepthLandmarkTool);
        ngRegisterDepthEditTool(new DepthRingTool);
        ngRegisterDepthEditTool(new DepthAttachedPointTool);
        ngRegisterDepthEditTool(new DepthPlaneTool);
    }
    return depthTools;
}

DepthEditTool ngDepthEditTool(DepthToolMode mode) {
    foreach (tool; ngDepthEditTools()) {
        if (tool.mode == mode) return tool;
    }
    return null;
}
