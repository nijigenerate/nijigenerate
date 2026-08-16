/*
    nijilive PathDeformer extended with nijigenerate-only metadata.

    Copyright (c) 2026, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.ext.nodes.expathdeformer;

import nijigenerate.ext.nodes.exdepthmapped;
import nijilive.core;
import nijilive.core.nodes;
import nijilive.core.nodes.deformer.path;
import nijilive.fmt.serialize;
import nijilive.math;

@TypeId("PathDeformer")
class ExPathDeformer : PathDeformer, DepthMappedNode {
    mixin ExDepthMapped;

public:
    this(Node parent = null, CurveType curveType = CurveType.Spline) {
        super(parent, curveType);
    }

    override
    void rebuffer(Vec2Array originalControlPoints) {
        auto oldVertices = vertices.dup;
        auto oldDepths = copyDepths();
        super.rebuffer(originalControlPoints);
        if (oldDepths is null) return;
        float[] resampledDepths;
        if (ngResamplePathDepths(oldVertices, oldDepths, vertices, resampledDepths)) {
            replaceDepths(resampledDepths);
        } else {
            replaceDepths(oldDepths);
            resizeDepthsToVertices(vertices.length);
        }
    }

    override
    void copyFrom(Node src, bool clone = false, bool deepCopy = true) {
        super.copyFrom(src, clone, deepCopy);
        copyDepthsFrom(src);
        resizeDepthsToVertices(vertices.length);
    }

    override
    void serializeSelfImpl(ref InochiSerializer serializer, bool recursive = true, SerializeNodeFlags flags = SerializeNodeFlags.All) {
        super.serializeSelfImpl(serializer, recursive, flags);
        serializeDepths(serializer);
    }

    override
    SerdeException deserializeFromFghj(Fghj data) {
        if (auto exc = super.deserializeFromFghj(data)) return exc;
        return deserializeDepths(data, vertices.length);
    }
}

void ngRegisterExPathDeformer() {
    inRegisterNodeType!ExPathDeformer();
}
