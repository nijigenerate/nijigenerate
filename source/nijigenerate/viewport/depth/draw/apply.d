module nijigenerate.viewport.depth.draw.apply;

import nijigenerate.actions : AsyncGroupAction, GroupAction;
import nijigenerate.actions.depth : DepthOperationMappedChangeAction;
import nijigenerate.commands : Context;
import nijigenerate.commands.depth.bone : ngBeginDepthBoneRefreshActionSink, ngEndDepthBoneRefreshActionSink;
import nijigenerate.core.actionstack : incActionPush, ngGuardActionStackScopes;
import nijigenerate.ext.nodes.exdepthmapped : DepthMappedNode;
import nijigenerate.ext.nodes.exdepthops : DepthOperationMappedNode;
import nijigenerate.viewport.depth.common.session : ngDepthViewWorkingDepthsChangeAction;
import nijigenerate.viewport.depth.common.targetview;
import nijigenerate.viewport.depth.draw.composer;
import nijigenerate.viewport.depth.tools.operation : ngComputeDepthsFromOps;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import i18n;

private class DepthDrawApplyAction : AsyncGroupAction {
    override string describe() {
        return _("Applied DepthDraw depth map");
    }

    override string describeUndo() {
        return _("DepthDraw depth map apply was reverted");
    }

    override string getName() {
        return this.stringof;
    }
}

struct DepthDrawApplySummary {
    bool succeeded;
    size_t changedTargets;
    size_t changedVertices;
    size_t layersApplied;
    size_t layersSkipped;
    size_t missingSamples;
}

private size_t countChangedVertices(const(float)[] before, const(float)[] after) {
    if (before.length != after.length) return after.length;
    size_t changed;
    foreach (i, depth; after) {
        if (before[i] != depth) changed++;
    }
    return changed;
}

private DepthDrawApplySummary summarizeDepthDrawApply(DepthTargetView target, ref DepthDrawComposeResult result) {
    DepthDrawApplySummary summary;
    if (target is null) return summary;
    auto grid = target.getTarget();
    if (grid is null || result.targetGridUuid != grid.uuid) return summary;
    if (result.depths.length != grid.vertices.length) return summary;

    summary.missingSamples = result.missingVertices;
    foreach (stats; result.layerStats) {
        if (stats.contributedVertices > 0) {
            summary.layersApplied++;
        } else {
            summary.layersSkipped++;
        }
    }

    auto depthMapped = cast(DepthMappedNode)grid;
    if (depthMapped is null) return summary;
    auto before = depthMapped.copyDepths();
    summary.changedVertices = countChangedVertices(before, result.depths);
    summary.changedTargets = summary.changedVertices > 0 ? 1 : 0;
    summary.succeeded = true;
    return summary;
}

DepthDrawApplySummary ngApplyDepthDrawTargetResultWithSummary(
    Context ctx,
    DepthTargetView target,
    ref DepthDrawComposeResult result
) {
    auto summary = summarizeDepthDrawApply(target, result);
    if (!summary.succeeded || ctx is null) {
        summary.succeeded = false;
        return summary;
    }

    ngGuardActionStackScopes();
    auto group = new DepthDrawApplyAction();
    ngBeginDepthBoneRefreshActionSink(group);
    scope(exit) ngEndDepthBoneRefreshActionSink(group);
    auto targetNode = target.getTarget();
    auto depthMapped = cast(DepthMappedNode)targetNode;
    auto beforeDepths = depthMapped.copyDepths();
    auto appliedDepths = result.depths.dup;
    auto operated = cast(DepthOperationMappedNode)targetNode;
    auto grid = cast(GridDeformer)targetNode;
    if (operated !is null && grid !is null) {
        auto operations = operated.copyDepthOps();
        if (operations.length > 0) {
            auto operationAction = new DepthOperationMappedChangeAction(targetNode);
            operated.replaceDepthOpBaseDepths(result.depths);
            operationAction.updateNewState();
            group.addAction(operationAction);
            appliedDepths = ngComputeDepthsFromOps(grid, operations, result.depths);
        }
    }
    target.replaceWorkingDepths(appliedDepths);
    group.addAction(ngDepthViewWorkingDepthsChangeAction(target, "Apply DepthDraw Depth Map"));
    if (!group.empty()) incActionPush(group);
    summary.changedVertices = countChangedVertices(beforeDepths, appliedDepths);
    summary.changedTargets = summary.changedVertices > 0 ? 1 : 0;
    return summary;
}

bool ngApplyDepthDrawTargetResult(Context ctx, DepthTargetView target, ref DepthDrawComposeResult result) {
    return ngApplyDepthDrawTargetResultWithSummary(ctx, target, result).succeeded;
}
