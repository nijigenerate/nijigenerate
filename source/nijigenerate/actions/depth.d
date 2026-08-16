/*
    Undo/redo actions for optional per-vertex depth maps.

    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.actions.depth;

import i18n;
import nijigenerate.actions;
import nijigenerate.actions.depthboneinvalidation;
import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.ext.nodes.exdepthops;
import nijilive.core.nodes;
import std.format;

class DepthMappedChangeAction : LazyBoundAction {
private:
    Node node;
    DepthMappedNode depthMapped;
    float[] oldDepths;
    float[] newDepths;
    string reason;

    float[] capture() {
        return depthMapped.copyDepths();
    }

    void apply(float[] depths) {
        depthMapped.replaceDepths(depths);
        node.notifyChange(node, NotifyReason.AttributeChanged);
        ngNotifyDepthBoneTargetChanged(node, DepthBoneMutationKind.TargetDepth, reason);
    }

public:
    this(Node node, string reason = "Depth Map") {
        this.node = node;
        this.reason = reason;
        this.depthMapped = cast(DepthMappedNode)node;
        assert(this.depthMapped !is null);
        this.oldDepths = capture();
    }

    override
    void updateNewState() {
        newDepths = capture();
    }

    override
    void clear() { }

    override
    void rollback() {
        apply(oldDepths);
    }

    override
    void redo() {
        apply(newDepths);
    }

    override
    string describe() {
        return _("Changed depth map of %s").format(node.name);
    }

    override
    string describeUndo() {
        return _("Depth map of %s was changed").format(node.name);
    }

    override
    string getName() {
        return this.stringof;
    }

    override bool merge(Action other) { return false; }
    override bool canMerge(Action other) { return false; }
}

class DepthOperationMappedChangeAction : LazyBoundAction {
private:
    struct State {
        ExDepthOp[] operations;
        float[] baseDepths;
    }

    Node node;
    DepthOperationMappedNode depthOperated;
    State oldState;
    State newState;

    State capture() {
        return State(depthOperated.copyDepthOps(), depthOperated.copyDepthOpBaseDepths());
    }

    void apply(State state) {
        depthOperated.replaceDepthOps(state.operations);
        depthOperated.replaceDepthOpBaseDepths(state.baseDepths);
        node.notifyChange(node, NotifyReason.AttributeChanged);
    }

public:
    this(Node node) {
        this.node = node;
        this.depthOperated = cast(DepthOperationMappedNode)node;
        assert(this.depthOperated !is null);
        this.oldState = capture();
    }

    override
    void updateNewState() {
        newState = capture();
    }

    override
    void clear() { }

    override
    void rollback() {
        apply(oldState);
    }

    override
    void redo() {
        apply(newState);
    }

    override
    string describe() {
        return _("Changed depth operations of %s").format(node.name);
    }

    override
    string describeUndo() {
        return _("Depth operations of %s were changed").format(node.name);
    }

    override
    string getName() {
        return this.stringof;
    }

    override bool merge(Action other) { return false; }
    override bool canMerge(Action other) { return false; }
}
