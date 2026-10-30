// Existing regression.d16535-16611; include inside regression::detail after helpers and existing GPU hooks.
void testDepthBoneSkinningLockToRootTerminal() {
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::commands::depth::bone_gpu_async;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    resetCase();
    fakeDepthBoneGpuNextJobId = 1;
    fakeDepthBoneGpuJobVertexCounts = nullptr;
    fakeDepthBoneGpuSubmitCount = 0;
    fakeDepthBoneGpuPollCount = 0;
    fakeDepthBoneGpuSubmitFailAfter = 0;
    fakeDepthBoneGpuNotReadyPolls = 0;
    fakeDepthBoneGpuOutputX = 2.0f;
    fakeDepthBoneGpuOutputY = -1.0f;
    ngSetDepthBoneGpuAsyncTestHooks(&fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll);
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto root = ExDepthRigRoot::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        root->setName(NullableString::fromStaticLiteral("skinning-depth-root"));
        auto shin = ngCreateDepthBone(root, text("Shin"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        auto foot = ngCreateDepthBone(shin, text("Foot"), Vec3{0, 100, 0}, Vec3{0, 200, 0});
        auto target = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        target->setName(NullableString::fromStaticLiteral("skinning-target-grid"));
        target->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-10, 140}, Vec2{10, 140}, Vec2{-10, 180}, Vec2{10, 180}}));
        ExDepthRigBinding binding;
        binding.targetUuid = target->getUUID();
        binding.targetKind = ExDepthTargetKind::Grid;
        binding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(shin->getUUID()),
            static_cast<std::uint64_t>(foot->getUUID())};
        binding.influenceRule.maxInfluences = 4;
        binding.influenceRule.radiusScale = 1.0f;
        binding.influenceRule.minimumRadius = 1.0f;
        root->bindings = Slice<ExDepthRigBinding>{binding};
        auto param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("DepthSkinningParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
        auto shinTx = newValueBinding(param, shin, text("transform.t.x"));
        shinTx->setValue(Vec2u{1, 0}, 30.0f);
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(nijigenerate::project::incActivePuppet());
        ctx->armedParameters(Context::Parameters{param});
        foot->lockToRoot = false;
        target->getDeformation().fill(Vec2{0, 0});
        fakeDepthBoneGpuOutputX = 30.0f;
        fakeDepthBoneGpuOutputY = 0.0f;
        auto unlockedTargets = Slice<std::shared_ptr<Node>>{target};
        require(ngRequireObject(ngCmd<DepthBoneCommand::PreviewDepthBoneDeform>(ctx, root, unlockedTargets))->succeeded,
            text("skin fixture should preview with unlocked terminal bone"));
        ngFlushDepthBoneDirtyImmediate();
        auto unlockedDeformation = target->getDeformation();
        for (std::size_t i = 0; i < unlockedDeformation.size(); ++i) {
            Vec2 offset = unlockedDeformation[i];
            require(near(offset.x, 30.0f),
                text("unlocked terminal bone should inherit parent translation beyond the terminal segment"));
        }
        foot->lockToRoot = true;
        target->getDeformation().fill(Vec2{0, 0});
        fakeDepthBoneGpuOutputX = 0.0f;
        fakeDepthBoneGpuOutputY = 0.0f;
        auto lockedTargets = Slice<std::shared_ptr<Node>>{target};
        require(ngRequireObject(ngCmd<DepthBoneCommand::PreviewDepthBoneDeform>(ctx, root, lockedTargets))->succeeded,
            text("skin fixture should preview with locked terminal bone"));
        ngFlushDepthBoneDirtyImmediate();
        auto lockedDeformation = target->getDeformation();
        for (std::size_t i = 0; i < lockedDeformation.size(); ++i) {
            Vec2 offset = lockedDeformation[i];
            require(nearVec2(offset, Vec2{0, 0}),
                text("locked terminal bone should keep vertices beyond the terminal bone fixed to root"));
        }
    }, [] {
        ngClearDepthBoneGpuAsyncTestHooks();
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
        fakeDepthBoneGpuOutputX = 2.0f;
        fakeDepthBoneGpuOutputY = -1.0f;
    });
}
