// Complete existing regression.d5551-5609 only.
void testDepthCompositeMapOpsWorkflow() {
    using Text = nijigenerate::compat::RetainedString;
    using Floats = nijigenerate::compat::Slice<float>;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    using Object = nijigenerate::core::settings::SettingsJsonObject;
    using nijigenerate::ext::nodes::ExGridDeformer;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::vertex::define_mesh::DefineGridCommand;
    using nijigenerate::commands::depth::map::DepthMapCommand;
    using nicxlive::core::d_runtime::SourceAllocator;
    namespace actions = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto nearFloats = [](Floats a, Floats b) {
        return nearFloatArray(std::span<const float>(a.data(), a.size()), std::span<const float>(b.data(), b.size()));
    };
    const auto sameFloats = [](Floats a, Floats b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) if (!(a[i] == b[i])) return false;
        return true;
    };
    resetCase();
    auto grid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    grid->setName(text("depth-composite-grid").toNullableString());
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{grid});
    Floats coarseAxisX{-20.0f, 20.0f};
    Floats coarseAxisY{-10.0f, 10.0f};
    check(constructSourceCommand<DefineGridCommand>(coarseAxisX, coarseAxisY)->run(ctx)->succeeded,
        "depth composite should define a coarse grid first");
    Floats coarseDepths{0.0f, 0.2f, 0.6f, 0.8f};
    check(ngCmd<DepthMapCommand::SetDepths>(ctx, grid, coarseDepths)->succeeded,
        "depth composite should set coarse depths matching topology");
    Floats axisX{-20.0f, 0.0f, 20.0f};
    Floats axisY{-10.0f, 0.0f, 10.0f};
    check(constructSourceCommand<DefineGridCommand>(axisX, axisY)->run(ctx)->succeeded,
        "depth composite should subdivide the grid");
    Floats depths{0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    check(grid->vertices.size() == 9 && nearFloats(grid->copyDepths(), depths),
        "grid subdivision should bilinearly resample the existing depth map");
    actions::incActionUndo();
    check(grid->vertices.size() == 4 && nearFloats(grid->copyDepths(), coarseDepths),
        "undo grid subdivision should restore the coarse grid and depths");
    actions::incActionRedo();
    check(grid->vertices.size() == 9 && nearFloats(grid->copyDepths(), depths),
        "redo grid subdivision should restore the resampled grid depths");
    check(grid->copyDepths().size() == grid->vertices.size(), "depth composite depths should match vertex count");
    Object attachedFields;
    attachedFields["index"] = Json(4);
    attachedFields["amount"] = Json(0.75);
    check(ngCmd<DepthMapCommand::AddDepthOp>(ctx, grid, depthCommandOp(text("attached-point"), attachedFields), -1)->succeeded,
        "depth composite should add an attached-point operation");
    Object planeFields;
    planeFields["p0"] = jsonVec2Value(-20, -10);
    planeFields["p1"] = jsonVec2Value(20, -10);
    planeFields["p2"] = jsonVec2Value(0, 10);
    planeFields["z0"] = Json(0.0);
    planeFields["z1"] = Json(0.5);
    planeFields["z2"] = Json(-0.25);
    check(ngCmd<DepthMapCommand::AddDepthOp>(ctx, grid, depthCommandOp(text("plane"), planeFields), -1)->succeeded,
        "depth composite should add a plane operation");
    check(grid->copyDepthOps().size() == 2, "depth composite should keep both operations");
    auto beforeApply = grid->copyDepths().duplicate();
    check(ngCmd<DepthMapCommand::ApplyDepthOps>(ctx, grid)->succeeded, "depth composite should apply operations to depths");
    check(grid->copyDepths().size() == grid->vertices.size(), "applied depths should still match vertex count");
    check(!sameFloats(grid->copyDepths(), beforeApply), "depth composite apply should change depth values");
    actions::incActionUndo();
    check(sameFloats(grid->copyDepths(), beforeApply), "undo apply should restore previous depth values while keeping topology");
    check(grid->vertices.size() == 9, "undo apply should not undo the grid topology");
    actions::incActionRedo();
    check(grid->copyDepths().size() == grid->vertices.size() && !sameFloats(grid->copyDepths(), beforeApply),
        "redo apply should restore baked depths with coherent length");
}
