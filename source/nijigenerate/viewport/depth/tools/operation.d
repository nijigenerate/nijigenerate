/*
    Depth edit operation objects.

    Copyright © 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.depth.tools.operation;

import bindbc.opengl;
import nijigenerate.core.dbg;
import nijigenerate.ext.nodes.exdepthops;
import nijigenerate.viewport.depth.camera;
import nijigenerate.viewport.depth.mesheditor.node;
import nijilive;
import std.algorithm : clamp, max, min, sort;
import std.format : format;
import std.math : abs, cos, exp, pow, round, sin, sqrt;
import i18n;

enum vec4 DepthOperationColor = vec4(0.1, 0.4, 0.9, 0.9);
enum vec4 DepthOperationSelectedColor = vec4(0.1, 0.4, 0.9, 1.0);
enum vec4 DepthOperationPositiveColor = vec4(0.05, 0.62, 1.0, 0.92);
enum vec4 DepthOperationPositiveSelectedColor = vec4(0.0, 0.8, 1.0, 1.0);
enum vec4 DepthOperationNegativeColor = vec4(1.0, 0.32, 0.18, 0.92);
enum vec4 DepthOperationNegativeSelectedColor = vec4(1.0, 0.48, 0.22, 1.0);
enum vec4 DepthOperationHandleColor = vec4(0.1, 0.65, 1.0, 1.0);
enum vec4 DepthOperationAmountColor = vec4(1.0, 0.9, 0.2, 1.0);

float depthToolRound(float value) {
    return cast(float)(round(value * 1000.0f) / 1000.0f);
}

vec4 depthOperationColor(float depth, bool selected = false) {
    if (depth > 0.000001f) return selected ? DepthOperationPositiveSelectedColor : DepthOperationPositiveColor;
    if (depth < -0.000001f) return selected ? DepthOperationNegativeSelectedColor : DepthOperationNegativeColor;
    return selected ? DepthOperationSelectedColor : DepthOperationColor;
}

abstract class DepthOperation {
    abstract DepthOperation clone();
    abstract void apply(DepthMeshEditorOne editor);
    abstract void draw(DepthMeshEditorOne editor, ref DepthCamera3D depthCamera, bool selected, DepthOperationHandle hotHandle);
    abstract DepthOperationHandle hit(DepthMeshEditorOne editor, vec2 mouse, ref DepthCamera3D depthCamera, float radius, out float distance);
    abstract void drag(DepthOperationHandle handle, DepthMeshEditorOne editor, DepthOperation startOperation, vec2 startLocal, vec2 currentLocal, float startMouseY, float currentMouseY, bool snapToGrid);
    abstract string label();
    abstract string valueLabel();
}

enum DepthOperationHandle {
    None,
    Body,
    P0,
    P1,
    Amount,
    RadiusX,
    RadiusY,
    P0Angle,
    P1Angle,
}

float distanceToSegment(vec2 point, vec2 a, vec2 b) {
    auto v = b - a;
    auto len2 = v.x * v.x + v.y * v.y;
    if (len2 <= 0.000001f) return (point - a).length();
    auto t = clamp(((point.x - a.x) * v.x + (point.y - a.y) * v.y) / len2, 0.0f, 1.0f);
    auto closest = a + v * t;
    return (point - closest).length();
}

void drawDepthPoint(vec2 point, vec4 color, float size) {
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    inDbgSetBuffer(Vec3Array([vec3(point.x, point.y, 0)]));
    inDbgPointsSize(size);
    inDbgDrawPoints(color);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);
}

class DepthAttachedPointOperation : DepthOperation {
    size_t index;
    float amount;

    this(size_t index, float amount) {
        this.index = index;
        this.amount = amount;
    }

    override DepthOperation clone() {
        return new DepthAttachedPointOperation(index, amount);
    }

    override void apply(DepthMeshEditorOne editor) {
        editor.addDepth(index, amount);
    }

    override void draw(DepthMeshEditorOne editor, ref DepthCamera3D depthCamera, bool selected, DepthOperationHandle hotHandle) {
        auto point = editor.localVertex(index);
        auto base = editor.projectLocalPoint(point, 0, depthCamera);
        auto projected = editor.projectLocalPoint(point, amount, depthCamera);
        bool hotAmount = hotHandle == DepthOperationHandle.Amount;
        bool hotBody = hotHandle == DepthOperationHandle.Body;
        auto color = depthOperationColor(amount, selected || hotAmount);
        drawDepthLinePoints(base, projected, color, hotAmount ? 3.0f : 1.8f);
        if (selected || hotBody || hotAmount) {
            drawDepthPoint(base, hotBody ? DepthOperationAmountColor : DepthOperationHandleColor, hotBody ? 13 : 8);
        }
        drawDepthPoint(projected, hotAmount ? DepthOperationAmountColor : color, hotAmount ? 15 : (selected ? 13 : 9));
    }

    override DepthOperationHandle hit(DepthMeshEditorOne editor, vec2 mouse, ref DepthCamera3D depthCamera, float radius, out float distance) {
        auto local = editor.localVertex(index);
        auto point = editor.projectLocalPoint(local, amount, depthCamera);
        distance = (mouse - point).length();
        if (distance <= radius) return DepthOperationHandle.Amount;
        auto base = editor.projectLocalPoint(local, 0, depthCamera);
        distance = (mouse - base).length();
        if (distance <= radius) return DepthOperationHandle.Body;
        return distance <= radius ? DepthOperationHandle.Amount : DepthOperationHandle.None;
    }

    override void drag(DepthOperationHandle handle, DepthMeshEditorOne editor, DepthOperation startOperation, vec2 startLocal, vec2 currentLocal, float startMouseY, float currentMouseY, bool snapToGrid) {
        auto start = cast(DepthAttachedPointOperation)startOperation;
        if (start is null) return;
        final switch (handle) {
            case DepthOperationHandle.Body:
                auto nearest = editor.nearestLocalVertexIndex(currentLocal);
                if (nearest >= 0) index = cast(size_t)nearest;
                break;
            case DepthOperationHandle.Amount:
                amount = start.amount - (currentMouseY - startMouseY) * 0.006f;
                break;
            case DepthOperationHandle.None:
            case DepthOperationHandle.P0:
            case DepthOperationHandle.P1:
            case DepthOperationHandle.RadiusX:
            case DepthOperationHandle.RadiusY:
            case DepthOperationHandle.P0Angle:
            case DepthOperationHandle.P1Angle:
                break;
        }
    }

    override string label() {
        return _("attached point %s").format(index);
    }

    override string valueLabel() {
        return "%.2f".format(amount);
    }
}

class DepthRingOperation : DepthOperation {
    vec2 p0;
    vec2 p1;
    float amount;
    float width;
    float hardness;
    float p0Angle = 180.0f;
    float p1Angle = 0.0f;

    this(vec2 p0, vec2 p1, DepthBrushSettings settings) {
        this.p0 = p0;
        this.p1 = p1;
        this.amount = settings.amount;
        this.width = settings.radiusY;
        this.hardness = settings.hardness;
    }

    override DepthOperation clone() {
        auto settings = DepthBrushSettings();
        settings.amount = amount;
        settings.radiusY = width;
        settings.hardness = hardness;
        auto result = new DepthRingOperation(p0, p1, settings);
        result.p0Angle = p0Angle;
        result.p1Angle = p1Angle;
        return result;
    }

    override void apply(DepthMeshEditorOne editor) {
        applyRingNormalSurfaces(editor, [this]);
    }

    override void draw(DepthMeshEditorOne editor, ref DepthCamera3D depthCamera, bool selected, DepthOperationHandle hotHandle) {
        bool hotBody = hotHandle == DepthOperationHandle.Body;
        auto color = depthOperationColor(amount, selected || hotBody);
        drawDepthRingCurve(editor, this, depthCamera, false, color, hotBody ? 4.5f : 2.5f);
        drawDepthLinePoints(ringEndpointPoint(editor, this, true, depthCamera), ringEndpointPoint(editor, this, false, depthCamera), vec4(0.55, 0.55, 0.55, 0.55), 1.2f);
        if (selected || hotHandle != DepthOperationHandle.None) {
            auto p0h = ringAngleHandlePoint(editor, this, true, depthCamera);
            auto p1h = ringAngleHandlePoint(editor, this, false, depthCamera);
            drawDepthLinePoints(ringEndpointPoint(editor, this, true, depthCamera), p0h, DepthOperationHandleColor, hotHandle == DepthOperationHandle.P0Angle ? 2.8f : 1.4f);
            drawDepthLinePoints(ringEndpointPoint(editor, this, false, depthCamera), p1h, DepthOperationHandleColor, hotHandle == DepthOperationHandle.P1Angle ? 2.8f : 1.4f);
            drawDepthPoint(ringEndpointPoint(editor, this, true, depthCamera), hotHandle == DepthOperationHandle.P0 ? DepthOperationAmountColor : DepthOperationHandleColor, hotHandle == DepthOperationHandle.P0 ? 13 : 9);
            drawDepthPoint(ringEndpointPoint(editor, this, false, depthCamera), hotHandle == DepthOperationHandle.P1 ? DepthOperationAmountColor : DepthOperationHandleColor, hotHandle == DepthOperationHandle.P1 ? 13 : 9);
            drawDepthPoint(ringAmountHandlePoint(editor, this, depthCamera), hotHandle == DepthOperationHandle.Amount ? DepthOperationAmountColor : color, hotHandle == DepthOperationHandle.Amount ? 14 : 10);
            drawDepthPoint(p0h, hotHandle == DepthOperationHandle.P0Angle ? DepthOperationAmountColor : DepthOperationHandleColor, hotHandle == DepthOperationHandle.P0Angle ? 13 : 8);
            drawDepthPoint(p1h, hotHandle == DepthOperationHandle.P1Angle ? DepthOperationAmountColor : DepthOperationHandleColor, hotHandle == DepthOperationHandle.P1Angle ? 13 : 8);
        }
    }

    override DepthOperationHandle hit(DepthMeshEditorOne editor, vec2 mouse, ref DepthCamera3D depthCamera, float radius, out float distance) {
        auto h0 = ringEndpointPoint(editor, this, true, depthCamera);
        auto h1 = ringEndpointPoint(editor, this, false, depthCamera);
        auto hm = ringAmountHandlePoint(editor, this, depthCamera);
        auto ha0 = ringAngleHandlePoint(editor, this, true, depthCamera);
        auto ha1 = ringAngleHandlePoint(editor, this, false, depthCamera);
        distance = (mouse - hm).length();
        if (distance <= radius) return DepthOperationHandle.Amount;
        distance = (mouse - ha0).length();
        if (distance <= radius) return DepthOperationHandle.P0Angle;
        distance = (mouse - ha1).length();
        if (distance <= radius) return DepthOperationHandle.P1Angle;
        distance = (mouse - h0).length();
        if (distance <= radius) return DepthOperationHandle.P0;
        distance = (mouse - h1).length();
        if (distance <= radius) return DepthOperationHandle.P1;
        distance = ringCurveDistance(editor, this, mouse, depthCamera);
        return distance <= radius ? DepthOperationHandle.Body : DepthOperationHandle.None;
    }

    override void drag(DepthOperationHandle handle, DepthMeshEditorOne editor, DepthOperation startOperation, vec2 startLocal, vec2 currentLocal, float startMouseY, float currentMouseY, bool snapToGrid) {
        auto start = cast(DepthRingOperation)startOperation;
        if (start is null) return;
        final switch (handle) {
            case DepthOperationHandle.P0:
                p0 = snapToGrid ? editor.snapLocalPoint(currentLocal) : currentLocal;
                break;
            case DepthOperationHandle.P1:
                p1 = snapToGrid ? editor.snapLocalPoint(currentLocal) : currentLocal;
                break;
            case DepthOperationHandle.Amount:
                amount = start.amount - (currentMouseY - startMouseY) * 0.006f;
                break;
            case DepthOperationHandle.P0Angle:
                p0Angle = round(start.p0Angle - (currentMouseY - startMouseY) * 0.7f);
                break;
            case DepthOperationHandle.P1Angle:
                p1Angle = round(start.p1Angle - (currentMouseY - startMouseY) * 0.7f);
                break;
            case DepthOperationHandle.Body:
                auto delta = currentLocal - startLocal;
                p0 = start.p0 + delta;
                p1 = start.p1 + delta;
                if (snapToGrid) {
                    p0 = editor.snapLocalPoint(p0);
                    p1 = editor.snapLocalPoint(p1);
                }
                break;
            case DepthOperationHandle.None:
            case DepthOperationHandle.RadiusX:
            case DepthOperationHandle.RadiusY:
                break;
        }
    }

    override string label() {
        return _("ring %.0f,%.0f").format(p0.x, p0.y);
    }

    override string valueLabel() {
        return "%.2f".format(amount);
    }
}

class DepthPlaneOperation : DepthOperation {
    vec2 center;
    float radiusX;
    float radiusY;
    float angle;
    float targetDepth;
    float flattenStrength;

    this(vec2 center, float radiusX, float radiusY, DepthBrushSettings settings) {
        this.center = center;
        this.radiusX = radiusX;
        this.radiusY = radiusY;
        this.angle = settings.angle;
        this.targetDepth = settings.amount;
        this.flattenStrength = settings.flattenStrength;
    }

    override DepthOperation clone() {
        auto settings = DepthBrushSettings();
        settings.angle = angle;
        settings.amount = targetDepth;
        settings.flattenStrength = flattenStrength;
        return new DepthPlaneOperation(center, radiusX, radiusY, settings);
    }

    override void apply(DepthMeshEditorOne editor) {
        applyPlaneFlatten(editor, center, radiusX, radiusY, angle, targetDepth, flattenStrength);
    }

    override void draw(DepthMeshEditorOne editor, ref DepthCamera3D depthCamera, bool selected, DepthOperationHandle hotHandle) {
        bool hotBody = hotHandle == DepthOperationHandle.Body;
        auto color = depthOperationColor(targetDepth, selected || hotBody);
        drawDepthEllipse(editor, center, radiusX, radiusY, angle, targetDepth, depthCamera, color, hotBody ? 4.0f : 2.0f);
        if (selected || hotHandle != DepthOperationHandle.None) {
            auto angleRad = angle * 3.14159265358979323846f / 180.0f;
            auto ux = vec2(cos(angleRad), sin(angleRad));
            auto uy = vec2(-sin(angleRad), cos(angleRad));
            drawDepthPoint(editor.projectLocalPoint(center, targetDepth, depthCamera), hotHandle == DepthOperationHandle.Amount ? DepthOperationAmountColor : color, hotHandle == DepthOperationHandle.Amount ? 14 : 10);
            drawDepthPoint(editor.projectLocalPoint(center + ux * radiusX, targetDepth, depthCamera), hotHandle == DepthOperationHandle.RadiusX ? DepthOperationAmountColor : DepthOperationHandleColor, hotHandle == DepthOperationHandle.RadiusX ? 13 : 9);
            drawDepthPoint(editor.projectLocalPoint(center + uy * radiusY, targetDepth, depthCamera), hotHandle == DepthOperationHandle.RadiusY ? DepthOperationAmountColor : DepthOperationHandleColor, hotHandle == DepthOperationHandle.RadiusY ? 13 : 9);
        }
    }

    override DepthOperationHandle hit(DepthMeshEditorOne editor, vec2 mouse, ref DepthCamera3D depthCamera, float radius, out float distance) {
        auto angleRad = angle * 3.14159265358979323846f / 180.0f;
        auto ux = vec2(cos(angleRad), sin(angleRad));
        auto uy = vec2(-sin(angleRad), cos(angleRad));
        auto amountHandle = editor.projectLocalPoint(center, targetDepth, depthCamera);
        distance = (mouse - amountHandle).length();
        if (distance <= radius) return DepthOperationHandle.Amount;
        auto rxHandle = editor.projectLocalPoint(center + ux * radiusX, targetDepth, depthCamera);
        distance = (mouse - rxHandle).length();
        if (distance <= radius) return DepthOperationHandle.RadiusX;
        auto ryHandle = editor.projectLocalPoint(center + uy * radiusY, targetDepth, depthCamera);
        distance = (mouse - ryHandle).length();
        if (distance <= radius) return DepthOperationHandle.RadiusY;
        auto centerHandle = editor.projectLocalPoint(center, 0, depthCamera);
        distance = (mouse - centerHandle).length();
        if (distance <= radius) return DepthOperationHandle.Body;
        return DepthOperationHandle.None;
    }

    override void drag(DepthOperationHandle handle, DepthMeshEditorOne editor, DepthOperation startOperation, vec2 startLocal, vec2 currentLocal, float startMouseY, float currentMouseY, bool snapToGrid) {
        auto start = cast(DepthPlaneOperation)startOperation;
        if (start is null) return;
        final switch (handle) {
            case DepthOperationHandle.Body:
                center = start.center + (currentLocal - startLocal);
                break;
            case DepthOperationHandle.Amount:
                targetDepth = start.targetDepth - (currentMouseY - startMouseY) * 0.006f;
                break;
            case DepthOperationHandle.RadiusX:
                radiusX = max(1.0f, abs(currentLocal.x - center.x));
                break;
            case DepthOperationHandle.RadiusY:
                radiusY = max(1.0f, abs(currentLocal.y - center.y));
                break;
            case DepthOperationHandle.None:
            case DepthOperationHandle.P0:
            case DepthOperationHandle.P1:
            case DepthOperationHandle.P0Angle:
            case DepthOperationHandle.P1Angle:
                break;
        }
    }

    override string label() {
        return _("plane %.0f,%.0f").format(center.x, center.y);
    }

    override string valueLabel() {
        return "%.2f".format(targetDepth);
    }
}

ExDepthOp toExDepthOp(DepthOperation operation) {
    ExDepthOp result;
    if (auto attached = cast(DepthAttachedPointOperation)operation) {
        result.type = ExDepthOpType.AttachedPoint;
        result.index = attached.index;
        result.amount = attached.amount;
    } else if (auto ring = cast(DepthRingOperation)operation) {
        result.type = ExDepthOpType.Ring;
        result.p0 = ring.p0;
        result.p1 = ring.p1;
        result.amount = ring.amount;
        result.width = ring.width;
        result.hardness = ring.hardness;
        result.p0Angle = ring.p0Angle;
        result.p1Angle = ring.p1Angle;
    } else if (auto plane = cast(DepthPlaneOperation)operation) {
        result.type = ExDepthOpType.Plane;
        result.center = plane.center;
        result.radiusX = plane.radiusX;
        result.radiusY = plane.radiusY;
        result.angle = plane.angle;
        result.targetDepth = plane.targetDepth;
        result.flattenStrength = plane.flattenStrength;
    }
    return result;
}

DepthOperation depthOperationFromExDepthOp(ExDepthOp op) {
    final switch (op.type) {
        case ExDepthOpType.AttachedPoint:
            return new DepthAttachedPointOperation(op.index, op.amount);

        case ExDepthOpType.Ring:
            auto settings = DepthBrushSettings();
            settings.amount = op.amount;
            settings.radiusY = op.width;
            settings.hardness = op.hardness;
            auto result = new DepthRingOperation(op.p0, op.p1, settings);
            result.p0Angle = op.p0Angle;
            result.p1Angle = op.p1Angle;
            return result;

        case ExDepthOpType.Plane:
            auto settings = DepthBrushSettings();
            settings.amount = op.targetDepth;
            settings.angle = op.angle;
            settings.flattenStrength = op.flattenStrength;
            return new DepthPlaneOperation(op.center, op.radiusX, op.radiusY, settings);
    }
}

void applyRingDepth(DepthMeshEditorOne editor, vec2 p0, vec2 p1, float amount, float width, float hardness) {
    auto vertices = editor.getVertices();
    auto vx = p1.x - p0.x;
    auto vy = p1.y - p0.y;
    auto len = max(1.0f, sqrt(vx * vx + vy * vy));
    auto ux = vx / len;
    auto uy = vy / len;
    auto half = len * 0.5f;
    auto cx = (p0.x + p1.x) * 0.5f;
    auto cy = (p0.y + p1.y) * 0.5f;
    width = max(0.5f, width);
    hardness = max(0.1f, hardness);

    foreach (i, point; vertices) {
        auto px = point.x - cx;
        auto py = point.y - cy;
        auto along = px * ux + py * uy;
        auto side = abs(-px * uy + py * ux);
        if (abs(along) > half || side > width * 2.25f) continue;

        auto t = along / max(1.0f, half);
        auto ellipse = sqrt(max(0.0f, 1.0f - t * t));
        auto sideFalloff = exp(-pow(side / width, hardness) * 2.2f);
        editor.addDepth(i, amount * ellipse * sideFalloff);
    }
}

enum RingOrientation {
    Horizontal,
    Vertical,
}

struct RingSample {
    float cross;
    float z;
    float normalSlope;
    float width;
    float hardness;
    size_t count;
}

RingOrientation ringOrientation(DepthRingOperation op) {
    return abs(op.p1.x - op.p0.x) >= abs(op.p1.y - op.p0.y)
        ? RingOrientation.Horizontal
        : RingOrientation.Vertical;
}

float ringCrossCoord(DepthRingOperation op, char axis) {
    return axis == 'x'
        ? (op.p0.y + op.p1.y) * 0.5f
        : (op.p0.x + op.p1.x) * 0.5f;
}

bool ringSampleAt(DepthRingOperation op, vec2 point, char axis, out RingSample sample) {
    auto a = op.p0;
    auto b = op.p1;
    auto along = axis == 'x' ? point.x : point.y;
    auto aAlong = axis == 'x' ? a.x : a.y;
    auto bAlong = axis == 'x' ? b.x : b.y;
    auto minAlong = min(aAlong, bAlong);
    auto maxAlong = max(aAlong, bAlong);
    if (along < minAlong || along > maxAlong) return false;

    auto span = bAlong - aAlong;
    if (abs(span) < 0.000001f) return false;

    auto ratio = clamp((along - aAlong) / span, 0.0f, 1.0f);
    auto angle0 = op.p0Angle * 3.14159265358979323846f / 180.0f;
    auto angle1 = op.p1Angle * 3.14159265358979323846f / 180.0f;
    auto angle = angle0 + (angle1 - angle0) * ratio;
    auto tangentSlope = op.amount * cos(angle) * (angle1 - angle0) / span;

    sample = RingSample(
        ringCrossCoord(op, axis),
        op.amount * sin(angle),
        -tangentSlope,
        max(1.0f, op.width),
        max(0.1f, op.hardness),
        1
    );
    return true;
}

RingSample[] combineRingSamplesByCross(RingSample[] samples) {
    RingSample[] groups;
    enum epsilon = 0.0001f;
    foreach (sample; samples) {
        ptrdiff_t groupIndex = -1;
        foreach (i, group; groups) {
            if (abs(group.cross - sample.cross) <= epsilon) {
                groupIndex = cast(ptrdiff_t)i;
                break;
            }
        }
        if (groupIndex < 0) {
            groups ~= sample;
            continue;
        }
        auto group = groups[groupIndex];
        auto count = group.count;
        group.z += sample.z;
        group.normalSlope += sample.normalSlope;
        group.width = max(group.width, sample.width);
        group.hardness = (group.hardness * count + sample.hardness) / cast(float)(count + 1);
        group.count = count + 1;
        groups[groupIndex] = group;
    }
    return groups;
}

float hermiteDepth(float z0, float m0, float z1, float m1, float span, float t) {
    auto t2 = t * t;
    auto t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * z0
        + (t3 - 2 * t2 + t) * m0 * span
        + (-2 * t3 + 3 * t2) * z1
        + (t3 - t2) * m1 * span;
}

void applyRingFamilySurface(DepthMeshEditorOne editor, DepthRingOperation[] rings, char axis) {
    if (rings.length == 0) return;
    sort!((a, b) => ringCrossCoord(a, axis) < ringCrossCoord(b, axis))(rings);
    auto vertices = editor.getVertices();
    foreach (i, point; vertices) {
        RingSample[] samples;
        foreach (op; rings) {
            RingSample sample;
            if (ringSampleAt(op, point, axis, sample)) samples ~= sample;
        }
        if (samples.length == 0) continue;
        sort!((a, b) => a.cross < b.cross)(samples);
        auto candidates = combineRingSamplesByCross(samples);
        if (candidates.length == 0) continue;

        auto cross = axis == 'x' ? point.y : point.x;
        ptrdiff_t beforeIndex = -1;
        ptrdiff_t afterIndex = -1;
        foreach (j, sample; candidates) {
            if (sample.cross <= cross) beforeIndex = cast(ptrdiff_t)j;
            if (afterIndex < 0 && sample.cross >= cross) afterIndex = cast(ptrdiff_t)j;
        }

        float value;
        if (beforeIndex >= 0 && afterIndex >= 0 && beforeIndex != afterIndex) {
            auto before = candidates[beforeIndex];
            auto after = candidates[afterIndex];
            auto span = max(0.000001f, after.cross - before.cross);
            auto t = clamp((cross - before.cross) / span, 0.0f, 1.0f);
            value = hermiteDepth(before.z, before.normalSlope, after.z, after.normalSlope, span, t);
        } else {
            auto nearest = candidates[beforeIndex >= 0 ? beforeIndex : afterIndex];
            auto distance = abs(cross - nearest.cross);
            auto falloff = exp(-pow(distance / max(1.0f, nearest.width), nearest.hardness) * 2.2f);
            value = nearest.z * falloff;
        }

        if (abs(value) > 0.000001f) editor.addDepth(i, value);
    }
}

void applyRingNormalSurfaces(DepthMeshEditorOne editor, DepthRingOperation[] rings) {
    DepthRingOperation[] horizontal;
    DepthRingOperation[] vertical;
    foreach (op; rings) {
        final switch (ringOrientation(op)) {
            case RingOrientation.Horizontal:
                horizontal ~= op;
                break;
            case RingOrientation.Vertical:
                vertical ~= op;
                break;
        }
    }
    applyRingFamilySurface(editor, horizontal, 'x');
    applyRingFamilySurface(editor, vertical, 'y');
}

float ringBaseDepthAt(DepthMeshEditorOne editor, DepthRingOperation op, float ratio) {
    auto d0 = editor.depthAtLocalPoint(op.p0);
    auto d1 = editor.depthAtLocalPoint(op.p1);
    return d0 + (d1 - d0) * ratio;
}

vec2 ringEndpointPoint(DepthMeshEditorOne editor, DepthRingOperation op, bool first, ref DepthCamera3D depthCamera) {
    auto point = first ? op.p0 : op.p1;
    return editor.projectLocalPoint(point, editor.depthAtLocalPoint(point), depthCamera);
}

vec2 ringCurvePoint(DepthMeshEditorOne editor, DepthRingOperation op, float ratio, bool backSide, ref DepthCamera3D depthCamera) {
    auto angle0 = op.p0Angle * 3.14159265358979323846f / 180.0f + (backSide ? 3.14159265358979323846f : 0.0f);
    auto angle1 = op.p1Angle * 3.14159265358979323846f / 180.0f + (backSide ? 3.14159265358979323846f : 0.0f);
    auto angle = angle0 + (angle1 - angle0) * ratio;
    auto point = op.p0 + (op.p1 - op.p0) * ratio;
    return editor.projectLocalPoint(point, ringBaseDepthAt(editor, op, ratio) + sin(angle) * op.amount, depthCamera);
}

vec2 ringAmountHandlePoint(DepthMeshEditorOne editor, DepthRingOperation op, ref DepthCamera3D depthCamera) {
    auto angle = ((op.p0Angle + op.p1Angle) * 0.5f) * 3.14159265358979323846f / 180.0f;
    auto point = (op.p0 + op.p1) * 0.5f;
    return editor.projectLocalPoint(point, ringBaseDepthAt(editor, op, 0.5f) + sin(angle) * op.amount, depthCamera);
}

vec2 ringAngleHandlePoint(DepthMeshEditorOne editor, DepthRingOperation op, bool first, ref DepthCamera3D depthCamera) {
    auto tangent = op.p1 - op.p0;
    auto len = max(1.0f, tangent.length());
    tangent = tangent / len;
    auto angle = (first ? op.p0Angle : op.p1Angle) * 3.14159265358979323846f / 180.0f;
    auto base = first ? op.p0 : op.p1;
    auto localRadius = editor.depthDisplayScale() * (0.17f / DepthDisplayZScale);
    auto depthRadius = 0.22f / DepthDisplayZScale;
    return editor.projectLocalPoint(
        base + tangent * (cos(angle) * localRadius),
        editor.depthAtLocalPoint(base) + sin(angle) * depthRadius,
        depthCamera
    );
}

float ringCurveDistance(DepthMeshEditorOne editor, DepthRingOperation op, vec2 mouse, ref DepthCamera3D depthCamera) {
    enum segments = 48;
    float best = float.max;
    foreach (backSide; 0 .. 2) {
        auto prev = ringCurvePoint(editor, op, 0, backSide == 1, depthCamera);
        foreach (i; 1 .. segments + 1) {
            auto ratio = cast(float)i / segments;
            auto next = ringCurvePoint(editor, op, ratio, backSide == 1, depthCamera);
            best = min(best, distanceToSegment(mouse, prev, next));
            prev = next;
        }
    }
    return best;
}

void drawDepthPolyline(Vec3Array points, vec4 color, float width) {
    if (points.length < 2) return;
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    Vec3Array linePoints;
    foreach (i; 0 .. points.length - 1) {
        linePoints ~= points[i];
        linePoints ~= points[i + 1];
    }
    inDbgSetBuffer(linePoints);
    inDbgLineWidth(width);
    inDbgDrawLines(color);
    inDbgLineWidth(1.0f);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);
}

void drawDepthRingCurve(DepthMeshEditorOne editor, DepthRingOperation op, ref DepthCamera3D depthCamera, bool backSide, vec4 color, float width) {
    enum segments = 48;
    Vec3Array points;
    foreach (i; 0 .. segments + 1) {
        auto projected = ringCurvePoint(editor, op, cast(float)i / segments, backSide, depthCamera);
        points ~= vec3(projected.x, projected.y, 0);
    }
    drawDepthPolyline(points, color, width);
}

float ellipseNorm(vec2 point, vec2 center, float radiusX, float radiusY, float angleDeg) {
    auto angle = -angleDeg * 3.14159265358979323846f / 180.0f;
    auto dx = point.x - center.x;
    auto dy = point.y - center.y;
    auto x = cos(angle) * dx - sin(angle) * dy;
    auto y = sin(angle) * dx + cos(angle) * dy;
    return sqrt((x / max(1.0f, radiusX)) ^^ 2 + (y / max(1.0f, radiusY)) ^^ 2);
}

void applyPlaneFlatten(DepthMeshEditorOne editor, vec2 center, float radiusX, float radiusY, float angle, float targetDepth, float flattenStrength) {
    auto vertices = editor.getVertices();
    float[] depths = editor.copyEditorDepths();
    float ringSum;
    size_t ringCount;
    float insideSum;
    size_t insideCount;

    foreach (i, point; vertices) {
        auto n = ellipseNorm(point, center, radiusX, radiusY, angle);
        if (n <= 1.0f) {
            insideSum += depths[i];
            insideCount++;
        } else if (n <= 1.35f) {
            ringSum += depths[i];
            ringCount++;
        }
    }
    if (insideCount == 0) return;

    auto target = ringCount > 0 ? ringSum / cast(float)ringCount : insideSum / cast(float)insideCount;
    if (targetDepth != 0) target = targetDepth;
    foreach (i, point; vertices) {
        auto n = ellipseNorm(point, center, radiusX, radiusY, angle);
        if (n > 1.0f) continue;
        auto centerWeight = 1.0f - clamp(n, 0.0f, 1.0f);
        auto strength = clamp(flattenStrength, 0.0f, 1.0f) * (0.45f + centerWeight * 0.55f);
        editor.setDepth(i, depths[i] * (1.0f - strength) + target * strength);
    }
}

void drawDepthLine(DepthMeshEditorOne editor, vec2 p0, vec2 p1, ref DepthCamera3D depthCamera, vec4 color, float width = 2.5f) {
    drawDepthLinePoints(editor.projectLocalPoint(p0, 0, depthCamera), editor.projectLocalPoint(p1, 0, depthCamera), color, width);
}

void drawDepthLinePoints(vec2 p0, vec2 p1, vec4 color, float width = 2.5f) {
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    Vec3Array points;
    points ~= vec3(p0.x, p0.y, 0);
    points ~= vec3(p1.x, p1.y, 0);
    inDbgSetBuffer(points);
    inDbgLineWidth(width);
    inDbgDrawLines(color);
    inDbgLineWidth(1.0f);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);
}

void drawDepthEllipse(DepthMeshEditorOne editor, vec2 center, float radiusX, float radiusY, float angleDeg, float depth, ref DepthCamera3D depthCamera, vec4 color, float width = 2.0f) {
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    Vec3Array points;
    auto angle = angleDeg * 3.14159265358979323846f / 180.0f;
    auto ux = vec2(cos(angle), sin(angle));
    auto uy = vec2(-sin(angle), cos(angle));
    enum segments = 48;
    foreach (i; 0 .. segments) {
        auto a0 = cast(float)i / segments * 2.0f * 3.14159265358979323846f;
        auto a1 = cast(float)(i + 1) / segments * 2.0f * 3.14159265358979323846f;
        auto l0 = center + ux * (cos(a0) * radiusX) + uy * (sin(a0) * radiusY);
        auto l1 = center + ux * (cos(a1) * radiusX) + uy * (sin(a1) * radiusY);
        auto w0 = editor.projectLocalPoint(l0, depth, depthCamera);
        auto w1 = editor.projectLocalPoint(l1, depth, depthCamera);
        points ~= vec3(w0.x, w0.y, 0);
        points ~= vec3(w1.x, w1.y, 0);
    }
    inDbgSetBuffer(points);
    inDbgLineWidth(width);
    inDbgDrawLines(color);
    inDbgLineWidth(1.0f);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);
}
