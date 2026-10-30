// Existing regression.d16677-16944; include inside regression::detail after helpers.
void testDepthBoneUpdateVisualizationState() {
    using namespace nijigenerate::commands::depth::bone_status;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::ext::nodes;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::Puppet;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Mat4;
    using nijigenerate::ext::ExParameter;
    namespace paths = nijigenerate::compat::phobos;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto readSource = [](std::initializer_list<RetainedString> segments) {
        auto path = paths::ngBuildPath(std::span<const RetainedString>(segments.begin(), segments.size()));
        return paths::ngReadUtf8Text(path);
    };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    root->setName(NullableString::fromStaticLiteral("update-status-root"));
    auto grid = ExGridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    grid->setName(NullableString::fromStaticLiteral("update-status-grid"));
    grid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-10, -20}, Vec2{30, -20}, Vec2{30, 40}, Vec2{-10, 40}}));
    auto param = ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("UpdateStatusParam"); }, [] { return false; });
    ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
    const auto statusFor = [&](std::shared_ptr<Node> target) {
        auto statuses = ngDepthBoneUpdateStatuses(nijigenerate::project::incActivePuppet(), true);
        for (auto status : statuses) if (status.target == target) return status;
        check(false, "DepthBone update status should include the requested target");
        return DepthBoneUpdateStatus{};
    };
    ngDepthBoneUpdateDetected(root, grid, param, Vec2u{1, 0}, text("depth edited"));
    auto status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Detected,
        "DepthBone target status should start at detected before work is planned");
    check(status.hasLocalBounds && nearVec2(status.localBoundsMin, Vec2{-10, -20}) &&
        nearVec2(status.localBoundsMax, Vec2{30, 40}),
        "DepthBone target status should cache its local bounds at the update event");
    check(status.hasLabelWorld,
        "DepthBone target status should decide its viewport label anchor when the update generation starts");
    auto initialLabelWorld = status.labelWorld;
    check(status.localOutline.size() == 8 && status.pendingLocalOutline.size() == 64,
        "DepthBone target status should cache solid and pending outline geometry outside the frame loop");
    auto detectedGeneration = status.generation;
    ngDepthBoneUpdateDetected(root, grid, param, Vec2u{2, 0}, text("coalesced depth edit"));
    status = statusFor(grid);
    check(status.generation == detectedGeneration && status.keypoint == Vec2u{2, 0} &&
        status.reason == "coalesced depth edit",
        "repeated invalidations before dispatch should coalesce without rebuilding a new visual generation");
    Outline gridOutline;
    Mat4 gridToWorld;
    Vec3 gridLabelWorld;
    check(ngBuildDepthBoneUpdateTargetOutline(grid, gridOutline, gridToWorld, gridLabelWorld),
        "DepthBone update visualization should build a GridDeformer outline");
    check(gridOutline.size() == 8 && nearVec3(gridOutline[0], Vec3{-14, -24, 0}) &&
        nearVec3(gridOutline[1], Vec3{34, -24, 0}),
        "DepthBone GridDeformer outline should be a padded four-edge rectangle");
    ngDepthBoneUpdatePlanned(root, grid, param, Vec2u{1, 0}, text("depth edited"), 3);
    status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Queued && status.expectedWork == 3,
        "planned DepthBone work should report the expected keypoint count");
    check(status.appliedWork == 0 && ngRequireObject(status.target)->name == grid->name,
        "viewport progress must start empty and identify the affected GridDeformer by name");
    ngDepthBoneUpdateQueued(101, root, grid, param, Vec2u{0, 0}, text("depth edited"));
    status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Queued && status.queuedWork == 1,
        "enqueued DepthBone work should report one waiting target write");
    ngDepthBoneUpdateProcessing(101, root, grid);
    status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Processing && status.processingWork == 1,
        "submitted DepthBone work should report processing");
    ngDepthBoneUpdateApplied(101, root, grid);
    status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Processing && status.appliedWork == 1,
        "a generation must remain running between GPU chunks after processing has started");
    check(near(static_cast<float>(status.appliedWork) / status.expectedWork, 1.0f / 3.0f),
        "viewport progress must represent applied work as a fraction instead of X/N text");
    ngDepthBoneUpdateQueued(102, root, grid, param, Vec2u{1, 0}, text("depth edited"));
    ngDepthBoneUpdateQueued(103, root, grid, param, Vec2u{2, 0}, text("depth edited"));
    ngDepthBoneUpdateQueued(104, root, grid, param, Vec2u{3, 0}, text("overlapping depth edit"));
    status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Processing && status.expectedWork == 4 &&
        status.appliedWork == 1 && status.queuedWork == 3,
        "additional queued work must increase the visible total without reverting RUN to WAIT");
    ngDepthBoneUpdateApplied(102, root, grid);
    ngDepthBoneUpdateApplied(103, root, grid);
    ngDepthBoneUpdateProcessing(104, root, grid);
    status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Processing && status.appliedWork == 3 && status.expectedWork == 4,
        "running DepthBone work must retain an unfinished progress denominator");
    grid->getDeformation().fill(Vec2{20, 10});
    ngDepthBoneUpdateApplied(104, root, grid);
    status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Applied && status.appliedWork == 4 && status.expectedWork == 4,
        "DepthBone target status should become applied only after all writebacks complete");
    check(near(static_cast<float>(status.appliedWork) / status.expectedWork, 1.0f),
        "viewport progress must be full when every target writeback is applied");
    check(nearVec3(status.labelWorld, initialLabelWorld),
        "a DepthBone update generation must keep its first label position even when completion refreshes changed target bounds");
    auto genericOverlaySource = readSource({regressionRepoRoot(), text("source"), text("nijigenerate"),
        text("viewport"), text("asyncderivedupdateoverlay.d")});
    auto genericRegistrySource = readSource({regressionRepoRoot(), text("source"), text("nijigenerate"),
        text("core"), text("asyncderivedupdate.d")});
    auto depthAdapterSource = readSource({regressionRepoRoot(), text("source"), text("nijigenerate"),
        text("commands"), text("depth"), text("bone_status.d")});
    auto depthBoneOverlaySource = readSource({regressionRepoRoot(), text("source"), text("nijigenerate"),
        text("viewport"), text("model"), text("depthboneoverlay.d")});
    auto viewportPanelSource = readSource({regressionRepoRoot(), text("source"), text("nijigenerate"),
        text("panels"), text("viewport.d")});
    auto modelViewportSource = readSource({regressionRepoRoot(), text("source"), text("nijigenerate"),
        text("viewport"), text("model"), text("package.d")});
    auto deformViewportSource = readSource({regressionRepoRoot(), text("source"), text("nijigenerate"),
        text("viewport"), text("model"), text("deform.d")});
    check(genericOverlaySource.view().find("igProgressBar(") == std::string_view::npos &&
        genericOverlaySource.view().find("ParentWindow.DrawList") == std::string_view::npos &&
        genericOverlaySource.view().find("ImDrawList_AddRectFilled(") != std::string_view::npos &&
        genericOverlaySource.view().find("ImDrawList_AddText(") != std::string_view::npos &&
        genericOverlaySource.view().find("incUiAccentColor(AsyncDerivedUpdateOverlayOpacity)") != std::string_view::npos &&
        genericOverlaySource.view().find("AsyncDerivedUpdateFontScale") != std::string_view::npos &&
        genericOverlaySource.view().find("AsyncDerivedUpdateVerticalPadding = 1.0f") != std::string_view::npos &&
        genericOverlaySource.view().find("snapshot.visual.anchorWorld") != std::string_view::npos &&
        genericOverlaySource.view().find("ImDrawList_PushClipRect(") != std::string_view::npos &&
        genericRegistrySource.view().find("const(vec3)[] outlineWorld") != std::string_view::npos &&
        genericRegistrySource.view().find("GridDeformer") == std::string_view::npos &&
        genericRegistrySource.view().find("PathDeformer") == std::string_view::npos &&
        genericRegistrySource.view().find("ExDepthRigRoot") == std::string_view::npos &&
        depthAdapterSource.view().find("incAsyncDerivedUpdateTrackTarget(") != std::string_view::npos &&
        depthAdapterSource.view().find("incAsyncDerivedUpdateQueue(") != std::string_view::npos &&
        depthBoneOverlaySource.view().find("DepthBoneUpdateStatus") == std::string_view::npos &&
        viewportPanelSource.view().find("viewport.drawOverlay(") != std::string_view::npos &&
        viewportPanelSource.view().find("drawAsyncDerivedUpdateViewportOverlay(") != std::string_view::npos &&
        viewportPanelSource.view().find("drawAsyncDerivedUpdateDetailsUi(") != std::string_view::npos &&
        modelViewportSource.view().find("drawAsyncDerivedUpdateViewportOverlay(") == std::string_view::npos &&
        deformViewportSource.view().find("drawAsyncDerivedUpdateViewportOverlay(") == std::string_view::npos,
        "derived-update progress must be a domain-independent, compact, theme-accented custom viewport overlay");
    auto appliedGeneration = status.generation;
    check(grid->getDeformation().size() == grid->getVertices().size(),
        "GridDeformer update visualization fixture should have matching deformation data");
    ngDepthBoneUpdateDetected(root, grid, param, Vec2u{1, 0}, text("mesh changed"));
    status = statusFor(grid);
    check(status.generation > appliedGeneration && status.appliedWork == 0 && status.expectedWork == 0 &&
        !nearVec3(status.labelWorld, initialLabelWorld), "a new DepthBone invalidation should start a fresh visual generation");
    const bool boundsMatch = nearVec2(status.localBoundsMax, Vec2{50, 50});
    const std::array<format::TypedFormatArgument, 2> boundsArguments{
        format::ngFormatArgument(status.localBoundsMax.x), format::ngFormatArgument(status.localBoundsMax.y)};
    auto boundsMessage = nijigenerate::platform::detail::ngPathAllocation([&] {
        return RetainedString(format::ngFormatTyped(
            "a new update event should refresh cached bounds after deformation changes; actual=(%s,%s)", boundsArguments));
    });
    require(boundsMatch, boundsMessage);
    ngDepthBoneUpdatePlanned(root, grid, param, Vec2u{1, 0}, text("mesh changed"), 1);
    ngDepthBoneUpdateQueued(201, root, grid, param, Vec2u{1, 0}, text("mesh changed"), 1);
    ngDepthBoneUpdateProcessing(201, root, grid);
    ngDepthBoneUpdateStale(201, root, grid, text("target vertices changed"));
    status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Stale && status.staleWork == 1 && status.detail == "target vertices changed",
        "stale DepthBone work should remain visible with its rejection reason");
    ngDepthBoneUpdateFailed(root, grid, param, Vec2u{1, 0}, text("mesh changed"), text("writeback failed"));
    status = statusFor(grid);
    check(status.state == DepthBoneUpdateState::Failed && status.detail == "writeback failed",
        "failed DepthBone work should remain visible with its failure reason");
    auto path = ExPathDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    path->setName(NullableString::fromStaticLiteral("update-status-path"));
    path->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{100, 200}, Vec2{140, 200}, Vec2{140, 260}}));
    ngDepthBoneUpdateDetected(root, path, param, Vec2u{0, 0}, text("path depth edited"));
    ngDepthBoneUpdatePlanned(root, path, param, Vec2u{0, 0}, text("path depth edited"), 1);
    ngDepthBoneUpdateQueued(301, root, path, param, Vec2u{0, 0}, text("path depth edited"));
    ngDepthBoneUpdateBatchCanceled(301, text("superseded path update"));
    status = statusFor(path);
    check(status.state == DepthBoneUpdateState::Stale && status.detail == "superseded path update",
        "canceled PathDeformer work should expose its stale reason");
    auto silentPath = ExPathDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    silentPath->setName(NullableString::fromStaticLiteral("update-status-silent-cancel-path"));
    silentPath->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{200, 300}, Vec2{240, 300}, Vec2{240, 360}}));
    ngDepthBoneUpdateDetected(root, silentPath, param, Vec2u{0, 0}, text("silent batch replacement"));
    ngDepthBoneUpdatePlanned(root, silentPath, param, Vec2u{0, 0}, text("silent batch replacement"), 1);
    ngDepthBoneUpdateQueued(302, root, silentPath, param, Vec2u{0, 0}, text("silent batch replacement"));
    ngDepthBoneUpdateBatchCanceled(302);
    status = statusFor(silentPath);
    check(status.state == DepthBoneUpdateState::Queued && status.queuedWork == 0 && status.staleWork == 0 && status.detail.size() == 0,
        "a silent batch replacement must discard in-flight work without inventing a stale or canceled state");
    ngDepthBoneUpdateStale(999, root, silentPath, text("replacement target revision changed"));
    status = statusFor(silentPath);
    check(status.state == DepthBoneUpdateState::Stale && status.staleWork == 1 &&
        status.detail == "replacement target revision changed",
        "a stale callback without a surviving work handle must preserve the prior DepthBone status behavior");
    Outline pathOutline;
    Mat4 pathToWorld;
    Vec3 pathLabelWorld;
    check(ngBuildDepthBoneUpdateTargetOutline(path, pathOutline, pathToWorld, pathLabelWorld) && pathOutline.size() == 8,
        "DepthBone update visualization should build a PathDeformer outline");
    auto mixedPlanGrid = ExGridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    mixedPlanGrid->setName(NullableString::fromStaticLiteral("update-status-mixed-plan-grid"));
    mixedPlanGrid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{10, 0}, Vec2{10, 10}, Vec2{0, 10}}));
    ngDepthBoneUpdateDetected(root, mixedPlanGrid, param, Vec2u{0, 0}, text("mixed planning"));
    ngDepthBoneUpdatePlanned(root, mixedPlanGrid, param, Vec2u{0, 0}, text("mixed planning"), 2);
    ngDepthBoneUpdatePlanned(root, mixedPlanGrid, nullptr, Vec2u{0, 0}, text("mixed planning"), 5);
    ngDepthBoneUpdatePlanned(root, mixedPlanGrid, param, Vec2u{0, 0}, text("mixed planning"), 3);
    status = statusFor(mixedPlanGrid);
    check(status.expectedWork == 6, "parameter and parameter-less planning must retain the previous incremental denominator semantics");
    auto noParamRoot = ExDepthRigRoot::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    auto noParamBone = ngCreateDepthBone(noParamRoot, text("NoParamBone"), Vec3{0, 0, 0}, Vec3{0, 20, 0});
    auto noParamTarget = ExGridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    noParamTarget->setName(NullableString::fromStaticLiteral("update-status-no-param-grid"));
    noParamTarget->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{0, 0}, Vec2{10, 0}, Vec2{10, 10}, Vec2{0, 10}}));
    ExDepthRigBinding noParamBinding;
    noParamBinding.targetUuid = noParamTarget->getUUID();
    noParamBinding.targetKind = ExDepthTargetKind::Grid;
    noParamBinding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(noParamBone->getUUID())};
    noParamRoot->bindings = Slice<ExDepthRigBinding>{noParamBinding};
    ngRequireObject(nijigenerate::project::incActivePuppet())->rescanNodes();
    ngMarkDepthBoneDirty(noParamRoot, nullptr, Vec2u{0, 0}, text("depth edited without parameter"), DepthBoneDirtyScope::AllKeypoints);
    ngFlushDepthBoneDirty();
    status = statusFor(noParamTarget);
    check(status.state == DepthBoneUpdateState::Failed && status.detail.view().find("No parameter drives") != std::string_view::npos,
        "DepthBone visualization should report an unqueueable update instead of waiting forever");
    auto replacementPuppet = std::allocate_shared<Puppet>(SourceAllocator<Puppet>{});
    check(ngDepthBoneUpdateStatuses(replacementPuppet, true).size() == 0 &&
        ngDepthBoneUpdateStatuses(nijigenerate::project::incActivePuppet(), true).size() == 0,
        "querying a replacement project must discard DepthBone adapter records that retain the old puppet graph");
    ngClearDepthBoneUpdateStatuses();
    check(ngDepthBoneUpdateStatuses(nijigenerate::project::incActivePuppet(), true).size() == 0,
        "clearing DepthBone update visualization state should remove every target record");
}
