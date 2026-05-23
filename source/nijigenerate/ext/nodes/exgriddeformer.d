/*
    nijilive GridDeformer extended with nijigenerate-only metadata.

    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.ext.nodes.exgriddeformer;

import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.ext.nodes.exdepthops;
import nijilive.core;
import nijilive.core.nodes;
import nijilive.core.nodes.deformer.grid;
import nijilive.fmt.serialize;
import nijilive.math;

@TypeId("GridDeformer")
class ExGridDeformer : GridDeformer, DepthMappedNode, DepthOperationMappedNode {
    mixin ExDepthMapped;
    mixin ExDepthOperated;

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
        copyDepthOpsFrom(src);
        resizeDepthsToVertices(vertices.length);
    }

    override
    void serializeSelfImpl(ref InochiSerializer serializer, bool recursive = true, SerializeNodeFlags flags = SerializeNodeFlags.All) {
        super.serializeSelfImpl(serializer, recursive, flags);
        serializeDepths(serializer);
        serializeDepthOps(serializer);
    }

    override
    SerdeException deserializeFromFghj(Fghj data) {
        if (auto exc = super.deserializeFromFghj(data)) return exc;
        if (auto exc = deserializeDepths(data, vertices.length)) return exc;
        return deserializeDepthOps(data);
    }
}

void ngRegisterExGridDeformer() {
    inRegisterNodeType!ExGridDeformer();
}
