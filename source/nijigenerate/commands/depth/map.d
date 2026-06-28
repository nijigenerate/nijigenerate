module nijigenerate.commands.depth.map;

import nijigenerate.actions.depth;
import nijigenerate.actions : GroupAction;
import nijigenerate.api.mcp.task : ngMcpEnqueueAction;
import nijigenerate.commands.base;
import nijigenerate.commands.depth.bone : ngBeginDepthBoneRefreshActionSink, ngEndDepthBoneRefreshActionSink,
    ngFlushDepthBoneDirty, ngHasPendingDepthBoneRefreshForSink, ngMarkDepthBoneDirtyForTarget,
    ngPendingDepthBoneRefreshWorkForSink;
import nijigenerate.core.actionstack : incActionPush, ngGuardActionStackScopes;
import nijigenerate.ext.nodes.exdepthmapped;
import nijigenerate.ext.nodes.exdepthops;
import nijigenerate.io.depthmap_psd;
import nijigenerate.project : incActivePuppet;
import nijigenerate.viewport.depth.mesheditor.node : DepthMeshEditorOne;
import nijigenerate.viewport.depth.tools.operation : applyRingNormalSurfaces, depthOperationFromExDepthOp, toExDepthOp;
import nijigenerate.viewport.depth.tools.operation : DepthAttachedPointOperation, DepthPlaneOperation, DepthRingOperation;
import nijilive;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import std.algorithm.comparison : max;
import std.exception : enforce;
import std.json : JSONType, JSONValue;
import std.math : isFinite;
import std.string : format;
import nijigenerate.widgets.notification : NotificationPopup;
import bindbc.imgui : ImGuiIO, ImVec2, igProgressBar, igText;
import i18n;

enum DepthMapCommand {
    ListDepths,
    SetDepths,
    ClearDepths,
    ListDepthOps,
    SetDepthOps,
    AddDepthOp,
    UpdateDepthOp,
    RemoveDepthOp,
    MoveDepthOp,
    ClearDepthOps,
    ApplyDepthOps,
    ImportPSDDepths,
}

Command[DepthMapCommand] commands;

private DepthMappedNode requireDepthMapped(Node node) {
    auto mapped = cast(DepthMappedNode)node;
    enforce(mapped !is null, "target must support depth maps");
    return mapped;
}

private class PsdDepthImportChangeAction : GroupAction {
    override string describe() {
        return _("Imported PSD depth map");
    }

    override string describeUndo() {
        return _("PSD depth map import was reverted");
    }

    override string getName() {
        return this.stringof;
    }
}

private class PsdDepthImportRefreshJob {
    private GroupAction group;
    private size_t changedGrids;
    private size_t completedWork;
    private size_t totalWork;
    private bool finished;
    private ulong popupId;

    this(GroupAction group, size_t changedGrids) {
        this.group = group;
        this.changedGrids = changedGrids;
        auto pending = ngPendingDepthBoneRefreshWorkForSink(group);
        totalWork = max(cast(size_t)1, pending);
    }

    void start() {
        import std.string : toStringz;

        auto self = this;
        popupId = NotificationPopup.instance().popup((ImGuiIO* io) {
            size_t done, total, remaining;
            self.snapshot(done, total, remaining);
            float ratio = total > 0 ? cast(float)done / cast(float)total : 1.0f;
            igText(_("Finalizing PSD depth import...").toStringz);
            igProgressBar(ratio, ImVec2(320, 0));
        }, -1);
        scheduleNext();
    }

    private void snapshot(out size_t done, out size_t total, out size_t remaining) {
        remaining = ngPendingDepthBoneRefreshWorkForSink(group);
        total = max(totalWork, completedWork + remaining);
        done = total > remaining ? total - remaining : completedWork;
    }

    private void scheduleNext() {
        auto self = this;
        ngMcpEnqueueAction({
            self.step();
        });
    }

    private void step() {
        if (finished) return;
        auto before = ngPendingDepthBoneRefreshWorkForSink(group);
        if (before == 0) {
            complete();
            return;
        }

        ngFlushDepthBoneDirty();

        auto after = ngPendingDepthBoneRefreshWorkForSink(group);
        if (after < before) {
            completedWork += before - after;
        } else {
            completedWork++;
        }
        totalWork = max(totalWork, completedWork + after);

        if (after == 0) {
            complete();
        } else {
            scheduleNext();
        }
    }

    private void complete() {
        if (finished) return;
        finished = true;
        NotificationPopup.instance().close(popupId);
        if (!group.empty()) incActionPush(group);
        if (activePsdDepthImportRefreshJob is this) activePsdDepthImportRefreshJob = null;
        NotificationPopup.instance().popup(_("PSD depth map imported"), 3);
    }
}

private PsdDepthImportRefreshJob activePsdDepthImportRefreshJob;

private DepthOperationMappedNode requireDepthOperated(Node node) {
    auto operated = cast(DepthOperationMappedNode)node;
    enforce(operated !is null, "target must support depth operations");
    return operated;
}

private GridDeformer requireDepthGrid(Node node) {
    auto grid = cast(GridDeformer)node;
    enforce(grid !is null, "target must be a GridDeformer");
    return grid;
}

private float finiteFloat(float value, string name) {
    enforce(value.isFinite, "%s must be finite".format(name));
    return value;
}

private float jsonFloat(JSONValue value, string name, float fallback = 0.0f) {
    if (value.type == JSONType.null_) return fallback;
    final switch (value.type) {
        case JSONType.float_:
            return finiteFloat(cast(float)value.floating, name);
        case JSONType.integer:
            return finiteFloat(cast(float)value.integer, name);
        case JSONType.uinteger:
            return finiteFloat(cast(float)value.uinteger, name);
        case JSONType.null_:
            return fallback;
        case JSONType.object:
        case JSONType.array:
        case JSONType.string:
        case JSONType.true_:
        case JSONType.false_:
            enforce(false, "%s must be a number".format(name));
    }
    assert(0);
}

private size_t jsonSize(JSONValue value, string name, size_t fallback = 0) {
    if (value.type == JSONType.null_) return fallback;
    final switch (value.type) {
        case JSONType.integer:
            enforce(value.integer >= 0, "%s must be >= 0".format(name));
            return cast(size_t)value.integer;
        case JSONType.uinteger:
            return cast(size_t)value.uinteger;
        case JSONType.float_:
            enforce(value.floating >= 0 && value.floating == cast(long)value.floating, "%s must be a non-negative integer".format(name));
            return cast(size_t)value.floating;
        case JSONType.null_:
            return fallback;
        case JSONType.object:
        case JSONType.array:
        case JSONType.string:
        case JSONType.true_:
        case JSONType.false_:
            enforce(false, "%s must be a non-negative integer".format(name));
    }
    assert(0);
}

private vec2 jsonVec2(JSONValue value, string name, vec2 fallback = vec2(0, 0)) {
    if (value.type == JSONType.null_) return fallback;
    enforce(value.type == JSONType.array && value.array.length == 2, "%s must be [x, y]".format(name));
    return vec2(jsonFloat(value.array[0], name ~ "[0]"), jsonFloat(value.array[1], name ~ "[1]"));
}

private JSONValue vec2ToJson(vec2 value) {
    JSONValue result = JSONValue.emptyArray;
    result.array ~= JSONValue(cast(double)value.x);
    result.array ~= JSONValue(cast(double)value.y);
    return result;
}

JSONValue ngDepthOpToJson(ExDepthOp op) {
    JSONValue[string] obj;
    obj["type"] = JSONValue(op.typeName());
    final switch (op.type) {
        case ExDepthOpType.AttachedPoint:
            obj["index"] = JSONValue(cast(long)op.index);
            obj["amount"] = JSONValue(cast(double)op.amount);
            break;
        case ExDepthOpType.Ring:
            obj["p0"] = vec2ToJson(op.p0);
            obj["p1"] = vec2ToJson(op.p1);
            obj["amount"] = JSONValue(cast(double)op.amount);
            obj["width"] = JSONValue(cast(double)op.width);
            obj["hardness"] = JSONValue(cast(double)op.hardness);
            obj["p0Angle"] = JSONValue(cast(double)op.p0Angle);
            obj["p1Angle"] = JSONValue(cast(double)op.p1Angle);
            break;
        case ExDepthOpType.Plane:
            obj["center"] = vec2ToJson(op.center);
            obj["radiusX"] = JSONValue(cast(double)op.radiusX);
            obj["radiusY"] = JSONValue(cast(double)op.radiusY);
            obj["angle"] = JSONValue(cast(double)op.angle);
            obj["targetDepth"] = JSONValue(cast(double)op.targetDepth);
            obj["flattenStrength"] = JSONValue(cast(double)op.flattenStrength);
            break;
    }
    return JSONValue(obj);
}

private ExDepthOp depthOpFromJson(JSONValue value) {
    enforce(value.type == JSONType.object, "operation must be an object");
    enforce("type" in value.object, "operation.type is required");
    enforce(value["type"].type == JSONType.string, "operation.type must be a string");

    ExDepthOp op;
    auto type = value["type"].str;
    switch (type) {
        case "attached-point":
        case "AttachedPoint":
            op.type = ExDepthOpType.AttachedPoint;
            op.index = jsonSize(value.object.get("index", JSONValue(null)), "index");
            op.amount = jsonFloat(value.object.get("amount", JSONValue(0.0)), "amount");
            return op;
        case "ring":
        case "Ring":
            op.type = ExDepthOpType.Ring;
            op.p0 = jsonVec2(value.object.get("p0", JSONValue(null)), "p0");
            op.p1 = jsonVec2(value.object.get("p1", JSONValue(null)), "p1");
            op.amount = jsonFloat(value.object.get("amount", JSONValue(0.0)), "amount");
            op.width = max(0.5f, jsonFloat(value.object.get("width", JSONValue(1.0)), "width"));
            op.hardness = max(0.1f, jsonFloat(value.object.get("hardness", JSONValue(1.0)), "hardness"));
            op.p0Angle = jsonFloat(value.object.get("p0Angle", JSONValue(180.0)), "p0Angle");
            op.p1Angle = jsonFloat(value.object.get("p1Angle", JSONValue(0.0)), "p1Angle");
            return op;
        case "plane":
        case "Plane":
            op.type = ExDepthOpType.Plane;
            op.center = jsonVec2(value.object.get("center", JSONValue(null)), "center");
            op.radiusX = max(1.0f, jsonFloat(value.object.get("radiusX", JSONValue(1.0)), "radiusX"));
            op.radiusY = max(1.0f, jsonFloat(value.object.get("radiusY", JSONValue(1.0)), "radiusY"));
            op.angle = jsonFloat(value.object.get("angle", JSONValue(0.0)), "angle");
            op.targetDepth = jsonFloat(value.object.get("targetDepth", value.object.get("amount", JSONValue(0.0))), "targetDepth");
            op.flattenStrength = jsonFloat(value.object.get("flattenStrength", JSONValue(1.0)), "flattenStrength");
            return op;
        default:
            enforce(false, "unknown depth operation type: " ~ type);
    }
    assert(0);
}

private ExDepthOp[] depthOpsFromJson(JSONValue value) {
    enforce(value.type == JSONType.array, "operations must be an array");
    ExDepthOp[] result;
    foreach (entry; value.array) result ~= depthOpFromJson(entry);
    return result;
}

private JSONValue depthOpsToJson(ExDepthOp[] ops) {
    JSONValue result = JSONValue.emptyArray;
    foreach (i, op; ops) {
        auto obj = ngDepthOpToJson(op);
        obj.object["indexInList"] = JSONValue(cast(long)i);
        result.array ~= obj;
    }
    return result;
}

private JSONValue depthsToJson(float[] depths) {
    if (depths is null) return JSONValue(null);
    JSONValue result = JSONValue.emptyArray;
    foreach (depth; depths) result.array ~= JSONValue(cast(double)depth);
    return result;
}

private DepthMappedChangeAction applyDepthsChangeAction(Node target, float[] nextDepths, string reason) {
    auto mapped = requireDepthMapped(target);
    auto action = new DepthMappedChangeAction(target);
    mapped.replaceDepths(nextDepths);
    action.updateNewState();
    ngMarkDepthBoneDirtyForTarget(target, reason);
    return action;
}

private void replaceDepthsWithUndo(Node target, float[] nextDepths, string reason) {
    incActionPush(applyDepthsChangeAction(target, nextDepths, reason));
}

JSONValue ngPsdDepthImportSummaryToJson(PsdDepthImportResult imported, size_t changedGrids) {
    JSONValue[string] obj;
    obj["changedGrids"] = JSONValue(cast(long)changedGrids);
    obj["matchedLayers"] = JSONValue(cast(long)imported.matchedLayers);
    obj["unmatchedLayers"] = JSONValue(cast(long)imported.unmatchedLayers);
    obj["ambiguousLayers"] = JSONValue(cast(long)imported.ambiguousLayers);
    obj["skippedGrids"] = JSONValue(cast(long)imported.skippedGrids);

    JSONValue mappings = JSONValue.emptyArray;
    foreach (mapping; imported.mappings) {
        JSONValue[string] entry;
        entry["layerPath"] = JSONValue(mapping.layerPath);
        entry["layerName"] = JSONValue(mapping.layerName);
        entry["matchedNodeName"] = JSONValue(mapping.matchedNodeName);
        entry["targetGridName"] = JSONValue(mapping.targetGridName);
        entry["matched"] = JSONValue(mapping.matched);
        entry["ambiguous"] = JSONValue(mapping.ambiguous);
        entry["ignored"] = JSONValue(mapping.ignored);
        entry["manual"] = JSONValue(mapping.manual);
        entry["status"] = JSONValue(mapping.status);
        mappings.array ~= JSONValue(entry);
    }
    obj["mappings"] = mappings;

    return JSONValue(obj);
}

ExCommandResult!JSONValue ngApplyPsdDepthImportResult(PsdDepthImportResult imported) {
    size_t changedGrids;

    if (imported.grids.length > 0) {
        if (activePsdDepthImportRefreshJob !is null) {
            return ExCommandResult!JSONValue(
                false,
                ngPsdDepthImportSummaryToJson(imported, changedGrids),
                "PSD depth map import is still finalizing"
            );
        }
        ngGuardActionStackScopes();
        auto group = new PsdDepthImportChangeAction();
        ngBeginDepthBoneRefreshActionSink(group);
        scope(exit) ngEndDepthBoneRefreshActionSink(group);
        foreach (gridResult; imported.grids) {
            if (gridResult.skipped) continue;
            if (gridResult.grid is null) continue;
            enforce(gridResult.depths.length == gridResult.grid.vertices.length, "imported depths length must match target vertices");
            group.addAction(applyDepthsChangeAction(gridResult.grid, gridResult.depths, "Import PSD Depth Map"));
            changedGrids++;
        }
        if (ngHasPendingDepthBoneRefreshForSink(group)) {
            activePsdDepthImportRefreshJob = new PsdDepthImportRefreshJob(group, changedGrids);
            activePsdDepthImportRefreshJob.start();
        } else if (!group.empty()) {
            incActionPush(group);
        }
    }

    return ExCommandResult!JSONValue(
        true,
        ngPsdDepthImportSummaryToJson(imported, changedGrids),
        "PSD depth map imported"
    );
}

private void replaceDepthOpsWithUndo(Node target, ExDepthOp[] nextOps, string reason) {
    auto operated = requireDepthOperated(target);
    auto action = new DepthOperationMappedChangeAction(target);
    operated.replaceDepthOps(nextOps);
    action.updateNewState();
    incActionPush(action);
    ngMarkDepthBoneDirtyForTarget(target, reason);
}

private float[] computeDepthsFromOps(GridDeformer grid, ExDepthOp[] ops) {
    auto editor = new DepthMeshEditorOne(grid, false);
    scope(exit) editor.dispose();
    editor.clearBaseDepths();

    DepthRingOperation[] rings;
    DepthAttachedPointOperation[] attachedPoints;
    DepthPlaneOperation[] planes;
    foreach (op; ops) {
        auto operation = depthOperationFromExDepthOp(op);
        if (auto ring = cast(DepthRingOperation)operation) {
            rings ~= ring;
        } else if (auto attached = cast(DepthAttachedPointOperation)operation) {
            attachedPoints ~= attached;
        } else if (auto plane = cast(DepthPlaneOperation)operation) {
            planes ~= plane;
        }
    }

    applyRingNormalSurfaces(editor, rings);
    foreach (op; attachedPoints) op.apply(editor);
    foreach (op; planes) op.apply(editor);
    return editor.copyEditorDepths();
}

@EffectApply
class ListDepthsCommand : ExCommand!(TW!(Node, "target", "DepthMapped target node")) {
    this() { super(_("List Depths"), _("List per-vertex depth values")); }

    override ExCommandResult!JSONValue run(Context ctx) {
        auto depths = requireDepthMapped(target).copyDepths();
        JSONValue[string] obj;
        obj["target"] = JSONValue(target.uuid);
        obj["depths"] = depthsToJson(depths);
        obj["count"] = JSONValue(depths is null ? -1L : cast(long)depths.length);
        return ExCommandResult!JSONValue(true, JSONValue(obj));
    }
}

@EffectApply
class SetDepthsCommand : ExCommand!(
    TW!(Node, "target", "DepthMapped target node"),
    TW!(float[], "depths", "Per-vertex depth values; length must match target vertices")
) {
    this() { super(_("Set Depths"), _("Set per-vertex depth values")); }

    override CommandResult run(Context ctx) {
        auto grid = requireDepthGrid(target);
        enforce(depths.length == grid.vertices.length, "depths length must match target vertices");
        replaceDepthsWithUndo(target, depths, "Set Depths");
        return CommandResult(true);
    }
}

@EffectApply
class ClearDepthsCommand : ExCommand!(TW!(Node, "target", "DepthMapped target node")) {
    this() { super(_("Clear Depths"), _("Clear per-vertex depth values")); }

    override CommandResult run(Context ctx) {
        replaceDepthsWithUndo(target, null, "Clear Depths");
        return CommandResult(true);
    }
}

@EffectApply
class ListDepthOpsCommand : ExCommand!(TW!(Node, "target", "Depth operation target node")) {
    this() { super(_("List Depth Operations"), _("List saved depth operations")); }

    override ExCommandResult!JSONValue run(Context ctx) {
        auto ops = requireDepthOperated(target).copyDepthOps();
        JSONValue[string] obj;
        obj["target"] = JSONValue(target.uuid);
        obj["operations"] = depthOpsToJson(ops);
        obj["count"] = JSONValue(cast(long)ops.length);
        return ExCommandResult!JSONValue(true, JSONValue(obj));
    }
}

@EffectApply
class SetDepthOpsCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(JSONValue, "operations", "Depth operations array")
) {
    this() { super(_("Set Depth Operations"), _("Replace saved depth operations")); }

    override CommandResult run(Context ctx) {
        replaceDepthOpsWithUndo(target, depthOpsFromJson(operations), "Set Depth Operations");
        return CommandResult(true);
    }
}

@EffectApply
class AddDepthOpCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(JSONValue, "operation", "Depth operation object"),
    TW!(int, "index", "Insertion index, or -1 to append")
) {
    this() { super(_("Add Depth Operation"), _("Add one saved depth operation")); }

    override CommandResult run(Context ctx) {
        auto operated = requireDepthOperated(target);
        auto ops = operated.copyDepthOps();
        auto op = depthOpFromJson(operation);
        if (index < 0 || index >= ops.length) {
            ops ~= op;
        } else {
            auto i = cast(size_t)index;
            ops = ops[0 .. i] ~ [op] ~ ops[i .. $];
        }
        replaceDepthOpsWithUndo(target, ops, "Add Depth Operation");
        return CommandResult(true);
    }
}

@EffectApply
class UpdateDepthOpCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(int, "index", "Operation index"),
    TW!(JSONValue, "operation", "Replacement depth operation object")
) {
    this() { super(_("Update Depth Operation"), _("Replace one saved depth operation")); }

    override CommandResult run(Context ctx) {
        auto operated = requireDepthOperated(target);
        auto ops = operated.copyDepthOps();
        enforce(index >= 0 && index < ops.length, "operation index out of range");
        ops[cast(size_t)index] = depthOpFromJson(operation);
        replaceDepthOpsWithUndo(target, ops, "Update Depth Operation");
        return CommandResult(true);
    }
}

@EffectApply
class RemoveDepthOpCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(int, "index", "Operation index")
) {
    this() { super(_("Remove Depth Operation"), _("Remove one saved depth operation")); }

    override CommandResult run(Context ctx) {
        auto operated = requireDepthOperated(target);
        auto ops = operated.copyDepthOps();
        enforce(index >= 0 && index < ops.length, "operation index out of range");
        auto i = cast(size_t)index;
        ops = ops[0 .. i] ~ ops[i + 1 .. $];
        replaceDepthOpsWithUndo(target, ops, "Remove Depth Operation");
        return CommandResult(true);
    }
}

@EffectApply
class MoveDepthOpCommand : ExCommand!(
    TW!(Node, "target", "Depth operation target node"),
    TW!(int, "fromIndex", "Current operation index"),
    TW!(int, "toIndex", "Destination operation index")
) {
    this() { super(_("Move Depth Operation"), _("Move one saved depth operation")); }

    override CommandResult run(Context ctx) {
        auto operated = requireDepthOperated(target);
        auto ops = operated.copyDepthOps();
        enforce(fromIndex >= 0 && fromIndex < ops.length, "fromIndex out of range");
        enforce(toIndex >= 0 && toIndex < ops.length, "toIndex out of range");
        auto op = ops[cast(size_t)fromIndex];
        auto from = cast(size_t)fromIndex;
        ops = ops[0 .. from] ~ ops[from + 1 .. $];
        auto to = cast(size_t)toIndex;
        ops = ops[0 .. to] ~ [op] ~ ops[to .. $];
        replaceDepthOpsWithUndo(target, ops, "Move Depth Operation");
        return CommandResult(true);
    }
}

@EffectApply
class ClearDepthOpsCommand : ExCommand!(TW!(Node, "target", "Depth operation target node")) {
    this() { super(_("Clear Depth Operations"), _("Clear saved depth operations")); }

    override CommandResult run(Context ctx) {
        replaceDepthOpsWithUndo(target, null, "Clear Depth Operations");
        return CommandResult(true);
    }
}

@EffectApply
class ApplyDepthOpsCommand : ExCommand!(TW!(Node, "target", "Depth operation target node")) {
    this() { super(_("Apply Depth Operations"), _("Bake saved depth operations into per-vertex depths")); }

    override CommandResult run(Context ctx) {
        auto grid = requireDepthGrid(target);
        auto ops = requireDepthOperated(target).copyDepthOps();
        replaceDepthsWithUndo(target, computeDepthsFromOps(grid, ops), "Apply Depth Operations");
        return CommandResult(true);
    }
}

@EffectApply
class ImportPSDDepthsCommand : ExCommand!(
    TW!(string, "path", "Path to PSD depth map file"),
    TW!(bool, "invert", "Invert depth values; default is white as front"),
    TW!(float, "backDepth", "Depth value for black/back"),
    TW!(float, "frontDepth", "Depth value for white/front"),
    TW!(float, "depthScale", "Multiplier applied to imported depth values"),
    TW!(string, "convolution", "Sampling convolution mode"),
    TW!(string, "channel", "Depth channel"),
    TW!(int, "customRadius", "Custom convolution radius"),
    TW!(float, "alphaThreshold", "Minimum alpha for valid depth pixels"),
    TW!(string, "missingPolicy", "Policy for vertices with no sampled depth"),
    TW!(bool, "matchDirectGridName", "Allow direct PSD layer name to GridDeformer name matching")
) {
    this(
        string path,
        bool invert = false,
        float backDepth = -1.0f,
        float frontDepth = 1.0f,
        float depthScale = 1.0f,
        string convolution = "Gaussian3x3",
        string channel = "AverageRGB",
        int customRadius = 3,
        float alphaThreshold = 0.01f,
        string missingPolicy = "KeepExisting",
        bool matchDirectGridName = true
    ) {
        super(
            _("Import PSD Depth Map"),
            _("Import per-layer PSD grayscale depth data into mapped GridDeformers."),
            path,
            invert,
            backDepth,
            frontDepth,
            depthScale,
            convolution,
            channel,
            customRadius,
            alphaThreshold,
            missingPolicy,
            matchDirectGridName
        );
    }

    override
    ExCommandResult!JSONValue run(Context ctx) {
        auto puppet = incActivePuppet();
        if (puppet is null) return ExCommandResult!JSONValue(false, JSONValue(null), "No active puppet");
        if (!path.length) return ExCommandResult!JSONValue(false, JSONValue(null), "Path not provided");

        PsdDepthImportSettings settings;
        settings.invert = invert;
        settings.backDepth = backDepth;
        settings.frontDepth = frontDepth;
        settings.depthScale = depthScale;
        settings.alphaThreshold = alphaThreshold;
        settings.convolution = ngPsdDepthConvolutionFromString(convolution);
        settings.channel = ngPsdDepthChannelFromString(channel);
        settings.customRadius = customRadius;
        settings.missingPolicy = ngPsdDepthMissingPolicyFromString(missingPolicy);
        settings.matchDirectGridName = matchDirectGridName;

        auto imported = ngBuildPsdDepthsFromPSD(puppet, path, settings);
        return ngApplyPsdDepthImportResult(imported);
    }
}

void ngInitCommands(T)() if (is(T == DepthMapCommand)) {
    auto listDepths = new ListDepthsCommand();
    ngRegisterCommandMeta(listDepths);
    commands[DepthMapCommand.ListDepths] = listDepths;

    auto setDepths = new SetDepthsCommand();
    ngRegisterCommandMeta(setDepths);
    commands[DepthMapCommand.SetDepths] = setDepths;

    auto clearDepths = new ClearDepthsCommand();
    ngRegisterCommandMeta(clearDepths);
    commands[DepthMapCommand.ClearDepths] = clearDepths;

    auto listOps = new ListDepthOpsCommand();
    ngRegisterCommandMeta(listOps);
    commands[DepthMapCommand.ListDepthOps] = listOps;

    auto setOps = new SetDepthOpsCommand();
    ngRegisterCommandMeta(setOps);
    commands[DepthMapCommand.SetDepthOps] = setOps;

    auto addOp = new AddDepthOpCommand();
    ngRegisterCommandMeta(addOp);
    commands[DepthMapCommand.AddDepthOp] = addOp;

    auto updateOp = new UpdateDepthOpCommand();
    ngRegisterCommandMeta(updateOp);
    commands[DepthMapCommand.UpdateDepthOp] = updateOp;

    auto removeOp = new RemoveDepthOpCommand();
    ngRegisterCommandMeta(removeOp);
    commands[DepthMapCommand.RemoveDepthOp] = removeOp;

    auto moveOp = new MoveDepthOpCommand();
    ngRegisterCommandMeta(moveOp);
    commands[DepthMapCommand.MoveDepthOp] = moveOp;

    auto clearOps = new ClearDepthOpsCommand();
    ngRegisterCommandMeta(clearOps);
    commands[DepthMapCommand.ClearDepthOps] = clearOps;

    auto applyOps = new ApplyDepthOpsCommand();
    ngRegisterCommandMeta(applyOps);
    commands[DepthMapCommand.ApplyDepthOps] = applyOps;

    auto importPsdDepths = new ImportPSDDepthsCommand(
        "", false, -1.0f, 1.0f, 1.0f, "Gaussian3x3", "AverageRGB", 3, 0.01f, "KeepExisting", true
    );
    ngRegisterCommandMeta(importPsdDepths);
    commands[DepthMapCommand.ImportPSDDepths] = importPsdDepths;
}
