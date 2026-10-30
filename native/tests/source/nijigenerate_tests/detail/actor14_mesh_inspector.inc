// Existing regression.d3415-3517; include inside regression::detail after helpers.
void testMeshDeformerInspectorCommandsUndoRedo() {
    using namespace nijigenerate::commands::inspector::apply_node;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::panels::inspector::ngCreateInspectorDeferred;
    using nijigenerate::panels::inspector::ModelEditSubMode;
    using nijigenerate::panels::inspector::Selection;
    using nijigenerate::panels::inspector::TypedInspector;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::MeshGroup;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::PathDeformer;
    using nicxlive::core::nodes::ConnectedPendulumDriver;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    resetCase();
    auto meshGroup = MeshGroup::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    meshGroup->setName(NullableString::fromStaticLiteral("meshgroup-inspector-target"));
    auto meshInspector = ngCreateInspectorDeferred<MeshGroupInspector, MeshGroup>(
        [&] { return Selection<MeshGroup>{meshGroup}; }, [] { return ModelEditSubMode::Layout; });
    auto meshCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    meshCtx->nodes(Context::Nodes{meshGroup});
    meshCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(meshInspector)});
    check(ngRequireObject(constructSourceCommand<MeshGroupDynamicCommandBase>(true)->run(meshCtx))->succeeded,
        "MeshGroup dynamic inspector command should succeed");
    check(meshGroup->dynamic, "MeshGroup dynamic inspector command should apply");
    incActionUndo();
    check(!meshGroup->dynamic, "undo MeshGroup dynamic should restore default");
    incActionRedo();
    check(meshGroup->dynamic, "redo MeshGroup dynamic should restore new value");
    incActionClearHistory();
    check(ngRequireObject(constructSourceCommand<MeshGroupTranslateChildrenCommandBase>(false)->run(meshCtx))->succeeded,
        "MeshGroup translateChildren inspector command should succeed");
    check(!meshGroup->getTranslateChildren(), "MeshGroup translateChildren inspector command should apply");
    incActionUndo();
    check(meshGroup->getTranslateChildren(), "undo MeshGroup translateChildren should restore default");
    incActionRedo();
    check(!meshGroup->getTranslateChildren(), "redo MeshGroup translateChildren should restore new value");
    auto grid = GridDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    grid->setName(NullableString::fromStaticLiteral("grid-inspector-target"));
    auto gridInspector = ngCreateInspectorDeferred<GridInspector, GridDeformer>(
        [&] { return Selection<GridDeformer>{grid}; }, [] { return ModelEditSubMode::Layout; });
    auto gridCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    gridCtx->nodes(Context::Nodes{grid});
    gridCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(gridInspector)});
    incActionClearHistory();
    check(ngRequireObject(constructSourceCommand<GridDeformDynamicCommandBase>(true)->run(gridCtx))->succeeded,
        "GridDeformer dynamic inspector command should succeed");
    check(grid->dynamic, "GridDeformer dynamic inspector command should apply");
    incActionUndo();
    check(!grid->dynamic, "undo GridDeformer dynamic should restore default");
    incActionRedo();
    check(grid->dynamic, "redo GridDeformer dynamic should restore new value");
    auto path = PathDeformer::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    path->setName(NullableString::fromStaticLiteral("path-inspector-target"));
    path->driver = std::allocate_shared<ConnectedPendulumDriver>(SourceAllocator<ConnectedPendulumDriver>{}, path);
    auto pathInspector = ngCreateInspectorDeferred<PathApplyInspector, PathDeformer>(
        [&] { return Selection<PathDeformer>{path}; }, [] { return ModelEditSubMode::Layout; });
    auto pathCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    pathCtx->nodes(Context::Nodes{path});
    pathCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(pathInspector)});
    incActionClearHistory();
    check(ngRequireObject(constructSourceCommand<PathDeformDynamicCommandBase>(true)->run(pathCtx))->succeeded,
        "PathDeformer dynamic inspector command should succeed");
    check(path->dynamic, "PathDeformer dynamic inspector command should apply");
    incActionUndo();
    check(!path->dynamic, "undo PathDeformer dynamic should restore default");
    incActionRedo();
    check(path->dynamic, "redo PathDeformer dynamic should restore new value");
    incActionClearHistory();
    check(ngRequireObject(constructSourceCommand<PathDeformPhysicsEnabledCommandBase>(false)->run(pathCtx))->succeeded,
        "PathDeformer physicsEnabled inspector command should succeed");
    check(!path->physicsEnabled(), "PathDeformer physicsEnabled inspector command should apply");
    incActionUndo();
    check(path->physicsEnabled(), "undo PathDeformer physicsEnabled should restore old driver");
    incActionRedo();
    check(!path->physicsEnabled(), "redo PathDeformer physicsEnabled should disable driver again");
    incActionClearHistory();
    check(ngRequireObject(constructSourceCommand<PathDeformPhysicsEnabledCommandBase>(true)->run(pathCtx))->succeeded,
        "PathDeformer physicsEnabled re-enable command should succeed");
    check(path->physicsEnabled(), "PathDeformer physicsEnabled re-enable should create driver");
    incActionClearHistory();
    check(ngRequireObject(constructSourceCommand<PathDeformCurveTypeCommandBase>(PathDeformer::CurveType::Bezier)->run(pathCtx))->succeeded,
        "PathDeformer curveType inspector command should succeed");
    check(path->curveType == PathDeformer::CurveType::Bezier, "PathDeformer curveType inspector command should apply");
    incActionUndo();
    check(path->curveType == PathDeformer::CurveType::Spline, "undo PathDeformer curveType should restore default");
    incActionRedo();
    check(path->curveType == PathDeformer::CurveType::Bezier, "redo PathDeformer curveType should restore new value");
    incActionClearHistory();
    auto oldGravity = ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->gravity();
    check(ngRequireObject(constructSourceCommand<PathDeformGravityCommandBase>(2.25f)->run(pathCtx))->succeeded,
        "PathDeformer gravity inspector command should succeed");
    check(near(ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->gravity(), 2.25f),
        "PathDeformer gravity inspector command should apply");
    incActionUndo();
    check(near(ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->gravity(), oldGravity),
        "undo PathDeformer gravity should restore old value");
    incActionRedo();
    check(near(ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->gravity(), 2.25f),
        "redo PathDeformer gravity should restore new value");
    incActionClearHistory();
    auto oldRestoreConstant = ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->restoreConstant();
    check(ngRequireObject(constructSourceCommand<PathDeformRestoreConstantCommandBase>(3.5f)->run(pathCtx))->succeeded,
        "PathDeformer restoreConstant inspector command should succeed");
    check(near(ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->restoreConstant(), 3.5f),
        "PathDeformer restoreConstant inspector command should apply");
    incActionUndo();
    check(near(ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->restoreConstant(), oldRestoreConstant),
        "undo PathDeformer restoreConstant should restore old value");
    incActionRedo();
    check(near(ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->restoreConstant(), 3.5f),
        "redo PathDeformer restoreConstant should restore new value");
    incActionClearHistory();
    auto oldDamping = ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->damping();
    check(ngRequireObject(constructSourceCommand<PathDeformDampingCommandBase>(0.4f)->run(pathCtx))->succeeded,
        "PathDeformer damping inspector command should succeed");
    check(near(ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->damping(), 0.4f),
        "PathDeformer damping inspector command should apply");
    incActionUndo();
    check(near(ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->damping(), oldDamping),
        "undo PathDeformer damping should restore old value");
    incActionRedo();
    check(near(ngRequireObject(std::dynamic_pointer_cast<ConnectedPendulumDriver>(path->driver))->damping(), 0.4f),
        "redo PathDeformer damping should restore new value");
}
