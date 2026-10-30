// Existing regression.d2032-2081; include inside regression::detail after helpers.
void testSelectorQueryAndTreeStore() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::GridDeformer;
    using namespace nijigenerate::core::selector;
    resetCase();
    ensureRegressionNodeTypesRegistered();
    auto body = Node::createSourceDeferred([] {
        return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
    });
    body->setName(NullableString::fromStaticLiteral("Body"));
    auto face = Node::createSourceDeferred([&] { return body; });
    face->setName(NullableString::fromStaticLiteral("Face"));
    auto grid = GridDeformer::createSourceDeferred([&] { return body; });
    grid->setName(NullableString::fromStaticLiteral("Grid"));
    auto part = newMeshPart(RetainedString::fromStaticLiteral("PartA"));
    body->addChild(part);
    auto param = nijigenerate::ext::ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("ParamA"); }, [] { return false; });
    ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
    auto binding = newValueBinding(param, face, RetainedString::fromStaticLiteral("transform.t.x"), true);
    binding->setValue({1, 0}, 0.5f);
    auto allNodes = runSelector(RetainedString::fromStaticLiteral("*"));
    require(resourceNamed(allNodes, RetainedString::fromStaticLiteral("Body"), ResourceType::Node),
        RetainedString::fromStaticLiteral("selector * should include Body node"));
    require(resourceNamed(allNodes, RetainedString::fromStaticLiteral("Face"), ResourceType::Node),
        RetainedString::fromStaticLiteral("selector * should include Face node"));
    require(resourceNamed(allNodes, RetainedString::fromStaticLiteral("Grid"), ResourceType::Node),
        RetainedString::fromStaticLiteral("selector * should include GridDeformer node"));
    require(resourceNamed(allNodes, RetainedString::fromStaticLiteral("PartA"), ResourceType::Node),
        RetainedString::fromStaticLiteral("selector * should include Part node"));
    require(resourceNamed(allNodes, RetainedString::fromStaticLiteral("ParamA"), ResourceType::Parameter),
        RetainedString::fromStaticLiteral("selector * should include parameters"));
    auto directGrid = runSelector(RetainedString::fromStaticLiteral("Node.Body > GridDeformer.Grid"));
    require(directGrid.size() == 1,
        RetainedString::fromStaticLiteral("selector should find direct GridDeformer child"));
    require(ngRequireObject(directGrid[0])->name() == "Grid" &&
        ngRequireObject(directGrid[0])->typeId() == "GridDeformer",
        RetainedString::fromStaticLiteral("selector direct child should return GridDeformer"));
    auto attrFace = runSelector(RetainedString::fromStaticLiteral("*[name=\"Face\"]"));
    require(attrFace.size() == 1 && ngRequireObject(attrFace[0])->name() == "Face",
        RetainedString::fromStaticLiteral("selector attribute name query should find Face"));
    auto boundParam = runSelector(RetainedString::fromStaticLiteral("Node.Face Parameter"));
    require(boundParam.size() == 1 && ngRequireObject(boundParam[0])->name() == "ParamA",
        RetainedString::fromStaticLiteral("selector should find parameter bound to Face"));
    auto boundBinding = runSelector(RetainedString::fromStaticLiteral("Node.Face Binding"));
    require(boundBinding.size() == 1 && ngRequireObject(boundBinding[0])->type() == ResourceType::Binding,
        RetainedString::fromStaticLiteral("selector should find binding bound to Face"));
    auto parameterBinding = runSelector(RetainedString::fromStaticLiteral("Parameter.ParamA Binding"));
    require(parameterBinding.size() == 1 && ngRequireObject(parameterBinding[0])->type() == ResourceType::Binding,
        RetainedString::fromStaticLiteral("selector should walk from parameter to binding"));
    auto treeResources = runSelector(RetainedString::fromStaticLiteral("Node.Body, GridDeformer.Grid"));
    auto store = std::allocate_shared<TreeStore_<false>>(
        nicxlive::core::d_runtime::SourceAllocator<TreeStore_<false>>{});
    store->setResources(treeResources);
    require(store->roots.size() == 1,
        RetainedString::fromStaticLiteral("tree store should preserve selected ancestor as root"));
    require(ngRequireObject(store->roots[0])->name() == "Body",
        RetainedString::fromStaticLiteral("tree store root should be selected Body ancestor"));
    require(store->children.contains(store->roots[0]),
        RetainedString::fromStaticLiteral("tree store should register root children"));
    {
        const auto found = store->children.find(store->roots[0]);
        if (found == store->children.end()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), 2079);
        require(found->second.size() == 1,
            RetainedString::fromStaticLiteral("tree store should attach selected grid under Body"));
    }
    {
        const auto found = store->children.find(store->roots[0]);
        if (found == store->children.end()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), 2080);
        require(ngRequireObject(found->second[0])->name() == "Grid",
            RetainedString::fromStaticLiteral("tree store child should be Grid"));
    }
}
