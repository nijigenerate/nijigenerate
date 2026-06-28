/*
    Depth editing shared types.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.camera;

import nijigenerate;
import nijigenerate.project;
import nijigenerate.viewport.base;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import std.algorithm : clamp, max, min;
import std.math : abs, cos, sin;

enum DepthToolMode {
    DirectDepth,
    Ring,
    AttachedPoint,
    Plane,
}

struct DepthBrushSettings {
    bool snapToGrid = true;
    float amount = 0.18f;
    float radiusX = 58.0f;
    float radiusY = 42.0f;
    float angle = 0.0f;
    float hardness = 1.85f;
    float flattenStrength = 0.75f;
}

struct DepthCamera3D {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float zoom = 1.0f;
    vec2 pan = vec2(0);
}

void focusGridOrigin(GridDeformer grid) {
    if (grid is null) return;
    int width;
    int height;
    inGetViewport(width, height);

    float largestBounds = 1.0f;
    if (grid.vertices.length > 0) {
        auto first = grid.vertices[0];
        auto minPoint = vec2(first.x, first.y);
        auto maxPoint = minPoint;
        foreach (v; grid.vertices) {
            minPoint.x = min(minPoint.x, v.x);
            minPoint.y = min(minPoint.y, v.y);
            maxPoint.x = max(maxPoint.x, v.x);
            maxPoint.y = max(maxPoint.y, v.y);
        }
        auto size = maxPoint - minPoint;
        largestBounds = max(1.0f, max(size.x, size.y));
    }

    float largestViewport = max(1.0f, cast(float)min(width, height));
    float factor = largestViewport / largestBounds;
    if (auto project = incActiveProject()) {
        project.CameraFocused.emit(clamp(factor * 0.90f, 0.1f, 2.5f), vec2(0, 0));
    }
}

vec2 projectDepthPoint(vec2 point, float depth, ref DepthCamera3D camera) {
    float cy = cos(camera.yaw);
    float sy = sin(camera.yaw);
    float cp = cos(camera.pitch);
    float sp = sin(camera.pitch);

    float x = point.x;
    float y = point.y;
    float z = depth;

    float rx = x * cy + z * sy;
    float rz = -x * sy + z * cy;
    float ry = y * cp - rz * sp;

    return vec2(rx, ry) * camera.zoom + camera.pan;
}

vec2 unprojectDepthPoint(vec2 point, float depth, ref DepthCamera3D camera) {
    float cy = cos(camera.yaw);
    float sy = sin(camera.yaw);
    float cp = cos(camera.pitch);
    float sp = sin(camera.pitch);
    if (abs(cy) < 0.0001f) cy = cy < 0 ? -0.0001f : 0.0001f;
    if (abs(cp) < 0.0001f) cp = cp < 0 ? -0.0001f : 0.0001f;

    vec2 normalized = (point - camera.pan) / camera.zoom;
    float x = (normalized.x - depth * sy) / cy;
    float rz = -x * sy + depth * cy;
    float y = (normalized.y + rz * sp) / cp;
    return vec2(x, y);
}
