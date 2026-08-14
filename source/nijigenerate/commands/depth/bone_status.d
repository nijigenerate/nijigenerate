/*
    Copyright © 2026, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.commands.depth.bone_status;

import core.time : MonoTime, seconds;
import nijigenerate.ext.nodes.exdepthbone : ExDepthRigRoot;
import nijilive;
import nijilive.core.nodes.deformable : Deformable;
import std.algorithm.comparison : max, min;

enum DepthBoneUpdateState {
    Detected,
    Queued,
    Processing,
    Applied,
    Stale,
    Failed,
}

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

private struct DepthBoneUpdateRecord {
    ExDepthRigRoot root;
    Node target;
    Puppet puppet;
    Parameter parameter;
    vec2u keypoint;
    string reason;
    string detail;
    size_t expectedWork;
    size_t appliedWork;
    size_t staleWork;
    size_t queuedWork;
    size_t processingWork;
    size_t[Parameter] plannedByParameter;
    uint retryCount;
    ulong generation;
    bool detected;
    bool processingStarted;
    bool failed;
    MonoTime changedAt;
    bool hasLocalBounds;
    vec2 localBoundsMin;
    vec2 localBoundsMax;
    Vec3Array localOutline;
    Vec3Array pendingLocalOutline;
    vec3 labelLocal;
    bool hasLabelWorld;
    vec3 labelWorld;
}

private enum DepthBoneWorkState {
    Queued,
    Processing,
}

private struct DepthBoneUpdateWork {
    ulong targetKey;
    ulong generation;
    DepthBoneWorkState state;
}

private struct DepthBoneUpdateWorkKey {
    uint batchId;
    uint targetUuid;
}

private DepthBoneUpdateRecord[ulong] updateRecords;
private DepthBoneUpdateWork[DepthBoneUpdateWorkKey] updateWork;

private ulong statusTargetKey(ExDepthRigRoot root, Node target) {
    if (root is null || target is null) return 0;
    return (cast(ulong)root.uuid << 32) | cast(ulong)target.uuid;
}

private ref DepthBoneUpdateRecord ensureRecord(ExDepthRigRoot root, Node target) {
    auto key = statusTargetKey(root, target);
    auto record = key in updateRecords;
    if (record is null) {
        DepthBoneUpdateRecord initial;
        initial.root = root;
        initial.target = target;
        initial.generation = 1;
        initial.changedAt = MonoTime.currTime;
        updateRecords[key] = initial;
        record = key in updateRecords;
    }
    record.root = root;
    record.target = target;
    record.puppet = target.puppet();
    return *record;
}

private void removeTargetWork(ulong targetKey) {
    DepthBoneUpdateWorkKey[] removeKeys;
    foreach (key, work; updateWork) {
        if (work.targetKey != targetKey) continue;
        auto record = targetKey in updateRecords;
        if (record !is null && record.generation == work.generation) {
            final switch (work.state) {
            case DepthBoneWorkState.Queued:
                if (record.queuedWork > 0) record.queuedWork--;
                break;
            case DepthBoneWorkState.Processing:
                if (record.processingWork > 0) record.processingWork--;
                break;
            }
        }
        removeKeys ~= key;
    }
    foreach (key; removeKeys) updateWork.remove(key);
}

private void refreshLocalBounds(ref DepthBoneUpdateRecord record) {
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

private void beginGeneration(ref DepthBoneUpdateRecord record) {
    auto key = statusTargetKey(record.root, record.target);
    removeTargetWork(key);
    record.generation++;
    if (record.generation == 0) record.generation = 1;
    record.expectedWork = 0;
    record.appliedWork = 0;
    record.staleWork = 0;
    record.queuedWork = 0;
    record.processingWork = 0;
    record.plannedByParameter = null;
    record.retryCount = 0;
    record.detail = null;
    record.failed = false;
    record.detected = true;
    record.processingStarted = false;
    // Freeze the progress label anchor for this entire update generation.
    // Completion may refresh target bounds, but must not move a RUN label.
    record.hasLabelWorld = false;
    refreshLocalBounds(record);
    record.changedAt = MonoTime.currTime;
}

private bool hasCurrentWork(ref DepthBoneUpdateRecord record) {
    return record.queuedWork > 0 || record.processingWork > 0;
}

private void removePreviousAppliedStatuses(Puppet puppet, ulong detectedTargetKey) {
    ulong[] removeKeys;
    foreach (targetKey, ref record; updateRecords) {
        if (targetKey == detectedTargetKey || record.puppet !is puppet) continue;
        if (record.failed || record.detected || record.staleWork > 0 ||
            hasCurrentWork(record) || record.appliedWork < record.expectedWork) continue;
        removeKeys ~= targetKey;
    }
    foreach (targetKey; removeKeys) {
        removeTargetWork(targetKey);
        updateRecords.remove(targetKey);
    }
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
    if (!record.detected || hasCurrentWork(*record))
        beginGeneration(*record);
    record.parameter = parameter;
    record.keypoint = keypoint;
    record.reason = reason;
    record.changedAt = MonoTime.currTime;
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
    if (!record.detected && !hasCurrentWork(*record) &&
        record.expectedWork <= record.appliedWork) {
        beginGeneration(*record);
    }
    record.detected = false;
    record.parameter = parameter;
    record.keypoint = keypoint;
    record.reason = reason;
    if (parameter !is null) {
        auto previous = parameter in record.plannedByParameter;
        auto oldCount = previous is null ? 0 : *previous;
        if (expectedWork > oldCount) {
            record.expectedWork += expectedWork - oldCount;
            record.plannedByParameter[parameter] = expectedWork;
        }
    } else if (expectedWork > record.expectedWork) {
        record.expectedWork = expectedWork;
    }
    record.changedAt = MonoTime.currTime;
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
    if (!record.detected && !hasCurrentWork(*record) &&
        record.expectedWork <= record.appliedWork) {
        beginGeneration(*record);
    }
    record.detected = false;
    record.parameter = parameter;
    record.keypoint = keypoint;
    record.reason = reason;
    record.retryCount = retryCount;
    record.failed = false;
    record.detail = null;
    auto workKey = DepthBoneUpdateWorkKey(batchId, target.uuid);
    if (workKey !in updateWork) {
        DepthBoneUpdateWork work;
        work.targetKey = statusTargetKey(root, target);
        work.generation = record.generation;
        work.state = DepthBoneWorkState.Queued;
        updateWork[workKey] = work;
        record.queuedWork++;
    }
    auto trackedWork = record.appliedWork +
        record.queuedWork + record.processingWork;
    if (record.expectedWork < trackedWork)
        record.expectedWork = trackedWork;
    record.changedAt = MonoTime.currTime;
}

void ngDepthBoneUpdateProcessing(uint batchId, ExDepthRigRoot root, Node target) {
    if (root is null || target is null) return;
    auto workKey = DepthBoneUpdateWorkKey(batchId, target.uuid);
    auto work = workKey in updateWork;
    if (work is null) return;
    auto record = statusTargetKey(root, target) in updateRecords;
    if (work.state == DepthBoneWorkState.Queued &&
        record !is null && record.generation == work.generation) {
        if (record.queuedWork > 0) record.queuedWork--;
        record.processingWork++;
        record.processingStarted = true;
        record.changedAt = MonoTime.currTime;
    }
    work.state = DepthBoneWorkState.Processing;
}

void ngDepthBoneUpdateApplied(uint batchId, ExDepthRigRoot root, Node target) {
    if (root is null || target is null) return;
    auto workKey = DepthBoneUpdateWorkKey(batchId, target.uuid);
    auto work = workKey in updateWork;
    if (work is null) return;
    auto generation = work.generation;
    auto workState = work.state;
    updateWork.remove(workKey);
    auto record = statusTargetKey(root, target) in updateRecords;
    if (record is null || record.generation != generation) return;
    final switch (workState) {
    case DepthBoneWorkState.Queued:
        if (record.queuedWork > 0) record.queuedWork--;
        break;
    case DepthBoneWorkState.Processing:
        if (record.processingWork > 0) record.processingWork--;
        break;
    }
    record.appliedWork++;
    record.detected = false;
    record.failed = false;
    record.detail = null;
    if (record.appliedWork >= record.expectedWork)
        refreshLocalBounds(*record);
    record.changedAt = MonoTime.currTime;
}

void ngDepthBoneUpdateStale(
    uint batchId,
    ExDepthRigRoot root,
    Node target,
    string detail,
) {
    if (root is null || target is null) return;
    auto workKey = DepthBoneUpdateWorkKey(batchId, target.uuid);
    auto work = workKey in updateWork;
    ulong generation;
    DepthBoneWorkState workState;
    bool hadWork;
    if (work !is null) {
        generation = work.generation;
        workState = work.state;
        hadWork = true;
        updateWork.remove(workKey);
    }
    auto record = statusTargetKey(root, target) in updateRecords;
    if (record is null || (generation != 0 && record.generation != generation)) return;
    if (hadWork) {
        final switch (workState) {
        case DepthBoneWorkState.Queued:
            if (record.queuedWork > 0) record.queuedWork--;
            break;
        case DepthBoneWorkState.Processing:
            if (record.processingWork > 0) record.processingWork--;
            break;
        }
    }
    record.staleWork++;
    record.detail = detail;
    record.detected = false;
    record.changedAt = MonoTime.currTime;
}

void ngDepthBoneUpdateBatchCanceled(uint batchId, string detail = null) {
    DepthBoneUpdateWorkKey[] removeKeys;
    foreach (key, work; updateWork) {
        if (key.batchId != batchId) continue;
        auto record = work.targetKey in updateRecords;
        if (record !is null && record.generation == work.generation) {
            final switch (work.state) {
            case DepthBoneWorkState.Queued:
                if (record.queuedWork > 0) record.queuedWork--;
                break;
            case DepthBoneWorkState.Processing:
                if (record.processingWork > 0) record.processingWork--;
                break;
            }
        }
        if (detail.length > 0) {
            if (record !is null && record.generation == work.generation) {
                record.staleWork++;
                record.detail = detail;
                record.detected = false;
                record.changedAt = MonoTime.currTime;
            }
        }
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
    removeTargetWork(statusTargetKey(root, target));
    record.parameter = parameter;
    record.keypoint = keypoint;
    record.reason = reason;
    record.detail = detail;
    record.detected = false;
    record.failed = true;
    refreshLocalBounds(*record);
    record.changedAt = MonoTime.currTime;
}

void ngDepthBoneUpdateAbort(string detail) {
    foreach (ref record; updateRecords) {
        if (!hasCurrentWork(record)) continue;
        record.detected = false;
        record.failed = true;
        record.detail = detail;
        record.queuedWork = 0;
        record.processingWork = 0;
        record.changedAt = MonoTime.currTime;
    }
    updateWork = null;
}

private DepthBoneUpdateState recordState(
    ref DepthBoneUpdateRecord record,
) {
    if (record.failed) return DepthBoneUpdateState.Failed;
    if (record.processingWork > 0) return DepthBoneUpdateState.Processing;
    // Once one job in this generation has reached the GPU, keep the
    // user-facing lifecycle in RUN until the whole planned generation ends.
    // Chunk production and readback have different per-frame limits, so the
    // instantaneous GPU queue can legitimately become empty between chunks;
    // exposing that implementation detail as WAIT makes the outline and badge
    // alternate between purple/dashed and cyan/solid while progress advances.
    if (record.processingStarted && record.appliedWork < record.expectedWork) {
        if (record.staleWork > 0 && record.queuedWork == 0)
            return DepthBoneUpdateState.Stale;
        return DepthBoneUpdateState.Processing;
    }
    if (record.queuedWork > 0) return DepthBoneUpdateState.Queued;
    if (record.detected) return DepthBoneUpdateState.Detected;
    if (record.staleWork > 0 && record.appliedWork < record.expectedWork)
        return DepthBoneUpdateState.Stale;
    if (record.appliedWork < record.expectedWork)
        return DepthBoneUpdateState.Queued;
    return DepthBoneUpdateState.Applied;
}

DepthBoneUpdateStatus[] ngDepthBoneUpdateStatuses(Puppet puppet, bool includeExpired = false) {
    DepthBoneUpdateStatus[] result;
    ulong[] removeKeys;
    auto now = MonoTime.currTime;
    foreach (targetKey, ref record; updateRecords) {
        if (record.root is null || record.target is null ||
            (puppet !is null && record.puppet !is puppet)) {
            removeKeys ~= targetKey;
            continue;
        }
        auto state = recordState(record);
        if (!includeExpired && state == DepthBoneUpdateState.Applied &&
            now - record.changedAt > seconds(2)) {
            removeKeys ~= targetKey;
            continue;
        }
        DepthBoneUpdateStatus status;
        status.root = record.root;
        status.target = record.target;
        status.parameter = record.parameter;
        status.keypoint = record.keypoint;
        status.state = state;
        status.reason = record.reason;
        status.detail = record.detail;
        status.expectedWork = record.expectedWork;
        status.appliedWork = record.appliedWork;
        status.queuedWork = record.queuedWork;
        status.processingWork = record.processingWork;
        status.staleWork = record.staleWork;
        status.retryCount = record.retryCount;
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
        removeTargetWork(targetKey);
        updateRecords.remove(targetKey);
    }
    return result;
}

void ngClearDepthBoneUpdateStatuses() {
    updateRecords = null;
    updateWork = null;
}
