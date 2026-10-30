// Existing regression.d2170-2208,2402-2604,2745-2833,3168-3217,4241-4275; include inside detail after helpers.
void testProjectCameraViewportRoundTrip() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::ext::nodes::ExCamera;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    resetCase();
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-camera-viewport")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array saveSegments{fixtureDir, RetainedString::fromStaticLiteral("camera-viewport")};
        auto saveBase = ngBuildPath(saveSegments);
        auto savePath = concatenateSourceStrings({saveBase, RetainedString::fromStaticLiteral(".inx")});
        auto camera = ExCamera::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        camera->setName(NullableString::fromStaticLiteral("camera-viewport-roundtrip"));
        camera->setViewport(Vec2{1280, 720});
        auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
        ctx->puppet(nijigenerate::project::incActivePuppet());
        require(ngRequireObject(constructSourceCommand<SaveFileCommand>(saveBase)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("SaveFileCommand should save camera viewport fixture"));
        require(ngFileExists(savePath.view()) && ngIsFile(savePath.view()),
            RetainedString::fromStaticLiteral("camera viewport fixture should exist"));
        nijigenerate::project::incNewProject();
        require(findNodeRecursive(ngRequireObject(nijigenerate::project::incActivePuppet())->root,
            "camera-viewport-roundtrip") == nullptr,
            RetainedString::fromStaticLiteral("new project should clear camera viewport fixture"));
        require(ngRequireObject(constructSourceCommand<OpenFileCommand>(savePath)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("OpenFileCommand should load camera viewport fixture"));
        auto loadedCamera = std::dynamic_pointer_cast<ExCamera>(findNodeRecursive(
            ngRequireObject(nijigenerate::project::incActivePuppet())->root, "camera-viewport-roundtrip"));
        require(loadedCamera != nullptr,
            RetainedString::fromStaticLiteral("INX round-trip should restore Camera node"));
        require(nearVec2(ngRequireObject(loadedCamera)->getViewport(), Vec2{1280, 720}),
            RetainedString::fromStaticLiteral("INX round-trip should restore Camera viewport"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

void testProjectNewSaveOpenCommandPaths() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::AddNodeCommand;
    using nijigenerate::commands::node::node::SetNodeNameCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-save-open")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array saveSegments{fixtureDir, RetainedString::fromStaticLiteral("roundtrip")};
        auto saveBase = ngBuildPath(saveSegments);
        auto savePath = concatenateSourceStrings({saveBase, RetainedString::fromStaticLiteral(".inx")});
        if (ngFileExists(savePath.view()) && ngIsFile(savePath.view()))
            nijigenerate::platform::ngAutosaveRemove(RetainedStringSlice(savePath));
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(nijigenerate::project::incActivePuppet());
        auto addResult = constructSourceCommand<AddNodeCommand>(RetainedString::fromStaticLiteral("Node"))->run(ctx);
        require(ngRequireObject(addResult)->succeeded,
            RetainedString::fromStaticLiteral("AddNodeCommand should succeed before save"));
        auto node = ngRequireObject(addResult)->created[0];
        ctx->nodes(Context::Nodes{node});
        struct NameArgument {
            operator Slice<RetainedString>() && {
                return Slice<RetainedString>{RetainedString::fromStaticLiteral("Roundtrip Node")};
            }
        };
        require(ngRequireObject(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{},
            NameArgument{})->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("SetNodeNameCommand should succeed before save"));
        require(incActionIsModified(),
            RetainedString::fromStaticLiteral("node edits should mark project modified before save"));
        require(ngRequireObject(constructSourceCommand<SaveFileCommand>(saveBase)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("SaveFileCommand should succeed"));
        require(ngFileExists(savePath.view()) && ngIsFile(savePath.view()),
            RetainedString::fromStaticLiteral("SaveFileCommand should write an .inx file"));
        require(!incActionIsModified(),
            RetainedString::fromStaticLiteral("manual save should mark current action index as saved"));
        nijigenerate::project::incNewProject();
        require(findDirectNode(nijigenerate::project::incActivePuppet(), "Roundtrip Node") == nullptr,
            RetainedString::fromStaticLiteral("new project should clear saved node before open"));
        auto openResult = constructSourceCommand<OpenFileCommand>(savePath)->run(ctx);
        require(ngRequireObject(openResult)->succeeded,
            RetainedString::fromStaticLiteral("OpenFileCommand should succeed"));
        require(findDirectNode(nijigenerate::project::incActivePuppet(), "Roundtrip Node") != nullptr,
            RetainedString::fromStaticLiteral("OpenFileCommand should restore saved node"));
        require(!incActionIsModified(),
            RetainedString::fromStaticLiteral("open project should start with a clean action history"));
        require(ngRequireObject(nijigenerate::project::incActiveProject())->path == savePath,
            RetainedString::fromStaticLiteral("opened project path should be the saved .inx path"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

void testPuppetInspectorStateRoundTrip() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using namespace nijigenerate::project;
    resetCase();
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-puppet-inspector")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto part = newMeshPart(RetainedString::fromStaticLiteral("puppet-inspector-texture-part"));
        ngRequireObject(ngRequireObject(incActivePuppet())->meta)->name = "Inspector Puppet";
        ngRequireObject(ngRequireObject(incActivePuppet())->meta)->artist = "Artist A, Artist B";
        ngRequireObject(ngRequireObject(incActivePuppet())->meta)->rigger = "Rigger A";
        ngRequireObject(ngRequireObject(incActivePuppet())->meta)->contact = "contact@example.invalid";
        ngRequireObject(ngRequireObject(incActivePuppet())->meta)->licenseURL = "https://example.invalid/license";
        ngRequireObject(ngRequireObject(incActivePuppet())->meta)->copyright = "Copyright Fixture";
        ngRequireObject(ngRequireObject(incActivePuppet())->meta)->reference = "https://example.invalid/source";
        ngRequireObject(ngRequireObject(incActivePuppet())->meta)->preservePixels = true;
        ngRequireObject(ngRequireObject(incActivePuppet())->physics)->pixelsPerMeter = 123.0f;
        ngRequireObject(ngRequireObject(incActivePuppet())->physics)->gravity = 8.5f;
        ngRequireObject(incActivePuppet())->populateTextureSlots();
        ngRequireObject(incActivePuppet())->updateTextureState();
        require(ngRequireObject(incActivePuppet())->getRootParts().size() == 1,
            RetainedString::fromStaticLiteral("puppet inspector fixture should expose root part count"));
        require(ngRequireObject(incActivePuppet())->textureSlots.size() == 1 &&
            ngRequireObject(incActivePuppet())->textureSlots[0] == part->textures[0],
            RetainedString::fromStaticLiteral("puppet inspector fixture should populate texture atlas slots"));
        const std::array saveSegments{fixtureDir, RetainedString::fromStaticLiteral("puppet-inspector")};
        auto saveBase = ngBuildPath(saveSegments);
        auto savePath = concatenateSourceStrings({saveBase, RetainedString::fromStaticLiteral(".inx")});
        auto ctx = std::allocate_shared<Context>(nicxlive::core::d_runtime::SourceAllocator<Context>{});
        require(ngRequireObject(constructSourceCommand<SaveFileCommand>(saveBase)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("puppet inspector fixture should save"));
        incNewProject();
        require(ngRequireObject(constructSourceCommand<OpenFileCommand>(savePath)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("puppet inspector fixture should load"));
        require(ngRequireObject(ngRequireObject(incActivePuppet())->meta)->name == "Inspector Puppet",
            RetainedString::fromStaticLiteral("puppet metadata name should round-trip"));
        require(ngRequireObject(ngRequireObject(incActivePuppet())->meta)->artist == "Artist A, Artist B",
            RetainedString::fromStaticLiteral("puppet metadata artists should round-trip"));
        require(ngRequireObject(ngRequireObject(incActivePuppet())->meta)->rigger == "Rigger A",
            RetainedString::fromStaticLiteral("puppet metadata riggers should round-trip"));
        require(ngRequireObject(ngRequireObject(incActivePuppet())->meta)->contact == "contact@example.invalid",
            RetainedString::fromStaticLiteral("puppet metadata contact should round-trip"));
        require(ngRequireObject(ngRequireObject(incActivePuppet())->meta)->licenseURL == "https://example.invalid/license",
            RetainedString::fromStaticLiteral("puppet metadata license URL should round-trip"));
        require(ngRequireObject(ngRequireObject(incActivePuppet())->meta)->copyright == "Copyright Fixture",
            RetainedString::fromStaticLiteral("puppet metadata copyright should round-trip"));
        require(ngRequireObject(ngRequireObject(incActivePuppet())->meta)->reference == "https://example.invalid/source",
            RetainedString::fromStaticLiteral("puppet metadata origin/reference should round-trip"));
        require(ngRequireObject(ngRequireObject(incActivePuppet())->meta)->preservePixels,
            RetainedString::fromStaticLiteral("puppet preserve-pixels setting should round-trip"));
        require(near(ngRequireObject(ngRequireObject(incActivePuppet())->physics)->pixelsPerMeter, 123.0f),
            RetainedString::fromStaticLiteral("puppet physics pixels-per-meter should round-trip"));
        require(near(ngRequireObject(ngRequireObject(incActivePuppet())->physics)->gravity, 8.5f),
            RetainedString::fromStaticLiteral("puppet physics gravity should round-trip"));
        require(ngRequireObject(incActivePuppet())->textureSlots.size() == 1,
            RetainedString::fromStaticLiteral("loaded puppet should restore texture slots"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

void testRenderCameraExportCommands() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::ext::nodes::ExCamera;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::ShallowTexture;
    resetCase();
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-render-camera")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        struct MissingPathArgument {
            const RetainedString& directory;
            operator RetainedString() && {
                const std::array segments{directory, RetainedString::fromStaticLiteral("missing-camera")};
                return ngBuildPath(segments);
            }
        };
        auto missing = std::allocate_shared<ExportPNGCommand>(SourceAllocator<ExportPNGCommand>{},
            MissingPathArgument{fixtureDir}, RetainedString::fromStaticLiteral("missing"))->run(
                std::allocate_shared<Context>(SourceAllocator<Context>{}));
        require(!ngRequireObject(missing)->succeeded,
            RetainedString::fromStaticLiteral("camera export should reject a missing camera"));
        auto camera = ExCamera::createSourceDeferred([] {
            return ngRequireObject(nijigenerate::project::incActivePuppet())->root;
        });
        camera->setName(NullableString::fromStaticLiteral("small-camera"));
        camera->setViewport(Vec2{8, 6});
        camera->localTransform.translation = Vec3{4, -3, 0};
        camera->localTransform.scale = Vec2{2, 0.5f};
        ngRequireObject(ngRequireObject(nijigenerate::project::incActivePuppet())->root)->build();
        const auto viewport = camera->getViewport();
        require(viewport.x == 8 && viewport.y == 6,
            RetainedString::fromStaticLiteral("ExCamera should expose configured viewport dimensions"));
        const std::array pngSegments{fixtureDir, RetainedString::fromStaticLiteral("camera-export")};
        auto pngBase = ngBuildPath(pngSegments);
        auto png = constructSourceCommand<ExportPNGCommand>(pngBase,
            RetainedString::fromStaticLiteral("small-camera"), true, false)->run(
                std::allocate_shared<Context>(SourceAllocator<Context>{}));
        require(ngRequireObject(png)->succeeded,
            RetainedString::fromStaticLiteral("ExportPNGCommand should succeed with a named camera"));
        auto pngImage = ShallowTexture(concatenateSourceStrings({pngBase,
            RetainedString::fromStaticLiteral(".png")}), 4);
        require(pngImage.width == 8 && pngImage.height == 6,
            RetainedString::fromStaticLiteral("ExportPNGCommand should use the camera viewport dimensions"));
        const std::array tgaSegments{fixtureDir, RetainedString::fromStaticLiteral("camera-export")};
        auto tgaBase = ngBuildPath(tgaSegments);
        auto tga = constructSourceCommand<ExportTGACommand>(tgaBase,
            RetainedString::fromStaticLiteral("small-camera"), true, false)->run(
                std::allocate_shared<Context>(SourceAllocator<Context>{}));
        require(ngRequireObject(tga)->succeeded,
            RetainedString::fromStaticLiteral("ExportTGACommand should succeed with a named camera"));
        auto tgaImage = ShallowTexture(concatenateSourceStrings({tgaBase,
            RetainedString::fromStaticLiteral(".tga")}), 4);
        require(tgaImage.width == 8 && tgaImage.height == 6,
            RetainedString::fromStaticLiteral("ExportTGACommand should use the camera viewport dimensions"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

void testNativeSavePathOverwriteAndReload() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::node::node::AddNodeCommand;
    using nijigenerate::commands::node::node::SetNodeNameCommand;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::io::save::CloseAskHandler;
    using nijigenerate::io::save::incFileSave;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::project;
    using namespace nijigenerate::core::actionstack;
    resetCase();
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-native-save")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array saveSegments{fixtureDir, RetainedString::fromStaticLiteral("native-save")};
        auto saveBase = ngBuildPath(saveSegments);
        auto savePath = concatenateSourceStrings({saveBase, RetainedString::fromStaticLiteral(".inx")});
        auto swapPath = concatenateSourceStrings({savePath, RetainedString::fromStaticLiteral(".swp")});
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(incActivePuppet());
        auto first = ngRequireObject(constructSourceCommand<AddNodeCommand>(
            RetainedString::fromStaticLiteral("Node"))->run(ctx))->created[0];
        ctx->nodes(Context::Nodes{first});
        struct FirstNameArgument {
            operator Slice<RetainedString>() && {
                return Slice<RetainedString>{RetainedString::fromStaticLiteral("first-saved-node")};
            }
        };
        require(ngRequireObject(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{},
            FirstNameArgument{})->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("first saved node rename should succeed"));
        require(ngRequireObject(constructSourceCommand<SaveFileCommand>(saveBase)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("initial native save should succeed"));
        require(incProjectPath() == saveBase,
            RetainedString::fromStaticLiteral("native save should keep project path without implicit extension"));
        require(ngFileExists(savePath.view()) && ngIsFile(savePath.view()),
            RetainedString::fromStaticLiteral("native save should write .inx file"));
        require(!ngFileExists(swapPath.view()),
            RetainedString::fromStaticLiteral("native save should rename away .swp file"));
        require(!incActionIsModified(),
            RetainedString::fromStaticLiteral("native save should mark action index clean"));
        ctx->nodes(Context::Nodes{});
        ctx->hasNodes(false);
        auto second = ngRequireObject(constructSourceCommand<AddNodeCommand>(
            RetainedString::fromStaticLiteral("Node"))->run(ctx))->created[0];
        ctx->nodes(Context::Nodes{second});
        struct SecondNameArgument {
            operator Slice<RetainedString>() && {
                return Slice<RetainedString>{RetainedString::fromStaticLiteral("second-saved-node")};
            }
        };
        require(ngRequireObject(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{},
            SecondNameArgument{})->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("second saved node rename should succeed"));
        require(incActionIsModified(),
            RetainedString::fromStaticLiteral("post-save edit should mark project dirty before overwrite"));
        require(ngRequireObject(constructSourceCommand<SaveFileCommand>(saveBase)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("overwrite native save should succeed"));
        require(!ngFileExists(swapPath.view()),
            RetainedString::fromStaticLiteral("overwrite save should leave no .swp file"));
        require(!incActionIsModified(),
            RetainedString::fromStaticLiteral("overwrite save should mark action index clean"));
        incNewProject();
        require(findDirectNode(incActivePuppet(), "first-saved-node") == nullptr,
            RetainedString::fromStaticLiteral("new project should clear first saved node"));
        require(ngRequireObject(constructSourceCommand<OpenFileCommand>(savePath)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("reload overwritten native save should succeed"));
        require(findDirectNode(incActivePuppet(), "first-saved-node") != nullptr,
            RetainedString::fromStaticLiteral("reload should preserve first saved node"));
        require(findDirectNode(incActivePuppet(), "second-saved-node") != nullptr,
            RetainedString::fromStaticLiteral("reload should include overwritten second node"));
        auto puppet = incActivePuppet();
        auto preservedPath = incProjectPath();
        nijigenerate::platform::ngAutosaveRemove(RetainedStringSlice(savePath));
        ngMkdirRecurse(savePath.view());
        const std::array sentinelSegments{savePath, RetainedString::fromStaticLiteral("sentinel")};
        ngFileWrite(RetainedStringSlice(ngBuildPath(sentinelSegments)), "preserve");
        ctx->puppet(puppet);
        ctx->nodes(Context::Nodes{findDirectNode(puppet, "second-saved-node")});
        struct UnsavedNameArgument {
            operator Slice<RetainedString>() && {
                return Slice<RetainedString>{RetainedString::fromStaticLiteral("unsaved-node")};
            }
        };
        require(ngRequireObject(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{},
            UnsavedNameArgument{})->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("unsaved edit should succeed"));
        require(!incFileSave(),
            RetainedString::fromStaticLiteral("Save action must report a failed final rename"));
        require(incActionIsModified() && incProjectPath() == preservedPath && incActivePuppet() == puppet,
            RetainedString::fromStaticLiteral("Failed save must preserve the dirty model and its project path"));
        class ObservedCloseHandler : public CloseAskHandler {
        public:
            bool closed = false;
            void onProjectClose() override { closed = true; }
        };
        auto closeHandler = std::allocate_shared<ObservedCloseHandler>(SourceAllocator<ObservedCloseHandler>{});
        closeHandler->onClickYes();
        require(!closeHandler->closed && incActivePuppet() == puppet && incActionIsModified(),
            RetainedString::fromStaticLiteral("Save-before-close must keep the project open when saving fails"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

void testProjectImportImagesCommandPaths() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nicxlive::core::d_runtime::SourceAllocator;
    resetCase();
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-image-import")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array imageASegments{fixtureDir, RetainedString::fromStaticLiteral("layer-a.png")};
        auto imageA = ngBuildPath(imageASegments);
        const std::array imageBSegments{fixtureDir, RetainedString::fromStaticLiteral("layer-b.png")};
        auto imageB = ngBuildPath(imageBSegments);
        writeRegressionPng(imageA, 255, 0, 0, 2, 3, 255);
        writeRegressionPng(imageB, 0, 255, 0, 4, 2, 255);
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        require(ngRequireObject(constructSourceCommand<ImportImageFolderCommand>(fixtureDir)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("ImportImageFolderCommand should succeed"));
        auto imported = nijigenerate::project::incActivePuppet();
        require(imported != nullptr,
            RetainedString::fromStaticLiteral("image-folder import should leave an active puppet"));
        require(findDirectPart(imported, "layer-a") != nullptr,
            RetainedString::fromStaticLiteral("image-folder import should create layer-a part"));
        require(findDirectPart(imported, "layer-b") != nullptr,
            RetainedString::fromStaticLiteral("image-folder import should create layer-b part"));
        auto partA = findDirectPart(imported, "layer-a");
        auto partB = findDirectPart(imported, "layer-b");
        require(ngRequireObject(partA)->textures.size() > 0 && partA->textures[0] != nullptr,
            RetainedString::fromStaticLiteral("imported layer-a should have a texture"));
        require(ngRequireObject(partB)->textures.size() > 0 && partB->textures[0] != nullptr,
            RetainedString::fromStaticLiteral("imported layer-b should have a texture"));
        require(ngRequireObject(ngRequireObject(partA)->textures[0])->width() == 2 &&
            ngRequireObject(partA->textures[0])->height() == 3,
            RetainedString::fromStaticLiteral("layer-a texture dimensions should match source PNG"));
        require(ngRequireObject(ngRequireObject(partB)->textures[0])->width() == 4 &&
            ngRequireObject(partB->textures[0])->height() == 2,
            RetainedString::fromStaticLiteral("layer-b texture dimensions should match source PNG"));
        resetCase();
        struct ImagePathsArgument {
            const RetainedString& first;
            const RetainedString& second;
            operator RetainedString() && {
                return concatenateSourceStrings({first, RetainedString::fromStaticLiteral("|"), second});
            }
        };
        require(ngRequireObject(std::allocate_shared<MergeImageFilesCommand>(SourceAllocator<MergeImageFilesCommand>{},
            ImagePathsArgument{imageA, imageB})->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("MergeImageFilesCommand should succeed"));
        auto merged = nijigenerate::project::incActivePuppet();
        require(findDirectPart(merged, "layer-a.png") != nullptr,
            RetainedString::fromStaticLiteral("merge image files should add layer-a part"));
        require(findDirectPart(merged, "layer-b.png") != nullptr,
            RetainedString::fromStaticLiteral("merge image files should add layer-b part"));
        require(directPartNames(merged).size() == 2,
            RetainedString::fromStaticLiteral("merge image files should create exactly the two fixture parts"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

void testProjectINPImportMergeRoundTripCommandPaths() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::project;
    resetCase();
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-inp-roundtrip")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array exportSegments{fixtureDir, RetainedString::fromStaticLiteral("roundtrip.inp")};
        auto exportPath = ngBuildPath(exportSegments);
        auto sourcePart = newMeshPart(RetainedString::fromStaticLiteral("inp-roundtrip-part"));
        sourcePart->localTransform.translation.x = 12.0f;
        sourcePart->tint = Vec3{0.25f, 0.5f, 0.75f};
        auto sourceParam = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("INPParam"); }, [] { return false; });
        sourceParam->min = Vec2{0, 0};
        sourceParam->max = Vec2{1, 0};
        ngRequireObject(incActivePuppet())->parameters.push_back(sourceParam);
        auto sourceBinding = newValueBinding(sourceParam, sourcePart,
            RetainedString::fromStaticLiteral("transform.t.x"), true);
        ngRequireObject(sourceBinding)->setValue({1, 0}, 18.0f);
        ngRequireObject(ngRequireObject(incActivePuppet())->root)->build();
        ngRequireObject(incActivePuppet())->populateTextureSlots();
        const auto output = nijigenerate::io::ngWriteProjectPuppetMemory(*ngRequireObject(incActivePuppet()));
        ngFileWrite(RetainedStringSlice(exportPath),
            std::string_view(reinterpret_cast<const char*>(output.data()), output.size()));
        require(ngFileExists(ngSetExtension(exportPath, RetainedString::fromStaticLiteral("inp")).view()),
            RetainedString::fromStaticLiteral("generated INP fixture should be written"));
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        auto importResult = constructSourceCommand<ImportINPCommand>(exportPath)->run(ctx);
        require(ngRequireObject(importResult)->succeeded,
            RetainedString::fromStaticLiteral("ImportINPCommand should import generated INP fixture"));
        auto importedPart = findDirectPart(incActivePuppet(), "inp-roundtrip-part");
        require(importedPart != nullptr,
            RetainedString::fromStaticLiteral("INP import should restore exported Part"));
        auto importedParam = findParameter(incActivePuppet(), "INPParam");
        require(importedParam != nullptr,
            RetainedString::fromStaticLiteral("INP import should restore exported parameter"));
        require(ngRequireObject(importedParam)->bindings.size() == 1,
            RetainedString::fromStaticLiteral("INP import should restore exported binding"));
        require(ngRequireObject(ngRequireObject(importedParam)->bindings[0])->getTarget().target.resource() == importedPart,
            RetainedString::fromStaticLiteral("INP import should reconnect binding target"));
        resetCase();
        auto basePart = newMeshPart(RetainedString::fromStaticLiteral("merge-base-part"));
        auto baseParam = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("BaseParam"); }, [] { return false; });
        ngRequireObject(incActivePuppet())->parameters.push_back(baseParam);
        auto mergeResult = constructSourceCommand<MergeINPCommand>(exportPath, true, true)->run(ctx);
        require(ngRequireObject(mergeResult)->succeeded,
            RetainedString::fromStaticLiteral("MergeINPCommand should merge generated INP fixture"));
        require(findDirectPart(incActivePuppet(), "merge-base-part") == basePart,
            RetainedString::fromStaticLiteral("INP merge should preserve existing Part"));
        auto mergedPart = findDirectPart(incActivePuppet(), "inp-roundtrip-part");
        require(mergedPart != nullptr,
            RetainedString::fromStaticLiteral("INP merge should add imported Part"));
        require(findParameter(incActivePuppet(), "BaseParam") == baseParam,
            RetainedString::fromStaticLiteral("INP merge should preserve existing parameter"));
        auto mergedParam = findParameter(incActivePuppet(), "INPParam");
        require(mergedParam != nullptr,
            RetainedString::fromStaticLiteral("INP merge should add imported parameter"));
        require(ngRequireObject(mergedParam)->bindings.size() == 1,
            RetainedString::fromStaticLiteral("INP merge should add imported binding"));
        require(ngRequireObject(ngRequireObject(mergedParam)->bindings[0])->getTarget().target.resource() == mergedPart,
            RetainedString::fromStaticLiteral("INP merge should reconnect imported binding target"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

void testProjectSessionImportCommandPath() {
    using namespace nijigenerate::compat::phobos;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::puppet::tool::ImportSessionDataCommand;
    using nicxlive::core::PuppetExtData;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::project;
    resetCase();
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-session-import")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const auto sessionKey = RetainedString::fromStaticLiteral("com.inochi2d.inochi-session.bindings");
        const std::array sessionSegments{fixtureDir, RetainedString::fromStaticLiteral("session-source.inp")};
        auto sessionPath = ngBuildPath(sessionSegments);
        static constexpr char payload[] = R"({"tracking":"ok","version":1})";
        ngRequireObject(incActivePuppet())->extData[sessionKey] = PuppetExtData::Bytes::staticView(
            reinterpret_cast<std::uint8_t*>(const_cast<char*>(payload)), sizeof(payload) - 1);
        const auto output = nijigenerate::io::ngWriteProjectPuppetMemory(*ngRequireObject(incActivePuppet()));
        ngFileWrite(RetainedStringSlice(sessionPath),
            std::string_view(reinterpret_cast<const char*>(output.data()), output.size()));
        require(ngFileExists(ngSetExtension(sessionPath, RetainedString::fromStaticLiteral("inp")).view()),
            RetainedString::fromStaticLiteral("session source INP should be written"));
        resetCase();
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(incActivePuppet());
        require(ngRequireObject(constructSourceCommand<ImportSessionDataCommand>(sessionPath)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("ImportSessionDataCommand should import session extData"));
        {
            const auto& entries = ngRequireObject(incActivePuppet())->extData;
            require(entries.find(sessionKey) != entries.end(),
                RetainedString::fromStaticLiteral("session import should set extData key"));
        }
        {
            const auto& entries = ngRequireObject(incActivePuppet())->extData;
            const auto found = entries.find(sessionKey);
            if (found == entries.end()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
                RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), 2823);
            const auto bytes = found->second;
            require(bytes.size() == sizeof(payload) - 1 && std::memcmp(bytes.data(), payload, sizeof(payload) - 1) == 0,
                RetainedString::fromStaticLiteral("session import should copy extData payload"));
        }
        const std::array missingSegments{fixtureDir, RetainedString::fromStaticLiteral("session-missing.inp")};
        auto missingPath = ngBuildPath(missingSegments);
        resetCase();
        const auto missingOutput = nijigenerate::io::ngWriteProjectPuppetMemory(*ngRequireObject(incActivePuppet()));
        ngFileWrite(RetainedStringSlice(missingPath),
            std::string_view(reinterpret_cast<const char*>(missingOutput.data()), missingOutput.size()));
        resetCase();
        ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->puppet(incActivePuppet());
        require(!ngRequireObject(constructSourceCommand<ImportSessionDataCommand>(missingPath)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("ImportSessionDataCommand should reject INP without session extData"));
        const auto& entries = ngRequireObject(incActivePuppet())->extData;
        require(entries.find(sessionKey) == entries.end(),
            RetainedString::fromStaticLiteral("failed session import should not add extData"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}

void testProjectINXSerializationRoundTrip() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::ext::ExParameter;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::param::ValueParameterBinding;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::project;
    resetCase();
    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-inx-roundtrip")};
    auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array saveSegments{fixtureDir, RetainedString::fromStaticLiteral("native-roundtrip")};
        auto saveBase = ngBuildPath(saveSegments);
        auto savePath = concatenateSourceStrings({saveBase, RetainedString::fromStaticLiteral(".inx")});
        const std::array imageSegments{fixtureDir, RetainedString::fromStaticLiteral("inx-roundtrip-part.png")};
        auto sourceImage = ngBuildPath(imageSegments);
        writeRegressionPng(sourceImage, 64, 128, 192, 4, 4, 255);
        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        require(ngRequireObject(constructSourceCommand<ImportImageFolderCommand>(fixtureDir)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("ImportImageFolderCommand should create texture-backed INX fixture"));
        auto sourcePart = findDirectPart(incActivePuppet(), "inx-roundtrip-part");
        require(sourcePart != nullptr,
            RetainedString::fromStaticLiteral("image import should create source Part for INX fixture"));
        ngRequireObject(sourcePart)->localTransform.translation.x = 12.5f;
        sourcePart->localTransform.translation.y = -4.0f;
        sourcePart->tint = Vec3{0.25f, 0.5f, 0.75f};
        sourcePart->opacity = 0.65f;
        auto sourceParam = ExParameter::createSourceDeferred(
            [] { return NullableString::fromStaticLiteral("INXParam"); }, [] { return false; });
        sourceParam->min = Vec2{0, 0};
        sourceParam->max = Vec2{1, 0};
        ngRequireObject(incActivePuppet())->parameters.push_back(sourceParam);
        auto sourceBinding = newValueBinding(sourceParam, sourcePart,
            RetainedString::fromStaticLiteral("transform.t.x"), true);
        ngRequireObject(sourceBinding)->setValue({1, 0}, 18.0f);
        ngRequireObject(ngRequireObject(incActivePuppet())->root)->build();
        ngRequireObject(incActivePuppet())->populateTextureSlots();
        require(ngRequireObject(constructSourceCommand<SaveFileCommand>(saveBase)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("SaveFileCommand should save generated INX fixture"));
        require(ngFileExists(savePath.view()) && ngIsFile(savePath.view()),
            RetainedString::fromStaticLiteral("generated INX fixture should exist"));
        incNewProject();
        require(findDirectPart(incActivePuppet(), "inx-roundtrip-part") == nullptr,
            RetainedString::fromStaticLiteral("new project should clear generated INX fixture"));
        require(ngRequireObject(constructSourceCommand<OpenFileCommand>(savePath)->run(ctx))->succeeded,
            RetainedString::fromStaticLiteral("OpenFileCommand should load generated INX fixture"));
        auto importedPart = findDirectPart(incActivePuppet(), "inx-roundtrip-part");
        require(importedPart != nullptr,
            RetainedString::fromStaticLiteral("INX round-trip should restore Part node"));
        require(ngRequireObject(importedPart)->textures[0] != nullptr,
            RetainedString::fromStaticLiteral("INX round-trip should restore Part texture slot"));
        require(ngRequireObject(importedPart->textures[0])->width() == 4 &&
            ngRequireObject(importedPart->textures[0])->height() == 4,
            RetainedString::fromStaticLiteral("INX round-trip should restore texture dimensions"));
        require(importedPart->getVertices().size() > 0,
            RetainedString::fromStaticLiteral("INX round-trip should restore mesh vertices"));
        require(importedPart->getMesh().indices.size() > 0,
            RetainedString::fromStaticLiteral("INX round-trip should restore mesh indices"));
        require(near(importedPart->localTransform.translation.x, 12.5f),
            RetainedString::fromStaticLiteral("INX round-trip should restore transform X"));
        require(near(importedPart->localTransform.translation.y, -4.0f),
            RetainedString::fromStaticLiteral("INX round-trip should restore transform Y"));
        require(nearVec3(importedPart->tint, Vec3{0.25f, 0.5f, 0.75f}),
            RetainedString::fromStaticLiteral("INX round-trip should restore tint"));
        require(near(importedPart->opacity, 0.65f),
            RetainedString::fromStaticLiteral("INX round-trip should restore opacity"));
        auto importedParam = findParameter(incActivePuppet(), "INXParam");
        require(importedParam != nullptr,
            RetainedString::fromStaticLiteral("INX round-trip should restore parameter"));
        require(ngRequireObject(importedParam)->bindings.size() == 1,
            RetainedString::fromStaticLiteral("INX round-trip should restore value binding"));
        require(ngRequireObject(importedParam->bindings[0])->getTarget().target.resource() == importedPart,
            RetainedString::fromStaticLiteral("INX round-trip should reconnect binding target"));
        auto importedBinding = std::dynamic_pointer_cast<ValueParameterBinding>(importedParam->bindings[0]);
        require(importedBinding != nullptr,
            RetainedString::fromStaticLiteral("INX round-trip should preserve ValueParameterBinding type"));
        require(near(ngRequireObject(importedBinding)->getValue({1, 0}), 18.0f),
            RetainedString::fromStaticLiteral("INX round-trip should restore binding key value"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}
