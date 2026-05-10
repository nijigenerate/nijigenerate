/*
    Undo/redo actions for optional per-vertex depth maps.

    Copyright ©      2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.actions.depth;

import i18n;
import nijigenerate.actions;
import nijigenerate.ext.nodes.exdepthmapped;
import nijilive.core.nodes;
import std.format;

class DepthMappedChangeAction : LazyBoundAction {
private:
    Node node;
    DepthMappedNode depthMapped;
    float[] oldDepths;
    float[] newDepths;

    float[] capture() {
        return depthMapped.copyDepths();
    }

    void apply(float[] depths) {
        depthMapped.replaceDepths(depths);
        node.notifyChange(node, NotifyReason.AttributeChanged);
    }

public:
    this(Node node) {
        this.node = node;
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
