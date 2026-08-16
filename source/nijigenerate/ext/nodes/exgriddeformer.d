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
import std.math : isFinite;

private ExDepthOp[] remapIndexBoundDepthOperations(
    Vec2Array oldVertices,
    Vec2Array newVertices,
    ExDepthOp[] operations,
) {
    enum float matchingVertexDistanceSquared = 1.0e-8f;
    ExDepthOp[] result;
    foreach (operation; operations) {
        if (operation.type != ExDepthOpType.AttachedPoint) {
            result ~= operation;
            continue;
        }
        if (operation.index >= oldVertices.length || newVertices.length == 0) continue;

        auto source = oldVertices[operation.index];
        float bestDistance = float.infinity;
        size_t bestIndex;
        bool found;
        foreach (i, candidate; newVertices) {
            auto delta = candidate - source;
            auto distance = delta.x * delta.x + delta.y * delta.y;
            if (!distance.isFinite || (found && distance >= bestDistance)) continue;
            bestDistance = distance;
            bestIndex = i;
            found = true;
        }
        if (!found || bestDistance > matchingVertexDistanceSquared) continue;
        operation.index = bestIndex;
        result ~= operation;
    }
    return result;
}

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
        auto oldVertices = vertices.dup;
        auto oldDepths = copyDepths();
        auto oldOperations = copyDepthOps();
        auto oldOperationBaseDepths = copyDepthOpBaseDepths();
        super.rebuffer(gridPoints);
        if (oldDepths !is null) {
            float[] resampledDepths;
            if (ngResampleGridDepths(oldVertices, oldDepths, vertices, resampledDepths)) {
                replaceDepths(resampledDepths);
            } else {
                replaceDepths(oldDepths);
                resizeDepthsToVertices(vertices.length);
            }
        }
        if (oldOperationBaseDepths !is null) {
            float[] resampledBaseDepths;
            if (ngResampleGridDepths(oldVertices, oldOperationBaseDepths, vertices, resampledBaseDepths)) {
                replaceDepthOpBaseDepths(resampledBaseDepths);
            } else {
                replaceDepthOpBaseDepths(oldOperationBaseDepths);
                resizeDepthOpBaseDepthsToVertices(vertices.length);
            }
        }
        replaceDepthOps(remapIndexBoundDepthOperations(oldVertices, vertices, oldOperations));
    }

    override
    void copyFrom(Node src, bool clone = false, bool deepCopy = true) {
        super.copyFrom(src, clone, deepCopy);
        copyDepthsFrom(src);
        copyDepthOpsFrom(src);
        resizeDepthsToVertices(vertices.length);
        resizeDepthOpBaseDepthsToVertices(vertices.length);
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
        if (auto exc = deserializeDepthOps(data)) return exc;
        if (depthOpBaseDepths.length > 0 && depthOpBaseDepths.length != vertices.length) {
            return new SerdeException("depth-op-base-depths length must match vertices length");
        }
        return null;
    }
}

void ngRegisterExGridDeformer() {
    inRegisterNodeType!ExGridDeformer();
}
