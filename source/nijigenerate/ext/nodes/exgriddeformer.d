/*
    nijilive GridDeformer extended with nijigenerate-only metadata.

    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.ext.nodes.exgriddeformer;

import nijigenerate.ext.nodes.exdepthmapped;
import nijilive.core;
import nijilive.core.nodes;
import nijilive.core.nodes.deformer.grid;
import nijilive.fmt.serialize;
import nijilive.math;

@TypeId("GridDeformer")
class ExGridDeformer : GridDeformer, DepthMappedNode {
    mixin ExDepthMapped;

public:
    this(Node parent = null) {
        super(parent);
    }

    override
    void rebuffer(Vec2Array gridPoints) {
        super.rebuffer(gridPoints);
        resizeDepthsToVertices(vertices.length);
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

void incRegisterExGridDeformer() {
    inRegisterNodeType!ExGridDeformer();
}
