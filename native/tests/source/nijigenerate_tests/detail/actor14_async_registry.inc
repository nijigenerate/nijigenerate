// Existing regression.d16946-17088; include in regression::detail after helpers.
void testAsyncDerivedUpdateRegistry() {
    using namespace nijigenerate::core::asyncderivedupdate;
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::math::Vec3;
    resetCase();
    incAsyncDerivedUpdateClearAll();
    auto registryScope = incAsyncDerivedUpdateCreateScope();
    AsyncDerivedUpdateOrigin origin;
    origin.projectScope = registryScope;
    origin.transactionId = 41;
    origin.providerId = 0x54455354;
    origin.operationName = RetainedString::fromStaticLiteral("Fixture Edit");
    auto run = incAsyncDerivedUpdateBeginRun(origin);
    AsyncDerivedUpdateTargetDesc desc;
    desc.key = AsyncDerivedUpdateTargetKey(origin.providerId, registryScope.id, 77);
    desc.label = RetainedString::fromStaticLiteral("fixture-grid");
    desc.reason = RetainedString::fromStaticLiteral("fixture changed");
    desc.sourceRevision = 1;
    desc.visual.viewportChannel = static_cast<std::uint32_t>(AsyncDerivedUpdateViewportChannel::Model);
    desc.visual.hasAnchor = true;
    desc.visual.anchorWorld = Vec3{10, 20, 0};
    nijigenerate::compat::Slice<Vec3> producerOutline{
        Vec3{0, 0, 0}, Vec3{10, 0, 0}, Vec3{10, 0, 0}, Vec3{10, 10, 0}};
    desc.visual.outlineWorld = AsyncDerivedUpdateOutline(producerOutline);
    auto target = incAsyncDerivedUpdateTrackTarget(run, desc);
    require(target.valid(), RetainedString::fromStaticLiteral(
        "generic derived-update registry should accept a scoped target"));
    auto mismatchedDesc = desc;
    ++mismatchedDesc.key.scopeId;
    require(!incAsyncDerivedUpdateTrackTarget(run, mismatchedDesc).valid(),
        RetainedString::fromStaticLiteral("a run must reject targets from another project scope"));
    desc.visual.anchorWorld = Vec3{100, 200, 0};
    producerOutline[0] = Vec3{999, 999, 0};
    incAsyncDerivedUpdateSetExpected(target, 2);
    auto first = incAsyncDerivedUpdateQueue(target);
    incAsyncDerivedUpdateStart(first);
    incAsyncDerivedUpdateApplied(first);
    incAsyncDerivedUpdateApplied(first);
    AsyncDerivedUpdateSnapshot snapshot;
    require(incAsyncDerivedUpdateTargetSnapshot(target, snapshot, true)
        && snapshot.state == AsyncDerivedUpdateState::Running
        && snapshot.expectedUnits == 2 && snapshot.appliedUnits == 1 && snapshot.runningUnits == 0,
        RetainedString::fromStaticLiteral("derived-update progress must stay RUN between chunks and ignore duplicate completion"));
    require(nearVec3(snapshot.visual.anchorWorld, Vec3{10, 20, 0})
        && nearVec3(snapshot.visual.outlineWorld[0], Vec3{0, 0, 0}),
        RetainedString::fromStaticLiteral("derived-update visual position and outline must be frozen at target registration"));
    AsyncDerivedUpdateSnapshot reread;
    require(incAsyncDerivedUpdateTargetSnapshot(target, reread, true)
        && nearVec3(reread.visual.outlineWorld[0], Vec3{0, 0, 0}),
        RetainedString::fromStaticLiteral("consumer snapshots must expose the registry-owned immutable geometry"));
    auto second = incAsyncDerivedUpdateQueue(target);
    incAsyncDerivedUpdateStart(second);
    incAsyncDerivedUpdateApplied(second);
    require(incAsyncDerivedUpdateTargetSnapshot(target, snapshot, true)
        && snapshot.state == AsyncDerivedUpdateState::Applied
        && near(incAsyncDerivedUpdateProgress(snapshot), 1.0f),
        RetainedString::fromStaticLiteral("derived-update progress should complete only after every explicit work handle is applied"));
    origin.transactionId = 43;
    auto coalesceRun = incAsyncDerivedUpdateBeginRun(origin);
    auto coalesceDesc = desc;
    coalesceDesc.key.subjectId = 88;
    coalesceDesc.mergePolicy = AsyncDerivedUpdateMergePolicy::CoalescePending;
    auto coalescedTarget = incAsyncDerivedUpdateTrackTarget(coalesceRun, coalesceDesc);
    origin.transactionId = 44;
    auto coalesceReplacementRun = incAsyncDerivedUpdateBeginRun(origin);
    coalesceDesc.label = RetainedString::fromStaticLiteral("coalesced-fixture");
    auto coalescedReplacement = incAsyncDerivedUpdateTrackTarget(coalesceReplacementRun, coalesceDesc);
    require(coalescedTarget == coalescedReplacement
        && incAsyncDerivedUpdateTargetSnapshot(coalescedTarget, snapshot, true)
        && snapshot.origin.transactionId == 44
        && snapshot.label.view() == "coalesced-fixture",
        RetainedString::fromStaticLiteral("pending updates may coalesce while retaining the newest operation context"));
    incAsyncDerivedUpdateEndRun(coalesceRun);
    auto coalescedWork = incAsyncDerivedUpdateQueue(coalescedTarget);
    require(coalescedWork.valid(), RetainedString::fromStaticLiteral(
        "ending a replaced producer run must not seal a target transferred to the newer run"));
    incAsyncDerivedUpdateStart(coalescedWork);
    incAsyncDerivedUpdateApplied(coalescedWork);
    incAsyncDerivedUpdateEndRun(coalesceReplacementRun);
    require(incAsyncDerivedUpdateTargetSnapshot(coalescedTarget, snapshot, true)
        && snapshot.state == AsyncDerivedUpdateState::Applied
        && !incAsyncDerivedUpdateQueue(coalescedTarget).valid(),
        RetainedString::fromStaticLiteral("ending a run must seal its no-work targets and reject late queue additions"));
    incAsyncDerivedUpdateForgetTarget(coalescedTarget);
    auto visible = incAsyncDerivedUpdateSnapshots(registryScope,
        static_cast<std::uint32_t>(AsyncDerivedUpdateViewportChannel::Model), true);
    require(visible.size() == 1 && visible[0].label.view() == "fixture-grid",
        RetainedString::fromStaticLiteral("scope and viewport filtering must expose only the exact registered target"));
    origin.transactionId = 42;
    auto replacementRun = incAsyncDerivedUpdateBeginRun(origin);
    desc.sourceRevision = 2;
    desc.visual.anchorWorld = Vec3{30, 40, 0};
    producerOutline[0] = Vec3{30, 40, 0};
    auto replacement = incAsyncDerivedUpdateTrackTarget(replacementRun, desc);
    visible = incAsyncDerivedUpdateSnapshots(registryScope,
        static_cast<std::uint32_t>(AsyncDerivedUpdateViewportChannel::Model), true);
    require(replacement.valid() && visible.size() == 1 && visible[0].target == replacement
        && visible[0].sourceRevision == 2 && !incAsyncDerivedUpdateTargetSnapshot(target, snapshot, true),
        RetainedString::fromStaticLiteral("a newer generation must supersede the same target without showing unrelated or duplicate records"));
    auto registryOwner = std::allocate_shared<nijigenerate::actions::AsyncGroupAction>(
        nicxlive::core::d_runtime::SourceAllocator<nijigenerate::actions::AsyncGroupAction>{});
    auto registryToken = registryOwner->asyncToken();
    incAsyncDerivedUpdateSetOwnerToken(replacementRun, registryToken);
    auto canceledWork = incAsyncDerivedUpdateQueue(replacement);
    incAsyncDerivedUpdateStart(canceledWork);
    registryOwner->rollback();
    require(incAsyncDerivedUpdateTargetSnapshot(replacement, snapshot, true)
        && snapshot.state == AsyncDerivedUpdateState::Canceled
        && !incAsyncDerivedUpdateShowsViewportProgress(snapshot),
        RetainedString::fromStaticLiteral("generic viewport progress must follow its owner token and stop on undo"));
    registryOwner->redo();
    require(registryToken.canceled() && registryOwner->asyncToken().active(),
        RetainedString::fromStaticLiteral("redo must issue a fresh async token without reactivating the canceled generation"));
    const std::array sourceSegments{regressionRepoRoot(), RetainedString::fromStaticLiteral("source"),
        RetainedString::fromStaticLiteral("nijigenerate"), RetainedString::fromStaticLiteral("viewport"),
        RetainedString::fromStaticLiteral("vertex"), RetainedString::fromStaticLiteral("automesh"),
        RetainedString::fromStaticLiteral("package.d")};
    const auto autoMeshUiSource = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(sourceSegments));
    require(autoMeshUiSource.view().find("asyncderivedupdate") == std::string_view::npos,
        RetainedString::fromStaticLiteral("AutoMesh must keep its existing explicit-operation UI outside the derived-update registry"));
    incAsyncDerivedUpdateClearScope(registryScope);
    require(incAsyncDerivedUpdateSnapshots(registryScope, 0, true).size() == 0,
        RetainedString::fromStaticLiteral("clearing a project scope must remove all of its derived-update state"));
}
