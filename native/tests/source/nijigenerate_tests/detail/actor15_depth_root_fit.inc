// Existing regression.d14461-14731. Include once inside detail after existing GPU hooks.
extern thread_local std::size_t fitZMutationCount;
thread_local nijigenerate::actions::depthboneinvalidation::DepthBoneMutation fitZLastMutation;
extern thread_local nijigenerate::actions::depthboneinvalidation::DepthBoneMutationHook fitZForwardMutationHook;

void countFitZMutation(nijigenerate::actions::depthboneinvalidation::DepthBoneMutation mutation) {
    ++fitZMutationCount;
    fitZLastMutation = mutation;
    if (fitZForwardMutationHook) fitZForwardMutationHook(mutation);
}

void testDepthRigRootFitZToDepth() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::ngVecaFromVectorsSource;
    using nicxlive::common::SharedArray;
    using nicxlive::core::nodes::Deformable;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::CommandIrreversibleEffect;
    using nijigenerate::actions::AsyncGroupAction;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::param::DeformationParameterBinding;
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::commands::depth::bone_gpu_async;
    using namespace nijigenerate::commands::depth::bone_status;
    using namespace nijigenerate::actions::depthboneinvalidation;
    namespace history = nijigenerate::core::actionstack;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto formatted = []<class... Args>(std::string_view pattern, Args&&... args) {
        const std::array<format::TypedFormatArgument, sizeof...(Args)> arguments{
            format::ngFormatArgument(std::forward<Args>(args))...};
        auto rendered = format::ngFormatTyped(pattern, arguments);
        return nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(rendered)); });
    };
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    root->setName(text("fit-depth-root").toNullableString());
    auto target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    target->setName(text("fit-depth-grid").toNullableString());
    target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
        Vec2{-10, -10}, Vec2{10, -10}, Vec2{-10, 10}, Vec2{10, 10}}));
    target->replaceDepths(Slice<float>{0.5f, 0.5f, 0.5f, 0.5f});
    auto bone = ngCreateDepthBone(root, text("FitBone"), Vec3{-10, -10, 0}, Vec3{-10, 90, 0});
    auto siblingBone = ngCreateDepthBone(root, text("FitSiblingBone"), Vec3{10, -10, 0}, Vec3{10, 90, 0});
    nijigenerate::project::incActivePuppet()->rescanNodes();
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    const auto commandAt = [&](DepthBoneCommand key, std::size_t line) {
        auto found = commands.find(key);
        if (found == commands.end()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            text("source/nijigenerate_tests/regression.d"), line);
        return found->second;
    };
    auto rootFitCommand = commandAt(DepthBoneCommand::FitDepthRigRootZToDepth, 14494);
    auto boneFitCommand = commandAt(DepthBoneCommand::FitDepthBoneZToDepth, 14496);
    check(rootFitCommand && rootFitCommand->mcpExposed(),
        "DepthRigRoot Fit Z to Depth command must be registered and MCP-visible");
    check(boneFitCommand && boneFitCommand->mcpExposed(),
        "DepthBone Fit Z to Depth command must be registered and MCP-visible");
    check(rootFitCommand->irreversibleEffect() == CommandIrreversibleEffect::configEdit,
        "DepthRigRoot Fit Z to Depth command must declare its config-edit effect");
    check(boneFitCommand->irreversibleEffect() == CommandIrreversibleEffect::configEdit,
        "DepthBone Fit Z to Depth command must declare its config-edit effect");
    check(nijigenerate::commands::ngCommandIdFromKey(DepthBoneCommand::FitDepthRigRootZToDepth) ==
        "DepthBoneCommand.FitDepthRigRootZToDepth",
        "DepthRigRoot Fit Z to Depth must have a stable public command id");
    check(nijigenerate::commands::ngCommandIdFromKey(DepthBoneCommand::FitDepthBoneZToDepth) ==
        "DepthBoneCommand.FitDepthBoneZToDepth",
        "DepthBone Fit Z to Depth must have a stable public command id");
    auto rootFitArgs = FitDepthRigRootZToDepthCommand::reflectArgMeta();
    auto boneFitArgs = FitDepthBoneZToDepthCommand::reflectArgMeta();
    check(rootFitArgs.size() == 1 && rootFitArgs[0].fieldName.view() == "root" &&
        rootFitArgs[0].fieldDesc.view().find("DepthRigRoot") != std::string_view::npos,
        "DepthRigRoot Fit Z to Depth must publish an explicit root argument");
    check(boneFitArgs.size() == 1 && boneFitArgs[0].fieldName.view() == "bone" &&
        boneFitArgs[0].fieldDesc.view().find("DepthBone") != std::string_view::npos,
        "DepthBone Fit Z to Depth must publish an explicit bone argument");
    check(ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, target, bone)->succeeded,
        "fit fixture should add a depth bone source");
    check(root->depthBones().size() == 2, "fit fixture should expose two descendant DepthBones");
    check(root->bindings.size() == 1, "fit fixture should keep one DepthRig binding");
    check(root->bindings[0].targetUuid == target->getUUID(), "fit fixture binding should point to the depth grid target");
    const auto actualBoneIds = root->bindings[0].sourceBoneUuids;
    const auto expectedBoneIds = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
    bool sameBoneIds = actualBoneIds.size() == expectedBoneIds.size();
    if (sameBoneIds) {
        for (std::size_t i = 0; i < expectedBoneIds.size(); ++i)
            if (!(actualBoneIds[i] == expectedBoneIds[i])) { sameBoneIds = false; break; }
    }
    check(sameBoneIds, "fit fixture binding should point to the source DepthBone");
    check(target->copyDepths().size() == target->vertices.size(), "fit fixture depths should match grid vertices");
    auto originalLocalZ = bone->localTransform.translation.z;
    auto siblingOriginalLocalZ = siblingBone->localTransform.translation.z;
    float baselineProposedLocalZ = std::numeric_limits<float>::quiet_NaN();
    float baselineRootDepth = std::numeric_limits<float>::quiet_NaN();
    check(ngDepthRigNodeCurrentScaledDepth(bone, baselineProposedLocalZ, baselineRootDepth),
        "fit fixture should expose its unrotated sampled depth");
    history::incActionClearHistory();
    fitZMutationCount = 0;
    fitZLastMutation = DepthBoneMutation{};
    fitZForwardMutationHook = ngDepthBoneMutationHook;
    ngDepthBoneMutationHook = &countFitZMutation;
    auto fitResult = ngCmd<DepthBoneCommand::FitDepthRigRootZToDepth>(ctx, root);
    ngDepthBoneMutationHook = fitZForwardMutationHook;
    fitZForwardMutationHook = nullptr;
    check(fitResult->succeeded, "DepthRigRoot Fit Z to Depth command returned false");
    check(fitZMutationCount == 1,
        "one Fit Z command must publish exactly one DepthBone invalidation, not one per changed bone");
    check(fitZLastMutation.kind == DepthBoneMutationKind::RigConfiguration && fitZLastMutation.target == root,
        "Fit Z must invalidate the completed DepthRigRoot pose as one transaction");
    const bool changedZ = !near(bone->localTransform.translation.z, originalLocalZ);
    auto changedZText = formatted("%s", bone->localTransform.translation.z);
    require(changedZ, concatenateSourceStrings({
        text("DepthRigRoot Fit Z to Depth should update descendant DepthBone local Z; got "), changedZText}));
    const bool positiveZ = bone->localTransform.translation.z > originalLocalZ;
    auto positiveZText = formatted("%s", bone->localTransform.translation.z);
    require(positiveZ, concatenateSourceStrings({
        text("positive stored depth should fit descendant DepthBone local Z to the positive side; got "), positiveZText}));
    check(!near(siblingBone->localTransform.translation.z, siblingOriginalLocalZ),
        "DepthRigRoot Fit Z to Depth should update every fittable sibling DepthBone");
    check(history::incActionHistory().size() == 1, "DepthRigRoot Fit Z to Depth should push one undo action");
    auto fittedLocalZ = bone->localTransform.translation.z;
    auto siblingFittedLocalZ = siblingBone->localTransform.translation.z;
    history::incActionUndo();
    check(near(bone->localTransform.translation.z, originalLocalZ),
        "undo DepthRigRoot Fit Z to Depth should restore the previous DepthBone local Z");
    check(near(siblingBone->localTransform.translation.z, siblingOriginalLocalZ),
        "undo DepthRigRoot Fit Z to Depth should restore every changed sibling DepthBone local Z");
    history::incActionRedo();
    check(near(bone->localTransform.translation.z, fittedLocalZ),
        "redo DepthRigRoot Fit Z to Depth should restore the fitted DepthBone local Z");
    check(near(siblingBone->localTransform.translation.z, siblingFittedLocalZ),
        "redo DepthRigRoot Fit Z to Depth should restore every fitted sibling DepthBone local Z");
    history::incActionUndo();
    history::incActionClearHistory();
    auto boneFitResult = ngCmd<DepthBoneCommand::FitDepthBoneZToDepth>(ctx, bone);
    check(boneFitResult->succeeded, "DepthBone Fit Z to Depth command returned false");
    check(near(bone->localTransform.translation.z, fittedLocalZ),
        "DepthBone Fit Z to Depth command should produce the fitted local Z");
    check(history::incActionHistory().size() == 1, "DepthBone Fit Z to Depth command should push one undo action");
    history::incActionUndo();
    check(near(bone->localTransform.translation.z, originalLocalZ),
        "undo DepthBone Fit Z to Depth should restore the original local Z");
    history::incActionRedo();
    check(near(bone->localTransform.translation.z, fittedLocalZ),
        "redo DepthBone Fit Z to Depth should restore the fitted local Z");
    auto rotatedSetting = root->bindings[0].sourceSetting(bone->getUUID());
    rotatedSetting.rotation = 1.0471976f;
    rotatedSetting.depthScale = 2.0f;
    rotatedSetting.depthOffset = 0.5f;
    root->bindings[0].setSourceSetting(rotatedSetting);
    bone->localTransform.translation.z = originalLocalZ;
    bone->localTransform.update();
    bone->transformChanged();
    float rotatedProposedLocalZ = std::numeric_limits<float>::quiet_NaN();
    float rotatedRootDepth = std::numeric_limits<float>::quiet_NaN();
    check(ngDepthRigNodeCurrentScaledDepth(bone, rotatedProposedLocalZ, rotatedRootDepth),
        "rotated fit fixture should expose its sampled depth");
    // rawDepth=0.5, scale=2, offset=0.5 gives perpendicular depth d=1.5.
    // At R=60 degrees the fitted line is (d*tan(R), d), while Z remains d.
    auto expectedRotatedRootDepth = baselineRootDepth * 3.0f;
    const bool rotatedDepthMatches = near(rotatedRootDepth, expectedRotatedRootDepth);
    require(rotatedDepthMatches, formatted(
        "Fit Z should preserve the perpendicular plane depth while tilting the source line; got %s expected %s",
        rotatedRootDepth, expectedRotatedRootDepth));
    history::incActionClearHistory();
    auto rotatedFitResult = ngCmd<DepthBoneCommand::FitDepthBoneZToDepth>(ctx, bone);
    check(rotatedFitResult->succeeded, "DepthBone Fit Z to Depth should accept a rotated BoneSource relationship");
    const bool rotatedLocalMatches = near(bone->localTransform.translation.z, rotatedProposedLocalZ);
    require(rotatedLocalMatches, formatted(
        "Fit Z should apply the local translation computed from the rotated sampled depth; got %s expected %s",
        bone->localTransform.translation.z, rotatedProposedLocalZ));
    // Fit Z locates the sample at x+d*tan(R), while its Z remains d.
    target->replaceDepths(Slice<float>{0.25f, 0.75f, 0.25f, 0.75f});
    auto worldScale = nijigenerate::viewport::depth::common::targetview::ngDepthDisplayScaleForTargetsInNodeSpace(
        root, Slice<std::shared_ptr<Deformable>>{target});
    auto expectedDistance = (0.75f * rotatedSetting.depthScale + rotatedSetting.depthOffset) * worldScale;
    auto expectedTangentSlope = std::sin(rotatedSetting.rotation) / std::cos(rotatedSetting.rotation);
    bone->localTransform.translation.x = 10.0f + expectedDistance * expectedTangentSlope;
    bone->localTransform.translation.y = -10.0f;
    bone->localTransform.translation.z = originalLocalZ;
    bone->localTransform.update();
    bone->transformChanged();
    float nearestProposedLocalZ = std::numeric_limits<float>::quiet_NaN();
    float nearestRootDepth = std::numeric_limits<float>::quiet_NaN();
    check(ngDepthRigNodeCurrentScaledDepth(bone, nearestProposedLocalZ, nearestRootDepth),
        "rotated Fit Z fixture should find a sample at its unchanged X position");
    auto expectedNearestRootDepth = expectedDistance;
    const bool nearestDepthMatches = near(nearestRootDepth, expectedNearestRootDepth);
    require(nearestDepthMatches, formatted(
        "Fit Z should choose x+d*tan(rotation) while preserving Z=d; got %s expected %s",
        nearestRootDepth, expectedNearestRootDepth));
    // Preserve the original dispatch identity check for two parameter keypoints.
    auto fitParam = ExParameter::createSourceDeferred(
        [] { return nicxlive::core::common::NullableString::fromStaticLiteral("FitDepthParam"); }, [] { return false; });
    fitParam->min = Vec2{0, 0};
    fitParam->max = Vec2{1, 0};
    fitParam->value = Vec2{1, 0};
    nijigenerate::project::incActivePuppet()->parameters.push_back(fitParam);
    auto fitYaw = newValueBinding(fitParam, bone, text("transform.r.y"));
    fitYaw->setValue(Vec2u{1, 0}, 0.35f);
    bone->localTransform.translation.z = originalLocalZ;
    siblingBone->localTransform.translation.z = siblingOriginalLocalZ;
    bone->localTransform.update();
    siblingBone->localTransform.update();
    bone->transformChanged();
    siblingBone->transformChanged();
    fakeDepthBoneGpuNextJobId = 1;
    fakeDepthBoneGpuJobVertexCounts = nullptr;
    fakeDepthBoneGpuSubmitCount = 0;
    fakeDepthBoneGpuPollCount = 0;
    fakeDepthBoneGpuSubmitFailAfter = 0;
    fakeDepthBoneGpuNotReadyPolls = 0;
    ngSetDepthBoneGpuAsyncTestHooks(&fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll);
    check(ngCmd<DepthBoneCommand::FitDepthRigRootZToDepth>(ctx, root)->succeeded,
        "dispatch-count Fit Z command should succeed");
    ngFlushDepthBoneDirtyImmediate();
    ngClearDepthBoneGpuAsyncTestHooks();
    const bool submittedTwice = fakeDepthBoneGpuSubmitCount == 2;
    require(submittedTwice, formatted(
        "Fit Z must submit each affected target/parameter/keypoint exactly once; submitted=%s", fakeDepthBoneGpuSubmitCount));
    auto fitDeform = std::dynamic_pointer_cast<DeformationParameterBinding>(
        fitParam->getBinding(target, nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
    check(fitDeform && fitDeform->getValue(Vec2u{0, 0}).vertexOffsets.size() == target->vertices.size() &&
        fitDeform->getValue(Vec2u{1, 0}).vertexOffsets.size() == target->vertices.size(),
        "Fit Z GPU submissions must write both distinct parameter keypoints without duplication");
    // Undo while the original derived work is pending must cancel execution and progress.
    history::incActionClearHistory();
    bone->localTransform.translation.z = originalLocalZ;
    siblingBone->localTransform.translation.z = siblingOriginalLocalZ;
    bone->localTransform.update();
    siblingBone->localTransform.update();
    bone->transformChanged();
    siblingBone->transformChanged();
    fakeDepthBoneGpuNotReadyPolls = 1000;
    fakeDepthBoneGpuCancelCount = 0;
    ngSetDepthBoneGpuAsyncTestHooks(
        &fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll, &fakeDepthBoneGpuCancel);
    check(ngCmd<DepthBoneCommand::FitDepthRigRootZToDepth>(ctx, root)->succeeded,
        "pending Fit Z cancellation fixture should start successfully");
    ngFlushDepthBoneDirty();
    auto fitOwner = std::dynamic_pointer_cast<AsyncGroupAction>(history::incActionTop());
    check(fitOwner && ngHasPendingDepthBoneRefreshForSink(fitOwner),
        "pending Fit Z derived work must belong to its async history action");
    bool foundRunningFitStatus = false;
    for (auto status : ngDepthBoneUpdateStatuses(nijigenerate::project::incActivePuppet(), true)) {
        if (status.root == root && status.target == target)
            foundRunningFitStatus = status.state == DepthBoneUpdateState::Queued ||
                status.state == DepthBoneUpdateState::Processing;
    }
    check(foundRunningFitStatus, "running Fit Z derived work should expose active generic progress");
    auto canceledFitToken = fitOwner->asyncToken();
    history::incActionUndo();
    check(!ngHasPendingDepthBoneRefreshForSink(fitOwner),
        "undoing Fit Z must remove its queued and running derived work");
    check(fakeDepthBoneGpuCancelCount > 0 && fakeDepthBoneGpuJobVertexCounts.size() == 0,
        "undoing Fit Z must cancel submitted backend jobs instead of only dropping frontend records");
    bool foundCanceledFitStatus = false;
    for (auto status : ngDepthBoneUpdateStatuses(nijigenerate::project::incActivePuppet(), true)) {
        if (status.root == root && status.target == target)
            foundCanceledFitStatus = status.state == DepthBoneUpdateState::Canceled;
    }
    check(foundCanceledFitStatus, "undoing Fit Z must retain only a terminal canceled diagnostic status");
    check(canceledFitToken.canceled(), "undoing Fit Z must invalidate the exact async generation token");
    history::incActionRedo();
    check(fitOwner->asyncToken().active() && canceledFitToken.canceled(),
        "redoing Fit Z must issue a fresh token without reviving canceled work");
    check(ngHasPendingDepthBoneRefreshForSink(fitOwner),
        "redoing Fit Z must schedule fresh derived work under the new token");
    history::incActionUndo();
    fakeDepthBoneGpuNotReadyPolls = 0;
    ngFlushDepthBoneDirtyImmediate();
    ngClearDepthBoneGpuAsyncTestHooks();
}
