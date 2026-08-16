/*
    Shared extension for optional per-vertex depth values.

    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.ext.nodes.exdepthmapped;

import nijilive.core.nodes;
import nijilive.fmt.serialize;
import nijilive.math : Vec2Array;

import std.algorithm.sorting : sort;
import std.math : isClose;

interface DepthMappedNode {
    float[] copyDepths();
    void replaceDepths(float[] values);
}

float ngFiniteDepthOrZero(float value) pure nothrow @safe {
    return value == value && value != float.infinity && value != -float.infinity ? value : 0.0f;
}

void ngNormalizeDepths(float[] values) pure nothrow @safe {
    foreach (ref value; values)
        value = ngFiniteDepthOrZero(value);
}

private enum float DepthGridAxisTolerance = 1e-4f;

private bool sameDepthGridAxisValue(float a, float b) {
    return isClose(a, b, DepthGridAxisTolerance, DepthGridAxisTolerance);
}

private float[] depthGridAxis(float[] values) {
    if (values.length == 0) return values;
    values.sort();
    size_t write = 1;
    foreach (i; 1 .. values.length) {
        if (!sameDepthGridAxisValue(values[write - 1], values[i])) {
            values[write] = values[i];
            write++;
        }
    }
    values.length = write;
    return values;
}

private bool locateDepthGridInterval(
    const(float)[] axis,
    float value,
    out size_t cell,
    out float weight,
) {
    if (axis.length < 2) return false;
    if (value <= axis[0]) {
        cell = 0;
        auto span = axis[1] - axis[0];
        weight = span > 0.0f ? (value - axis[0]) / span : 0.0f;
        return true;
    }
    if (value >= axis[$ - 1]) {
        cell = axis.length - 2;
        auto span = axis[$ - 1] - axis[$ - 2];
        weight = span > 0.0f ? (value - axis[$ - 2]) / span : 1.0f;
        return true;
    }
    foreach (i; 0 .. axis.length - 1) {
        auto a = axis[i];
        auto b = axis[i + 1];
        if (value < a || value > b) continue;
        auto span = b - a;
        cell = i;
        weight = span > 0.0f ? (value - a) / span : 0.0f;
        return true;
    }
    return false;
}

/** Interpolate or extrapolate a rectangular GridDeformer depth field. */
bool ngResampleGridDepths(
    Vec2Array oldVertices,
    const(float)[] oldDepths,
    Vec2Array newVertices,
    out float[] result,
) {
    result = null;
    if (oldVertices.length < 4 || oldDepths.length != oldVertices.length)
        return false;

    float[] xs;
    float[] ys;
    xs.length = oldVertices.length;
    ys.length = oldVertices.length;
    foreach (i, vertex; oldVertices) {
        xs[i] = vertex.x;
        ys[i] = vertex.y;
    }
    xs = depthGridAxis(xs);
    ys = depthGridAxis(ys);
    auto cols = xs.length;
    auto rows = ys.length;
    if (cols < 2 || rows < 2 || cols * rows != oldVertices.length)
        return false;

    // Depths use the same row-major vertex order as GridDeformer.
    foreach (y; 0 .. rows) {
        foreach (x; 0 .. cols) {
            auto vertex = oldVertices[y * cols + x];
            if (!sameDepthGridAxisValue(vertex.x, xs[x]) ||
                !sameDepthGridAxisValue(vertex.y, ys[y]))
                return false;
        }
    }

    result.length = newVertices.length;
    foreach (i, vertex; newVertices) {
        size_t cellX;
        size_t cellY;
        float u;
        float v;
        if (!locateDepthGridInterval(xs, vertex.x, cellX, u) ||
            !locateDepthGridInterval(ys, vertex.y, cellY, v)) {
            result[i] = 0.0f;
            continue;
        }

        auto index00 = cellY * cols + cellX;
        auto index10 = cellY * cols + cellX + 1;
        auto index01 = (cellY + 1) * cols + cellX;
        auto index11 = (cellY + 1) * cols + cellX + 1;
        auto top = oldDepths[index00] * (1.0f - u) + oldDepths[index10] * u;
        auto bottom = oldDepths[index01] * (1.0f - u) + oldDepths[index11] * u;
        result[i] = ngFiniteDepthOrZero(top * (1.0f - v) + bottom * v);
    }
    return true;
}

mixin template ExDepthMapped() {
public:
    float[] depths = null;

    float[] copyDepths() {
        ngNormalizeDepths(depths);
        return depths is null ? null : depths.dup;
    }

    void replaceDepths(float[] values) {
        depths = values is null ? null : values.dup;
        ngNormalizeDepths(depths);
    }

    void copyDepthsFrom(Node src) {
        if (auto depthMapped = cast(DepthMappedNode)src) {
            replaceDepths(depthMapped.copyDepths());
        } else {
            depths = null;
        }
    }

    void resizeDepthsToVertices(size_t vertexCount) {
        if (depths is null) return;

        ngNormalizeDepths(depths);
        auto oldLength = depths.length;
        depths.length = vertexCount;
        foreach (i; oldLength .. depths.length)
            depths[i] = 0.0f;
    }

    void serializeDepths(ref InochiSerializer serializer) {
        if (depths is null) return;

        serializer.putKey("depths");
        auto state = serializer.listBegin();
        foreach (depth; depths) {
            serializer.elemBegin();
            serializer.serializeValue(ngFiniteDepthOrZero(depth));
        }
        serializer.listEnd(state);
    }

    SerdeException deserializeDepths(Fghj data, size_t vertexCount) {
        if (data["depths"].isEmpty) return null;

        if (data["depths"].kind == Fghj.Kind.null_) {
            depths = null;
            return null;
        }

        depths.length = 0;
        foreach (entry; data["depths"].byElement) {
            float depth;
            if (auto exc = entry.deserializeValue(depth)) return exc;
            depths ~= ngFiniteDepthOrZero(depth);
        }
        if (depths.length != vertexCount) {
            return new SerdeException("depths length must match vertices length");
        }
        return null;
    }
}
