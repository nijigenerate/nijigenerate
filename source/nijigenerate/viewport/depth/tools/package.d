/*
    Depth edit tool registry.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools;

public import nijigenerate.viewport.depth.tools.attachedpoint;
public import nijigenerate.viewport.depth.tools.base;
public import nijigenerate.viewport.depth.camera;
public import nijigenerate.viewport.depth.tools.landmark;
public import nijigenerate.viewport.depth.tools.operation;
public import nijigenerate.viewport.depth.tools.plane;
public import nijigenerate.viewport.depth.tools.ring;
public import nijigenerate.viewport.depth.tools.select;

private DepthEditTool[] depthTools;

void incRegisterDepthEditTool(DepthEditTool tool) {
    foreach (registered; depthTools) {
        if (registered.mode == tool.mode) return;
    }
    depthTools ~= tool;
}

DepthEditTool[] incDepthEditTools() {
    if (depthTools.length == 0) {
        incRegisterDepthEditTool(new DepthSelectTool);
        incRegisterDepthEditTool(new DepthLandmarkTool);
        incRegisterDepthEditTool(new DepthRingTool);
        incRegisterDepthEditTool(new DepthAttachedPointTool);
        incRegisterDepthEditTool(new DepthPlaneTool);
    }
    return depthTools;
}

DepthEditTool incDepthEditTool(DepthToolMode mode) {
    foreach (tool; incDepthEditTools()) {
        if (tool.mode == mode) return tool;
    }
    return null;
}
