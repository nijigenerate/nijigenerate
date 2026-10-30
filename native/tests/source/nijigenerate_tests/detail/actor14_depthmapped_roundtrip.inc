// Existing regression.d3901-4190; include inside regression::detail after helpers.
void testDepthMappedNodeSerializationRoundTrip() {
    using namespace nijigenerate::ext::nodes;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::puppet::file::SaveFileCommand;
    using nijigenerate::actions::deformable::GridDeformerDefineAction;
    using nijigenerate::actions::deformable::DeformableChangeAction;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    namespace paths = nijigenerate::compat::phobos;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    const auto exactDepths = [](auto left, auto buildRight) {
        const auto right = buildRight();
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) if (left[i] != right[i]) return false;
        return true;
    };
    const auto nearDepths = [](auto left, auto buildRight) {
        const auto right = buildRight();
        return nearFloatArray(std::span<const float>(left.data(), left.size()),
            std::span<const float>(right.data(), right.size()));
    };
    const auto formatDepths = [](std::string_view pattern, Slice<float> values) {
        const std::array<format::TypedFormatArgument, 1> arguments{format::ngFormatArgument(values)};
        return nijigenerate::platform::detail::ngPathAllocation([&] {
            return RetainedString(format::ngFormatTyped(pattern, arguments));
        });
    };
    resetCase();
    ensureRegressionNodeTypesRegistered();
    const std::array fixtureSegments{paths::ngTempDirectory(), text("nijigenerate-regression-depthmapped")};
    auto fixtureDir = paths::ngBuildPath(fixtureSegments);
    if (paths::ngFileExists(fixtureDir.view())) paths::ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    paths::ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto grid = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        grid->setName(NullableString::fromStaticLiteral("depth-grid"));
        grid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{1, 1}}));
        grid->replaceDepths(Slice<float>{0.0f, 0.25f, -0.5f, 1.0f});
        ExDepthOp ring;
        ring.type = ExDepthOpType::Ring;
        ring.p0 = Vec2{-1, 0};
        ring.p1 = Vec2{1, 0};
        ring.amount = 0.75f;
        ring.width = 0.5f;
        ring.hardness = 0.25f;
        ExDepthOp attached;
        attached.type = ExDepthOpType::AttachedPoint;
        attached.index = 3;
        attached.amount = 0.5f;
        grid->replaceDepthOps(Slice<ExDepthOp>{attached, ring});
        grid->replaceDepthOpBaseDepths(Slice<float>{0.1f, 0.2f, 0.3f, 0.4f});
        auto defineAction = GridDeformerDefineAction::createSource([&] {
            return std::tuple{RetainedStringSlice(text("Resize depth grid")), grid};
        });
        grid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{0, -1}, Vec2{1, -1},
            Vec2{-1, 1}, Vec2{0, 1}, Vec2{1, 1}}));
        auto definedBaseDepths = grid->copyDepthOpBaseDepths();
        check(grid->copyDepthOps()[0].index == 5,
            "grid definition must remap attached depth operations to the same geometric point");
        defineAction->updateNewState();
        defineAction->rollback();
        check(exactDepths(grid->copyDepthOpBaseDepths(), [] { return Slice<float>{0.1f, 0.2f, 0.3f, 0.4f}; }),
            "undoing a grid definition must restore the exact pre-operation base depths");
        check(grid->copyDepthOps()[0].index == 3,
            "undoing a grid definition must restore the original attached depth operation index");
        defineAction->redo();
        check(exactDepths(grid->copyDepthOpBaseDepths(), [&] { return definedBaseDepths; }),
            "redoing a grid definition must restore the exact resampled pre-operation base depths");
        check(grid->copyDepthOps()[0].index == 5,
            "redoing a grid definition must restore the remapped attached depth operation index");
        defineAction->rollback();
        auto deformableAction = DeformableChangeAction::createSource([&] {
            return std::tuple{RetainedStringSlice(text("Resize depth grid")), grid};
        });
        grid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{0, -1}, Vec2{1, -1},
            Vec2{-1, 1}, Vec2{0, 1}, Vec2{1, 1}}));
        auto changedBaseDepths = grid->copyDepthOpBaseDepths();
        check(grid->copyDepthOps()[0].index == 5,
            "deformable topology changes must remap attached depth operations to the same geometric point");
        deformableAction->updateNewState();
        deformableAction->rollback();
        check(exactDepths(grid->copyDepthOpBaseDepths(), [] { return Slice<float>{0.1f, 0.2f, 0.3f, 0.4f}; }),
            "undoing a deformable topology change must restore the exact pre-operation base depths");
        check(grid->copyDepthOps()[0].index == 3,
            "undoing a deformable topology change must restore the original attached depth operation index");
        deformableAction->redo();
        check(exactDepths(grid->copyDepthOpBaseDepths(), [&] { return changedBaseDepths; }),
            "redoing a deformable topology change must restore the exact resampled pre-operation base depths");
        check(grid->copyDepthOps()[0].index == 5,
            "redoing a deformable topology change must restore the remapped attached depth operation index");
        deformableAction->rollback();
        auto removedPointGrid = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        removedPointGrid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{1, 1}}));
        removedPointGrid->replaceDepthOps(Slice<ExDepthOp>{attached});
        removedPointGrid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-0.5f, -0.5f}, Vec2{0, -0.5f}, Vec2{0.5f, -0.5f},
            Vec2{-0.5f, 0.5f}, Vec2{0, 0.5f}, Vec2{0.5f, 0.5f}}));
        check(removedPointGrid->copyDepthOps().size() == 0,
            "grid topology changes must clear attached depth operations whose vertex no longer exists");
        auto movedPointGrid = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        movedPointGrid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{1, 1}}));
        movedPointGrid->replaceDepthOps(Slice<ExDepthOp>{attached});
        movedPointGrid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1.2f, -0.8f}, Vec2{0.8f, -1.1f},
            Vec2{-0.9f, 1.2f}, Vec2{1.1f, 0.9f}}));
        check(movedPointGrid->copyDepthOps().size() == 1 && movedPointGrid->copyDepthOps()[0].index == 3,
            "moving vertices without changing topology must preserve attached depth operation indices");
        auto equalCountTopologyGrid = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        equalCountTopologyGrid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{0, -1}, Vec2{1, -1},
            Vec2{-1, 1}, Vec2{0, 1}, Vec2{1, 1}}));
        ExDepthOp removedEqualCountPoint = attached;
        removedEqualCountPoint.index = 1;
        equalCountTopologyGrid->replaceDepthOps(Slice<ExDepthOp>{removedEqualCountPoint});
        auto equalCountDefineAction = GridDeformerDefineAction::createSource([&] {
            return std::tuple{RetainedStringSlice(text("Reshape equal-count grid")), equalCountTopologyGrid};
        });
        equalCountTopologyGrid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{1, -1}, Vec2{-1, 0},
            Vec2{1, 0}, Vec2{-1, 1}, Vec2{1, 1}}));
        equalCountDefineAction->updateNewState();
        check(equalCountTopologyGrid->copyDepthOps().size() == 0,
            "grid definition must clear attached points removed by an equal-vertex-count topology change");
        equalCountDefineAction->rollback();
        check(equalCountTopologyGrid->copyDepthOps().size() == 1 && equalCountTopologyGrid->copyDepthOps()[0].index == 1,
            "undoing an equal-count topology change must restore the attached point");
        equalCountDefineAction->redo();
        check(equalCountTopologyGrid->copyDepthOps().size() == 0,
            "redoing an equal-count topology change must clear the removed attached point again");
        auto copied = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        copied->setName(NullableString::fromStaticLiteral("copied-depth-grid"));
        copied->copyDepthsFrom(grid);
        copied->copyDepthOpsFrom(grid);
        check(exactDepths(copied->copyDepths(), [] { return Slice<float>{0.0f, 0.25f, -0.5f, 1.0f}; }),
            "DepthMapped copy should duplicate depths");
        check(copied->copyDepthOps().size() == 2 && copied->copyDepthOps()[0].type == ExDepthOpType::AttachedPoint &&
            copied->copyDepthOps()[1].type == ExDepthOpType::Ring, "DepthOperation copy should duplicate operations");
        check(exactDepths(copied->copyDepthOpBaseDepths(), [] { return Slice<float>{0.1f, 0.2f, 0.3f, 0.4f}; }),
            "DepthOperation copy should duplicate the pre-operation base depths");
        copied->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{0, -1}, Vec2{1, -1},
            Vec2{-1, 1}, Vec2{0, 1}, Vec2{1, 1}}));
        check(copied->copyDepths().size() == copied->getVertices().size(),
            "DepthMapped rebuffer should resize depth array to vertices");
        check(copied->copyDepthOpBaseDepths().size() == copied->getVertices().size(),
            "DepthOperation rebuffer should resample its base depth array with the target vertices");
        auto resized = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        resized->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{1, 1}}));
        resized->replaceDepths(Slice<float>{0.0f, 2.0f, 4.0f, 6.0f});
        resized->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-0.5f, -0.5f}, Vec2{0.5f, -0.5f},
            Vec2{-0.5f, 0.5f}, Vec2{0.5f, 0.5f}}));
        check(nearDepths(resized->copyDepths(), [] { return Slice<float>{1.5f, 2.5f, 3.5f, 4.5f}; }),
            "resizing a grid within its old bounds should bilinearly resample depths");
        resized->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-2, -2}, Vec2{2, -2}, Vec2{-2, 2}, Vec2{2, 2}}));
        check(nearDepths(resized->copyDepths(), [] { return Slice<float>{-3.0f, 1.0f, 5.0f, 9.0f}; }),
            "expanding a grid should linearly extrapolate depths from its edge cells");
        auto descendingGrid = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        descendingGrid->getVertices() = Vec2Array(SharedArray<Vec2>{Vec2{1, 1}, Vec2{-1, 1}, Vec2{1, -1}, Vec2{-1, -1}});
        Slice<float> descendingDepths;
        auto descendingVertices = descendingGrid->getVertices();
        for (std::size_t i = 0; i < descendingVertices.size(); ++i) {
            Vec2 vertex = descendingVertices[i];
            descendingDepths.append(vertex.x + vertex.y * 2.0f);
        }
        descendingGrid->replaceDepths(descendingDepths);
        descendingGrid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{0, -1}, Vec2{1, -1},
            Vec2{-1, 0}, Vec2{0, 0}, Vec2{1, 0}, Vec2{-1, 1}, Vec2{0, 1}, Vec2{1, 1}}));
        Slice<float> expectedDescendingDepths;
        auto expectedDescendingVertices = descendingGrid->getVertices();
        for (std::size_t i = 0; i < expectedDescendingVertices.size(); ++i) {
            Vec2 vertex = expectedDescendingVertices[i];
            expectedDescendingDepths.append(vertex.x + vertex.y * 2.0f);
        }
        const bool descendingMatches = nearDepths(descendingGrid->copyDepths(), [&] { return expectedDescendingDepths; });
        auto descendingMessageValues = descendingGrid->copyDepths();
        require(descendingMatches, formatDepths(
            "grid depth resampling must preserve depth positions from descending source axes: %s", descendingMessageValues));
        auto nonFiniteGrid = ExGridDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        nonFiniteGrid->setName(NullableString::fromStaticLiteral("non-finite-depth-grid"));
        nonFiniteGrid->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{1, 1}}));
        nonFiniteGrid->replaceDepths(Slice<float>{std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 0.375f});
        check(exactDepths(nonFiniteGrid->copyDepths(), [] { return Slice<float>{0.0f, 0.0f, 0.0f, 0.375f}; }),
            "DepthMapped assignment must immediately replace NaN and infinities with zero");
        ngRequireObject(ngRequireObject(nijigenerate::project::incActivePuppet())->root)->build();
        const std::array saveSegments{fixtureDir, text("depthmapped-roundtrip")};
        auto saveBase = paths::ngBuildPath(saveSegments);
        auto savePath = paths::ngSetExtension(saveBase, text(".inx"));
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        check(ngRequireObject(constructSourceCommand<SaveFileCommand>(saveBase)->run(ctx))->succeeded,
            "SaveFileCommand should save depth-mapped fixture");
        check(paths::ngFileExists(savePath.view()) && paths::ngIsFile(savePath.view()), "depth-mapped INX fixture should exist");
        auto savedBytes = nicxlive::core::d_runtime::ngReadBinaryFile(savePath);
        const std::string_view savedText(reinterpret_cast<const char*>(savedBytes.data()), savedBytes.size());
        check(savedText.find("depth-grid") != std::string_view::npos,
            "depth-mapped INX fixture should contain depth-grid node before load");
        check(exactDepths(nonFiniteGrid->copyDepths(), [] { return Slice<float>{0.0f, 0.0f, 0.0f, 0.375f}; }),
            "saving must preserve already-normalized in-memory depths");
        ensureRegressionNodeTypesRegistered();
        auto loadedPuppet = nijigenerate::io::project_reader::ngLoadProjectPuppet<nijigenerate::ext::ExPuppet>(savePath);
        auto loaded = std::dynamic_pointer_cast<ExGridDeformer>(findNodeRecursive(ngRequireObject(loadedPuppet)->root, "depth-grid"));
        auto loadedNonFinite = std::dynamic_pointer_cast<ExGridDeformer>(findNodeRecursive(
            ngRequireObject(loadedPuppet)->root, "non-finite-depth-grid"));
        const bool gridPresent = loaded != nullptr;
        auto gridTree = nodeTreeSummary(ngRequireObject(loadedPuppet)->root);
        require(gridPresent, concatenateSourceStrings({text(
            "depth-mapped INX round-trip should restore ExGridDeformer; tree:\n"), gridTree}));
        check(exactDepths(ngRequireObject(loaded)->copyDepths(), [] { return Slice<float>{0.0f, 0.25f, -0.5f, 1.0f}; }),
            "depth-mapped INX round-trip should restore depths");
        check(loadedNonFinite != nullptr, "depth-mapped INX round-trip should restore the non-finite depth fixture");
        check(exactDepths(ngRequireObject(loadedNonFinite)->copyDepths(), [] { return Slice<float>{0.0f, 0.0f, 0.0f, 0.375f}; }),
            "depth-mapped INX save must serialize NaN and infinities as finite neutral depths");
        auto loadedOps = loaded->copyDepthOps();
        check(loadedOps.size() == 2, "depth-mapped INX round-trip should restore depth operation count");
        check(loadedOps[0].type == ExDepthOpType::AttachedPoint && loadedOps[0].index == 3,
            "depth-mapped INX round-trip should restore attached-point operation state");
        check(loadedOps[1].type == ExDepthOpType::Ring, "depth-mapped INX round-trip should restore operation type");
        check(nearVec2(loadedOps[1].p0, Vec2{-1, 0}) && nearVec2(loadedOps[1].p1, Vec2{1, 0}),
            "depth-mapped INX round-trip should restore ring endpoints");
        check(near(loadedOps[1].amount, 0.75f) && near(loadedOps[1].width, 0.5f) && near(loadedOps[1].hardness, 0.25f),
            "depth-mapped INX round-trip should restore ring settings");
        check(exactDepths(loaded->copyDepthOpBaseDepths(), [] { return Slice<float>{0.1f, 0.2f, 0.3f, 0.4f}; }),
            "depth-mapped INX round-trip should restore the pre-operation base depths");
        auto path = ExPathDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        path->setName(NullableString::fromStaticLiteral("depth-path"));
        path->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-2, 0}, Vec2{0, 1}, Vec2{2, 0}}));
        path->replaceDepths(Slice<float>{-0.25f, 0.0f, 0.75f});
        auto resampledPath = ExPathDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        resampledPath->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-2, 0}, Vec2{0, 1}, Vec2{2, 0}}));
        resampledPath->replaceDepths(Slice<float>{-0.25f, 0.0f, 0.75f});
        resampledPath->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-2, 0}, Vec2{-1, 0.5f}, Vec2{0, 1}, Vec2{1, 0.5f}, Vec2{2, 0}}));
        const bool pathDepthsMatch = nearDepths(resampledPath->copyDepths(), [] {
            return Slice<float>{-0.25f, -0.125f, 0.0f, 0.375f, 0.75f};
        });
        auto pathMessageValues = resampledPath->copyDepths();
        require(pathDepthsMatch, formatDepths(
            "DepthMapped PathDeformer rebuffer should interpolate depths by path position: %s", pathMessageValues));
        auto copiedPath = ExPathDeformer::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        copiedPath->copyDepthsFrom(path);
        check(exactDepths(copiedPath->copyDepths(), [] { return Slice<float>{-0.25f, 0.0f, 0.75f}; }),
            "DepthMapped PathDeformer copy should duplicate depths");
        copiedPath->rebuffer(Vec2Array(SharedArray<Vec2>{Vec2{-2, 0}, Vec2{-1, 0.5f}, Vec2{0, 1}, Vec2{1, 0.5f}, Vec2{2, 0}}));
        check(copiedPath->copyDepths().size() == copiedPath->getVertices().size(),
            "DepthMapped PathDeformer rebuffer should resize depth array to vertices");
        ngRequireObject(ngRequireObject(nijigenerate::project::incActivePuppet())->root)->build();
        const std::array pathSaveSegments{fixtureDir, text("depthmapped-path-roundtrip")};
        auto pathSaveBase = paths::ngBuildPath(pathSaveSegments);
        auto pathSavePath = paths::ngSetExtension(pathSaveBase, text(".inx"));
        check(ngRequireObject(constructSourceCommand<SaveFileCommand>(pathSaveBase)->run(ctx))->succeeded,
            "SaveFileCommand should save depth-mapped PathDeformer fixture");
        ensureRegressionNodeTypesRegistered();
        auto loadedPathPuppet = nijigenerate::io::project_reader::ngLoadProjectPuppet<nijigenerate::ext::ExPuppet>(pathSavePath);
        auto loadedPath = std::dynamic_pointer_cast<ExPathDeformer>(findNodeRecursive(
            ngRequireObject(loadedPathPuppet)->root, "depth-path"));
        const bool pathPresent = loadedPath != nullptr;
        auto pathTree = nodeTreeSummary(ngRequireObject(loadedPathPuppet)->root);
        require(pathPresent, concatenateSourceStrings({text(
            "depth-mapped INX round-trip should restore ExPathDeformer; tree:\n"), pathTree}));
        check(exactDepths(ngRequireObject(loadedPath)->copyDepths(), [] { return Slice<float>{-0.25f, 0.0f, 0.75f}; }),
            "depth-mapped INX round-trip should restore PathDeformer depths");
    }, [&] {
        if (paths::ngFileExists(fixtureDir.view())) paths::ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}
