module nijigenerate.viewport.depth.common.targetview;

import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.viewport.depth.camera;
import nijilive;
import std.algorithm : max, min, sort, uniq;
import std.array : array;
import std.math : abs, isFinite, round;

enum DepthTargetDisplayPlaneSize = 2.9f;
enum DepthTargetDisplayZScale = 0.42f;

float ngDepthDisplayScaleForBounds(vec2 minPoint, vec2 maxPoint) {
    auto size = maxPoint - minPoint;
    return max(1.0f, max(size.x, size.y) * (DepthTargetDisplayZScale / DepthTargetDisplayPlaneSize));
}

float ngDepthDisplayScaleForDocument(int width, int height) {
    return ngDepthDisplayScaleForBounds(vec2(0, 0), vec2(cast(float)width, cast(float)height));
}

float ngDepthDisplayScaleForTargets(Deformable[] targets) {
    bool hasBounds;
    vec2 minPoint;
    vec2 maxPoint;
    foreach (target; targets) {
        if (target is null) continue;
        foreach (vertex; target.vertices) {
            if (!hasBounds) {
                minPoint = vertex;
                maxPoint = vertex;
                hasBounds = true;
            } else {
                minPoint.x = min(minPoint.x, vertex.x);
                minPoint.y = min(minPoint.y, vertex.y);
                maxPoint.x = max(maxPoint.x, vertex.x);
                maxPoint.y = max(maxPoint.y, vertex.y);
            }
        }
    }
    return hasBounds ? ngDepthDisplayScaleForBounds(minPoint, maxPoint) : 0.0f;
}

float ngDepthDisplayScaleForTargetsInNodeSpace(Node root, Deformable[] targets) {
    bool hasBounds;
    vec2 minPoint;
    vec2 maxPoint;
    auto rootInverse = root is null ? mat4.identity : root.transform.matrix.inverse;
    foreach (target; targets) {
        auto targetNode = cast(Node)target;
        if (target is null || targetNode is null) continue;
        auto targetToRoot = rootInverse * targetNode.transform.matrix;
        foreach (vertex; target.vertices) {
            auto transformed = targetToRoot * vec4(vertex.x, vertex.y, 0.0f, 1.0f);
            if (!transformed.x.isFinite || !transformed.y.isFinite) continue;
            auto point = vec2(transformed.x, transformed.y);
            if (!hasBounds) {
                minPoint = point;
                maxPoint = point;
                hasBounds = true;
            } else {
                minPoint.x = min(minPoint.x, point.x);
                minPoint.y = min(minPoint.y, point.y);
                maxPoint.x = max(maxPoint.x, point.x);
                maxPoint.y = max(maxPoint.y, point.y);
            }
        }
    }
    return hasBounds ? ngDepthDisplayScaleForBounds(minPoint, maxPoint) : 0.0f;
}

float ngDepthDisplayScaleForTarget(Deformable target) {
    if (target is null || target.vertices.length == 0) return 1.0f;
    auto first = target.vertices[0];
    auto minPoint = vec2(first.x, first.y);
    auto maxPoint = minPoint;
    foreach (i, vertex; target.vertices) {
        if (i == 0) continue;
        minPoint.x = min(minPoint.x, vertex.x);
        minPoint.y = min(minPoint.y, vertex.y);
        maxPoint.x = max(maxPoint.x, vertex.x);
        maxPoint.y = max(maxPoint.y, vertex.y);
    }
    return ngDepthDisplayScaleForBounds(minPoint, maxPoint);
}

float ngDepthTargetRoundDepth(float value) {
    return cast(float)(round(value * 1000.0f) / 1000.0f);
}

float ngDepthTargetClampDepth(float value) {
    return value.isFinite ? ngDepthTargetRoundDepth(value) : 0.0f;
}

class DepthTargetView {
private:
    Deformable target;
    DepthMappedNode depthMapped;
    vec2 minPoint = vec2(0);
    vec2 maxPoint = vec2(1);
    ushort[] indices;

    float[] sortedUnique(float[] values) {
        sort(values);
        return values.uniq.array;
    }

    void rebuildBounds() {
        auto verts = target.vertices.toArray();
        if (verts.length == 0) {
            minPoint = vec2(0);
            maxPoint = vec2(1);
            return;
        }
        minPoint = verts[0];
        maxPoint = verts[0];
        foreach (v; verts[1 .. $]) {
            minPoint.x = min(minPoint.x, v.x);
            minPoint.y = min(minPoint.y, v.y);
            maxPoint.x = max(maxPoint.x, v.x);
            maxPoint.y = max(maxPoint.y, v.y);
        }
    }

    void rebuildTopology() {
        indices.length = 0;
        auto verts = target.vertices.toArray();
        if (verts.length == 0) return;

        float[] xs;
        float[] ys;
        foreach (v; verts) {
            xs ~= v.x;
            ys ~= v.y;
        }
        xs = sortedUnique(xs);
        ys = sortedUnique(ys);
        if (xs.length < 2 || ys.length < 2 || xs.length * ys.length != verts.length) return;

        ushort[ulong] lookup;
        foreach (i, v; verts) {
            size_t xi;
            size_t yi;
            foreach (j, x; xs) if (x == v.x) { xi = j; break; }
            foreach (j, y; ys) if (y == v.y) { yi = j; break; }
            lookup[yi * xs.length + xi] = cast(ushort)i;
        }

        foreach (y; 0 .. ys.length - 1) {
            foreach (x; 0 .. xs.length - 1) {
                auto k0 = y * xs.length + x;
                auto k1 = y * xs.length + x + 1;
                auto k2 = (y + 1) * xs.length + x;
                auto k3 = (y + 1) * xs.length + x + 1;
                auto p0 = k0 in lookup;
                auto p1 = k1 in lookup;
                auto p2 = k2 in lookup;
                auto p3 = k3 in lookup;
                if (p0 is null || p1 is null || p2 is null || p3 is null) continue;
                indices ~= [*p0, *p1, *p3, *p0, *p3, *p2];
            }
        }
    }

public:
    float[] depths;
    float[] baseDepths;
    vec2[] projectedPoints;

    this(Deformable target) {
        this.target = target;
        this.depthMapped = cast(DepthMappedNode)target;
        resetFromTarget();
        rebuildBounds();
        rebuildTopology();
    }

    Deformable getTarget() {
        return target;
    }

    Node targetNode() {
        return cast(Node)target;
    }

    vec2[] getVertices() {
        return target.vertices.toArray();
    }

    ushort[] getIndices() {
        return indices.dup;
    }

    vec2 boundsMin() {
        return minPoint;
    }

    vec2 boundsMax() {
        return maxPoint;
    }

    void refreshGeometry() {
        rebuildBounds();
        rebuildTopology();
        normalizeDepthLength();
    }

    vec2 localVertex(size_t index) {
        auto vertices = getVertices();
        return index < vertices.length ? vertices[index] : vec2(0);
    }

    vec2 snapLocalPoint(vec2 point) {
        auto vertices = getVertices();
        if (vertices.length == 0) return point;
        auto best = vertices[0];
        auto bestDistance = (point - best).length();
        foreach (v; vertices[1 .. $]) {
            auto distance = (point - v).length();
            if (distance < bestDistance) {
                best = v;
                bestDistance = distance;
            }
        }
        return best;
    }

    ptrdiff_t nearestLocalVertexIndex(vec2 point) {
        auto vertices = getVertices();
        if (vertices.length == 0) return -1;
        ptrdiff_t best = 0;
        auto bestDistance = (point - vertices[0]).length();
        foreach (i, v; vertices[1 .. $]) {
            auto distance = (point - v).length();
            if (distance < bestDistance) {
                best = cast(ptrdiff_t)i + 1;
                bestDistance = distance;
            }
        }
        return best;
    }

    float depthDisplayScale() {
        return ngDepthDisplayScaleForBounds(minPoint, maxPoint);
    }

    vec2 depthViewToModel(vec2 point, ref DepthCamera3D depthCamera, float depth = 0.0f) {
        return unprojectDepthPoint(point, -depth * depthDisplayScale(), depthCamera);
    }

    vec2 modelToDepthView(vec2 point, float depth, ref DepthCamera3D depthCamera) {
        return projectDepthPoint(point, -depth * depthDisplayScale(), depthCamera);
    }

    vec2 projectLocalPoint(vec2 point, float depth, ref DepthCamera3D depthCamera) {
        return modelToDepthView(point, depth, depthCamera);
    }

    vec2 localToWorld(vec2 point) {
        return point;
    }

    vec2 worldToLocal(vec2 point) {
        return point;
    }

    vec2 displayWorldToLocal(vec2 point, ref DepthCamera3D depthCamera, float depth = 0.0f) {
        return depthViewToModel(point, depthCamera, depth);
    }

    vec2 projectedVertex(size_t index) {
        return index < projectedPoints.length ? projectedPoints[index] : vec2(0);
    }

    ptrdiff_t nearestProjectedVertex(vec2 point, float radius) {
        ptrdiff_t best = -1;
        float bestDistance = radius;
        foreach (i, projected; projectedPoints) {
            auto distance = (projected - point).length();
            if (distance < bestDistance) {
                best = cast(ptrdiff_t)i;
                bestDistance = distance;
            }
        }
        return best;
    }

    float[] copyWorkingDepths() {
        return depths.dup;
    }

    void replaceWorkingDepths(float[] values) {
        depths = values.dup;
        normalizeDepthLength();
    }

    void resetWorkingDepths() {
        replaceWorkingDepths(baseDepths);
    }

    void clearBaseDepths() {
        baseDepths.length = target.vertices.length;
        baseDepths[] = 0;
        replaceWorkingDepths(baseDepths);
    }

    void resetFromTarget() {
        depths = depthMapped !is null ? depthMapped.copyDepths() : null;
        normalizeDepthLength();
        baseDepths = depths.dup;
    }

    float getDepth(size_t index) {
        return index < depths.length ? depths[index] : 0;
    }

    float depthAtLocalPoint(vec2 point) {
        auto vertices = getVertices();
        if (vertices.length == 0 || depths.length == 0) return 0;
        size_t bestIndex;
        auto bestDistance = (point - vertices[0]).length();
        foreach (i, v; vertices[1 .. $]) {
            auto distance = (point - v).length();
            if (distance < bestDistance) {
                bestIndex = i + 1;
                bestDistance = distance;
            }
        }
        return bestIndex < depths.length ? depths[bestIndex] : 0;
    }

    void setDepth(size_t index, float value) {
        if (index >= depths.length) return;
        depths[index] = ngDepthTargetClampDepth(value);
    }

    void addDepth(size_t index, float value) {
        setDepth(index, getDepth(index) + value);
    }

    void normalizeDepthLength() {
        if (depths is null || depths.length != target.vertices.length) {
            auto oldLength = depths.length;
            depths.length = target.vertices.length;
            foreach (i; oldLength .. depths.length) depths[i] = 0.0f;
        }
    }
}
