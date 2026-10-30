// Existing regression.d14733-14807; include inside regression::detail after helpers and existing GPU hooks.
nicxlive::core::math::Vec2Array depthBoneYawOffsetsWithChildZ(float childZ) {
    using namespace nijigenerate::ext::nodes;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::depth::bone::DepthBoneCommand;
    using nijigenerate::commands::depth::bone::ngFlushDepthBoneDirtyImmediate;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::math::Vec3;
    using nijigenerate::ext::ExParameter;
    using nijigenerate::compat::Vec2u;
    resetCase();
    auto root = ExDepthRigRoot::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    root->setName(NullableString::fromStaticLiteral("z-axis-depth-root"));
    auto parent = ngCreateDepthBone(root, RetainedString::fromStaticLiteral("Parent"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
    auto child = ngCreateDepthBone(parent, RetainedString::fromStaticLiteral("Child"), Vec3{0, 100, 0}, Vec3{0, 200, 0});
    child->localTransform.translation.z = childZ;
    child->localTransform.update();
    child->transformChanged();
    auto target = GridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    target->setName(NullableString::fromStaticLiteral("z-axis-target-grid"));
    target->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{30, 40}, Vec2{50, 40}, Vec2{30, 70}, Vec2{50, 70}}));
    ExDepthRigBinding binding;
    binding.targetUuid = target->getUUID();
    binding.targetKind = ExDepthTargetKind::Grid;
    binding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(parent->getUUID())};
    binding.influenceRule.maxInfluences = 1;
    root->bindings = Slice<ExDepthRigBinding>{binding};
    auto param = ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("DepthZAxisParam"); }, [] { return false; });
    param->min = Vec2{0, 0};
    param->max = Vec2{1, 0};
    param->value = Vec2{1, 0};
    ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
    auto yaw = newValueBinding(param, parent, RetainedString::fromStaticLiteral("transform.r.y"));
    yaw->setValue(Vec2u{1, 0}, 0.45f);
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    ctx->armedParameters(Context::Parameters{param});
    auto targets = Slice<std::shared_ptr<Node>>{target};
    require(ngRequireObject(ngCmd<DepthBoneCommand::PreviewDepthBoneDeform>(ctx, root, targets))->succeeded,
        RetainedString::fromStaticLiteral("z-axis fixture should preview DepthBone deformation"));
    ngFlushDepthBoneDirtyImmediate();
    return target->getDeformation().dup();
}

void testDepthBonePlanarRestAxisIgnoresChildZ() {
    using namespace nijigenerate::commands::depth::bone_gpu_async;
    using nijigenerate::compat::RetainedString;
    using nicxlive::core::math::Vec2;
    fakeDepthBoneGpuNextJobId = 1;
    fakeDepthBoneGpuJobVertexCounts = nullptr;
    fakeDepthBoneGpuSubmitCount = 0;
    fakeDepthBoneGpuPollCount = 0;
    fakeDepthBoneGpuSubmitFailAfter = 0;
    fakeDepthBoneGpuNotReadyPolls = 0;
    ngSetDepthBoneGpuAsyncTestHooks(&fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll);
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto flat = depthBoneYawOffsetsWithChildZ(0.0f);
        auto raised = depthBoneYawOffsetsWithChildZ(80.0f);
        require(flat.size() == raised.size() && flat.size() > 0,
            RetainedString::fromStaticLiteral("z-axis fixture should produce comparable offsets"));
        bool hasMotion = false;
        for (std::size_t i = 0; i < flat.size(); ++i) {
            Vec2 offset = flat[i];
            if (offset.x < -0.0001f || offset.x > 0.0001f || offset.y < -0.0001f || offset.y > 0.0001f)
                hasMotion = true;
            require(nearVec2(offset, raised[i]), RetainedString::fromStaticLiteral(
                "DepthBone child Z should not change the planar rest rotation axis for zero-depth targets"));
        }
        require(hasMotion, RetainedString::fromStaticLiteral("z-axis fixture should exercise a non-zero yaw deformation"));
    }, [] {
        ngClearDepthBoneGpuAsyncTestHooks();
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
    });
}
