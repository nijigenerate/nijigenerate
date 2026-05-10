/*
    Shared extension for optional per-vertex depth values.

    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.ext.nodes.exdepthmapped;

import nijilive.core.nodes;
import nijilive.fmt.serialize;

interface DepthMappedNode {
    float[] copyDepths();
    void replaceDepths(float[] values);
}

mixin template ExDepthMapped() {
public:
    float[] depths = null;

    float[] copyDepths() {
        return depths is null ? null : depths.dup;
    }

    void replaceDepths(float[] values) {
        depths = values is null ? null : values.dup;
    }

    void copyDepthsFrom(Node src) {
        if (auto depthMapped = cast(DepthMappedNode)src) {
            depths = depthMapped.copyDepths();
        } else {
            depths = null;
        }
    }

    void resizeDepthsToVertices(size_t vertexCount) {
        if (depths !is null) depths.length = vertexCount;
    }

    void serializeDepths(ref InochiSerializer serializer) {
        if (depths is null) return;

        serializer.putKey("depths");
        serializer.serializeValue(depths);
    }

    SerdeException deserializeDepths(Fghj data, size_t vertexCount) {
        if (data["depths"].isEmpty) return null;

        if (data["depths"].kind == Fghj.Kind.null_) {
            depths = null;
            return null;
        }

        if (auto exc = data["depths"].deserializeValue(depths)) return exc;
        if (depths.length != vertexCount) {
            return new SerdeException("depths length must match vertices length");
        }
        return null;
    }
}
