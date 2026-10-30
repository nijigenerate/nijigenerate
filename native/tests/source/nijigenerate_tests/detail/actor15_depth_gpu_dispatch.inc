// Existing regression.d15658-16533. Include once inside detail after original GPU hooks.
void testDepthBoneGpuAllKeypointsDispatch() {
    using Text = nijigenerate::compat::RetainedString;
    using TextSlice = nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::math::ngVecaFromVectorsSource;
    using nicxlive::common::SharedArray;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Deformable;
    using nicxlive::core::param::ParameterBinding;
    using nicxlive::core::param::DeformationParameterBinding;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::actions::parameter::ParameterChangeBindingsValueAction;
    using nijigenerate::actions::parameter::ngCreateParameterBindingsActionDeferred;
    using nijigenerate::actions::binding::ParameterBindingRemoveAction;
    using nijigenerate::actions::binding::ngCreateBindingAddRemoveActionDeferred;
    using nijigenerate::actions::depthbone::DepthBoneRestChangeAction;
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
    const auto equalFloats = [](auto left, auto&& expectedProducer) {
        auto right = expectedProducer();
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) if (!(left[i] == right[i])) return false;
        return true;
    };
    resetCase();
    fakeDepthBoneGpuNextJobId = 1;
    fakeDepthBoneGpuJobVertexCounts = nullptr;
    fakeDepthBoneGpuSubmitCount = 0;
    fakeDepthBoneGpuPollCount = 0;
    fakeDepthBoneGpuSubmitFailAfter = 0;
    fakeDepthBoneGpuNotReadyPolls = 0;
    fakeDepthBoneGpuCancelCount = 0;
    fakeDepthBoneGpuLastSourceDepthOffset = 0.0f;
    fakeDepthBoneGpuLastBones = {};
    fakeDepthBoneGpuMutatingTarget = nullptr;
    fakeDepthBoneGpuMutateDepthOnReady = false;
    ngSetDepthBoneGpuAsyncTestHooks(
        &fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll, &fakeDepthBoneGpuCancel);
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        root->setName(text("gpu-all-keypoints-root").toNullableString());
        auto target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target->setName(text("gpu-all-keypoints-grid").toNullableString());
        target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
        target->replaceDepths(Slice<float>{0.0f, 0.25f, -0.5f, 1.0f});
        auto target2 = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target2->setName(text("gpu-all-keypoints-grid-2").toNullableString());
        target2->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-5, 0}, Vec2{5, 0}, Vec2{-5, 50}, Vec2{5, 50}}));
        target2->replaceDepths(Slice<float>{0.1f, 0.2f, 0.3f, 0.4f});
        Slice<std::shared_ptr<ExGridDeformer>> gpuTargets{target, target2};
        for (int i = 0; i < 10; ++i) {
            auto extraTarget = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
            extraTarget->setName(formatted("gpu-all-keypoints-grid-extra-%s", i).toNullableString());
            extraTarget->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
                Vec2{-5 - static_cast<float>(i), 0}, Vec2{5 + static_cast<float>(i), 0},
                Vec2{-5 - static_cast<float>(i), 50}, Vec2{5 + static_cast<float>(i), 50}}));
            extraTarget->replaceDepths(Slice<float>{0.05f * static_cast<float>(i),
                0.1f + 0.05f * static_cast<float>(i), 0.2f + 0.05f * static_cast<float>(i),
                0.3f + 0.05f * static_cast<float>(i)});
            gpuTargets.append(extraTarget);
        }
        auto bone = ngCreateDepthBone(root, text("GpuAllKeyBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        ExDepthRigBinding binding;
        ExDepthRigBinding binding2;
        Slice<ExDepthRigBinding> rigBindings;
        for (auto gpuTarget : gpuTargets) {
            ExDepthRigBinding gpuBinding;
            gpuBinding.targetUuid = gpuTarget->getUUID();
            gpuBinding.targetKind = ExDepthTargetKind::Grid;
            gpuBinding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
            gpuBinding.influenceRule.maxInfluences = 1;
            rigBindings.append(gpuBinding);
        }
        binding = rigBindings[0];
        binding2 = rigBindings[1];
        root->bindings = rigBindings;
        auto param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthGpuAllKeyParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        auto tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{1, 0}, 5.0f);
        ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, TextSlice(text("GPU all keypoints regression")),
            DepthBoneDirtyScope::AllKeypoints);
        auto detectedStatuses = ngDepthBoneUpdateStatuses(nijigenerate::project::incActivePuppet(), true);
        check(detectedStatuses.size() == gpuTargets.size(),
            "DepthBone invalidation should expose every affected target before dispatch");
        for (auto status : detectedStatuses) {
            check(status.state == DepthBoneUpdateState::Detected && status.expectedWork == 0 && status.appliedWork == 0,
                "DepthBone invalidation should expose a detected target before queue planning");
        }
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount >= gpuTargets.size() * 2 && fakeDepthBoneGpuPollCount >= gpuTargets.size() * 2,
            "GPU all-keypoints refresh should submit and poll every target binding across multiple frames");
        auto appliedStatuses = ngDepthBoneUpdateStatuses(nijigenerate::project::incActivePuppet(), true);
        check(appliedStatuses.size() == gpuTargets.size(),
            "DepthBone writeback should preserve one visual status per affected target");
        for (auto status : appliedStatuses) {
            check(status.state == DepthBoneUpdateState::Applied && status.expectedWork == 2 && status.appliedWork == 2 &&
                status.queuedWork == 0 && status.processingWork == 0,
                "DepthBone target should become applied only after both keypoints are written back");
        }
        std::shared_ptr<DeformationParameterBinding> deformBinding;
        std::shared_ptr<DeformationParameterBinding> deformBinding2;
        Vec2Array offsets;
        Vec2Array offsets2;
        for (std::size_t i = 0; i < gpuTargets.size(); ++i) {
            auto gpuTarget = gpuTargets[i];
            auto targetBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
                param->getBinding(gpuTarget, NullableString::fromStaticLiteral("deform")));
            const bool hasBinding = targetBinding != nullptr;
            require(hasBinding, formatted("GPU all-keypoints refresh should create a deform binding for target %s", i));
            auto targetOffsets = targetBinding->getValue(Vec2u{1, 0}).vertexOffsets;
            const bool countMatches = targetOffsets.size() == gpuTarget->vertices.size();
            require(countMatches, formatted("GPU all-keypoints refresh should write one offset per vertex for target %s", i));
            for (auto offset : targetOffsets) {
                const bool offsetMatches = nearVec2(offset, Vec2{2, -1});
                require(offsetMatches, formatted("GPU all-keypoints refresh should write fake GPU readback offsets for target %s", i));
            }
            const bool deformationCountMatches = gpuTarget->deformation.size() == gpuTarget->vertices.size();
            require(deformationCountMatches, formatted(
                "GPU all-keypoints refresh should update visual deformation for target %s", i));
            if (i == 0) { deformBinding = targetBinding; offsets = targetOffsets; }
            else if (i == 1) { deformBinding2 = targetBinding; offsets2 = targetOffsets; }
        }
        check(!ngHasPendingDepthBoneRefresh(), "generated DepthBone deform writeback must not recursively dirty itself");
        auto scopedChild = ExGridDeformer::createSourceDeferred([&] { return target; });
        scopedChild->setName(text("gpu-source-scope-dependent-child").toNullableString());
        scopedChild->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-4, 0}, Vec2{4, 0}, Vec2{-4, 40}, Vec2{4, 40}}));
        auto scopedChildBinding = root->bindings[0];
        scopedChildBinding.targetUuid = scopedChild->getUUID();
        root->bindings.append(scopedChildBinding);
        nijigenerate::project::incActivePuppet()->rescanNodes();
        auto scopedSourceCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        scopedSourceCtx->puppet(nijigenerate::project::incActivePuppet());
        scopedSourceCtx->armedParameters(Context::Parameters{param});
        auto submitCountBeforeScopedSource = fakeDepthBoneGpuSubmitCount;
        check(ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(scopedSourceCtx, root, target, bone,
            text("{\"weight\":0.9,\"depthOffset\":0.25,\"depthScale\":1.0,\"rotation\":0.0}"))->succeeded,
            "target-scoped BoneSource settings command should succeed");
        auto scopedDetectedStatuses = ngDepthBoneUpdateStatuses(nijigenerate::project::incActivePuppet(), true);
        check(scopedDetectedStatuses.size() == 2,
            "editing one BoneSource must expose its target and dependent child only");
        bool detectedScopedTarget = false;
        bool detectedScopedChild = false;
        for (auto status : scopedDetectedStatuses) {
            check(status.state == DepthBoneUpdateState::Detected,
                "target-scoped BoneSource status must remain detected before dispatch");
            if (status.target == target) detectedScopedTarget = true;
            else if (status.target == scopedChild) detectedScopedChild = true;
            else check(false, "editing one BoneSource must not expose unrelated sibling targets");
        }
        check(detectedScopedTarget && detectedScopedChild,
            "target-scoped BoneSource status must retain its hierarchy-dependent child");
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount - submitCountBeforeScopedSource == 4,
            "editing one BoneSource must dispatch only its two-target dependency branch across two keypoints");
        auto scopedAppliedStatuses = ngDepthBoneUpdateStatuses(nijigenerate::project::incActivePuppet(), true);
        check(scopedAppliedStatuses.size() == 2,
            "target-scoped BoneSource writeback must leave no unrelated viewport status");
        for (auto status : scopedAppliedStatuses) {
            check((status.target == target || status.target == scopedChild) && status.state == DepthBoneUpdateState::Applied &&
                status.expectedWork == 2 && status.appliedWork == 2,
                "only the affected BoneSource dependency branch may become applied");
        }
        root->bindings = root->bindings.slice(0, root->bindings.size() - 1);
        auto transformAction = ngCreateParameterBindingsActionDeferred<ParameterChangeBindingsValueAction>([&] {
            auto name = TextSlice(text("DepthBone transform invalidation fixture"));
            auto parameter = param;
            auto bindings = nijigenerate::actions::parameter::Bindings{tx};
            return std::tuple{name, parameter, bindings, 1, 0};
        });
        tx->setValue(Vec2u{1, 0}, 7.0f);
        transformAction->updateNewState();
        check(ngHasPendingDepthBoneRefresh(), "a transform binding action must enqueue its DepthBone keypoint");
        auto submitCountBeforeTransform = fakeDepthBoneGpuSubmitCount;
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount - submitCountBeforeTransform == gpuTargets.size(),
            "a transform binding action must refresh only its changed keypoint");
        transformAction->rollback();
        check(ngHasPendingDepthBoneRefresh(), "undoing a transform binding action must enqueue the same DepthBone keypoint");
        ngFlushDepthBoneDirtyImmediate();
        transformAction->redo();
        check(ngHasPendingDepthBoneRefresh(), "redoing a transform binding action must enqueue the same DepthBone keypoint");
        ngFlushDepthBoneDirtyImmediate();
        auto targetTranslation = newValueBinding(param, target, text("transform.t.x"));
        auto targetTransformAction = ngCreateParameterBindingsActionDeferred<ParameterChangeBindingsValueAction>([&] {
            auto name = TextSlice(text("GridDeformer transform invalidation fixture"));
            auto parameter = param;
            auto bindings = nijigenerate::actions::parameter::Bindings{targetTranslation};
            return std::tuple{name, parameter, bindings, 1, 0};
        });
        targetTranslation->setValue(Vec2u{1, 0}, 3.0f);
        targetTransformAction->updateNewState();
        check(ngHasPendingDepthBoneRefresh(),
            "setting a BoneSource GridDeformer transform must enqueue its DepthBone hierarchy");
        auto submitCountBeforeTargetTransform = fakeDepthBoneGpuSubmitCount;
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount - submitCountBeforeTargetTransform == gpuTargets.size(),
            "a GridDeformer transform binding action must refresh only its changed keypoint");
        targetTransformAction->rollback();
        check(ngHasPendingDepthBoneRefresh(),
            "undoing a GridDeformer transform binding must enqueue its DepthBone hierarchy");
        ngFlushDepthBoneDirtyImmediate();
        targetTransformAction->redo();
        check(ngHasPendingDepthBoneRefresh(),
            "redoing a GridDeformer transform binding must enqueue its DepthBone hierarchy");
        ngFlushDepthBoneDirtyImmediate();
        param->removeBinding(targetTranslation);
        auto targetTransformRemoveAction = ngCreateBindingAddRemoveActionDeferred<ParameterBindingRemoveAction>([&] {
            return std::tuple{param, targetTranslation};
        });
        check(ngHasPendingDepthBoneRefresh(),
            "removing a GridDeformer transform binding must enqueue its DepthBone hierarchy");
        auto submitCountBeforeTargetBindingRemoval = fakeDepthBoneGpuSubmitCount;
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount - submitCountBeforeTargetBindingRemoval == gpuTargets.size() * 2,
            "a transform binding structure change must refresh every affected keypoint");
        targetTransformRemoveAction->rollback();
        check(ngHasPendingDepthBoneRefresh(),
            "undoing a transform binding removal must enqueue its DepthBone hierarchy");
        ngFlushDepthBoneDirtyImmediate();
        targetTransformRemoveAction->redo();
        check(ngHasPendingDepthBoneRefresh(),
            "redoing a transform binding removal must enqueue its DepthBone hierarchy");
        ngFlushDepthBoneDirtyImmediate();
        auto oldRestHead = bone->restHead;
        auto oldRestTail = bone->restTail;
        auto oldRestRoll = bone->restRoll;
        auto newRestHead = oldRestHead + Vec3{1, 0, 0};
        bone->restHead = newRestHead;
        auto restAction = std::allocate_shared<DepthBoneRestChangeAction>(SourceAllocator<DepthBoneRestChangeAction>{},
            bone, oldRestHead, oldRestTail, oldRestRoll, newRestHead, oldRestTail, oldRestRoll);
        check(ngHasPendingDepthBoneRefresh(), "changing a DepthBone rest pose must enqueue generated targets");
        auto submitCountBeforeRest = fakeDepthBoneGpuSubmitCount;
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount - submitCountBeforeRest == gpuTargets.size() * 2,
            "a rest-pose change must refresh every keypoint exactly once");
        restAction->rollback();
        check(ngHasPendingDepthBoneRefresh(), "undoing a DepthBone rest change must enqueue generated targets");
        ngFlushDepthBoneDirtyImmediate();
        restAction->redo();
        check(ngHasPendingDepthBoneRefresh(), "redoing a DepthBone rest change must enqueue generated targets");
        ngFlushDepthBoneDirtyImmediate();
        auto nestedTarget = ExGridDeformer::createSourceDeferred([&] { return target; });
        nestedTarget->setName(text("gpu-binding-invalidation-nested-grid").toNullableString());
        nestedTarget->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-4, 0}, Vec2{4, 0}, Vec2{-4, 40}, Vec2{4, 40}}));
        ExDepthRigBinding nestedBinding = binding;
        nestedBinding.targetUuid = nestedTarget->getUUID();
        root->bindings.append(nestedBinding);
        nijigenerate::project::incActivePuppet()->rescanNodes();
        auto ancestorDeformAction = ngCreateParameterBindingsActionDeferred<ParameterChangeBindingsValueAction>([&] {
            auto name = TextSlice(text("Ancestor Grid deformation invalidation fixture"));
            auto parameter = param;
            auto bindings = nijigenerate::actions::parameter::Bindings{deformBinding};
            return std::tuple{name, parameter, bindings, 1, 0};
        });
        deformBinding->update(Vec2u{1, 0}, ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{1, 0}, Vec2{1, 0}, Vec2{1, 0}, Vec2{1, 0}}));
        ancestorDeformAction->updateNewState();
        check(ngHasPendingDepthBoneRefresh(),
            "editing an ancestor GridDeformer binding must enqueue nested DepthBone targets");
        auto submitCountBeforeAncestorDeform = fakeDepthBoneGpuSubmitCount;
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount - submitCountBeforeAncestorDeform == root->bindings.size(),
            "an ancestor GridDeformer binding edit must refresh one hierarchy batch at the changed keypoint");
        check(!ngHasPendingDepthBoneRefresh(), "DepthBone-generated hierarchy writeback must not feed back into invalidation");
        ancestorDeformAction->rollback();
        check(ngHasPendingDepthBoneRefresh(),
            "undoing an ancestor GridDeformer edit must enqueue nested DepthBone targets");
        ngFlushDepthBoneDirtyImmediate();
        ancestorDeformAction->redo();
        check(ngHasPendingDepthBoneRefresh(),
            "redoing an ancestor GridDeformer edit must enqueue nested DepthBone targets");
        ngFlushDepthBoneDirtyImmediate();
        root->bindings = root->bindings.slice(0, root->bindings.size() - 1);
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(nijigenerate::project::incActivePuppet());
        ctx->armedParameters(Context::Parameters{param});
        target->deformation.fill(Vec2{0, 0});
        target2->deformation.fill(Vec2{0, 0});
        check(ngCmd<DepthBoneCommand::PreviewDepthBoneDeform>(ctx, root, Context::Nodes{target, target2})->succeeded,
            "GPU preview command should succeed with fake GPU hooks");
        ngFlushDepthBoneDirtyImmediate();
        for (auto offset : target->deformation)
            check(nearVec2(offset, Vec2{2, -1}), "GPU preview command should write fake GPU readback offsets");
        for (auto offset : target2->deformation)
            check(nearVec2(offset, Vec2{2, -1}), "GPU preview command should write fake GPU readback offsets for the second target");
        ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, TextSlice(text("GPU keypoint regression")), DepthBoneDirtyScope::Keypoint);
        ngFlushDepthBoneDirtyImmediate();
        auto savedGpuRigBindings = root->bindings;
        root->bindings = root->bindings.slice(0, 1);
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuNotReadyPolls = 4;
        fakeDepthBoneGpuLastBones = {};
        tx->setValue(Vec2u{1, 0}, 6.0f);
        ngNotifyDepthBoneBindingValueChanged(tx, Vec2u{1, 0});
        ngFlushDepthBoneDirty();
        const bool initialSubmitted = fakeDepthBoneGpuSubmitCount == 1;
        require(initialSubmitted, formatted(
            "DepthBone transform refresh should submit the initial keypoint pose; submitted=%s", fakeDepthBoneGpuSubmitCount));
        auto initialDepthBonePose = fakeDepthBoneGpuLastBones.duplicate();
        tx->setValue(Vec2u{1, 0}, 12.0f);
        ngNotifyDepthBoneBindingValueChanged(tx, Vec2u{1, 0});
        ngFlushDepthBoneDirty();
        DepthBoneGpuOffsetPacket latestDepthBonePacket;
        TextSlice latestDepthBonePacketError;
        auto latestBindingOwner = root->bindings;
        auto* latestBinding = &latestBindingOwner[0];
        const bool latestBuilt = ngBuildDepthBoneGpuOffsetPacket(
            root, latestBinding, target, param, Vec2u{1, 0}, latestDepthBonePacket, latestDepthBonePacketError);
        require(latestBuilt, concatenateSourceStrings({
            text("latest DepthBone transform packet should build: "), latestDepthBonePacketError.retainedString()}));
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount == 2,
            "newer DepthBone transform should replace the in-flight keypoint batch without a stale retry");
        check(!equalFloats(fakeDepthBoneGpuLastBones, [&] { return initialDepthBonePose; }),
            "DepthBone transform fixture should produce a different bone matrix for the newer pose");
        check(equalFloats(fakeDepthBoneGpuLastBones, [&] { return latestDepthBonePacket.bones; }),
            "DepthBone transform refresh should dispatch the latest keypoint pose");
        check(!ngHasPendingDepthBoneRefresh(), "superseded DepthBone transform refresh should drain without stale work");
        fakeDepthBoneGpuNotReadyPolls = 0;
        root->bindings = savedGpuRigBindings;
        history::incActionClearHistory();
        auto applyResult = ngCmd<DepthBoneCommand::ApplyDepthBoneDeform>(ctx, root, Context::Nodes{target});
        check(applyResult->succeeded, "GPU apply command should succeed with fake GPU hooks");
        ngFlushDepthBoneDirtyImmediate();
        check(history::incActionHistory().size() == 1, "GPU apply command should push one grouped undo action");
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        check(deformBinding != nullptr, "GPU apply command should keep a deform binding");
        offsets = deformBinding->getValue(Vec2u{1, 0}).vertexOffsets;
        for (auto offset : offsets)
            check(nearVec2(offset, Vec2{2, -1}), "GPU apply command should store fake GPU readback offsets");
        history::incActionUndo();
        history::incActionRedo();
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        check(deformBinding && deformBinding->getValue(Vec2u{1, 0}).vertexOffsets.size() == target->vertices.size(),
            "GPU apply command undo/redo should preserve binding shape");
        namespace paths = nijigenerate::compat::phobos;
        const std::array fixtureSegments{text("out"), text("regression-temp"),
            text("nijigenerate-regression-depthbone-gpu")};
        auto fixtureDir = paths::ngBuildPath(fixtureSegments);
        paths::ngMkdirRecurse(fixtureDir.view());
        const std::array saveSegments{fixtureDir, text("gpu-writeback")};
        auto saveBase = paths::ngBuildPath(saveSegments);
        auto savePath = concatenateSourceStrings({saveBase, text(".inx")});
        if (paths::ngFileExists(savePath.view()) && paths::ngIsFile(savePath.view()))
            nijigenerate::platform::ngAutosaveRemove(TextSlice(savePath));
        nijigenerate::project::incActivePuppet()->populateTextureSlots();
        auto savePuppet = nijigenerate::project::incActivePuppet();
        nijigenerate::io::ngWriteProjectPuppetFile(*savePuppet, TextSlice(savePath));
        check(paths::ngFileExists(savePath.view()) && paths::ngIsFile(savePath.view()),
            "GPU writeback fixture should create an INX file");
        auto loadedPuppet = nijigenerate::io::project_reader::ngLoadProjectPuppet<nijigenerate::ext::ExPuppet>(savePath);
        auto loadedTarget = std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(
            findNodeRecursive(loadedPuppet->root, "gpu-all-keypoints-grid"));
        auto loadedParam = findParameter(loadedPuppet, "DepthGpuAllKeyParam");
        check(loadedTarget && loadedParam, "GPU writeback fixture should load target and parameter");
        auto loadedBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            loadedParam->getBinding(loadedTarget, NullableString::fromStaticLiteral("deform")));
        check(loadedBinding != nullptr, "GPU writeback fixture should persist deform binding");
        auto loadedOffsets = loadedBinding->getValue(Vec2u{1, 0}).vertexOffsets;
        check(loadedOffsets.size() == loadedTarget->vertices.size(), "GPU writeback fixture should persist offset count");
        resetCase();
        fakeDepthBoneGpuNextJobId = 1;
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuCancelCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 100;
        root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        root->setName(text("gpu-queued-stale-root").toNullableString());
        bone = ngCreateDepthBone(root, text("GpuQueuedStaleBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        Slice<std::shared_ptr<ExGridDeformer>> queuedTargets;
        Slice<ExDepthRigBinding> queuedBindings;
        for (int i = 0; i < 9; ++i) {
            auto queuedTarget = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
            queuedTarget->setName(formatted("gpu-queued-stale-grid-%s", i).toNullableString());
            queuedTarget->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
                Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
            ExDepthRigBinding queuedBinding;
            queuedBinding.targetUuid = queuedTarget->getUUID();
            queuedBinding.targetKind = ExDepthTargetKind::Grid;
            queuedBinding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
            queuedBindings.append(queuedBinding);
            queuedTargets.append(queuedTarget);
        }
        root->bindings = queuedBindings;
        param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthGpuQueuedStaleParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{1, 0}, 5.0f);
        ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, TextSlice(text("GPU queued stale owner regression")),
            DepthBoneDirtyScope::Keypoint);
        auto oldRootName = root->getName();
        root->setName(text("gpu-queued-stale-root-renamed").toNullableString());
        using NameAction = nijigenerate::actions::node::NodeValueChangeAction<Node, NullableString>;
        auto renameAction = nijigenerate::actions::node::detail::createNodeAction<NameAction>([&](NameAction* storage) {
            auto label = text("name");
            std::shared_ptr<Node> receiver = root;
            auto oldValue = oldRootName;
            auto newValue = root->getName();
            auto field = std::shared_ptr<NullableString>(root, &root->name);
            ::new (static_cast<void*>(storage)) NameAction(label, receiver, oldValue, newValue, field);
        });
        history::incActionPush(renameAction);
        auto queuedHistory = history::incActionHistory();
        auto queuedOwner = std::dynamic_pointer_cast<nijigenerate::actions::AsyncGroupAction>(
            queuedHistory[queuedHistory.size() - 1]);
        check(queuedOwner && queuedOwner->pendingAsyncCount() == 1,
            "queued stale fixture should attach one pending refresh to its initiating action");
        ngFlushDepthBoneDirty();
        check(fakeDepthBoneGpuSubmitCount == 8 && ngHasPendingDepthBoneRefresh(),
            "queued stale fixture should leave the ninth target waiting for GPU submission");
        queuedTargets[queuedTargets.size() - 1]->setParent(nullptr);
        nijigenerate::project::incActivePuppet()->rescanNodes();
        ngFlushDepthBoneDirtyImmediate();
        check(queuedOwner->pendingAsyncCount() == 0 && queuedOwner->state() == nijigenerate::actions::AsyncGroupActionState::Completed,
            "discarding a stale queued batch must settle its initiating action");
        check(fakeDepthBoneGpuCancelCount == 8 && !ngHasPendingDepthBoneRefresh(),
            "discarding a stale queued batch must cancel submitted siblings and drain the queue");
        resetCase();
        fakeDepthBoneGpuNextJobId = 1;
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 1;
        root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        root->setName(text("gpu-stale-root").toNullableString());
        target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target->setName(text("gpu-stale-grid").toNullableString());
        target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
        target2 = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target2->setName(text("gpu-stale-grid-2").toNullableString());
        target2->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-20, 0}, Vec2{20, 0}, Vec2{-20, 100}, Vec2{20, 100}}));
        bone = ngCreateDepthBone(root, text("GpuStaleBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        binding = ExDepthRigBinding{};
        binding.targetUuid = target->getUUID();
        binding.targetKind = ExDepthTargetKind::Grid;
        binding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
        binding2 = binding;
        binding2.targetUuid = target2->getUUID();
        root->bindings = Slice<ExDepthRigBinding>{binding, binding2};
        param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthGpuStaleParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{1, 0}, 5.0f);
        ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, TextSlice(text("GPU stale regression")), DepthBoneDirtyScope::Keypoint);
        ngFlushDepthBoneDirty();
        target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-12, 0}, Vec2{12, 0}, Vec2{-12, 100}, Vec2{12, 100}}));
        ngFlushDepthBoneDirtyImmediate();
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        check(deformBinding != nullptr, "GPU stale job should requeue and complete against the current target");
        offsets = deformBinding->getValue(Vec2u{1, 0}).vertexOffsets;
        check(offsets.size() == target->vertices.size(),
            "GPU stale requeue should write offsets for the current target vertex count");
        deformBinding2 = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target2, NullableString::fromStaticLiteral("deform")));
        check(deformBinding2 && deformBinding2->getValue(Vec2u{1, 0}).vertexOffsets.size() == target2->vertices.size(),
            "GPU stale requeue must regenerate and retain every target in the original hierarchy batch");
        check(fakeDepthBoneGpuSubmitCount == 4, "a stale two-target batch should resubmit both targets exactly once");
        resetCase();
        fakeDepthBoneGpuNextJobId = 1;
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuCancelCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 1;
        root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        root->setName(text("gpu-detached-root").toNullableString());
        target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target->setName(text("gpu-detached-grid").toNullableString());
        target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
        bone = ngCreateDepthBone(root, text("GpuDetachedBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        binding = ExDepthRigBinding{};
        binding.targetUuid = target->getUUID();
        binding.targetKind = ExDepthTargetKind::Grid;
        binding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
        root->bindings = Slice<ExDepthRigBinding>{binding};
        param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthGpuDetachedParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{1, 0}, 5.0f);
        ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, TextSlice(text("GPU detached target regression")),
            DepthBoneDirtyScope::Keypoint);
        ngFlushDepthBoneDirty();
        check(fakeDepthBoneGpuSubmitCount == 1 && fakeDepthBoneGpuJobVertexCounts.size() == 1,
            "detached-target fixture should leave one GPU job pending before the target is removed");
        target->setParent(nullptr);
        nijigenerate::project::incActivePuppet()->rescanNodes();
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount == 1, "a detached GPU target must not be requeued");
        check(fakeDepthBoneGpuCancelCount > 0 && fakeDepthBoneGpuJobVertexCounts.size() == 0,
            "a pending GPU job must be canceled when its target leaves the active puppet");
        check(param->getBinding(target, NullableString::fromStaticLiteral("deform")) == nullptr,
            "a detached GPU target must not receive deferred deform writeback");
        check(!ngHasPendingDepthBoneRefresh(), "detached GPU work must drain without leaving a pending refresh");
        resetCase();
        fakeDepthBoneGpuNextJobId = 1;
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
        root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        root->setName(text("gpu-stale-limit-root").toNullableString());
        target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target->setName(text("gpu-stale-limit-grid").toNullableString());
        target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
        target->replaceDepths(Slice<float>{0, 0.25f, 0.5f, 0.75f});
        bone = ngCreateDepthBone(root, text("GpuStaleLimitBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        binding = ExDepthRigBinding{};
        binding.targetUuid = target->getUUID();
        binding.targetKind = ExDepthTargetKind::Grid;
        binding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
        root->bindings = Slice<ExDepthRigBinding>{binding};
        param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthGpuStaleLimitParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{1, 0}, 5.0f);
        fakeDepthBoneGpuMutatingTarget = target;
        fakeDepthBoneGpuMutateDepthOnReady = true;
        ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, TextSlice(text("GPU stale retry limit regression")),
            DepthBoneDirtyScope::Keypoint);
        ngFlushDepthBoneDirtyImmediate();
        fakeDepthBoneGpuMutateDepthOnReady = false;
        fakeDepthBoneGpuMutatingTarget = nullptr;
        check(fakeDepthBoneGpuSubmitCount == DepthBoneGpuMaxStaleRetries + 1,
            "continuously stale GPU work should stop at the bounded retry limit");
        check(!ngHasPendingDepthBoneRefresh(), "continuously stale GPU work should drain instead of requeueing forever");
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        check(deformBinding == nullptr,
            "a continuously stale GPU job must not write a result from an obsolete depth snapshot");
        resetCase();
        fakeDepthBoneGpuNextJobId = 1;
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 1;
        root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        root->setName(text("gpu-writeback-transform-root").toNullableString());
        target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target->setName(text("gpu-writeback-transform-grid").toNullableString());
        target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
        target->replaceDepths(Slice<float>{0.25f, 0.5f, 0.75f, 1});
        auto scaleDriverTarget = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        scaleDriverTarget->setName(text("gpu-writeback-scale-driver-grid").toNullableString());
        scaleDriverTarget->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-5, 0}, Vec2{5, 0}, Vec2{-5, 50}, Vec2{5, 50}}));
        bone = ngCreateDepthBone(root, text("GpuWritebackTransformBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        binding = ExDepthRigBinding{};
        binding.targetUuid = target->getUUID();
        binding.targetKind = ExDepthTargetKind::Grid;
        binding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
        binding2 = ExDepthRigBinding{};
        binding2.targetUuid = scaleDriverTarget->getUUID();
        binding2.targetKind = ExDepthTargetKind::Grid;
        root->bindings = Slice<ExDepthRigBinding>{binding, binding2};
        param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthGpuWritebackTransformParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{1, 0}, 5.0f);
        auto scaleBeforePendingTransform = nijigenerate::viewport::depth::common::targetview::ngDepthDisplayScaleForTargetsInNodeSpace(
            root, Slice<std::shared_ptr<Deformable>>{target, scaleDriverTarget});
        ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, TextSlice(text("GPU writeback transform regression")),
            DepthBoneDirtyScope::Keypoint);
        ngFlushDepthBoneDirty();
        scaleDriverTarget->localTransform.scale = Vec2{20, 20};
        scaleDriverTarget->localTransform.update();
        scaleDriverTarget->transformChanged();
        auto scaleAfterPendingTransform = nijigenerate::viewport::depth::common::targetview::ngDepthDisplayScaleForTargetsInNodeSpace(
            root, Slice<std::shared_ptr<Deformable>>{target, scaleDriverTarget});
        check(!near(scaleBeforePendingTransform, scaleAfterPendingTransform),
            "GPU world-scale regression fixture should change display scale while a job is pending");
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount == 1,
            "GPU writeback should not requeue because pose-dependent world scale changed while pending");
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        check(deformBinding != nullptr, "GPU writeback should complete after a pending target transform change");
        offsets = deformBinding->getValue(Vec2u{1, 0}).vertexOffsets;
        check(offsets.size() == target->vertices.size(),
            "GPU writeback transform change should still write offsets for the current target vertex count");
        auto transformCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        transformCtx->puppet(nijigenerate::project::incActivePuppet());
        transformCtx->armedParameters(Context::Parameters{param});
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuNotReadyPolls = 1;
        auto sourceSettingsResult = ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(transformCtx, root, target, bone,
            text("{\"weight\":0.8,\"depthOffset\":1.5,\"depthScale\":0.65,\"rotation\":0.15}"));
        check(sourceSettingsResult->succeeded, "BoneSource offset/scale command should succeed");
        ngFlushDepthBoneDirty();
        scaleDriverTarget->localTransform.scale = Vec2{30, 30};
        scaleDriverTarget->localTransform.update();
        scaleDriverTarget->transformChanged();
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount == 2,
            "BoneSource offset/scale refresh should complete both keypoints without world-scale stale retries");
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
        for (auto depthOffset : Slice<float>{1.6f, 1.7f, 1.8f, 1.9f, 2.0f}) {
            auto settings = formatted("{\"weight\":0.8,\"depthOffset\":%s,\"depthScale\":0.65,\"rotation\":%s}",
                depthOffset, depthOffset / 10.0f);
            sourceSettingsResult = ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(
                transformCtx, root, target, bone, settings);
            check(sourceSettingsResult->succeeded, "rapid BoneSource offset command should succeed");
            ngFlushDepthBoneDirty();
            const bool noObsoleteSubmission = fakeDepthBoneGpuSubmitCount == 0;
            auto submissionMessage = formatted("depthOffset=%s submitted=%s", depthOffset, fakeDepthBoneGpuSubmitCount);
            require(noObsoleteSubmission, concatenateSourceStrings({
                text("rapid BoneSource edits should settle before dispatching obsolete all-keypoint GPU work; "),
                submissionMessage}));
        }
        auto rapidSourceWorldScale = nijigenerate::viewport::depth::common::targetview::ngDepthDisplayScaleForTargetsInNodeSpace(
            root, Slice<std::shared_ptr<Deformable>>{target, scaleDriverTarget});
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount == 2,
            "rapid BoneSource edits should dispatch only the final value for both keypoints");
        check(near(fakeDepthBoneGpuLastSourceDepthOffset, 2.0f * rapidSourceWorldScale),
            "rapid BoneSource refresh should dispatch the final depthOffset");
        check(near(fakeDepthBoneGpuLastSourceSinRotation, std::sin(0.2f)) &&
            near(fakeDepthBoneGpuLastSourceCosRotation, std::cos(0.2f)),
            "rapid BoneSource refresh should dispatch only the final rotation");
        check(!ngHasPendingDepthBoneRefresh(), "rapid BoneSource refresh should drain without leaving stale work");
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuNotReadyPolls = 4;
        sourceSettingsResult = ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(transformCtx, root, target, bone,
            text("{\"weight\":0.8,\"depthOffset\":2.1,\"depthScale\":0.65,\"rotation\":0.21}"));
        check(sourceSettingsResult->succeeded, "pending BoneSource offset fixture should accept its initial value");
        ngFlushDepthBoneDirty();
        ngFlushDepthBoneDirty();
        check(fakeDepthBoneGpuSubmitCount == 2, "pending BoneSource offset fixture should submit its first two keypoints");
        sourceSettingsResult = ngCmd<DepthBoneCommand::SetDepthBoneSourceSettings>(transformCtx, root, target, bone,
            text("{\"weight\":0.8,\"depthOffset\":2.2,\"depthScale\":0.65,\"rotation\":0.22}"));
        check(sourceSettingsResult->succeeded, "newer BoneSource offset should supersede in-flight work");
        ngFlushDepthBoneDirty();
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount == 4,
            "superseding an in-flight BoneSource refresh should submit only one replacement batch");
        check(near(fakeDepthBoneGpuLastSourceDepthOffset, 2.2f * rapidSourceWorldScale),
            "replacement BoneSource refresh should dispatch the latest depthOffset");
        check(near(fakeDepthBoneGpuLastSourceSinRotation, std::sin(0.22f)) &&
            near(fakeDepthBoneGpuLastSourceCosRotation, std::cos(0.22f)),
            "replacement BoneSource refresh should dispatch the latest rotation");
        check(!ngHasPendingDepthBoneRefresh(), "superseded BoneSource refresh should drain without stale retries");
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuNotReadyPolls = 1;
        auto setDepthsResult = ngCmd<nijigenerate::commands::depth::map::DepthMapCommand::SetDepths>(
            transformCtx, target, Slice<float>{0.1f, 0.3f, 0.6f, 0.9f});
        check(setDepthsResult->succeeded, "DepthEdit SetDepths command should succeed");
        ngFlushDepthBoneDirty();
        scaleDriverTarget->localTransform.scale = Vec2{40, 40};
        scaleDriverTarget->localTransform.update();
        scaleDriverTarget->transformChanged();
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount == 2,
            "DepthEdit depth refresh should complete both keypoints without world-scale stale retries");
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuNotReadyPolls = 1;
        auto forceCursor = param->findClosestKeypoint();
        ngMarkDepthBoneDirty(root, param, forceCursor, TextSlice(text("Force Depth Bone Refresh regression")),
            DepthBoneDirtyScope::AllKeypoints);
        ngFlushDepthBoneDirty();
        scaleDriverTarget->localTransform.scale = Vec2{50, 50};
        scaleDriverTarget->localTransform.update();
        scaleDriverTarget->transformChanged();
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount == 2,
            "inspector force refresh should complete both keypoints without world-scale stale retries");
        resetCase();
        fakeDepthBoneGpuNextJobId = 1;
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 1;
        fakeDepthBoneGpuNotReadyPolls = 0;
        root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        root->setName(text("gpu-submit-failure-root").toNullableString());
        target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target->setName(text("gpu-submit-failure-grid-1").toNullableString());
        target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
        target2 = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target2->setName(text("gpu-submit-failure-grid-2").toNullableString());
        target2->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
        bone = ngCreateDepthBone(root, text("GpuSubmitFailureBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        binding = ExDepthRigBinding{};
        binding.targetUuid = target->getUUID();
        binding.targetKind = ExDepthTargetKind::Grid;
        binding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
        binding2 = binding;
        binding2.targetUuid = target2->getUUID();
        root->bindings = Slice<ExDepthRigBinding>{binding, binding2};
        param = new2DParameter(text("DepthGpuSubmitFailureParam"));
        param->value = Vec2{1, 1};
        tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{2, 2}, 5.0f);
        auto submitFailureOwner = std::allocate_shared<nijigenerate::actions::AsyncGroupAction>(
            SourceAllocator<nijigenerate::actions::AsyncGroupAction>{});
        ngBeginDepthBoneRefreshActionSink(submitFailureOwner);
        ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, TextSlice(text("GPU submit failure regression")),
            DepthBoneDirtyScope::AllKeypoints);
        ngEndDepthBoneRefreshActionSink(submitFailureOwner);
        check(submitFailureOwner->pendingAsyncCount() == 1,
            "GPU submit failure regression must begin with one owned all-keypoint producer");
        bool submitFailureThrown = false;
        try { ngFlushDepthBoneDirtyImmediate(); }
        catch (const nijigenerate::compat::ThrowableException& error) {
            submitFailureThrown = error.msg.view().find("Depth Bone GPU async submit failed") != std::string_view::npos;
        }
        check(submitFailureThrown, "GPU submit failure must raise a fatal refresh error");
        check(fakeDepthBoneGpuSubmitCount >= 2, "GPU submit failure fixture should attempt both target submissions");
        check(submitFailureOwner->pendingAsyncCount() == 0 &&
            submitFailureOwner->state() == nijigenerate::actions::AsyncGroupActionState::Failed &&
            !ngHasPendingDepthBoneRefreshForSink(submitFailureOwner),
            "GPU abort must cancel upstream producers and settle their owning action");
        auto submitCountAfterAbort = fakeDepthBoneGpuSubmitCount;
        ngFlushDepthBoneDirtyImmediate();
        check(fakeDepthBoneGpuSubmitCount == submitCountAfterAbort,
            "GPU abort must not let a retained all-keypoint producer resubmit failed work");
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        deformBinding2 = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target2, NullableString::fromStaticLiteral("deform")));
        check(!deformBinding && !deformBinding2, "GPU submit failure must not write partial deform bindings before raising");
        resetCase();
        fakeDepthBoneGpuNextJobId = 1;
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
        root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        root->setName(text("gpu-packet-failure-root").toNullableString());
        target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        target->setName(text("gpu-packet-failure-grid").toNullableString());
        target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
        bone = ngCreateDepthBone(root, text("GpuPacketFailureBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        binding = ExDepthRigBinding{};
        binding.targetUuid = target->getUUID();
        binding.targetKind = ExDepthTargetKind::Grid;
        binding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
        binding.influenceRule.maxInfluences = DepthBoneGpuMaxInfluences + 1;
        root->bindings = Slice<ExDepthRigBinding>{binding};
        param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthGpuPacketFailureParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{1, 0}, 5.0f);
        ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, TextSlice(text("GPU packet failure regression")),
            DepthBoneDirtyScope::AllKeypoints);
        bool packetFailureThrown = false;
        try { ngFlushDepthBoneDirtyImmediate(); }
        catch (const nijigenerate::compat::ThrowableException& error) {
            packetFailureThrown = error.msg.view().find("Depth Bone GPU packet build failed") != std::string_view::npos &&
                error.msg.view().find("maximum influence count") != std::string_view::npos;
        }
        check(packetFailureThrown, "GPU packet build failure must raise a fatal refresh error");
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        check(deformBinding == nullptr, "GPU packet build failure must not silently write a partial deform binding");
    }, [] {
        ngClearDepthBoneGpuAsyncTestHooks();
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
        fakeDepthBoneGpuCancelCount = 0;
        fakeDepthBoneGpuLastSourceDepthOffset = 0.0f;
        fakeDepthBoneGpuLastBones = {};
        fakeDepthBoneGpuMutatingTarget = nullptr;
        fakeDepthBoneGpuMutateDepthOnReady = false;
    });
}
