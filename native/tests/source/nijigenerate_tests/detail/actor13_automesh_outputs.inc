// Existing regression.d18602-18667; include inside regression::detail after helpers.
std::shared_ptr<nijigenerate::viewport::common::mesh::IncMesh> runAutoMeshOnAlphaPart(
    nijigenerate::compat::RetainedString processorId) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::viewport::common::mesh::IncMesh;
    using nicxlive::core::nodes::MeshData;
    using nijigenerate::core::tasks::TaskFiber;
    auto processor = findAutoMeshProcessor(processorId);
    const bool present = processor != nullptr;
    require(present, concatenateSourceStrings({RetainedString::fromStaticLiteral("AutoMesh processor should exist: "), processorId}));
    auto part = newAutoMeshAlphaPart(concatenateSourceStrings({RetainedString::fromStaticLiteral("automesh-"), processorId}));
    auto mesh = IncMesh::createSourceDeferred([part] {
        auto receiver = ngRequireObject(part);
        auto& cell = receiver->getMesh();
        return std::shared_ptr<MeshData>(receiver->mesh, &cell);
    });
    std::shared_ptr<IncMesh> result;
    auto fiber = TaskFiber::createSourceDeferred([&] {
        return [&] { result = ngRequireObject(processor)->autoMesh(part, mesh); };
    });
    while (fiber->state() != TaskFiber::State::term) fiber->call();
    return result;
}

void requireAutoMeshOutput(nijigenerate::compat::RetainedString processorId,
    std::shared_ptr<nijigenerate::viewport::common::mesh::IncMesh> mesh, std::size_t minVertices) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    const bool present = mesh != nullptr;
    require(present, concatenateSourceStrings({RetainedString::fromStaticLiteral("AutoMesh should return a mesh: "), processorId}));
    const bool enoughVertices = ngRequireObject(mesh)->vertices.size() >= minVertices;
    require(enoughVertices, concatenateSourceStrings({RetainedString::fromStaticLiteral("AutoMesh should create expected vertices for "), processorId}));
    for (auto vertex : ngRequireObject(mesh)->vertices) {
        const bool vertexPresent = vertex != nullptr;
        require(vertexPresent, concatenateSourceStrings({RetainedString::fromStaticLiteral("AutoMesh should not create null vertices for "), processorId}));
        const bool notNaN = ngRequireObject(vertex)->position.x == ngRequireObject(vertex)->position.x &&
            ngRequireObject(vertex)->position.y == ngRequireObject(vertex)->position.y;
        require(notNaN, concatenateSourceStrings({RetainedString::fromStaticLiteral("AutoMesh should not create NaN vertices for "), processorId}));
    }
}

void testAutoMeshGridProcessorDeterministicOutput() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::viewport::vertex::automesh::meta::IAutoMeshReflect;
    resetCase();
    auto processor = findAutoMeshProcessor(RetainedString::fromStaticLiteral("grid"));
    require(processor != nullptr, RetainedString::fromStaticLiteral("grid AutoMesh processor should exist"));
    auto reflect = std::dynamic_pointer_cast<IAutoMeshReflect>(processor);
    require(reflect != nullptr, RetainedString::fromStaticLiteral("grid AutoMesh processor should be reflectable"));
    ngRequireObject(reflect)->writeValues(RetainedString::fromStaticLiteral("Simple"),
        RetainedString::fromStaticLiteral("{\"x_segments\":2,\"y_segments\":2,\"margin\":0}"));
    auto mesh = runAutoMeshOnAlphaPart(RetainedString::fromStaticLiteral("grid"));
    requireAutoMeshOutput(RetainedString::fromStaticLiteral("grid"), mesh, 9);
    require(ngRequireObject(mesh)->axes.size() == 2, RetainedString::fromStaticLiteral("grid AutoMesh should produce two grid axes"));
    require(ngRequireObject(mesh)->axes[0].size() == 3 && ngRequireObject(mesh)->axes[1].size() == 3,
        RetainedString::fromStaticLiteral("grid AutoMesh should honor 2x2 segment settings"));
}

void testAutoMeshContourProcessorDeterministicOutput() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::viewport::vertex::automesh::meta::IAutoMeshReflect;
    resetCase();
    auto processor = findAutoMeshProcessor(RetainedString::fromStaticLiteral("contour"));
    require(processor != nullptr, RetainedString::fromStaticLiteral("contour AutoMesh processor should exist"));
    auto reflect = std::dynamic_pointer_cast<IAutoMeshReflect>(processor);
    require(reflect != nullptr, RetainedString::fromStaticLiteral("contour AutoMesh processor should be reflectable"));
    ngRequireObject(reflect)->writeValues(RetainedString::fromStaticLiteral("Simple"),
        RetainedString::fromStaticLiteral("{\"sampling_step\":12,\"mask_threshold\":1}"));
    ngRequireObject(reflect)->writeValues(RetainedString::fromStaticLiteral("Advanced"),
        RetainedString::fromStaticLiteral("{\"min_distance\":4,\"max_distance\":24,\"scales\":[1.0,0.5,0.0]}"));
    auto mesh = runAutoMeshOnAlphaPart(RetainedString::fromStaticLiteral("contour"));
    requireAutoMeshOutput(RetainedString::fromStaticLiteral("contour"), mesh, 3);
}

void testAutoMeshSkeletonProcessorDeterministicOutput() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::viewport::vertex::automesh::meta::IAutoMeshReflect;
    resetCase();
    auto processor = findAutoMeshProcessor(RetainedString::fromStaticLiteral("skeleton"));
    require(processor != nullptr, RetainedString::fromStaticLiteral("skeleton AutoMesh processor should exist"));
    auto reflect = std::dynamic_pointer_cast<IAutoMeshReflect>(processor);
    require(reflect != nullptr, RetainedString::fromStaticLiteral("skeleton AutoMesh processor should be reflectable"));
    ngRequireObject(reflect)->writeValues(RetainedString::fromStaticLiteral("Simple"),
        RetainedString::fromStaticLiteral("{\"mask_threshold\":1,\"target_point_count\":6}"));
    auto mesh = runAutoMeshOnAlphaPart(RetainedString::fromStaticLiteral("skeleton"));
    requireAutoMeshOutput(RetainedString::fromStaticLiteral("skeleton"), mesh, 2);
}

void testAutoMeshOptimumProcessorDeterministicOutput() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::viewport::vertex::automesh::meta::IAutoMeshReflect;
    resetCase();
    auto processor = findAutoMeshProcessor(RetainedString::fromStaticLiteral("optimum"));
    require(processor != nullptr, RetainedString::fromStaticLiteral("optimum AutoMesh processor should exist"));
    auto reflect = std::dynamic_pointer_cast<IAutoMeshReflect>(processor);
    require(reflect != nullptr, RetainedString::fromStaticLiteral("optimum AutoMesh processor should be reflectable"));
    ngRequireObject(reflect)->writeValues(RetainedString::fromStaticLiteral("Simple"),
        RetainedString::fromStaticLiteral("{\"scales\":[0.5,0.0],\"min_distance\":4,\"mask_threshold\":1,\"div_per_part\":8}"));
    ngRequireObject(reflect)->writeValues(RetainedString::fromStaticLiteral("Advanced"),
        RetainedString::fromStaticLiteral("{\"large_threshold\":50,\"length_threshold\":20,\"ratio_threshold\":0.05}"));
    auto mesh = runAutoMeshOnAlphaPart(RetainedString::fromStaticLiteral("optimum"));
    requireAutoMeshOutput(RetainedString::fromStaticLiteral("optimum"), mesh, 3);
}
