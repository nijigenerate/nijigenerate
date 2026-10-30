// Complete original regression.d18699-18734, same module-private namespace.
void testAutoMeshAlphaProviderAndNonPartCachedInput() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::MeshData;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::viewport::vertex::automesh::PartAlphaProvider;
    using nijigenerate::viewport::common::mesh::IncMesh;
    using namespace nijigenerate::viewport::vertex::automesh::common;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto part = newAutoMeshAlphaPart(text("automesh-alpha-provider"));
    auto provider = PartAlphaProvider::createSource(part);
    nijigenerate::compat::ngRunWithScopeExit([&] {
        check(provider->width() == 48 && provider->height() == 48, "PartAlphaProvider should expose texture dimensions");
        check(provider->alphaPtr() != nullptr, "PartAlphaProvider should expose alpha data");
        auto input = alphaInputFromProviderWithImage(provider);
        check(input.w == 48 && input.h == 48 && input.img != nullptr,
            "alphaInputFromProviderWithImage should create image-backed input");
        bool hasOpaquePixel = false;
        for (auto pixel : input.img->data()) if (pixel == static_cast<std::uint8_t>(255)) { hasOpaquePixel = true; break; }
        check(hasOpaquePixel, "alpha image should preserve opaque pixels");
        auto processor = findAutoMeshProcessor(text("grid"));
        check(processor != nullptr, "grid AutoMesh processor should exist");
        auto grid = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        grid->setName(text("automesh-grid-target").toNullableString());
        AlphaInputCache cache;
        cache[grid->uuid] = input;
        setAutoMeshAlphaInputCache(&cache);
        nijigenerate::compat::ngRunWithScopeExit([&] {
            auto data = std::allocate_shared<MeshData>(SourceAllocator<MeshData>{});
            auto mesh = IncMesh::create(data);
            auto result = processor->autoMesh(grid, mesh);
            requireAutoMeshOutput(text("grid-cached-non-part"), result, 4);
        }, [&] { clearAutoMeshAlphaInputCache(&cache); });
    }, [&] { provider->dispose(); });
}

void testAutoMeshBatchConfigUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using nijigenerate::commands::Context;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::automesh::config::AutoMeshSetValuesCommand;
    using nijigenerate::viewport::vertex::automesh::meta::IAutoMeshReflect;
    namespace actions = nijigenerate::core::actionstack;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto processor = findAutoMeshProcessor(text("grid"));
    check(processor != nullptr, "grid AutoMesh processor should exist");
    auto reflect = std::dynamic_pointer_cast<IAutoMeshReflect>(processor);
    check(reflect != nullptr, "grid AutoMesh processor should be reflectable");
    auto oldSimple = reflect->values(text("Simple"));
    auto oldAdvanced = reflect->values(text("Advanced"));
    auto updates = oldSimple.view().find("\"x_segments\":4.0") != std::string_view::npos ||
        oldSimple.view().find("\"x_segments\":4") != std::string_view::npos
        ? text("{\"x_segments\":5,\"y_segments\":4,\"margin\":0.3}")
        : text("{\"x_segments\":4,\"y_segments\":3,\"margin\":0.2}");
    auto context = std::allocate_shared<Context>(SourceAllocator<Context>{});
    auto setResult = constructSourceCommand<AutoMeshSetValuesCommand>(text("grid"), text("Simple"), updates)->run(context);
    check(setResult->succeeded, "AutoMeshSetValuesCommand should succeed");
    check(actions::incActionHistory().size() == 1, "AutoMesh config update should push one undo entry");
    auto newSimple = reflect->values(text("Simple"));
    auto newAdvanced = reflect->values(text("Advanced"));
    check(newSimple != oldSimple, "AutoMesh simple config should change");
    check(newAdvanced != oldAdvanced, "AutoMesh derived advanced config should change with grid segments");
    actions::incActionUndo();
    check(jsonEquivalent(reflect->values(text("Simple")), oldSimple), "undo AutoMesh config should restore simple values");
    check(jsonEquivalent(reflect->values(text("Advanced")), oldAdvanced),
        "undo AutoMesh config should restore derived advanced values");
    actions::incActionRedo();
    check(jsonEquivalent(reflect->values(text("Simple")), newSimple), "redo AutoMesh config should restore simple values");
    check(jsonEquivalent(reflect->values(text("Advanced")), newAdvanced),
        "redo AutoMesh config should restore derived advanced values");
}
