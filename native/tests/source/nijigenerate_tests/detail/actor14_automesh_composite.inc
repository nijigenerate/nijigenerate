// Existing regression.d12002-12026; include inside regression::detail after AutoMesh helpers.
void testAutoMeshCompositeProcessorMatrix() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::automesh::config::AutoMeshSetValuesCommand;
    using nijigenerate::viewport::vertex::automesh::meta::IAutoMeshReflect;
    using namespace nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    resetCase();
    auto processor = findAutoMeshProcessor(text("grid"));
    require(processor != nullptr, text("AutoMesh composite grid processor should exist"));
    auto reflect = std::dynamic_pointer_cast<IAutoMeshReflect>(processor);
    require(reflect != nullptr, text("AutoMesh composite grid processor should be reflectable"));
    auto oldSimple = ngRequireObject(reflect)->values(text("Simple"));
    auto command = constructSourceCommand<AutoMeshSetValuesCommand>(text("grid"), text("Simple"),
        text(R"({"x_segments":3,"y_segments":2,"margin":0})"));
    auto context = std::allocate_shared<Context>(SourceAllocator<Context>{});
    require(ngRequireObject(command->run(context))->succeeded,
        text("AutoMesh composite should edit grid config through command"));
    require(ngRequireObject(reflect)->values(text("Simple")) != oldSimple, text("AutoMesh composite config should change"));
    incActionUndo();
    require(jsonEquivalent(ngRequireObject(reflect)->values(text("Simple")), oldSimple),
        text("AutoMesh composite undo should restore grid config"));
    incActionRedo();
    auto gridMesh = runAutoMeshOnAlphaPart(text("grid"));
    requireAutoMeshOutput(text("grid-composite"), gridMesh, 12);
    require(ngRequireObject(gridMesh)->axes.size() == 2 && gridMesh->axes[0].size() >= 3 &&
        gridMesh->axes[1].size() >= 3, text("AutoMesh composite grid output should expose coherent grid axes"));
    auto ids = Slice<RetainedString>{text("contour"), text("skeleton"), text("optimum")};
    for (auto id : ids) {
        auto mesh = runAutoMeshOnAlphaPart(id);
        auto label = concatenateSourceStrings({text("composite-"), id});
        requireAutoMeshOutput(label, mesh, id.view() == "skeleton" ? 2 : 3);
    }
}
