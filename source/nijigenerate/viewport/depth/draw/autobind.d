module nijigenerate.viewport.depth.draw.autobind;

import nijigenerate.ext.nodes.exdepthmapped : DepthMappedNode;
import nijigenerate.ext.nodes.expart : ExPart;
import nijigenerate.viewport.depth.draw.binding;
import nijigenerate.viewport.depth.draw.layer;
import nijigenerate.viewport.depth.draw.session;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.nodes.deformer.path : PathDeformer;

private struct Candidate {
    Node node;
    int priority;
}

struct DepthDrawAutoBindResult {
    string layerId;
    string layerPath;
    string layerName;
    string matchedNodeName;
    string targetGridName;
    ulong matchedNodeUuid;
    ulong targetGridUuid;
    bool matched;
    bool ambiguous;
    string status;
}

private string layerBaseName(string path) {
    if (path.length == 0) return null;
    foreach_reverse (i, c; path) {
        if (c == '/' || c == '\\') return path[i + 1 .. $];
    }
    return path;
}

private Deformable containingDepthTarget(Node node) {
    auto cursor = node;
    while (cursor !is null) {
        auto target = cast(Deformable)cursor;
        if (target !is null && cast(DepthMappedNode)target !is null) return target;
        cursor = cursor.parent;
    }
    return null;
}

private Candidate[] bestCandidates(Candidate[] candidates) {
    if (candidates.length == 0) return null;
    auto bestPriority = candidates[0].priority;
    foreach (candidate; candidates) {
        if (candidate.priority < bestPriority) bestPriority = candidate.priority;
    }

    Candidate[] result;
    foreach (candidate; candidates) {
        if (candidate.priority == bestPriority) result ~= candidate;
    }
    return result;
}

private Candidate[] matchCandidates(Puppet puppet, string layerPath, string layerName, bool matchDirectGridName) {
    Candidate[] candidates;
    if (puppet is null || puppet.root is null) return candidates;

    foreach (part; puppet.findNodesType!ExPart(puppet.root)) {
        auto path = part.layerPath.length ? part.layerPath : ("/" ~ part.name);
        if (path == layerPath) {
            candidates ~= Candidate(part, 0);
        } else if (layerBaseName(path) == layerName) {
            candidates ~= Candidate(part, 1);
        } else if (part.name == layerName) {
            candidates ~= Candidate(part, 2);
        }
    }

    if (matchDirectGridName) {
        foreach (grid; puppet.findNodesType!GridDeformer(puppet.root)) {
            if (grid.name == layerName && cast(DepthMappedNode)grid !is null) candidates ~= Candidate(grid, 3);
        }
        foreach (path; puppet.findNodesType!PathDeformer(puppet.root)) {
            if (path.name == layerName && cast(DepthMappedNode)path !is null) candidates ~= Candidate(path, 3);
        }
    }

    return bestCandidates(candidates);
}

DepthDrawAutoBindResult ngDepthDrawAutoBindLayer(
    Puppet puppet,
    DepthDrawLayer layer,
    bool matchDirectGridName = true
) {
    DepthDrawAutoBindResult result;
    result.layerId = layer.id;
    result.layerPath = layer.layerPath;
    result.layerName = layer.displayName.length ? layer.displayName : layerBaseName(layer.layerPath);

    auto candidates = matchCandidates(puppet, result.layerPath, result.layerName, matchDirectGridName);
    if (candidates.length == 0) {
        result.status = "Unmatched";
        return result;
    }

    result.ambiguous = candidates.length > 1;
    if (result.ambiguous) {
        result.status = "Ambiguous";
        return result;
    }
    auto matchedNode = candidates[0].node;
    auto target = containingDepthTarget(matchedNode);
    result.matchedNodeName = matchedNode.name;
    result.matchedNodeUuid = matchedNode.uuid;
    if (target is null) {
        result.status = "UnmatchedWithoutGrid";
        return result;
    }

    result.matched = true;
    result.targetGridName = target.name;
    result.targetGridUuid = target.uuid;
    result.status = "Matched";
    return result;
}

DepthDrawBinding ngDepthDrawBindingFromAutoBind(DepthDrawAutoBindResult result, int order = 0) {
    DepthDrawBinding binding;
    binding.layerId = result.layerId;
    binding.targetNodeUuid = result.matchedNodeUuid;
    binding.targetGridUuid = result.targetGridUuid;
    binding.order = order;
    binding.enabled = result.matched;
    return binding;
}

DepthDrawAutoBindResult[] ngDepthDrawAutoBindSession(
    DepthDrawSession session,
    Puppet puppet,
    bool matchDirectGridName = true
) {
    DepthDrawAutoBindResult[] results;
    if (session is null) return results;

    foreach (i, layer; session.layers) {
        auto result = ngDepthDrawAutoBindLayer(puppet, layer, matchDirectGridName);
        results ~= result;
        if (!result.matched) continue;
        auto alreadyBound = false;
        foreach (binding; session.bindings) {
            if (binding.layerId == layer.id) {
                alreadyBound = true;
                break;
            }
        }
        if (!alreadyBound) session.bindings ~= ngDepthDrawBindingFromAutoBind(result, cast(int)i);
    }

    return results;
}
