// Complete existing regression.d2370-2399 only.
void testProjectTextureMaintenanceCommands() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::Texture;
    using nicxlive::core::SourceTextureBytes;
    using nicxlive::core::nodes::MeshData;
    using nicxlive::core::nodes::Part;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::puppet::tool;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    SourceTextureBytes pixels{200, 100, 50, 128};
    auto texture = Texture::createDataSourceDeferred([&] { return pixels.duplicate(); },
        [] { return 1; }, [] { return 1; }, [] { return 4; }, [] { return 4; },
        [] { return false; }, [] { return false; });
    texture->lock();
    texture->setData(pixels.duplicate());
    MeshData data;
    data.vertices = nicxlive::core::math::ngVecaFromVectorsSource(
        nicxlive::common::SharedArray<Vec2>{Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1}});
    data.uvs = nicxlive::core::math::ngVecaFromVectorsSource(
        nicxlive::common::SharedArray<Vec2>{Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1}});
    data.indices = {0, 1, 2};
    auto part = Part::createDataSourceDeferred([&] { return data; },
        [&] { return nicxlive::common::SharedArray<std::shared_ptr<Texture>>{texture}; },
        [] { return std::uint32_t{0}; }, [] { return nijigenerate::project::incActivePuppet()->root; });
    part->setName(nicxlive::core::common::NullableString::fromStaticLiteral("texture-maintenance-part"));
    nijigenerate::project::incActivePuppet()->populateTextureSlots();
    check(nijigenerate::project::incActivePuppet()->textureSlots.size() == 1,
        "generated part should populate one texture slot");
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    auto premult = constructSourceCommand<PremultTextureCommand>()->run(ctx);
    check(premult->succeeded, "PremultTextureCommand should succeed");
    auto premultiplied = texture->getSourceTextureData();
    check(premultiplied[0] == static_cast<std::uint8_t>(200 * 128 / 255), "premultiply should update red channel");
    check(premultiplied[1] == static_cast<std::uint8_t>(100 * 128 / 255), "premultiply should update green channel");
    check(premultiplied[2] == static_cast<std::uint8_t>(50 * 128 / 255), "premultiply should update blue channel");
    check(premultiplied[3] == 128, "premultiply should preserve alpha channel");
    auto mipmaps = constructSourceCommand<RegenerateMipmapsCommand>()->run(ctx);
    check(mipmaps->succeeded, "RegenerateMipmapsCommand should succeed");
    auto rebleed = constructSourceCommand<RebleedTextureCommand>()->run(ctx);
    check(rebleed->succeeded, "RebleedTextureCommand should enqueue work and succeed");
    drainRegressionTasks();
    texture->unlock();
}

void testProjectRepairMaintenanceCommands() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::Texture;
    using nicxlive::core::SourceTextureBytes;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::commands::puppet::tool;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    resetCase();
    auto textureA = Texture::createDataSourceDeferred(
        [] { return SourceTextureBytes{255, 0, 0, 255}; },
        [] { return 1; }, [] { return 1; }, [] { return 4; }, [] { return 4; },
        [] { return false; }, [] { return false; });
    auto textureB = Texture::createDataSourceDeferred(
        [] { return SourceTextureBytes{0, 255, 0, 255}; },
        [] { return 1; }, [] { return 1; }, [] { return 4; }, [] { return 4; },
        [] { return false; }, [] { return false; });
    auto partA = nijigenerate::ext::nodes::incCreateExPart(textureA,
        nijigenerate::project::incActivePuppet()->root, NullableString::fromStaticLiteral("repair-part-a"));
    auto partB = nijigenerate::ext::nodes::incCreateExPart(textureB,
        nijigenerate::project::incActivePuppet()->root, NullableString::fromStaticLiteral("repair-part-b"));
    partA->layerPath = text("");
    partB->layerPath = text("");
    auto oldRootId = nijigenerate::project::incActivePuppet()->root->uuid;
    auto oldPartAId = partA->uuid;
    auto oldPartBId = partB->uuid;
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    auto fakeNames = constructSourceCommand<GenerateFakeLayerNameCommand>()->run(ctx);
    check(fakeNames->succeeded, "GenerateFakeLayerNameCommand should succeed");
    const bool firstPath = partA->layerPath.view() ==
        concatenateSourceStrings({text("/"), Text(partA->getName())}).view();
    check(firstPath, "fake layer names should update first part path");
    const bool secondPath = partB->layerPath.view() ==
        concatenateSourceStrings({text("/"), Text(partB->getName())}).view();
    check(secondPath, "fake layer names should update second part path");
    auto repair = constructSourceCommand<AttemptRepairPuppetCommand>()->run(ctx);
    check(repair->succeeded, "AttemptRepairPuppetCommand should succeed on generated puppet");
    auto regenIds = constructSourceCommand<RegenerateNodeIDsCommand>()->run(ctx);
    check(regenIds->succeeded, "RegenerateNodeIDsCommand should succeed");
    check(nijigenerate::project::incActivePuppet()->root->uuid != oldRootId,
        "RegenerateNodeIDsCommand should change root UUID");
    check(partA->uuid != oldPartAId, "RegenerateNodeIDsCommand should change first part UUID");
    check(partB->uuid != oldPartBId, "RegenerateNodeIDsCommand should change second part UUID");
    check(partA->uuid != partB->uuid, "RegenerateNodeIDsCommand should keep regenerated UUIDs unique");
}
