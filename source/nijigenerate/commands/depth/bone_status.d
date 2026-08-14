/*
    Copyright © 2026, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.commands.depth.bone_status;

import nijigenerate.core.asyncderivedupdate;
import nijigenerate.ext.nodes.exdepthbone : ExDepthRigRoot;
import nijigenerate.project : incActiveProject;
import nijilive;
import nijilive.core.nodes.deformable : Deformable;
import std.algorithm.comparison : max, min;

/** Stable provider identity for updates derived from DepthBone edits. */
enum uint NgDepthBoneDerivedUpdateProviderId = 0x44424F4E; // "DBON"

enum DepthBoneUpdateState {
    Detected,
    Queued,
    Processing,
    Applied,
    Stale,
    Failed,
}

/** Compatibility view used by DepthBone diagnostics and commands. */
struct DepthBoneUpdateStatus {
    ExDepthRigRoot root;
    Node target;
    Parameter parameter;
    vec2u keypoint;
    DepthBoneUpdateState state;
    string reason;
    string detail;
    size_t expectedWork;
    size_t appliedWork;
    size_t queuedWork;
    size_t processingWork;
    size_t staleWork;
    uint retryCount;
    ulong generation;
    bool hasLocalBounds;
    vec2 localBoundsMin;
    vec2 localBoundsMax;
    Vec3Array localOutline;
    Vec3Array pendingLocalOutline;
    vec3 labelLocal;
    bool hasLabelWorld;
    vec3 labelWorld;
}

private struct DepthBoneUpdateAdapterRecord {
    ExDepthRigRoot root;
    Node target;
    Puppet puppet;
    Parameter parameter;
    vec2u keypoint;
    string reason;
    size_t[Parameter] plannedByParameter;
    ulong generation;
    bool detected;
    bool hasLocalBounds;
    vec2 localBoundsMin;
    vec2 localBoundsMax;
    Vec3Array localOutline;
    Vec3Array pendingLocalOutline;
    vec3 labelLocal;
    bool hasLabelWorld;
    vec3 labelWorld;
    AsyncDerivedUpdateRunHandle run;
    AsyncDerivedUpdateTargetHandle progressTarget;
}

private struct DepthBoneUpdateWorkKey {
    uint batchId;
    uint targetUuid;
}

private struct DepthBoneUpdateWorkRecord {
    ulong targetKey;
    ulong generation;
    AsyncDerivedUpdateWorkHandle progressWork;
}

private DepthBoneUpdateAdapterRecord[ulong] updateRecords;
private DepthBoneUpdateWorkRecord[DepthBoneUpdateWorkKey] updateWork;
private AsyncDerivedUpdateScopeId[Puppet] fallbackScopes;

private ulong statusTargetKey(ExDepthRigRoot root, Node target) {
    if (root is null || target is null) return 0;
    return (cast(ulong)root.uuid << 32) | cast(ulong)target.uuid;
}

private AsyncDerivedUpdateScopeId updateScope(Puppet puppet) {
    auto project = incActiveProject();
    if (project !is null && project.puppet is puppet)
        return project.derivedUpdateScope;
    if (auto found = puppet in fallbackScopes) return *found;
    auto created = incAsyncDerivedUpdateCreateScope();
    fallbackScopes[puppet] = created;
    return created;
}

private ref DepthBoneUpdateAdapterRecord ensureRecord(
    ExDepthRigRoot root,
    Node target,
) {
    auto key = statusTargetKey(root, target);
    auto record = key in updateRecords;
    if (record is null) {
        DepthBoneUpdateAdapterRecord initial;
        initial.root = root;
        initial.target = target;
        initial.puppet = target.puppet();
        initial.generation = 1;
        updateRecords[key] = initial;
        record = key in updateRecords;
    }
    record.root = root;
    record.target = target;
    record.puppet = target.puppet();
    return *record;
}

private void refreshTargetVisual(ref DepthBoneUpdateAdapterRecord record) {
    auto target = cast(Deformable)record.target;
    if (target is null || target.vertices.length == 0) {
        record.hasLocalBounds = false;
        record.localOutline = Vec3Array.init;
        record.pendingLocalOutline = Vec3Array.init;
        return;
    }

    auto first = target.vertices[0].toVector();
    if (target.deformation.length > 0) first += target.deformation[0].toVector();
    auto minPoint = first;
    auto maxPoint = first;
    foreach (i; 1 .. target.vertices.length) {
        auto point = target.vertices[i].toVector();
        if (i < target.deformation.length) point += target.deformation[i].toVector();
        minPoint.x = min(minPoint.x, point.x);
        minPoint.y = min(minPoint.y, point.y);
        maxPoint.x = max(maxPoint.x, point.x);
        maxPoint.y = max(maxPoint.y, point.y);
    }
    record.hasLocalBounds = true;
    record.localBoundsMin = minPoint;
    record.localBoundsMax = maxPoint;

    auto span = maxPoint - minPoint;
    auto margin = max(4.0f, max(span.x, span.y) * 0.03f);
    auto outlineMin = minPoint - vec2(margin, margin);
    auto outlineMax = maxPoint + vec2(margin, margin);
    record.localOutline = Vec3Array([
        vec3(outlineMin.x, outlineMin.y, 0), vec3(outlineMax.x, outlineMin.y, 0),
        vec3(outlineMax.x, outlineMin.y, 0), vec3(outlineMax.x, outlineMax.y, 0),
        vec3(outlineMax.x, outlineMax.y, 0), vec3(outlineMin.x, outlineMax.y, 0),
        vec3(outlineMin.x, outlineMax.y, 0), vec3(outlineMin.x, outlineMin.y, 0),
    ]);
    record.pendingLocalOutline = Vec3Array.init;
    enum segmentCount = 16;
    foreach (i; 0 .. record.localOutline.length / 2) {
        auto start = record.localOutline[i * 2];
        auto end = record.localOutline[i * 2 + 1];
        auto delta = end - start;
        foreach (segment; 0 .. segmentCount) {
            if ((segment & 1) != 0) continue;
            auto t0 = cast(float)segment / segmentCount;
            auto t1 = cast(float)(segment + 1) / segmentCount;
            record.pendingLocalOutline ~= start + delta * t0;
            record.pendingLocalOutline ~= start + delta * t1;
        }
    }
    record.labelLocal = vec3(outlineMax.x, outlineMin.y, 0);
    if (!record.hasLabelWorld) {
        record.labelWorld = (record.target.getDynamicMatrix() *
            vec4(record.labelLocal, 1)).xyz;
        record.hasLabelWorld = true;
    }
}

private AsyncDerivedUpdateViewportVisual viewportVisual(
    ref DepthBoneUpdateAdapterRecord record,
) {
    AsyncDerivedUpdateViewportVisual visual;
    visual.viewportChannel =
        cast(uint)AsyncDerivedUpdateViewportChannel.Model;
    visual.hasAnchor = record.hasLabelWorld;
    visual.anchorWorld = record.labelWorld;
    auto targetToWorld = record.target.getDynamicMatrix();
    vec3[] outlineWorld;
    foreach (point; record.localOutline)
        outlineWorld ~= (targetToWorld * vec4(point, 1)).xyz;
    visual.outlineWorld = outlineWorld;
    return visual;
}

private void removeTargetWork(ulong targetKey) {
    DepthBoneUpdateWorkKey[] removeKeys;
    foreach (key, work; updateWork) {
        if (work.targetKey != targetKey) continue;
        incAsyncDerivedUpdateStale(
            work.progressWork, "Superseded by a newer Depth Bone update");
        removeKeys ~= key;
    }
    foreach (key; removeKeys) updateWork.remove(key);
}

private void beginGeneration(ref DepthBoneUpdateAdapterRecord record) {
    auto key = statusTargetKey(record.root, record.target);
    removeTargetWork(key);
    record.generation++;
    if (record.generation == 0) record.generation = 1;
    record.plannedByParameter = null;
    record.detected = true;
    record.hasLabelWorld = false;
    refreshTargetVisual(record);

    AsyncDerivedUpdateOrigin origin;
    origin.projectScope = updateScope(record.puppet);
    origin.transactionId = record.generation;
    origin.providerId = NgDepthBoneDerivedUpdateProviderId;
    origin.operationName = "Depth Bone Update";
    record.run = incAsyncDerivedUpdateBeginRun(origin);

    AsyncDerivedUpdateTargetDesc desc;
    desc.key = AsyncDerivedUpdateTargetKey(
        NgDepthBoneDerivedUpdateProviderId,
        origin.projectScope.id,
        key);
    desc.label = record.target.name;
    desc.reason = record.reason;
    desc.sourceRevision = record.generation;
    desc.mergePolicy = AsyncDerivedUpdateMergePolicy.SupersedeRunning;
    desc.visual = viewportVisual(record);
    record.progressTarget = incAsyncDerivedUpdateTrackTarget(record.run, desc);
}

private bool currentSnapshot(
    ref DepthBoneUpdateAdapterRecord record,
    out AsyncDerivedUpdateSnapshot snapshot,
) {
    return record.progressTarget.valid &&
        incAsyncDerivedUpdateTargetSnapshot(
            record.progressTarget, snapshot, true);
}

private bool hasCurrentWork(ref DepthBoneUpdateAdapterRecord record) {
    AsyncDerivedUpdateSnapshot snapshot;
    return currentSnapshot(record, snapshot) &&
        (snapshot.queuedUnits > 0 || snapshot.runningUnits > 0);
}

private bool completedGeneration(ref DepthBoneUpdateAdapterRecord record) {
    AsyncDerivedUpdateSnapshot snapshot;
    return currentSnapshot(record, snapshot) &&
        snapshot.expectedUnits <= snapshot.appliedUnits &&
        snapshot.queuedUnits == 0 && snapshot.runningUnits == 0;
}

private void ensureGeneration(ref DepthBoneUpdateAdapterRecord record) {
    AsyncDerivedUpdateSnapshot snapshot;
    if (!currentSnapshot(record, snapshot)) beginGeneration(record);
}

private void removePreviousAppliedStatuses(Puppet puppet, ulong detectedTargetKey) {
    ulong[] removeKeys;
    foreach (targetKey, ref record; updateRecords) {
        if (targetKey == detectedTargetKey || record.puppet !is puppet) continue;
        AsyncDerivedUpdateSnapshot snapshot;
        if (!currentSnapshot(record, snapshot) ||
            snapshot.state != AsyncDerivedUpdateState.Applied) continue;
        incAsyncDerivedUpdateForgetTarget(record.progressTarget);
        removeKeys ~= targetKey;
    }
    foreach (targetKey; removeKeys) updateRecords.remove(targetKey);
}

void ngDepthBoneUpdateDetected(
    ExDepthRigRoot root,
    Node target,
    Parameter parameter,
    vec2u keypoint,
    string reason,
) {
    if (root is null || target is null) return;
    auto targetKey = statusTargetKey(root, target);
    removePreviousAppliedStatuses(target.puppet(), targetKey);
    auto record = &ensureRecord(root, target);
    if (!record.progressTarget.valid || !record.detected || hasCurrentWork(*record))
        beginGeneration(*record);
    record.parameter = parameter;
    record.keypoint = keypoint;
    record.reason = reason;
    incAsyncDerivedUpdateSetContext(record.progressTarget, reason);
}

void ngDepthBoneUpdatePlanned(
    ExDepthRigRoot root,
    Node target,
    Parameter parameter,
    vec2u keypoint,
    string reason,
    size_t expectedWork,
) {
    if (root is null || target is null || expectedWork == 0) return;
    auto record = &ensureRecord(root, target);
    ensureGeneration(*record);
    if (!record.detected && !hasCurrentWork(*record) && completedGeneration(*record))
        beginGeneration(*record);
    record.detected = false;
    record.parameter = parameter;
    record.keypoint = keypoint;
    record.reason = reason;
    AsyncDerivedUpdateSnapshot snapshot;
    currentSnapshot(*record, snapshot);
    auto totalExpected = snapshot.expectedUnits;
    if (parameter !is null) {
        auto previous = parameter in record.plannedByParameter;
        auto oldCount = previous is null ? 0 : *previous;
        if (expectedWork > oldCount) {
            totalExpected += expectedWork - oldCount;
            record.plannedByParameter[parameter] = expectedWork;
        }
    } else if (expectedWork > totalExpected) {
        totalExpected = expectedWork;
    }
    incAsyncDerivedUpdateSetExpected(record.progressTarget, totalExpected);
    incAsyncDerivedUpdateSetContext(record.progressTarget, reason);
}

void ngDepthBoneUpdateQueued(
    uint batchId,
    ExDepthRigRoot root,
    Node target,
    Parameter parameter,
    vec2u keypoint,
    string reason,
    uint retryCount = 0,
) {
    if (batchId == 0 || root is null || target is null) return;
    auto record = &ensureRecord(root, target);
    ensureGeneration(*record);
    if (!record.detected && !hasCurrentWork(*record) && completedGeneration(*record))
        beginGeneration(*record);
    record.detected = false;
    record.parameter = parameter;
    record.keypoint = keypoint;
    record.reason = reason;
    incAsyncDerivedUpdateSetContext(
        record.progressTarget, reason, null, retryCount);
    auto workKey = DepthBoneUpdateWorkKey(batchId, target.uuid);
    if (workKey !in updateWork) {
        DepthBoneUpdateWorkRecord work;
        work.targetKey = statusTargetKey(root, target);
        work.generation = record.generation;
        work.progressWork = incAsyncDerivedUpdateQueue(record.progressTarget);
        updateWork[workKey] = work;
    }
}

void ngDepthBoneUpdateProcessing(uint batchId, ExDepthRigRoot root, Node target) {
    if (root is null || target is null) return;
    auto work = DepthBoneUpdateWorkKey(batchId, target.uuid) in updateWork;
    if (work is null) return;
    incAsyncDerivedUpdateStart(work.progressWork);
}

void ngDepthBoneUpdateApplied(uint batchId, ExDepthRigRoot root, Node target) {
    if (root is null || target is null) return;
    auto workKey = DepthBoneUpdateWorkKey(batchId, target.uuid);
    auto work = workKey in updateWork;
    if (work is null) return;
    auto record = statusTargetKey(root, target) in updateRecords;
    if (record is null || record.generation != work.generation) {
        updateWork.remove(workKey);
        return;
    }
    incAsyncDerivedUpdateApplied(work.progressWork);
    updateWork.remove(workKey);
    AsyncDerivedUpdateSnapshot snapshot;
    if (currentSnapshot(*record, snapshot) &&
        snapshot.appliedUnits >= snapshot.expectedUnits)
        refreshTargetVisual(*record);
}

void ngDepthBoneUpdateStale(
    uint batchId,
    ExDepthRigRoot root,
    Node target,
    string detail,
) {
    if (root is null || target is null) return;
    auto workKey = DepthBoneUpdateWorkKey(batchId, target.uuid);
    bool hadWork;
    if (auto work = workKey in updateWork) {
        hadWork = true;
        incAsyncDerivedUpdateStale(work.progressWork, detail);
        updateWork.remove(workKey);
    }
    if (auto record = statusTargetKey(root, target) in updateRecords) {
        record.detected = false;
        if (!hadWork)
            incAsyncDerivedUpdateMarkTargetStale(
                record.progressTarget, detail);
        incAsyncDerivedUpdateSetContext(
            record.progressTarget, record.reason, detail);
    }
}

void ngDepthBoneUpdateBatchCanceled(uint batchId, string detail = null) {
    DepthBoneUpdateWorkKey[] removeKeys;
    foreach (key, work; updateWork) {
        if (key.batchId != batchId) continue;
        if (detail.length)
            incAsyncDerivedUpdateStale(work.progressWork, detail);
        else
            incAsyncDerivedUpdateDiscard(work.progressWork);
        removeKeys ~= key;
    }
    foreach (key; removeKeys) updateWork.remove(key);
}

void ngDepthBoneUpdateFailed(
    ExDepthRigRoot root,
    Node target,
    Parameter parameter,
    vec2u keypoint,
    string reason,
    string detail,
) {
    if (root is null || target is null) return;
    auto record = &ensureRecord(root, target);
    ensureGeneration(*record);
    removeTargetWork(statusTargetKey(root, target));
    record.parameter = parameter;
    record.keypoint = keypoint;
    record.reason = reason;
    record.detected = false;
    refreshTargetVisual(*record);
    incAsyncDerivedUpdateSetContext(record.progressTarget, reason, detail);
    incAsyncDerivedUpdateFailTarget(record.progressTarget, detail);
}

void ngDepthBoneUpdateAbort(string detail) {
    foreach (ref record; updateRecords) {
        if (!hasCurrentWork(record)) continue;
        record.detected = false;
        incAsyncDerivedUpdateFailTarget(record.progressTarget, detail);
    }
    updateWork = null;
}

private DepthBoneUpdateState depthBoneState(AsyncDerivedUpdateState state) {
    final switch (state) {
    case AsyncDerivedUpdateState.Detected: return DepthBoneUpdateState.Detected;
    case AsyncDerivedUpdateState.Queued: return DepthBoneUpdateState.Queued;
    case AsyncDerivedUpdateState.Running: return DepthBoneUpdateState.Processing;
    case AsyncDerivedUpdateState.Applied: return DepthBoneUpdateState.Applied;
    case AsyncDerivedUpdateState.Stale: return DepthBoneUpdateState.Stale;
    case AsyncDerivedUpdateState.Failed: return DepthBoneUpdateState.Failed;
    case AsyncDerivedUpdateState.Canceled: return DepthBoneUpdateState.Stale;
    }
}

DepthBoneUpdateStatus[] ngDepthBoneUpdateStatuses(
    Puppet puppet,
    bool includeExpired = false,
) {
    DepthBoneUpdateStatus[] result;
    ulong[] removeKeys;
    foreach (targetKey, ref record; updateRecords) {
        if (record.root is null || record.target is null ||
            (puppet !is null && record.puppet !is puppet)) {
            if (record.root is null || record.target is null)
                removeKeys ~= targetKey;
            continue;
        }
        AsyncDerivedUpdateSnapshot snapshot;
        if (!incAsyncDerivedUpdateTargetSnapshot(
            record.progressTarget, snapshot, includeExpired)) {
            removeKeys ~= targetKey;
            continue;
        }
        DepthBoneUpdateStatus status;
        status.root = record.root;
        status.target = record.target;
        status.parameter = record.parameter;
        status.keypoint = record.keypoint;
        status.state = depthBoneState(snapshot.state);
        status.reason = snapshot.reason;
        status.detail = snapshot.detail;
        status.expectedWork = cast(size_t)snapshot.expectedUnits;
        status.appliedWork = cast(size_t)snapshot.appliedUnits;
        status.queuedWork = cast(size_t)snapshot.queuedUnits;
        status.processingWork = cast(size_t)snapshot.runningUnits;
        status.staleWork = cast(size_t)snapshot.staleUnits;
        status.retryCount = snapshot.retryCount;
        status.generation = record.generation;
        status.hasLocalBounds = record.hasLocalBounds;
        status.localBoundsMin = record.localBoundsMin;
        status.localBoundsMax = record.localBoundsMax;
        status.localOutline = record.localOutline;
        status.pendingLocalOutline = record.pendingLocalOutline;
        status.labelLocal = record.labelLocal;
        status.hasLabelWorld = record.hasLabelWorld;
        status.labelWorld = record.labelWorld;
        result ~= status;
    }
    foreach (targetKey; removeKeys) {
        if (auto record = targetKey in updateRecords)
            incAsyncDerivedUpdateForgetTarget(record.progressTarget);
        updateRecords.remove(targetKey);
    }
    return result;
}

/** Domain adapter helper used by regression tests and target snapshot creation. */
bool ngBuildDepthBoneUpdateTargetOutline(
    Node targetNode,
    out Vec3Array lines,
    out mat4 targetToWorld,
    out vec3 labelWorld,
) {
    lines = Vec3Array.init;
    targetToWorld = mat4.identity;
    labelWorld = vec3.init;
    auto target = cast(Deformable)targetNode;
    if (target is null || target.vertices.length == 0) return false;

    auto first = target.vertices[0].toVector();
    if (target.deformation.length > 0) first += target.deformation[0].toVector();
    auto minPoint = first;
    auto maxPoint = first;
    foreach (i; 1 .. target.vertices.length) {
        auto point = target.vertices[i].toVector();
        if (i < target.deformation.length) point += target.deformation[i].toVector();
        minPoint.x = min(minPoint.x, point.x);
        minPoint.y = min(minPoint.y, point.y);
        maxPoint.x = max(maxPoint.x, point.x);
        maxPoint.y = max(maxPoint.y, point.y);
    }
    auto span = maxPoint - minPoint;
    auto margin = max(4.0f, max(span.x, span.y) * 0.03f);
    auto outlineMin = minPoint - vec2(margin, margin);
    auto outlineMax = maxPoint + vec2(margin, margin);
    lines = Vec3Array([
        vec3(outlineMin.x, outlineMin.y, 0), vec3(outlineMax.x, outlineMin.y, 0),
        vec3(outlineMax.x, outlineMin.y, 0), vec3(outlineMax.x, outlineMax.y, 0),
        vec3(outlineMax.x, outlineMax.y, 0), vec3(outlineMin.x, outlineMax.y, 0),
        vec3(outlineMin.x, outlineMax.y, 0), vec3(outlineMin.x, outlineMin.y, 0),
    ]);
    targetToWorld = targetNode.getDynamicMatrix();
    labelWorld = (targetToWorld * vec4(outlineMax.x, outlineMin.y, 0, 1)).xyz;
    return true;
}

void ngClearDepthBoneUpdateStatuses() {
    incAsyncDerivedUpdateClearProvider(NgDepthBoneDerivedUpdateProviderId);
    updateRecords = null;
    updateWork = null;
    fallbackScopes = null;
}
