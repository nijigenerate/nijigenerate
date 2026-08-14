/*
    Copyright © 2026, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.core.asyncderivedupdate;

import core.time : MonoTime, seconds;
import nijilive : vec3;
import std.algorithm.comparison : min;

/**
    Progress reporting for asynchronous updates derived from another edit.

    This registry observes work; it does not schedule or execute it.  Explicit
    tools with their own UI (for example AutoMesh) do not register here.
*/

enum AsyncDerivedUpdateState {
    Detected,
    Queued,
    Running,
    Applied,
    Stale,
    Failed,
    Canceled,
}

enum AsyncDerivedUpdateMergePolicy {
    CoalescePending,
    ExtendRunning,
    SupersedeRunning,
}

enum AsyncDerivedUpdateDisplay : uint {
    None = 0,
    ViewportBadge = 1 << 0,
    ViewportOutline = 1 << 1,
    Details = 1 << 2,
}

enum AsyncDerivedUpdateViewportChannel : uint {
    None = 0,
    Model = 1 << 0,
    Depth = 1 << 1,
    Any = uint.max,
}

struct AsyncDerivedUpdateRunHandle {
    ulong id;
    bool valid() const { return id != 0; }
}

struct AsyncDerivedUpdateTargetHandle {
    ulong id;
    bool valid() const { return id != 0; }
}

struct AsyncDerivedUpdateWorkHandle {
    ulong id;
    bool valid() const { return id != 0; }
}

struct AsyncDerivedUpdateScopeId {
    ulong id;
    bool valid() const { return id != 0; }
}

struct AsyncDerivedUpdateOrigin {
    AsyncDerivedUpdateScopeId projectScope;
    ulong transactionId;
    uint providerId;
    string operationName;
}

struct AsyncDerivedUpdateTargetKey {
    uint providerId;
    ulong scopeId;
    ulong subjectId;
}

struct AsyncDerivedUpdateViewportVisual {
    uint viewportChannel;
    bool hasAnchor;
    vec3 anchorWorld;
    const(vec3)[] outlineWorld;
}

struct AsyncDerivedUpdateTargetDesc {
    AsyncDerivedUpdateTargetKey key;
    string label;
    string reason;
    ulong sourceRevision;
    AsyncDerivedUpdateMergePolicy mergePolicy =
        AsyncDerivedUpdateMergePolicy.SupersedeRunning;
    uint display =
        cast(uint)AsyncDerivedUpdateDisplay.ViewportBadge |
        cast(uint)AsyncDerivedUpdateDisplay.ViewportOutline |
        cast(uint)AsyncDerivedUpdateDisplay.Details;
    AsyncDerivedUpdateViewportVisual visual;
}

struct AsyncDerivedUpdateSnapshot {
    AsyncDerivedUpdateRunHandle run;
    AsyncDerivedUpdateTargetHandle target;
    AsyncDerivedUpdateOrigin origin;
    AsyncDerivedUpdateTargetKey key;
    string label;
    string reason;
    string detail;
    AsyncDerivedUpdateState state;
    ulong sourceRevision;
    ulong expectedUnits;
    ulong appliedUnits;
    ulong queuedUnits;
    ulong runningUnits;
    ulong staleUnits;
    uint retryCount;
    uint display;
    AsyncDerivedUpdateViewportVisual visual;
    MonoTime changedAt;
}

private enum WorkState {
    Queued,
    Running,
    Applied,
    Stale,
    Failed,
    Canceled,
}

private struct RunRecord {
    AsyncDerivedUpdateRunHandle handle;
    AsyncDerivedUpdateOrigin origin;
    AsyncDerivedUpdateTargetHandle[] targets;
    bool ended;
}

private struct TargetRecord {
    AsyncDerivedUpdateTargetHandle handle;
    AsyncDerivedUpdateRunHandle run;
    AsyncDerivedUpdateTargetDesc desc;
    ulong expectedUnits;
    ulong appliedUnits;
    ulong queuedUnits;
    ulong runningUnits;
    ulong staleUnits;
    uint retryCount;
    bool detected = true;
    bool runningStarted;
    bool failed;
    bool canceled;
    bool sealed;
    bool superseded;
    string detail;
    MonoTime changedAt;
}

private struct WorkRecord {
    AsyncDerivedUpdateWorkHandle handle;
    AsyncDerivedUpdateTargetHandle target;
    ulong units;
    WorkState state;
}

private RunRecord[ulong] runRecords;
private TargetRecord[ulong] targetRecords;
private WorkRecord[ulong] workRecords;
private AsyncDerivedUpdateTargetHandle[AsyncDerivedUpdateTargetKey]
    activeTargets;
private ulong nextRunId = 1;
private ulong nextTargetId = 1;
private ulong nextWorkId = 1;
private ulong nextScopeId = 1;

private ulong takeId(ref ulong nextId) {
    auto result = nextId++;
    if (nextId == 0) nextId = 1;
    if (result == 0) result = nextId++;
    return result;
}

AsyncDerivedUpdateScopeId incAsyncDerivedUpdateCreateScope() {
    return AsyncDerivedUpdateScopeId(takeId(nextScopeId));
}

private AsyncDerivedUpdateState targetState(ref TargetRecord target) {
    if (target.failed) return AsyncDerivedUpdateState.Failed;
    if (target.canceled) return AsyncDerivedUpdateState.Canceled;
    if (target.runningUnits > 0)
        return AsyncDerivedUpdateState.Running;
    // Running is sticky across producer/readback chunks.  Queue depth is an
    // implementation detail and must not make the visible state flicker.
    if (target.runningStarted && target.appliedUnits < target.expectedUnits) {
        if (target.staleUnits > 0 && target.queuedUnits == 0)
            return AsyncDerivedUpdateState.Stale;
        return AsyncDerivedUpdateState.Running;
    }
    if (target.queuedUnits > 0) return AsyncDerivedUpdateState.Queued;
    if (target.detected) return AsyncDerivedUpdateState.Detected;
    if (target.staleUnits > 0 &&
        target.appliedUnits < target.expectedUnits)
        return AsyncDerivedUpdateState.Stale;
    if (target.appliedUnits < target.expectedUnits)
        return AsyncDerivedUpdateState.Queued;
    return AsyncDerivedUpdateState.Applied;
}

private bool terminal(ref TargetRecord target) {
    auto state = targetState(target);
    return state == AsyncDerivedUpdateState.Applied ||
        state == AsyncDerivedUpdateState.Failed ||
        state == AsyncDerivedUpdateState.Canceled;
}

private void leaveInFlight(ref TargetRecord target, ref WorkRecord work) {
    final switch (work.state) {
    case WorkState.Queued:
        if (target.queuedUnits >= work.units)
            target.queuedUnits -= work.units;
        else
            target.queuedUnits = 0;
        break;
    case WorkState.Running:
        if (target.runningUnits >= work.units)
            target.runningUnits -= work.units;
        else
            target.runningUnits = 0;
        break;
    case WorkState.Applied:
    case WorkState.Stale:
    case WorkState.Failed:
    case WorkState.Canceled:
        break;
    }
}

private void supersedeTarget(ref TargetRecord target, string detail) {
    foreach (ref work; workRecords) {
        if (work.target != target.handle ||
            (work.state != WorkState.Queued &&
             work.state != WorkState.Running)) continue;
        leaveInFlight(target, work);
        work.state = WorkState.Stale;
        target.staleUnits += work.units;
    }
    target.detected = false;
    target.superseded = true;
    target.detail = detail;
    target.changedAt = MonoTime.currTime;
}

AsyncDerivedUpdateRunHandle incAsyncDerivedUpdateBeginRun(
    ref const AsyncDerivedUpdateOrigin origin,
) {
    auto handle = AsyncDerivedUpdateRunHandle(takeId(nextRunId));
    RunRecord record;
    record.handle = handle;
    record.origin = origin;
    runRecords[handle.id] = record;
    return handle;
}

AsyncDerivedUpdateTargetHandle incAsyncDerivedUpdateTrackTarget(
    AsyncDerivedUpdateRunHandle run,
    ref AsyncDerivedUpdateTargetDesc desc,
) {
    auto runRecord = run.id in runRecords;
    if (!run.valid || runRecord is null || runRecord.ended ||
        !desc.key.scopeId ||
        desc.key.scopeId != runRecord.origin.projectScope.id ||
        desc.key.providerId != runRecord.origin.providerId)
        return AsyncDerivedUpdateTargetHandle.init;

    if (auto active = desc.key in activeTargets) {
        if (auto previous = active.id in targetRecords) {
            if (!previous.superseded && !terminal(*previous)) {
                final switch (desc.mergePolicy) {
                case AsyncDerivedUpdateMergePolicy.CoalescePending:
                    if (!previous.runningStarted && !previous.sealed) {
                        previous.run = run;
                        runRecord.targets ~= previous.handle;
                        previous.desc.label = desc.label;
                        previous.desc.reason = desc.reason;
                        previous.desc.sourceRevision = desc.sourceRevision;
                        previous.changedAt = MonoTime.currTime;
                        return previous.handle;
                    }
                    break;
                case AsyncDerivedUpdateMergePolicy.ExtendRunning:
                    if (!previous.sealed) {
                        previous.run = run;
                        runRecord.targets ~= previous.handle;
                        previous.desc.label = desc.label;
                        previous.desc.reason = desc.reason;
                        previous.desc.sourceRevision = desc.sourceRevision;
                        previous.changedAt = MonoTime.currTime;
                        return previous.handle;
                    }
                    break;
                case AsyncDerivedUpdateMergePolicy.SupersedeRunning:
                    break;
                }
                supersedeTarget(*previous,
                    "Superseded by a newer derived update");
            } else {
                previous.superseded = true;
            }
        }
    }

    auto handle = AsyncDerivedUpdateTargetHandle(takeId(nextTargetId));
    TargetRecord record;
    record.handle = handle;
    record.run = run;
    record.desc = desc;
    // Producers may reuse their temporary geometry buffers after registration.
    // Keep the generation's visual snapshot owned by the registry.
    record.desc.visual.outlineWorld = desc.visual.outlineWorld.dup;
    record.changedAt = MonoTime.currTime;
    targetRecords[handle.id] = record;
    activeTargets[desc.key] = handle;
    runRecords[run.id].targets ~= handle;
    return handle;
}

void incAsyncDerivedUpdateSetExpected(
    AsyncDerivedUpdateTargetHandle handle,
    ulong units,
) {
    auto target = handle.id in targetRecords;
    if (target is null || target.superseded || target.failed ||
        target.canceled || target.sealed)
        return;
    if (units > target.expectedUnits) target.expectedUnits = units;
    target.detected = false;
    target.changedAt = MonoTime.currTime;
}

void incAsyncDerivedUpdateSetContext(
    AsyncDerivedUpdateTargetHandle handle,
    string reason,
    string detail = null,
    uint retryCount = 0,
) {
    auto target = handle.id in targetRecords;
    if (target is null || target.superseded) return;
    target.desc.reason = reason;
    target.detail = detail;
    target.retryCount = retryCount;
    target.changedAt = MonoTime.currTime;
}

AsyncDerivedUpdateWorkHandle incAsyncDerivedUpdateQueue(
    AsyncDerivedUpdateTargetHandle handle,
    ulong units = 1,
) {
    auto target = handle.id in targetRecords;
    if (target is null || target.superseded || target.failed ||
        target.canceled || target.sealed || units == 0)
        return AsyncDerivedUpdateWorkHandle.init;

    auto workHandle = AsyncDerivedUpdateWorkHandle(takeId(nextWorkId));
    WorkRecord work;
    work.handle = workHandle;
    work.target = handle;
    work.units = units;
    work.state = WorkState.Queued;
    workRecords[workHandle.id] = work;
    target.detected = false;
    target.queuedUnits += units;
    auto tracked = target.appliedUnits +
        target.queuedUnits + target.runningUnits;
    if (target.expectedUnits < tracked) target.expectedUnits = tracked;
    target.changedAt = MonoTime.currTime;
    return workHandle;
}

void incAsyncDerivedUpdateStart(AsyncDerivedUpdateWorkHandle handle) {
    auto work = handle.id in workRecords;
    if (work is null || work.state != WorkState.Queued) return;
    auto target = work.target.id in targetRecords;
    if (target is null || target.superseded) {
        work.state = WorkState.Stale;
        return;
    }
    leaveInFlight(*target, *work);
    work.state = WorkState.Running;
    target.runningUnits += work.units;
    target.runningStarted = true;
    target.changedAt = MonoTime.currTime;
}

void incAsyncDerivedUpdateApplied(AsyncDerivedUpdateWorkHandle handle) {
    auto work = handle.id in workRecords;
    if (work is null ||
        (work.state != WorkState.Queued && work.state != WorkState.Running))
        return;
    auto target = work.target.id in targetRecords;
    if (target is null || target.superseded) {
        work.state = WorkState.Stale;
        return;
    }
    leaveInFlight(*target, *work);
    work.state = WorkState.Applied;
    target.appliedUnits += work.units;
    target.detected = false;
    target.detail = null;
    target.changedAt = MonoTime.currTime;
    // Counters live on the target. Dropping terminal work keeps duplicate
    // callbacks harmless without retaining one record per completed unit.
    workRecords.remove(handle.id);
}

void incAsyncDerivedUpdateRetry(
    AsyncDerivedUpdateWorkHandle handle,
    string reason,
) {
    auto work = handle.id in workRecords;
    if (work is null || work.state == WorkState.Applied ||
        work.state == WorkState.Failed || work.state == WorkState.Canceled)
        return;
    auto target = work.target.id in targetRecords;
    if (target is null || target.superseded) return;
    if (work.state == WorkState.Queued || work.state == WorkState.Running)
        leaveInFlight(*target, *work);
    else if (work.state == WorkState.Stale &&
        target.staleUnits >= work.units)
        target.staleUnits -= work.units;
    work.state = WorkState.Queued;
    target.queuedUnits += work.units;
    target.retryCount++;
    target.desc.reason = reason;
    target.detail = null;
    target.changedAt = MonoTime.currTime;
}

void incAsyncDerivedUpdateStale(
    AsyncDerivedUpdateWorkHandle handle,
    string detail,
) {
    auto work = handle.id in workRecords;
    if (work is null ||
        (work.state != WorkState.Queued && work.state != WorkState.Running))
        return;
    auto target = work.target.id in targetRecords;
    if (target is null) {
        work.state = WorkState.Stale;
        return;
    }
    leaveInFlight(*target, *work);
    work.state = WorkState.Stale;
    target.staleUnits += work.units;
    target.detected = false;
    target.detail = detail;
    target.changedAt = MonoTime.currTime;
}

/**
    Remove an in-flight unit without changing the target's visible lifecycle.

    This is for a producer that silently replaces a batch and immediately
    schedules its successor. It is intentionally different from stale,
    failed, or user-visible cancellation.
*/
void incAsyncDerivedUpdateDiscard(AsyncDerivedUpdateWorkHandle handle) {
    auto work = handle.id in workRecords;
    if (work is null ||
        (work.state != WorkState.Queued && work.state != WorkState.Running))
        return;
    auto target = work.target.id in targetRecords;
    if (target !is null) {
        leaveInFlight(*target, *work);
        target.changedAt = MonoTime.currTime;
    }
    workRecords.remove(handle.id);
}

/** Record a stale unit when no producer work handle survived to report it. */
void incAsyncDerivedUpdateMarkTargetStale(
    AsyncDerivedUpdateTargetHandle handle,
    string detail,
    ulong units = 1,
) {
    auto target = handle.id in targetRecords;
    if (target is null || target.superseded || target.failed ||
        target.canceled || units == 0) return;
    target.staleUnits += units;
    target.detected = false;
    target.detail = detail;
    target.changedAt = MonoTime.currTime;
}

void incAsyncDerivedUpdateFailed(
    AsyncDerivedUpdateWorkHandle handle,
    string detail,
) {
    auto work = handle.id in workRecords;
    if (work is null || work.state == WorkState.Applied ||
        work.state == WorkState.Failed || work.state == WorkState.Canceled)
        return;
    auto targetHandle = work.target;
    incAsyncDerivedUpdateFailTarget(targetHandle, detail);
}

void incAsyncDerivedUpdateCanceled(
    AsyncDerivedUpdateWorkHandle handle,
    string detail,
) {
    auto work = handle.id in workRecords;
    if (work is null || work.state == WorkState.Applied ||
        work.state == WorkState.Failed || work.state == WorkState.Canceled)
        return;
    auto targetHandle = work.target;
    incAsyncDerivedUpdateCancelTarget(targetHandle, detail);
}

void incAsyncDerivedUpdateFailTarget(
    AsyncDerivedUpdateTargetHandle handle,
    string detail,
) {
    auto target = handle.id in targetRecords;
    if (target is null || target.superseded) return;
    ulong[] removeWork;
    foreach (workId, ref work; workRecords) {
        if (work.target != handle) continue;
        if (work.state == WorkState.Queued || work.state == WorkState.Running)
            leaveInFlight(*target, work);
        removeWork ~= workId;
    }
    foreach (workId; removeWork) workRecords.remove(workId);
    target.failed = true;
    target.detected = false;
    target.detail = detail;
    target.changedAt = MonoTime.currTime;
}

void incAsyncDerivedUpdateCancelTarget(
    AsyncDerivedUpdateTargetHandle handle,
    string detail,
) {
    auto target = handle.id in targetRecords;
    if (target is null || target.superseded) return;
    ulong[] removeWork;
    foreach (workId, ref work; workRecords) {
        if (work.target != handle) continue;
        if (work.state == WorkState.Queued || work.state == WorkState.Running)
            leaveInFlight(*target, work);
        removeWork ~= workId;
    }
    foreach (workId; removeWork) workRecords.remove(workId);
    target.canceled = true;
    target.detected = false;
    target.detail = detail;
    target.changedAt = MonoTime.currTime;
}

void incAsyncDerivedUpdateSeal(AsyncDerivedUpdateTargetHandle handle) {
    if (auto target = handle.id in targetRecords) {
        target.sealed = true;
        target.detected = false;
        target.changedAt = MonoTime.currTime;
    }
}

void incAsyncDerivedUpdateEndRun(AsyncDerivedUpdateRunHandle handle) {
    if (auto run = handle.id in runRecords) {
        run.ended = true;
        foreach (targetHandle; run.targets) {
            if (auto target = targetHandle.id in targetRecords) {
                target.sealed = true;
                target.detected = false;
                target.changedAt = MonoTime.currTime;
            }
        }
    }
}

private AsyncDerivedUpdateSnapshot buildSnapshot(ref TargetRecord target) {
    AsyncDerivedUpdateSnapshot snapshot;
    snapshot.run = target.run;
    snapshot.target = target.handle;
    if (auto run = target.run.id in runRecords)
        snapshot.origin = run.origin;
    snapshot.key = target.desc.key;
    snapshot.label = target.desc.label;
    snapshot.reason = target.desc.reason;
    snapshot.detail = target.detail;
    snapshot.state = targetState(target);
    snapshot.sourceRevision = target.desc.sourceRevision;
    snapshot.expectedUnits = target.expectedUnits;
    snapshot.appliedUnits = target.appliedUnits;
    snapshot.queuedUnits = target.queuedUnits;
    snapshot.runningUnits = target.runningUnits;
    snapshot.staleUnits = target.staleUnits;
    snapshot.retryCount = target.retryCount;
    snapshot.display = target.desc.display;
    snapshot.visual = target.desc.visual;
    snapshot.changedAt = target.changedAt;
    return snapshot;
}

private bool successExpired(ref TargetRecord target) {
    return targetState(target) == AsyncDerivedUpdateState.Applied &&
        MonoTime.currTime - target.changedAt > seconds(2);
}

bool incAsyncDerivedUpdateTargetSnapshot(
    AsyncDerivedUpdateTargetHandle handle,
    out AsyncDerivedUpdateSnapshot snapshot,
    bool includeExpired = false,
) {
    snapshot = AsyncDerivedUpdateSnapshot.init;
    auto target = handle.id in targetRecords;
    if (target is null || target.superseded ||
        (!includeExpired && successExpired(*target))) return false;
    snapshot = buildSnapshot(*target);
    return true;
}

AsyncDerivedUpdateSnapshot[] incAsyncDerivedUpdateSnapshots(
    AsyncDerivedUpdateScopeId projectScope,
    uint viewportChannel = 0,
    bool includeExpired = false,
) {
    AsyncDerivedUpdateSnapshot[] result;
    foreach (ref target; targetRecords) {
        if (target.superseded || target.desc.key.scopeId != projectScope.id ||
            (!includeExpired && successExpired(target))) continue;
        if (viewportChannel != 0) {
            if ((target.desc.visual.viewportChannel & viewportChannel) == 0)
                continue;
        }
        result ~= buildSnapshot(target);
    }
    return result;
}

float incAsyncDerivedUpdateProgress(
    ref const AsyncDerivedUpdateSnapshot snapshot,
) {
    if (snapshot.expectedUnits == 0) return 0.0f;
    return min(1.0f,
        cast(float)snapshot.appliedUnits /
        cast(float)snapshot.expectedUnits);
}

private void removeTarget(ulong targetId) {
    auto target = targetId in targetRecords;
    if (target is null) return;
    if (auto active = target.desc.key in activeTargets) {
        if (active.id == targetId) activeTargets.remove(target.desc.key);
    }
    ulong[] removeWork;
    foreach (workId, ref work; workRecords)
        if (work.target.id == targetId) removeWork ~= workId;
    foreach (workId; removeWork) workRecords.remove(workId);
    targetRecords.remove(targetId);
}

void incAsyncDerivedUpdateForgetTarget(
    AsyncDerivedUpdateTargetHandle handle,
) {
    removeTarget(handle.id);
}

void incAsyncDerivedUpdateUpdate() {
    ulong[] removeTargets;
    foreach (targetId, ref target; targetRecords)
        if (target.superseded || successExpired(target))
            removeTargets ~= targetId;
    foreach (targetId; removeTargets) removeTarget(targetId);

    ulong[] removeRuns;
    foreach (runId, ref run; runRecords) {
        bool hasTarget;
        foreach (target; run.targets) {
            if (target.id in targetRecords) {
                hasTarget = true;
                break;
            }
        }
        if (!hasTarget) removeRuns ~= runId;
    }
    foreach (runId; removeRuns) runRecords.remove(runId);
}

void incAsyncDerivedUpdateClearScope(AsyncDerivedUpdateScopeId projectScope) {
    ulong[] removeTargets;
    foreach (targetId, ref target; targetRecords)
        if (target.desc.key.scopeId == projectScope.id) removeTargets ~= targetId;
    foreach (targetId; removeTargets) removeTarget(targetId);

    ulong[] removeRuns;
    foreach (runId, ref run; runRecords)
        if (run.origin.projectScope == projectScope) removeRuns ~= runId;
    foreach (runId; removeRuns) runRecords.remove(runId);
}

void incAsyncDerivedUpdateClearProvider(uint providerId) {
    ulong[] removeTargets;
    foreach (targetId, ref target; targetRecords)
        if (target.desc.key.providerId == providerId)
            removeTargets ~= targetId;
    foreach (targetId; removeTargets) removeTarget(targetId);

    ulong[] removeRuns;
    foreach (runId, ref run; runRecords)
        if (run.origin.providerId == providerId) removeRuns ~= runId;
    foreach (runId; removeRuns) runRecords.remove(runId);
}

void incAsyncDerivedUpdateClearAll() {
    runRecords = null;
    targetRecords = null;
    workRecords = null;
    activeTargets = null;
}
