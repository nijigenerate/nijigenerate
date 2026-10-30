#include "bone_status.hpp"
#include "detail/bone_status_records_fwd.hpp"
#include "detail/bone_status_work_key.hpp"
#include <nijigenerate/core/asyncderivedupdate.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/deformable.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <compat/source_allocator.hpp>
#include <functional>
#include <array>

namespace nijigenerate::commands::depth::bone_status {
using namespace core::asyncderivedupdate;

namespace detail {
using PlannedParameterPolicy = nicxlive::core::common::ObjectClassKeyPolicy<std::shared_ptr<Parameter>>;
struct PlannedParameterHash {
    std::size_t operator()(const std::shared_ptr<Parameter>& value) const {
        return PlannedParameterPolicy::Hash{}(value);
    }
};
struct PlannedParameterEqual {
    bool operator()(const std::shared_ptr<Parameter>& query,
        const std::shared_ptr<Parameter>& stored) const {
        return PlannedParameterPolicy::Equal{}(query, stored);
    }
};
struct DepthBoneUpdateAdapterRecord {
    std::shared_ptr<ExDepthRigRoot> root;
    std::shared_ptr<Node> target;
    std::shared_ptr<Puppet> puppet;
    std::shared_ptr<Parameter> parameter;
    Keypoint keypoint{};
    compat::RetainedString reason;
    using Plans = nicxlive::core::common::ResourceKeyMap<std::shared_ptr<Parameter>, std::size_t,
        PlannedParameterHash, PlannedParameterEqual>;
    Plans plannedByParameter;
    std::uint64_t generation{};
    bool detected{};
    bool hasLocalBounds{};
    Vec2 localBoundsMin{NAN, NAN};
    Vec2 localBoundsMax{NAN, NAN};
    Outline localOutline;
    Outline pendingLocalOutline;
    Vec3 labelLocal{NAN, NAN, NAN};
    bool hasLabelWorld{};
    Vec3 labelWorld{NAN, NAN, NAN};
    AsyncDerivedUpdateRunHandle run;
    AsyncDerivedUpdateTargetHandle progressTarget;
};
struct DepthBoneUpdateWorkRecord {
    std::uint64_t targetKey{};
    std::uint64_t generation{};
    AsyncDerivedUpdateWorkHandle progressWork;
};
} // namespace detail

namespace {
using detail::DepthBoneUpdateAdapterRecord;
using detail::DepthBoneUpdateWorkRecord;
using detail::DepthBoneUpdateWorkKey;
struct TargetKeyHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        constexpr auto shift = sizeof(std::size_t) < sizeof(value) ? sizeof(std::size_t) * 8 : 0;
        return static_cast<std::size_t>(value ^ (shift ? value >> shift : 0));
    }
};
struct WorkHash {
    std::size_t operator()(DepthBoneUpdateWorkKey key) const noexcept {
        const std::array<std::uint32_t, 2> fields{key.batchId, key.targetUuid};
        return compat::ngPhobosStringHash({reinterpret_cast<const char*>(fields.data()),
            fields.size() * sizeof(std::uint32_t)});
    }
};
struct WorkEqual {
    bool operator()(DepthBoneUpdateWorkKey a, DepthBoneUpdateWorkKey b) const noexcept {
        return a.batchId == b.batchId && a.targetUuid == b.targetUuid;
    }
};
thread_local nicxlive::core::common::ResourceKeyMap<std::uint64_t,
    std::shared_ptr<DepthBoneUpdateAdapterRecord>, TargetKeyHash, std::equal_to<std::uint64_t>> updateRecords;
thread_local nicxlive::core::common::ResourceKeyMap<DepthBoneUpdateWorkKey,
    std::shared_ptr<DepthBoneUpdateWorkRecord>, WorkHash, WorkEqual> updateWork;
thread_local nicxlive::core::common::PolicyKeyMap<std::shared_ptr<Puppet>,
    AsyncDerivedUpdateScopeId,
    nicxlive::core::common::ObjectClassKeyPolicy<std::shared_ptr<Puppet>>> fallbackScopes;

float minimum(float a, float b) { return b < a ? b : a; }
float maximum(float a, float b) { return a < b ? b : a; }
std::uint64_t statusTargetKey(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target) {
    if (!root || !target) return 0;
    const auto rootUuid = root->getUUID();
    const auto targetUuid = target->getUUID();
    return (std::uint64_t(rootUuid) << 32) | std::uint64_t(targetUuid);
}
AsyncDerivedUpdateScopeId updateScope(std::shared_ptr<Puppet> puppet) {
    auto active = project::incActiveProject();
    if (active && nicxlive::core::common::ngClassObjectIs(active->puppet, puppet)) return active->derivedUpdateScope;
    auto found = fallbackScopes.find(puppet);
    if (found != fallbackScopes.end()) return found->second;
    auto created = incAsyncDerivedUpdateCreateScope();
    fallbackScopes[puppet] = created;
    return created;
}
std::shared_ptr<DepthBoneUpdateAdapterRecord> ensureRecord(
    std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target) {
    auto key = statusTargetKey(root, target);
    auto found = updateRecords.find(key);
    if (found == updateRecords.end()) {
        DepthBoneUpdateAdapterRecord initial;
        initial.root = root;
        initial.target = target;
        initial.puppet = target->puppetRef();
        initial.generation = 1;
        const auto prepared = std::allocate_shared<DepthBoneUpdateAdapterRecord>(
            nicxlive::core::d_runtime::SourceAllocator<DepthBoneUpdateAdapterRecord>{}, initial);
        auto slot = updateRecords[key];
        auto& stored = static_cast<std::shared_ptr<DepthBoneUpdateAdapterRecord>&>(slot);
        if (stored) *stored = *prepared;
        else stored = prepared;
        found = updateRecords.find(key);
    }
    auto record = found->second;
    record->root = root;
    record->target = target;
    record->puppet = target->puppetRef();
    return record;
}
Vec2 sourcePoint(nicxlive::core::math::Vec2Array& values, std::size_t index) {
    // The source is mutable opIndex followed by toVector, not const at().
    // Keep toVector's array check even when the opIndex assertion is disabled.
    nicxlive::core::math::detail::ngAssertVec2SourceIndex(index < values.size(), 139);
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, values.size(), compat::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 467);
    return values[index].toVector();
}
bool localBounds(std::shared_ptr<Node> node, Vec2& low, Vec2& high) {
    auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node);
    if (!target || target->getVertices().empty()) return false;
    auto first = sourcePoint(target->getVertices(), 0);
    if (!target->deformation.empty()) {
        auto offset = sourcePoint(target->deformation, 0);
        first.x += offset.x;
        first.y += offset.y;
    }
    low = high = first;
    const auto vertexCount = target->getVertices().size();
    for (std::size_t i = 1; i < vertexCount; ++i) {
        auto point = sourcePoint(target->getVertices(), i);
        if (i < target->deformation.size()) {
            auto offset = sourcePoint(target->deformation, i);
            point.x += offset.x;
            point.y += offset.y;
        }
        low.x = minimum(low.x, point.x);
        low.y = minimum(low.y, point.y);
        high.x = maximum(high.x, point.x);
        high.y = maximum(high.y, point.y);
    }
    return true;
}
Outline rectangle(Vec2 low, Vec2 high, Vec3* labelCorner = nullptr) {
    const float margin = maximum(4.0f, maximum(high.x - low.x, high.y - low.y) * 0.03f);
    const Vec2 a{low.x - margin, low.y - margin};
    const Vec2 b{high.x + margin, high.y + margin};
    if (labelCorner) *labelCorner = {b.x, a.y, 0};
    return Outline{{a.x,a.y,0}, {b.x,a.y,0}, {b.x,a.y,0}, {b.x,b.y,0},
        {b.x,b.y,0}, {a.x,b.y,0}, {a.x,b.y,0}, {a.x,a.y,0}};
}
void refreshTargetVisual(DepthBoneUpdateAdapterRecord& record) {
    Vec2 low, high;
    if (!localBounds(record.target, low, high)) {
        record.hasLocalBounds = false;
        record.localOutline.clear();
        record.pendingLocalOutline.clear();
        return;
    }
    record.hasLocalBounds = true;
    record.localBoundsMin = low;
    record.localBoundsMax = high;
    Vec3 labelCorner;
    record.localOutline = rectangle(low, high, &labelCorner);
    record.pendingLocalOutline.clear();
    const auto pairCount = record.localOutline.size() / 2;
    for (std::size_t i = 0; i < pairCount; ++i) {
        record.localOutline.checkViewIndex(i * 2);
        record.localOutline.checkViewIndex(i * 2 + 1);
        const auto start = record.localOutline.viewValue(i * 2);
        const auto end = record.localOutline.viewValue(i * 2 + 1);
        const Vec3 delta{end.x-start.x, end.y-start.y, end.z-start.z};
        for (int segment = 0; segment < 16; ++segment) {
            if ((segment & 1) != 0) continue;
            const float t0 = float(segment) / 16;
            const float t1 = float(segment + 1) / 16;
            const auto firstStart = record.localOutline.viewValue(i * 2);
            record.pendingLocalOutline.append(
                {firstStart.x+delta.x*t0,firstStart.y+delta.y*t0,firstStart.z+delta.z*t0});
            const auto secondStart = record.localOutline.viewValue(i * 2);
            record.pendingLocalOutline.append(
                {secondStart.x+delta.x*t1,secondStart.y+delta.y*t1,secondStart.z+delta.z*t1});
        }
    }
    record.labelLocal = labelCorner;
    if (!record.hasLabelWorld) {
        const auto matrix = compat::ngRequireObject(record.target)->getDynamicMatrix();
        record.labelWorld = core::math::ngTransformHomogeneousPoint(matrix, record.labelLocal);
        record.hasLabelWorld = true;
    }
}
AsyncDerivedUpdateViewportVisual viewportVisual(DepthBoneUpdateAdapterRecord& record) {
    AsyncDerivedUpdateViewportVisual visual;
    visual.viewportChannel = std::uint32_t(AsyncDerivedUpdateViewportChannel::Model);
    visual.hasAnchor = record.hasLabelWorld;
    visual.anchorWorld = record.labelWorld;
    auto matrix = compat::ngRequireObject(record.target)->getDynamicMatrix();
    compat::Slice<Vec3> world;
    const auto outlineLength = record.localOutline.size();
    for (std::size_t i = 0; i < outlineLength; ++i)
        world.append(core::math::ngTransformHomogeneousPoint(matrix, record.localOutline.viewValue(i)));
    visual.outlineWorld = AsyncDerivedUpdateOutline(world);
    return visual;
}
void removeTargetWork(std::uint64_t key) {
    compat::Slice<DepthBoneUpdateWorkKey> remove;
    for (const auto works = updateWork; const auto [workKey, stored] : works) {
        const auto work = *stored;
        if (work.targetKey != key) continue;
        incAsyncDerivedUpdateStale(work.progressWork,
            compat::RetainedString::fromStaticLiteral("Superseded by a newer Depth Bone update"));
        remove.append(workKey);
    }
    for (auto workKey : remove) updateWork.erase(workKey);
}
void beginGeneration(DepthBoneUpdateAdapterRecord& record) {
    const auto key = statusTargetKey(record.root, record.target);
    removeTargetWork(key);
    if (++record.generation == 0) record.generation = 1;
    record.plannedByParameter = {};
    record.detected = true;
    record.hasLabelWorld = false;
    refreshTargetVisual(record);
    AsyncDerivedUpdateOrigin origin;
    origin.projectScope = updateScope(record.puppet);
    origin.transactionId = record.generation;
    origin.providerId = NgDepthBoneDerivedUpdateProviderId;
    origin.operationName = compat::RetainedStringSlice(
        compat::RetainedString::fromStaticLiteral("Depth Bone Update"));
    record.run = incAsyncDerivedUpdateBeginRun(origin);
    AsyncDerivedUpdateTargetDesc desc;
    desc.key = {NgDepthBoneDerivedUpdateProviderId, origin.projectScope.id, key};
    desc.label = compat::RetainedString(compat::ngRequireObject(record.target)->getName());
    desc.reason = record.reason;
    desc.sourceRevision = record.generation;
    desc.mergePolicy = AsyncDerivedUpdateMergePolicy::SupersedeRunning;
    desc.visual = viewportVisual(record);
    record.progressTarget = incAsyncDerivedUpdateTrackTarget(record.run, desc);
}
bool currentSnapshot(DepthBoneUpdateAdapterRecord& record, AsyncDerivedUpdateSnapshot& snapshot) {
    snapshot = {};
    return record.progressTarget.valid() &&
        incAsyncDerivedUpdateTargetSnapshot(record.progressTarget, snapshot, true);
}
bool hasCurrentWork(DepthBoneUpdateAdapterRecord& record) {
    AsyncDerivedUpdateSnapshot snapshot;
    return currentSnapshot(record, snapshot) && (snapshot.queuedUnits > 0 || snapshot.runningUnits > 0);
}
bool completedGeneration(DepthBoneUpdateAdapterRecord& record) {
    AsyncDerivedUpdateSnapshot snapshot;
    return currentSnapshot(record, snapshot) && snapshot.expectedUnits <= snapshot.appliedUnits &&
        snapshot.queuedUnits == 0 && snapshot.runningUnits == 0;
}
void ensureGeneration(DepthBoneUpdateAdapterRecord& record) {
    AsyncDerivedUpdateSnapshot snapshot;
    if (!currentSnapshot(record, snapshot)) beginGeneration(record);
}
void removePreviousAppliedStatuses(std::shared_ptr<Puppet> puppet, std::uint64_t detectedKey) {
    compat::Slice<std::uint64_t> remove;
    for (const auto records = updateRecords; const auto [key, record] : records) {
        if (key == detectedKey || !nicxlive::core::common::ngClassObjectIs(record->puppet, puppet)) continue;
        AsyncDerivedUpdateSnapshot snapshot;
        if (!currentSnapshot(*record, snapshot) || snapshot.state != AsyncDerivedUpdateState::Applied) continue;
        incAsyncDerivedUpdateForgetTarget(record->progressTarget);
        remove.append(key);
    }
    for (auto key : remove) updateRecords.erase(key);
}
DepthBoneUpdateState depthBoneState(AsyncDerivedUpdateState state) {
    switch (state) {
    case AsyncDerivedUpdateState::Detected: return DepthBoneUpdateState::Detected;
    case AsyncDerivedUpdateState::Queued: return DepthBoneUpdateState::Queued;
    case AsyncDerivedUpdateState::Running: return DepthBoneUpdateState::Processing;
    case AsyncDerivedUpdateState::Applied: return DepthBoneUpdateState::Applied;
    case AsyncDerivedUpdateState::Stale: return DepthBoneUpdateState::Stale;
    case AsyncDerivedUpdateState::Failed: return DepthBoneUpdateState::Failed;
    case AsyncDerivedUpdateState::Canceled: return DepthBoneUpdateState::Canceled;
    }
    throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone_status.d"), 472);
}
} // namespace

void ngDepthBoneUpdateDetected(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target,
    std::shared_ptr<Parameter> parameter, Keypoint keypoint, compat::RetainedString reason) {
    if (!root || !target) return;
    auto key = statusTargetKey(root, target);
    removePreviousAppliedStatuses(target->puppetRef(), key);
    auto record = ensureRecord(root, target);
    AsyncDerivedUpdateSnapshot snapshot;
    const bool canceled = currentSnapshot(*record, snapshot) && snapshot.state == AsyncDerivedUpdateState::Canceled;
    if (!record->progressTarget.valid() || !record->detected || hasCurrentWork(*record) || canceled)
        beginGeneration(*record);
    record->parameter = parameter;
    record->keypoint = keypoint;
    record->reason = reason;
    incAsyncDerivedUpdateSetContext(record->progressTarget, reason);
}
void ngDepthBoneUpdatePlanned(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target,
    std::shared_ptr<Parameter> parameter, Keypoint keypoint, compat::RetainedString reason,
    std::size_t expectedWork) {
    if (!root || !target || expectedWork == 0) return;
    auto record = ensureRecord(root, target);
    ensureGeneration(*record);
    if (!record->detected && !hasCurrentWork(*record) && completedGeneration(*record)) beginGeneration(*record);
    record->detected = false;
    record->parameter = parameter;
    record->keypoint = keypoint;
    record->reason = reason;
    AsyncDerivedUpdateSnapshot snapshot;
    currentSnapshot(*record, snapshot);
    auto total = snapshot.expectedUnits;
    if (parameter) {
        std::size_t old = 0;
        const auto found = record->plannedByParameter.find(parameter);
        if (found != record->plannedByParameter.end()) old = found->second;
        if (expectedWork > old) {
            total += expectedWork - old;
            record->plannedByParameter[parameter] = expectedWork;
        }
    } else if (expectedWork > total) total = expectedWork;
    incAsyncDerivedUpdateSetExpected(record->progressTarget, total);
    incAsyncDerivedUpdateSetContext(record->progressTarget, reason);
}
void ngDepthBoneUpdateQueued(std::uint32_t batchId, std::shared_ptr<ExDepthRigRoot> root,
    std::shared_ptr<Node> target, std::shared_ptr<Parameter> parameter, Keypoint keypoint,
    compat::RetainedString reason, std::uint32_t retryCount) {
    if (batchId == 0 || !root || !target) return;
    auto record = ensureRecord(root, target);
    ensureGeneration(*record);
    if (!record->detected && !hasCurrentWork(*record) && completedGeneration(*record)) beginGeneration(*record);
    record->detected = false;
    record->parameter = parameter;
    record->keypoint = keypoint;
    record->reason = reason;
    incAsyncDerivedUpdateSetContext(record->progressTarget, reason, {}, retryCount);
    const DepthBoneUpdateWorkKey key{batchId, target->getUUID()};
    if (updateWork.find(key) == updateWork.end()) {
        DepthBoneUpdateWorkRecord work;
        work.targetKey = statusTargetKey(root, target);
        work.generation = record->generation;
        work.progressWork = incAsyncDerivedUpdateQueue(record->progressTarget);
        const auto stored = updateWork.find(key);
        if (stored != updateWork.end()) *stored->second = work;
        else updateWork[key] = std::allocate_shared<DepthBoneUpdateWorkRecord>(
            nicxlive::core::d_runtime::SourceAllocator<DepthBoneUpdateWorkRecord>{}, work);
    }
}
void ngDepthBoneUpdateProcessing(std::uint32_t batchId, std::shared_ptr<ExDepthRigRoot> root,
    std::shared_ptr<Node> target) {
    if (!root || !target) return;
    auto work = updateWork.find(DepthBoneUpdateWorkKey{batchId, target->getUUID()});
    if (work != updateWork.end()) incAsyncDerivedUpdateStart(work->second->progressWork);
}
void ngDepthBoneUpdateApplied(std::uint32_t batchId, std::shared_ptr<ExDepthRigRoot> root,
    std::shared_ptr<Node> target) {
    if (!root || !target) return;
    const DepthBoneUpdateWorkKey key{batchId, target->getUUID()};
    const auto foundWork = updateWork.find(key);
    if (foundWork == updateWork.end()) return;
    // The next virtual UUID reads can remove/reset the registry. Retain the
    // actual work object, so same-key writes remain visible without a snapshot.
    const auto work = foundWork->second;
    auto found = updateRecords.find(statusTargetKey(root, target));
    if (found == updateRecords.end() || found->second->generation != work->generation) {
        updateWork.erase(key);
        return;
    }
    auto record = found->second;
    incAsyncDerivedUpdateApplied(work->progressWork);
    updateWork.erase(key);
    AsyncDerivedUpdateSnapshot snapshot;
    if (currentSnapshot(*record, snapshot) && snapshot.appliedUnits >= snapshot.expectedUnits)
        refreshTargetVisual(*record);
}
void ngDepthBoneUpdateStale(std::uint32_t batchId, std::shared_ptr<ExDepthRigRoot> root,
    std::shared_ptr<Node> target, compat::RetainedString detail) {
    if (!root || !target) return;
    const DepthBoneUpdateWorkKey key{batchId, target->getUUID()};
    bool hadWork = false;
    if (auto work = updateWork.find(key); work != updateWork.end()) {
        hadWork = true;
        incAsyncDerivedUpdateStale(work->second->progressWork, detail);
        updateWork.erase(key);
    }
    if (auto found = updateRecords.find(statusTargetKey(root, target)); found != updateRecords.end()) {
        auto record = found->second;
        record->detected = false;
        if (!hadWork) incAsyncDerivedUpdateMarkTargetStale(record->progressTarget, detail);
        incAsyncDerivedUpdateSetContext(record->progressTarget, record->reason, detail);
    }
}
void ngDepthBoneUpdateBatchCanceled(std::uint32_t batchId, compat::RetainedString detail) {
    compat::Slice<DepthBoneUpdateWorkKey> remove;
    for (const auto works = updateWork; const auto [key, stored] : works) {
        const auto work = *stored;
        if (key.batchId != batchId) continue;
        if (!detail.empty()) incAsyncDerivedUpdateStale(work.progressWork, detail);
        else incAsyncDerivedUpdateDiscard(work.progressWork);
        remove.append(key);
    }
    for (auto key : remove) updateWork.erase(key);
}
void ngDepthBoneUpdateSetOwnerToken(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target,
    actions::AsyncActionToken token) {
    auto found = updateRecords.find(statusTargetKey(root, target));
    if (found == updateRecords.end() || !found->second->run.valid() || !token.valid()) return;
    incAsyncDerivedUpdateSetOwnerToken(found->second->run, token);
}
void ngDepthBoneUpdateFailed(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<Node> target,
    std::shared_ptr<Parameter> parameter, Keypoint keypoint, compat::RetainedString reason,
    compat::RetainedString detail) {
    if (!root || !target) return;
    auto record = ensureRecord(root, target);
    ensureGeneration(*record);
    removeTargetWork(statusTargetKey(root, target));
    record->parameter = parameter;
    record->keypoint = keypoint;
    record->reason = reason;
    record->detected = false;
    refreshTargetVisual(*record);
    incAsyncDerivedUpdateSetContext(record->progressTarget, reason, detail);
    incAsyncDerivedUpdateFailTarget(record->progressTarget, detail);
}
void ngDepthBoneUpdateAbort(compat::RetainedString detail) {
    for (const auto records = updateRecords; const auto [key, record] : records) {
        if (!hasCurrentWork(*record)) continue;
        record->detected = false;
        incAsyncDerivedUpdateFailTarget(record->progressTarget, detail);
    }
    updateWork = {};
}
compat::Slice<DepthBoneUpdateStatus> ngDepthBoneUpdateStatuses(std::shared_ptr<Puppet> puppet,
    bool includeExpired) {
    compat::Slice<DepthBoneUpdateStatus> result;
    compat::Slice<std::uint64_t> remove;
    for (const auto records = updateRecords; const auto [key, record] : records) {
        if (!record->root || !record->target ||
            (puppet && !nicxlive::core::common::ngClassObjectIs(record->puppet, puppet))) {
            remove.append(key);
            continue;
        }
        AsyncDerivedUpdateSnapshot snapshot;
        if (!incAsyncDerivedUpdateTargetSnapshot(record->progressTarget, snapshot, includeExpired)) {
            remove.append(key);
            continue;
        }
        DepthBoneUpdateStatus status;
        status.root = record->root;
        status.target = record->target;
        status.parameter = record->parameter;
        status.keypoint = record->keypoint;
        status.state = depthBoneState(snapshot.state);
        status.reason = snapshot.reason;
        status.detail = snapshot.detail;
        status.expectedWork = std::size_t(snapshot.expectedUnits);
        status.appliedWork = std::size_t(snapshot.appliedUnits);
        status.queuedWork = std::size_t(snapshot.queuedUnits);
        status.processingWork = std::size_t(snapshot.runningUnits);
        status.staleWork = std::size_t(snapshot.staleUnits);
        status.retryCount = snapshot.retryCount;
        status.generation = record->generation;
        status.hasLocalBounds = record->hasLocalBounds;
        status.localBoundsMin = record->localBoundsMin;
        status.localBoundsMax = record->localBoundsMax;
        status.localOutline = record->localOutline;
        status.pendingLocalOutline = record->pendingLocalOutline;
        status.labelLocal = record->labelLocal;
        status.hasLabelWorld = record->hasLabelWorld;
        status.labelWorld = record->labelWorld;
        result.append(status);
    }
    compat::Slice<DepthBoneUpdateWorkKey> removeWork;
    compat::Slice<std::shared_ptr<Puppet>> removedPuppets;
    for (auto key : remove) {
        if (auto found = updateRecords.find(key); found != updateRecords.end()) {
            const auto record = found->second;
            incAsyncDerivedUpdateForgetTarget(record->progressTarget);
            if (record->puppet) {
                const auto removedPuppet = record->puppet;
                removedPuppets.append(removedPuppet);
            }
        }
        for (const auto works = updateWork; const auto [workKey, stored] : works) {
            const auto work = *stored;
            if (work.targetKey == key) removeWork.append(workKey);
        }
        updateRecords.erase(key);
    }
    for (auto key : removeWork) updateWork.erase(key);
    for (auto removedPuppet : removedPuppets) {
        bool referenced = false;
        for (const auto records = updateRecords; const auto& [key, record] : records) {
            if (nicxlive::core::common::ngClassObjectIs(record->puppet, removedPuppet)) { referenced = true; break; }
        }
        if (!referenced) fallbackScopes.erase(removedPuppet);
    }
    return result;
}
bool ngBuildDepthBoneUpdateTargetOutline(std::shared_ptr<Node> targetNode, Outline& lines,
    Mat4& targetToWorld, Vec3& labelWorld) {
    lines.resetForOut();
    targetToWorld = Mat4::identity();
    labelWorld = {NAN, NAN, NAN};
    Vec2 low, high;
    if (!localBounds(targetNode, low, high)) return false;
    Vec3 labelCorner;
    lines = rectangle(low, high, &labelCorner);
    targetToWorld = targetNode->getDynamicMatrix();
    labelWorld = core::math::ngTransformHomogeneousPoint(targetToWorld, labelCorner);
    return true;
}
void ngClearDepthBoneUpdateStatuses() {
    incAsyncDerivedUpdateClearProvider(NgDepthBoneDerivedUpdateProviderId);
    updateRecords = {};
    updateWork = {};
    fallbackScopes = {};
}

} // namespace nijigenerate::commands::depth::bone_status

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::depth::bone_status::detail::DepthBoneUpdateAdapterRecord> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.depth.bone_status.DepthBoneUpdateAdapterRecord)";
    inline static constexpr char unqualifiedArgument[] = "(DepthBoneUpdateAdapterRecord)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.depth.bone_status.DepthBoneUpdateAdapterRecord";
    inline static constexpr char unqualifiedElement[] = "DepthBoneUpdateAdapterRecord";
};
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::depth::bone_status::detail::DepthBoneUpdateWorkRecord> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.depth.bone_status.DepthBoneUpdateWorkRecord)";
    inline static constexpr char unqualifiedArgument[] = "(DepthBoneUpdateWorkRecord)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.depth.bone_status.DepthBoneUpdateWorkRecord";
    inline static constexpr char unqualifiedElement[] = "DepthBoneUpdateWorkRecord";
};
}
