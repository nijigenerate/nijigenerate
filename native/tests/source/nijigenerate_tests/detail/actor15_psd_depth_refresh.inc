// Existing regression.d5251-5374. Include once inside detail after original GPU hooks and PSD helpers.
void testPsdDepthImportRefreshesDepthBoneBindings() {
    using Text = nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::ext::ExParameter;
    using nijigenerate::io::depthmap_psd::PsdDepthImportResult;
    using nijigenerate::io::depthmap_psd::PsdDepthGridResult;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::ngVecaFromVectorsSource;
    using nicxlive::common::SharedArray;
    using nicxlive::core::param::DeformationParameterBinding;
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::commands::depth::bone_gpu_async;
    using nijigenerate::commands::depth::map::ngPsdDepthImportProgressVisibleForRegression;
    using nijigenerate::api::mcp::task::ngMcpProcessQueue;
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
    fakeDepthBoneGpuNextJobId = 1;
    fakeDepthBoneGpuJobVertexCounts = nullptr;
    fakeDepthBoneGpuSubmitCount = 0;
    fakeDepthBoneGpuPollCount = 0;
    fakeDepthBoneGpuSubmitFailAfter = 0;
    fakeDepthBoneGpuNotReadyPolls = 0;
    fakeDepthBoneGpuMutatingTarget = nullptr;
    fakeDepthBoneGpuMutateDepthOnReady = false;
    ngSetDepthBoneGpuAsyncTestHooks(&fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll);
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto root = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        root->setName(text("psd-depth-refresh-root").toNullableString());
        auto bone = ngCreateDepthBone(root, text("PsdDepthRefreshBone"), Vec3{0, 0, 0}, Vec3{0, 100, 0});
        Slice<std::shared_ptr<ExGridDeformer>> targets;
        for (int i = 0; i < 4; ++i) {
            auto target = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
            target->setName(formatted("psd-depth-refresh-grid-%s", i).toNullableString());
            target->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{
                Vec2{-10, 0}, Vec2{10, 0}, Vec2{-10, 100}, Vec2{10, 100}}));
            target->replaceDepths(Slice<float>{0, 0, 0, 0});
            targets.append(target);
            ExDepthRigBinding binding;
            binding.targetUuid = target->getUUID();
            binding.targetKind = ExDepthTargetKind::Grid;
            binding.sourceBoneUuids = Slice<std::uint64_t>{static_cast<std::uint64_t>(bone->getUUID())};
            binding.influenceRule.maxInfluences = 1;
            root->bindings.append(binding);
        }
        auto param = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("PsdDepthRefreshParam"); }, [] { return false; });
        param->min = Vec2{0, 0};
        param->max = Vec2{1, 0};
        param->value = Vec2{1, 0};
        nijigenerate::project::incActivePuppet()->parameters.push_back(param);
        auto tx = newValueBinding(param, bone, text("transform.t.x"));
        tx->setValue(Vec2u{1, 0}, 5.0f);
        PsdDepthImportResult psdImport;
        for (std::size_t i = 0; i < targets.size(); ++i) {
            auto target = targets[i];
            PsdDepthGridResult gridResult;
            gridResult.grid = target;
            gridResult.depths = Slice<float>{0.1f + static_cast<float>(i), 0.2f + static_cast<float>(i),
                0.3f + static_cast<float>(i), 0.4f + static_cast<float>(i)};
            psdImport.grids.append(gridResult);
        }
        auto applyResult = applyPsdDepthImportForRegression(psdImport);
        check(applyResult->succeeded, "PSD depth import with DepthBone bindings should start successfully");
        for (int unused = 0; unused < 100; ++unused) ngMcpProcessQueue();
        check(fakeDepthBoneGpuSubmitCount >= targets.size() * 2,
            "PSD depth import should submit GPU refresh for every target binding and keypoint");
        for (std::size_t i = 0; i < targets.size(); ++i) {
            auto target = targets[i];
            auto deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
                param->getBinding(target, NullableString::fromStaticLiteral("deform")));
            const bool hasBinding = deformBinding != nullptr;
            require(hasBinding, formatted("PSD depth import should create a deform binding for every DepthBone target %s", i));
            auto offsets = deformBinding->getValue(Vec2u{1, 0}).vertexOffsets;
            const bool countMatches = offsets.size() == target->vertices.size();
            require(countMatches, formatted("PSD depth import should write every target vertex offset for target %s", i));
            for (auto offset : offsets) {
                const bool offsetMatches = nearVec2(offset, Vec2{2, -1});
                require(offsetMatches, formatted("PSD depth import should write fake GPU offsets for target %s", i));
            }
        }
        fakeDepthBoneGpuSubmitCount = 0;
        PsdDepthImportResult targetOnlyImport;
        for (std::size_t i = 0; i < targets.size(); ++i) {
            auto target = targets[i];
            PsdDepthGridResult gridResult;
            gridResult.grid = target;
            gridResult.depths = target->copyDepths();
            if (i == 0) gridResult.depths[0] += 0.125f;
            targetOnlyImport.grids.append(gridResult);
        }
        auto targetOnlyResult = applyPsdDepthImportForRegression(targetOnlyImport);
        check(targetOnlyResult->succeeded, "target-specific PSD depth update should start successfully");
        for (int unused = 0; unused < 100; ++unused) ngMcpProcessQueue();
        const bool submittedTwice = fakeDepthBoneGpuSubmitCount == 2;
        require(submittedTwice, formatted(
            "changing one PSD target must refresh only that target's two parameter keypoints, not the entire DepthRig: %s",
            fakeDepthBoneGpuSubmitCount));
        PsdDepthImportResult canceledImport;
        PsdDepthGridResult canceledGrid;
        canceledGrid.grid = targets[0];
        canceledGrid.depths = targets[0]->copyDepths();
        canceledGrid.depths[0] += 0.25f;
        canceledImport.grids.append(canceledGrid);
        fakeDepthBoneGpuNotReadyPolls = 1000;
        auto canceledResult = applyPsdDepthImportForRegression(canceledImport);
        check(canceledResult->succeeded, "cancelable PSD depth update should start successfully");
        ngMcpProcessQueue();
        check(ngPsdDepthImportProgressVisibleForRegression(),
            "pending PSD depth work should display its generic async progress notification");
        check(std::dynamic_pointer_cast<nijigenerate::actions::AsyncGroupAction>(history::incActionTop()) != nullptr,
            "pending PSD depth work should be owned by the undoable async action");
        history::incActionUndo();
        check(!ngPsdDepthImportProgressVisibleForRegression(),
            "undoing pending PSD depth work must remove its progress notification immediately");
        fakeDepthBoneGpuNotReadyPolls = 0;
        ngFlushDepthBoneDirtyImmediate();
    }, [] {
        ngClearDepthBoneGpuAsyncTestHooks();
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
        fakeDepthBoneGpuMutatingTarget = nullptr;
        fakeDepthBoneGpuMutateDepthOnReady = false;
    });
}
