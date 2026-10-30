#include "asyncderivedupdate.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <nijigenerate/compat/switch_error.hpp>
#include <compat/range_error.hpp>
#include <memory>
#include <functional>
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>
#include <utility>

namespace nijigenerate::core::asyncderivedupdate {
namespace {
using Clock = compat::SourceMonotonicClock;
enum class WorkState { Queued, Running, Applied, Stale, Failed, Canceled };
struct RunRecord {
    AsyncDerivedUpdateRunHandle handle;
    AsyncDerivedUpdateOrigin origin;
    compat::Slice<AsyncDerivedUpdateTargetHandle> targets;
    actions::AsyncActionToken ownerToken;
    bool ended{};
};
struct TargetRecord {
    AsyncDerivedUpdateTargetHandle handle;
    AsyncDerivedUpdateRunHandle run;
    AsyncDerivedUpdateTargetDesc desc;
    std::uint64_t expectedUnits{}, appliedUnits{}, queuedUnits{}, runningUnits{}, staleUnits{};
    std::uint32_t retryCount{};
    bool detected = true;
    bool runningStarted{}, failed{}, canceled{}, sealed{}, superseded{};
    compat::RetainedString detail;
    Clock::time_point changedAt{};
};
struct WorkRecord {
    AsyncDerivedUpdateWorkHandle handle;
    AsyncDerivedUpdateTargetHandle target;
    std::uint64_t units{};
    WorkState state{};
};
} // namespace
} // namespace nijigenerate::core::asyncderivedupdate

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::WorkState> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.WorkState)";
    inline static constexpr char unqualifiedArgument[] = "(WorkState)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.WorkState";
    inline static constexpr char unqualifiedElement[] = "WorkState";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::RunRecord> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.RunRecord)";
    inline static constexpr char unqualifiedArgument[] = "(RunRecord)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.RunRecord";
    inline static constexpr char unqualifiedElement[] = "RunRecord";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::TargetRecord> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.TargetRecord)";
    inline static constexpr char unqualifiedArgument[] = "(TargetRecord)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.TargetRecord";
    inline static constexpr char unqualifiedElement[] = "TargetRecord";
};
template<> struct SourceTypeSpellings<
    ::nijigenerate::core::asyncderivedupdate::WorkRecord> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.core.asyncderivedupdate.WorkRecord)";
    inline static constexpr char unqualifiedArgument[] = "(WorkRecord)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.core.asyncderivedupdate.WorkRecord";
    inline static constexpr char unqualifiedElement[] = "WorkRecord";
};
}

namespace nijigenerate::core::asyncderivedupdate {
namespace {
struct TargetKeyHash {
    std::size_t operator()(const AsyncDerivedUpdateTargetKey& key) const {
        // All fields are integral: clone.d needToHash returns false, so actual
        // TypeInfo_Struct.getHash hashes the complete initialized byte image.
        return nicxlive::core::d_runtime::ngPhobosStringHash(
            std::string_view(reinterpret_cast<const char*>(&key), sizeof(key)));
    }
};
struct UInt64Hash {
    std::size_t operator()(std::uint64_t value) const noexcept { return static_cast<std::size_t>(value); }
};
// Only the key protocol is local; canonical AA owns actual buckets and entries.
template<class Key, class Value, class Hash = UInt64Hash>
using Registry = nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, std::equal_to<Key>>;
// D module globals without __gshared are thread-local.
thread_local Registry<std::uint64_t, RunRecord> runRecords;
thread_local Registry<std::uint64_t, TargetRecord> targetRecords;
thread_local Registry<std::uint64_t, WorkRecord> workRecords;
thread_local Registry<AsyncDerivedUpdateTargetKey, AsyncDerivedUpdateTargetHandle, TargetKeyHash>
    activeTargets;
thread_local std::uint64_t nextRunId = 1, nextTargetId = 1, nextWorkId = 1, nextScopeId = 1;

template<class Map, class Key> auto findRecord(Map& map, const Key& key) {
    const auto entry = map.find(key);
    return entry.retainValue();
}
std::uint64_t takeId(std::uint64_t& nextId) {
    auto result = nextId++;
    if (nextId == 0) nextId = 1;
    if (result == 0) result = nextId++;
    return result;
}
AsyncDerivedUpdateState targetState(TargetRecord& target) {
    if (auto run = findRecord(runRecords, target.run.id)) {
        if (run->ownerToken.canceled()) return AsyncDerivedUpdateState::Canceled;
    }
    if (target.failed) return AsyncDerivedUpdateState::Failed;
    if (target.canceled) return AsyncDerivedUpdateState::Canceled;
    if (target.runningUnits > 0) return AsyncDerivedUpdateState::Running;
    if (target.runningStarted && target.appliedUnits < target.expectedUnits) {
        if (target.staleUnits > 0 && target.queuedUnits == 0) return AsyncDerivedUpdateState::Stale;
        return AsyncDerivedUpdateState::Running;
    }
    if (target.queuedUnits > 0) return AsyncDerivedUpdateState::Queued;
    if (target.detected) return AsyncDerivedUpdateState::Detected;
    if (target.staleUnits > 0 && target.appliedUnits < target.expectedUnits) return AsyncDerivedUpdateState::Stale;
    if (target.appliedUnits < target.expectedUnits) return AsyncDerivedUpdateState::Queued;
    return AsyncDerivedUpdateState::Applied;
}
bool terminal(TargetRecord& target) {
    auto state = targetState(target);
    return state == AsyncDerivedUpdateState::Applied || state == AsyncDerivedUpdateState::Failed ||
        state == AsyncDerivedUpdateState::Canceled;
}
void leaveInFlight(TargetRecord& target, WorkRecord& work) {
    switch (work.state) {
    case WorkState::Queued:
        target.queuedUnits = target.queuedUnits >= work.units ? target.queuedUnits - work.units : 0;
        break;
    case WorkState::Running:
        target.runningUnits = target.runningUnits >= work.units ? target.runningUnits - work.units : 0;
        break;
    case WorkState::Applied:
    case WorkState::Stale:
    case WorkState::Failed:
    case WorkState::Canceled:
        break;
    default: throw compat::ngStaticError<compat::SwitchError>(
        "No appropriate switch clause found", "source/nijigenerate/core/asyncderivedupdate.d", 222);
    }
}
void supersedeTarget(TargetRecord& target, compat::RetainedString detail) {
    for (auto& [id, work] : workRecords) {
        if (work.target != target.handle ||
            (work.state != WorkState::Queued && work.state != WorkState::Running)) continue;
        leaveInFlight(target, work);
        work.state = WorkState::Stale;
        target.staleUnits += work.units;
    }
    target.detected = false;
    target.superseded = true;
    target.detail = std::move(detail);
    target.changedAt = Clock::now();
}
bool ownerCanceled(TargetRecord& target) {
    auto run = findRecord(runRecords, target.run.id);
    return run && run->ownerToken.canceled();
}
AsyncDerivedUpdateSnapshot buildSnapshot(TargetRecord& target) {
    AsyncDerivedUpdateSnapshot snapshot;
    snapshot.run = target.run;
    snapshot.target = target.handle;
    if (auto run = findRecord(runRecords, target.run.id)) snapshot.origin = run->origin;
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
bool successExpired(TargetRecord& target) {
    return targetState(target) == AsyncDerivedUpdateState::Applied &&
        Clock::now() - target.changedAt > std::chrono::seconds(2);
}
void removeTarget(std::uint64_t targetId) {
    auto target = findRecord(targetRecords, targetId);
    if (!target) return;
    if (auto active = findRecord(activeTargets, target->desc.key)) {
        if (active->id == targetId) activeTargets.erase(target->desc.key);
    }
    compat::Slice<std::uint64_t> removeWork;
    for (auto& [workId, work] : workRecords) if (work.target.id == targetId) removeWork.append(workId);
    for (auto workId : removeWork) workRecords.erase(workId);
    targetRecords.erase(targetId);
}
} // namespace

AsyncDerivedUpdateScopeId incAsyncDerivedUpdateCreateScope() { return {takeId(nextScopeId)}; }
AsyncDerivedUpdateRunHandle incAsyncDerivedUpdateBeginRun(const AsyncDerivedUpdateOrigin& origin) {
    AsyncDerivedUpdateRunHandle handle{takeId(nextRunId)};
    RunRecord record;
    record.handle = handle;
    record.origin = origin;
    runRecords[handle.id] = record;
    return handle;
}
void incAsyncDerivedUpdateSetOwnerToken(AsyncDerivedUpdateRunHandle handle, actions::AsyncActionToken token) {
    auto run = findRecord(runRecords, handle.id);
    if (!run || run->ended || !token.valid()) return;
    run->ownerToken = std::move(token);
    const auto targets = run->targets;
    for (auto targetHandle : targets) {
        if (auto target = findRecord(targetRecords, targetHandle.id)) target->changedAt = Clock::now();
    }
}
AsyncDerivedUpdateTargetHandle incAsyncDerivedUpdateTrackTarget(
    AsyncDerivedUpdateRunHandle run, AsyncDerivedUpdateTargetDesc& desc) {
    auto runRecord = findRecord(runRecords, run.id);
    if (!run.valid() || !runRecord || runRecord->ended || !desc.key.scopeId ||
        desc.key.scopeId != runRecord->origin.projectScope.id || desc.key.providerId != runRecord->origin.providerId)
        return {};
    if (auto active = findRecord(activeTargets, desc.key)) {
        if (auto previous = findRecord(targetRecords, active->id)) {
            if (!previous->superseded && !terminal(*previous)) {
                auto transferToCurrentRun = [&] {
                    if (previous->run == run) return;
                    if (auto previousRun = findRecord(runRecords, previous->run.id)) {
                        compat::Slice<AsyncDerivedUpdateTargetHandle> retainedTargets;
                        const auto previousTargets = previousRun->targets;
                        for (auto candidate : previousTargets)
                            if (candidate != previous->handle) retainedTargets.append(candidate);
                        previousRun->targets = std::move(retainedTargets);
                    }
                    previous->run = run;
                    runRecord->targets.append(previous->handle);
                };
                switch (desc.mergePolicy) {
                case AsyncDerivedUpdateMergePolicy::CoalescePending:
                    if (!previous->runningStarted && !previous->sealed) {
                        transferToCurrentRun();
                        previous->desc.label = desc.label;
                        previous->desc.reason = desc.reason;
                        previous->desc.sourceRevision = desc.sourceRevision;
                        previous->changedAt = Clock::now();
                        return previous->handle;
                    }
                    break;
                case AsyncDerivedUpdateMergePolicy::ExtendRunning:
                    if (!previous->sealed) {
                        transferToCurrentRun();
                        previous->desc.label = desc.label;
                        previous->desc.reason = desc.reason;
                        previous->desc.sourceRevision = desc.sourceRevision;
                        previous->changedAt = Clock::now();
                        return previous->handle;
                    }
                    break;
                case AsyncDerivedUpdateMergePolicy::SupersedeRunning:
                    break;
                default: throw compat::ngStaticError<compat::SwitchError>(
                    "No appropriate switch clause found", "source/nijigenerate/core/asyncderivedupdate.d", 314);
                }
                supersedeTarget(*previous, compat::RetainedString::fromStaticLiteral("Superseded by a newer derived update"));
            } else {
                previous->superseded = true;
            }
        }
    }
    AsyncDerivedUpdateTargetHandle handle{takeId(nextTargetId)};
    TargetRecord record;
    record.handle = handle;
    record.run = run;
    record.desc = desc;
    record.desc.visual.outlineWorld = desc.visual.outlineWorld.duplicate();
    record.changedAt = Clock::now();
    targetRecords[handle.id] = record;
    activeTargets[desc.key] = handle;
    auto retainedRun = findRecord(runRecords, run.id);
    if (!retainedRun) throw compat::ngStaticError<nicxlive::core::d_runtime::RangeError>(
        "source/nijigenerate/core/asyncderivedupdate.d", 357);
    retainedRun->targets.append(handle);
    return handle;
}
void incAsyncDerivedUpdateSetExpected(AsyncDerivedUpdateTargetHandle handle, std::uint64_t units) {
    auto target = findRecord(targetRecords, handle.id);
    if (!target || target->superseded || target->failed || target->canceled || target->sealed || ownerCanceled(*target))
        return;
    if (units > target->expectedUnits) target->expectedUnits = units;
    target->detected = false;
    target->changedAt = Clock::now();
}
void incAsyncDerivedUpdateSetContext(AsyncDerivedUpdateTargetHandle handle, compat::RetainedString reason,
    compat::RetainedString detail, std::uint32_t retryCount) {
    auto target = findRecord(targetRecords, handle.id);
    if (!target || target->superseded) return;
    target->desc.reason = std::move(reason);
    target->detail = std::move(detail);
    target->retryCount = retryCount;
    target->changedAt = Clock::now();
}
AsyncDerivedUpdateWorkHandle incAsyncDerivedUpdateQueue(AsyncDerivedUpdateTargetHandle handle, std::uint64_t units) {
    auto target = findRecord(targetRecords, handle.id);
    if (!target || target->superseded || target->failed || target->canceled || target->sealed || ownerCanceled(*target) ||
        units == 0) return {};
    AsyncDerivedUpdateWorkHandle workHandle{takeId(nextWorkId)};
    WorkRecord work;
    work.handle = workHandle;
    work.target = handle;
    work.units = units;
    work.state = WorkState::Queued;
    workRecords[workHandle.id] = work;
    target->detected = false;
    target->queuedUnits += units;
    auto tracked = target->appliedUnits + target->queuedUnits + target->runningUnits;
    if (target->expectedUnits < tracked) target->expectedUnits = tracked;
    target->changedAt = Clock::now();
    return workHandle;
}
void incAsyncDerivedUpdateStart(AsyncDerivedUpdateWorkHandle handle) {
    auto work = findRecord(workRecords, handle.id);
    if (!work || work->state != WorkState::Queued) return;
    auto target = findRecord(targetRecords, work->target.id);
    if (!target || target->superseded || ownerCanceled(*target)) {
        work->state = WorkState::Stale;
        return;
    }
    leaveInFlight(*target, *work);
    work->state = WorkState::Running;
    target->runningUnits += work->units;
    target->runningStarted = true;
    target->changedAt = Clock::now();
}
void incAsyncDerivedUpdateApplied(AsyncDerivedUpdateWorkHandle handle) {
    auto work = findRecord(workRecords, handle.id);
    if (!work || (work->state != WorkState::Queued && work->state != WorkState::Running)) return;
    auto target = findRecord(targetRecords, work->target.id);
    if (!target || target->superseded || ownerCanceled(*target)) {
        work->state = WorkState::Stale;
        workRecords.erase(handle.id);
        return;
    }
    leaveInFlight(*target, *work);
    work->state = WorkState::Applied;
    target->appliedUnits += work->units;
    target->detected = false;
    target->detail.reset();
    target->changedAt = Clock::now();
    workRecords.erase(handle.id);
}
void incAsyncDerivedUpdateRetry(AsyncDerivedUpdateWorkHandle handle, compat::RetainedString reason) {
    auto work = findRecord(workRecords, handle.id);
    if (!work || work->state == WorkState::Applied || work->state == WorkState::Failed ||
        work->state == WorkState::Canceled) return;
    auto target = findRecord(targetRecords, work->target.id);
    if (!target || target->superseded) return;
    if (work->state == WorkState::Queued || work->state == WorkState::Running) leaveInFlight(*target, *work);
    else if (work->state == WorkState::Stale && target->staleUnits >= work->units) target->staleUnits -= work->units;
    work->state = WorkState::Queued;
    target->queuedUnits += work->units;
    target->retryCount++;
    target->desc.reason = std::move(reason);
    target->detail.reset();
    target->changedAt = Clock::now();
}
void incAsyncDerivedUpdateStale(AsyncDerivedUpdateWorkHandle handle, compat::RetainedString detail) {
    auto work = findRecord(workRecords, handle.id);
    if (!work || (work->state != WorkState::Queued && work->state != WorkState::Running)) return;
    auto target = findRecord(targetRecords, work->target.id);
    if (!target) {
        work->state = WorkState::Stale;
        return;
    }
    leaveInFlight(*target, *work);
    work->state = WorkState::Stale;
    target->staleUnits += work->units;
    target->detected = false;
    target->detail = std::move(detail);
    target->changedAt = Clock::now();
}
void incAsyncDerivedUpdateDiscard(AsyncDerivedUpdateWorkHandle handle) {
    auto work = findRecord(workRecords, handle.id);
    if (!work || (work->state != WorkState::Queued && work->state != WorkState::Running)) return;
    if (auto target = findRecord(targetRecords, work->target.id)) {
        leaveInFlight(*target, *work);
        target->changedAt = Clock::now();
    }
    workRecords.erase(handle.id);
}
void incAsyncDerivedUpdateMarkTargetStale(AsyncDerivedUpdateTargetHandle handle, compat::RetainedString detail,
    std::uint64_t units) {
    auto target = findRecord(targetRecords, handle.id);
    if (!target || target->superseded || target->failed || target->canceled || units == 0) return;
    target->staleUnits += units;
    target->detected = false;
    target->detail = std::move(detail);
    target->changedAt = Clock::now();
}
void incAsyncDerivedUpdateFailed(AsyncDerivedUpdateWorkHandle handle, compat::RetainedString detail) {
    auto work = findRecord(workRecords, handle.id);
    if (!work || work->state == WorkState::Applied || work->state == WorkState::Failed ||
        work->state == WorkState::Canceled) return;
    incAsyncDerivedUpdateFailTarget(work->target, std::move(detail));
}
void incAsyncDerivedUpdateCanceled(AsyncDerivedUpdateWorkHandle handle, compat::RetainedString detail) {
    auto work = findRecord(workRecords, handle.id);
    if (!work || work->state == WorkState::Applied || work->state == WorkState::Failed ||
        work->state == WorkState::Canceled) return;
    incAsyncDerivedUpdateCancelTarget(work->target, std::move(detail));
}
void incAsyncDerivedUpdateFailTarget(AsyncDerivedUpdateTargetHandle handle, compat::RetainedString detail) {
    auto target = findRecord(targetRecords, handle.id);
    if (!target || target->superseded) return;
    compat::Slice<std::uint64_t> removeWork;
    for (auto& [workId, work] : workRecords) {
        if (work.target != handle) continue;
        if (work.state == WorkState::Queued || work.state == WorkState::Running) leaveInFlight(*target, work);
        removeWork.append(workId);
    }
    for (auto workId : removeWork) workRecords.erase(workId);
    target->failed = true;
    target->detected = false;
    target->detail = std::move(detail);
    target->changedAt = Clock::now();
}
void incAsyncDerivedUpdateCancelTarget(AsyncDerivedUpdateTargetHandle handle, compat::RetainedString detail) {
    auto target = findRecord(targetRecords, handle.id);
    if (!target || target->superseded) return;
    compat::Slice<std::uint64_t> removeWork;
    for (auto& [workId, work] : workRecords) {
        if (work.target != handle) continue;
        if (work.state == WorkState::Queued || work.state == WorkState::Running) leaveInFlight(*target, work);
        removeWork.append(workId);
    }
    for (auto workId : removeWork) workRecords.erase(workId);
    target->canceled = true;
    target->detected = false;
    target->detail = std::move(detail);
    target->changedAt = Clock::now();
}
void incAsyncDerivedUpdateSeal(AsyncDerivedUpdateTargetHandle handle) {
    if (auto target = findRecord(targetRecords, handle.id)) {
        target->sealed = true;
        target->detected = false;
        target->changedAt = Clock::now();
    }
}
void incAsyncDerivedUpdateEndRun(AsyncDerivedUpdateRunHandle handle) {
    if (auto run = findRecord(runRecords, handle.id)) {
        run->ended = true;
        const auto targets = run->targets;
        for (auto targetHandle : targets) {
            if (auto target = findRecord(targetRecords, targetHandle.id)) {
                target->sealed = true;
                target->detected = false;
                target->changedAt = Clock::now();
            }
        }
    }
}
bool incAsyncDerivedUpdateTargetSnapshot(AsyncDerivedUpdateTargetHandle handle,
    AsyncDerivedUpdateSnapshot& snapshot, bool includeExpired) {
    snapshot = {};
    auto target = findRecord(targetRecords, handle.id);
    if (!target || target->superseded || (!includeExpired && successExpired(*target))) return false;
    snapshot = buildSnapshot(*target);
    return true;
}
compat::Slice<AsyncDerivedUpdateSnapshot> incAsyncDerivedUpdateSnapshots(AsyncDerivedUpdateScopeId projectScope,
    std::uint32_t viewportChannel, bool includeExpired) {
    compat::Slice<AsyncDerivedUpdateSnapshot> result;
    for (auto& [id, target] : targetRecords) {
        if (target.superseded || target.desc.key.scopeId != projectScope.id ||
            (!includeExpired && successExpired(target))) continue;
        if (viewportChannel != 0 && (target.desc.visual.viewportChannel & viewportChannel) == 0) continue;
        result.append(buildSnapshot(target));
    }
    return result;
}
float incAsyncDerivedUpdateProgress(const AsyncDerivedUpdateSnapshot& snapshot) {
    if (snapshot.expectedUnits == 0) return 0.0f;
    return std::min(1.0f, static_cast<float>(snapshot.appliedUnits) / static_cast<float>(snapshot.expectedUnits));
}
bool incAsyncDerivedUpdateShowsViewportProgress(const AsyncDerivedUpdateSnapshot& snapshot) {
    return snapshot.state != AsyncDerivedUpdateState::Canceled;
}
void incAsyncDerivedUpdateForgetTarget(AsyncDerivedUpdateTargetHandle handle) { removeTarget(handle.id); }
void incAsyncDerivedUpdateUpdate() {
    compat::Slice<std::uint64_t> removeTargets;
    for (auto& [targetId, target] : targetRecords)
        if (target.superseded || successExpired(target)) removeTargets.append(targetId);
    for (auto targetId : removeTargets) removeTarget(targetId);
    compat::Slice<std::uint64_t> removeRuns;
    for (auto& [runId, run] : runRecords) {
        bool hasTarget = false;
        const auto targets = run.targets;
        for (auto target : targets) {
            if (targetRecords.contains(target.id)) {
                hasTarget = true;
                break;
            }
        }
        if (!hasTarget) removeRuns.append(runId);
    }
    for (auto runId : removeRuns) runRecords.erase(runId);
}
void incAsyncDerivedUpdateClearScope(AsyncDerivedUpdateScopeId projectScope) {
    compat::Slice<std::uint64_t> removeTargets;
    for (auto& [targetId, target] : targetRecords)
        if (target.desc.key.scopeId == projectScope.id) removeTargets.append(targetId);
    for (auto targetId : removeTargets) removeTarget(targetId);
    compat::Slice<std::uint64_t> removeRuns;
    for (auto& [runId, run] : runRecords) if (run.origin.projectScope == projectScope) removeRuns.append(runId);
    for (auto runId : removeRuns) runRecords.erase(runId);
}
void incAsyncDerivedUpdateClearProvider(std::uint32_t providerId) {
    compat::Slice<std::uint64_t> removeTargets;
    for (auto& [targetId, target] : targetRecords)
        if (target.desc.key.providerId == providerId) removeTargets.append(targetId);
    for (auto targetId : removeTargets) removeTarget(targetId);
    compat::Slice<std::uint64_t> removeRuns;
    for (auto& [runId, run] : runRecords) if (run.origin.providerId == providerId) removeRuns.append(runId);
    for (auto runId : removeRuns) runRecords.erase(runId);
}
void incAsyncDerivedUpdateClearAll() {
    runRecords = {};
    targetRecords = {};
    workRecords = {};
    activeTargets = {};
}
} // namespace nijigenerate::core::asyncderivedupdate
