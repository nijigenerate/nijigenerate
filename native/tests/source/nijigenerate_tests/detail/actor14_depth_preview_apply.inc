// Existing regression.d14809-14904; include inside regression::detail after helpers and existing GPU hooks.
void testDepthBonePreviewApplyCommands() {
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::commands::depth::bone_gpu_async;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::ExCommandResult;
    using nijigenerate::core::settings::SettingsJsonValue;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::compat::ngJsonStringify;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::param::DeformationParameterBinding;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto uuidText = [](std::uint32_t source) {
        char buffer[64];
        std::size_t first = sizeof(buffer);
        std::uint64_t value = source;
        do {
            std::uint32_t digit;
            if (value < 10) { digit = static_cast<std::uint32_t>(value); value = 0; }
            else { digit = static_cast<std::uint32_t>(value % 10); value /= 10; }
            buffer[--first] = static_cast<char>(digit + '0');
        } while (value);
        auto output = allocateSourceStringBytes(sizeof(buffer) - first);
        for (std::size_t i = 0; i < output.size(); ++i) output[i] = buffer[first + i];
        return RetainedString::retainedView(output.retentionOwner(), output.data(), output.size());
    };
    resetCase();
    fakeDepthBoneGpuNextJobId = 1;
    fakeDepthBoneGpuJobVertexCounts = nullptr;
    fakeDepthBoneGpuSubmitCount = 0;
    fakeDepthBoneGpuPollCount = 0;
    fakeDepthBoneGpuSubmitFailAfter = 0;
    fakeDepthBoneGpuNotReadyPolls = 0;
    ngSetDepthBoneGpuAsyncTestHooks(&fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll);
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto root = ExDepthRigRoot::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        root->setName(NullableString::fromStaticLiteral("preview-depth-root"));
        auto target = GridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        target->setName(NullableString::fromStaticLiteral("preview-target-grid"));
        auto bone = ExDepthBone::createSourceDeferred([&] { return root; });
        bone->setName(NullableString::fromStaticLiteral("preview-depth-bone"));
        bone->boneId = text("PreviewBone");
        bone->restHead = Vec3{0, 0, 0};
        bone->restTail = Vec3{0, 100, 0};
        ngRequireObject(nijigenerate::project::incActivePuppet())->rescanNodes();
        auto param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthPreviewParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
        auto tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{1, 0}, 5.0f);
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(nijigenerate::project::incActivePuppet());
        ctx->armedParameters(Context::Parameters{param});
        auto unboundTarget = GridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        unboundTarget->setName(NullableString::fromStaticLiteral("unbound-preview-target-grid"));
        unboundTarget->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{1, 1}}));
        check(root->bindings.size() == 0 && ngRequireObject(ngCmd<DepthBoneCommand::PreviewDepthBoneInfluence>(
            ctx, root, unboundTarget, bone))->succeeded,
            "PreviewDepthBoneInfluence should support an unbound target with default influence settings");
        check(root->bindings.size() == 0 && unboundTarget->getDeformation().size() == unboundTarget->getVertices().size(),
            "PreviewDepthBoneInfluence must not persist an empty binding for an unbound target");
        check(ngRequireObject(ngCmd<DepthBoneCommand::AddDepthBoneSource>(ctx, root, target, bone))->succeeded,
            "preview fixture should add a depth bone source");
        auto listBones = std::dynamic_pointer_cast<ExCommandResult<SettingsJsonValue>>(
            ngCmd<DepthBoneCommand::ListDepthBones>(ctx, root));
        check(listBones && listBones->succeeded, "ListDepthBones should return a JSON payload");
        check(listBones->result.sourceIndexValue("items").sourceArrayValue().size() == 1,
            "ListDepthBones should list the fixture bone");
        check(listBones->result.sourceIndexValue("items").sourceArrayValue()[0].sourceIndexValue("boneId").as_string().view() ==
            "PreviewBone", "ListDepthBones should preserve boneId");
        auto listSources = std::dynamic_pointer_cast<ExCommandResult<SettingsJsonValue>>(
            ngCmd<DepthBoneCommand::ListDepthBoneSources>(ctx, root, target));
        check(listSources && listSources->succeeded, "ListDepthBoneSources should return a JSON payload");
        check(listSources->result.sourceIndexValue("sourceBoneUuids").sourceArrayValue().size() == 1,
            "ListDepthBoneSources should list the source bone");
        auto sourceUuid = ngJsonStringify(listSources->result.sourceIndexValue("sourceBoneUuids").sourceArrayValue()[0]);
        auto expectedUuid = uuidText(bone->getUUID());
        check(sourceUuid == expectedUuid, "ListDepthBoneSources should return source UUID");
        check(ngRequireObject(ngCmd<DepthBoneCommand::PreviewDepthBoneInfluence>(ctx, root, target, bone))->succeeded,
            "PreviewDepthBoneInfluence should succeed");
        check(target->getDeformation().size() == target->getVertices().size(),
            "PreviewDepthBoneInfluence should preserve deformation length");
        auto influenceOffsets = target->getDeformation();
        for (std::size_t i = 0; i < influenceOffsets.size(); ++i) {
            Vec2 offset = influenceOffsets[i];
            check(near(offset.y, 20.0f), "PreviewDepthBoneInfluence should write radius-scaled preview offsets");
        }
        target->getDeformation().fill(Vec2{0, 0});
        auto previewTargets = Slice<std::shared_ptr<Node>>{target};
        check(ngRequireObject(ngCmd<DepthBoneCommand::PreviewDepthBoneDeform>(ctx, root, previewTargets))->succeeded,
            "PreviewDepthBoneDeform should succeed");
        ngFlushDepthBoneDirtyImmediate();
        auto previewOffsets = target->getDeformation();
        for (std::size_t i = 0; i < previewOffsets.size(); ++i) {
            Vec2 offset = previewOffsets[i];
            check(nearVec2(offset, Vec2{2, -1}), "PreviewDepthBoneDeform should apply GPU preview offsets asynchronously");
        }
        incActionClearHistory();
        auto applyTargets = Slice<std::shared_ptr<Node>>{target};
        auto applyResult = ngCmd<DepthBoneCommand::ApplyDepthBoneDeform>(ctx, root, applyTargets);
        check(ngRequireObject(applyResult)->succeeded, "ApplyDepthBoneDeform should succeed");
        ngFlushDepthBoneDirtyImmediate();
        check(incActionHistory().size() == 1, "ApplyDepthBoneDeform should push one grouped undo action");
        auto deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        check(deformBinding != nullptr, "ApplyDepthBoneDeform should create a deform binding");
        auto appliedOffsets = ngRequireObject(deformBinding)->getValue(Vec2u{1, 0}).vertexOffsets;
        check(appliedOffsets.size() == target->getVertices().size(), "ApplyDepthBoneDeform should write offsets for every vertex");
        for (std::size_t i = 0; i < appliedOffsets.size(); ++i) {
            Vec2 offset = appliedOffsets[i];
            check(nearVec2(offset, Vec2{2, -1}), "ApplyDepthBoneDeform should store GPU offsets in the binding");
        }
        incActionUndo();
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        check(deformBinding && deformBinding->getValue(Vec2u{1, 0}).vertexOffsets.size() == target->getVertices().size(),
            "undo ApplyDepthBoneDeform should preserve an undoable deform binding shape after async writeback");
        incActionRedo();
        deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            param->getBinding(target, NullableString::fromStaticLiteral("deform")));
        check(deformBinding && deformBinding->getValue(Vec2u{1, 0}).vertexOffsets.size() == target->getVertices().size(),
            "redo ApplyDepthBoneDeform should restore the created deform binding and offsets");
    }, [] {
        ngClearDepthBoneGpuAsyncTestHooks();
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
    });
}
