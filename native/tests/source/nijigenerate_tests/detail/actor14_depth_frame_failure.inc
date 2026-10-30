// Existing regression.d17606-17653; include inside regression::detail after helpers and existing GPU hooks.
void testDepthBoneFrameFailureRecovery() {
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::commands::depth::bone_gpu_async;
    using namespace nijigenerate::ext::nodes;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::actions::AsyncGroupAction;
    using nijigenerate::actions::AsyncGroupActionState;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    auto scopes = Slice<DepthBoneDirtyScope>{DepthBoneDirtyScope::Keypoint, DepthBoneDirtyScope::AllKeypoints};
    for (auto scopeKind : scopes) {
        resetCase();
        fakeDepthBoneGpuNextJobId = 1;
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
        ngSetDepthBoneGpuAsyncTestHooks(
            &fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll, &fakeDepthBoneGpuCancel);
        nijigenerate::compat::ngRunWithScopeExit([&] {
            auto root = ExDepthRigRoot::createSourceDeferred([] {
                return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
            });
            auto target = ExGridDeformer::createSourceDeferred([] {
                return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
            });
            target->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
            auto invalid = Node::createSourceDeferred([] {
                return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
            });
            auto bone = ngCreateDepthBone(root, text("Head"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
            root->addBoneSource(target, ExDepthTargetKind::Grid, bone);
            // Simulate a stale binding from an older project or an interrupted mutation.
            root->addBoneSource(invalid, ExDepthTargetKind::Grid, bone);
            auto param = ExParameter::createSourceDeferred(
                [] { return NullableString::fromStaticLiteral("frame-failure-depth"); }, [] { return false; });
            ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
            auto tx = newValueBinding(param, bone, text("transform.t.x"));
            tx->setValue(Vec2u{1, 0}, 5.0f);
            auto owner = std::allocate_shared<AsyncGroupAction>(SourceAllocator<AsyncGroupAction>{});
            ngBeginDepthBoneRefreshActionSink(owner);
            ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, RetainedStringSlice(text("Invalid target regression")), scopeKind);
            ngEndDepthBoneRefreshActionSink(owner);
            require(!ngFlushDepthBoneDirtyForFrame(), text("frame refresh must report a recoverable packet failure"));
            require(!ngHasPendingDepthBoneRefresh() && owner->pendingAsyncCount() == 0 &&
                owner->state() == AsyncGroupActionState::Failed,
                text("failed refresh must discard producers and settle the owning action"));
            require(nijigenerate::core::tasks::ngGetStatus().view().find("missing or not deformable") != std::string_view::npos,
                text("frame refresh must expose the error instead of silently swallowing it"));
            require(param->getBinding(target, NullableString::fromStaticLiteral("deform")) == nullptr,
                text("packet failure must not apply a partial target batch"));
            require(ngFlushDepthBoneDirtyForFrame(), text("the next idle frame must not retry failed work"));
            root->removeBoneSource(invalid, bone);
            ngMarkDepthBoneDirty(root, param, Vec2u{1, 0}, RetainedStringSlice(text("Retry repaired rig")), scopeKind);
            ngFlushDepthBoneDirtyImmediate();
            require(param->getBinding(target, NullableString::fromStaticLiteral("deform")) != nullptr &&
                !ngHasPendingDepthBoneRefresh(), text("fresh updates must work after repairing the failed rig"));
        }, [] { ngClearDepthBoneGpuAsyncTestHooks(); });
    }
}
