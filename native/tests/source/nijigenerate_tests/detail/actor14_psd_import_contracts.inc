// Existing regression.d6130-7655. Include once inside detail after the original PSD and GPU helpers.
void testPsdDepthMapImportHelpers() {
    using Text = nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using namespace nijigenerate::io::depthmap_psd;
    using namespace nijigenerate::io::psdbudget;
    using namespace nijigenerate::io::psd;
    using namespace nijigenerate::io::depthsample;
    using namespace nijigenerate::commands::depth::map;
    using namespace nijigenerate::viewport::depth::draw;
    using namespace nijigenerate::ext::nodes;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::ngVecaFromVectorsSource;
    using nicxlive::common::SharedArray;
    using nicxlive::core::nodes::MeshData;
    using nicxlive::core::Texture;
    using nicxlive::core::ShallowTexture;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    namespace paths = nijigenerate::compat::phobos;
    namespace format = nijigenerate::core::translation;
    namespace history = nijigenerate::core::actionstack;
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
    const auto objectMember = [](nijigenerate::core::settings::SettingsJsonValue value,
            std::string_view key, std::uint32_t line) {
        const auto& object = value.sourceObjectRef();
        const auto* member = object.if_contains(key);
        if (!member) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            Text::fromStaticLiteral("source/nijigenerate_tests/regression.d"), line);
        return *member;
    };
    const auto exactDepths = [](auto left, auto buildRight) {
        const auto right = buildRight();
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) if (left[i] != right[i]) return false;
        return true;
    };
    const auto nearDepths = [](auto left, auto buildRight) {
        const auto right = buildRight();
        return nearFloatArray(std::span<const float>{left.data(), left.size()},
            std::span<const float>{right.data(), right.size()});
    };
    static_assert([]<class T>() { return !requires(T value) { value.layerTransforms; }; }
        .template operator()<PsdDepthImportSettings>(),
        "PSD depth import settings must not duplicate 3D Adjust layer state");
    static_assert([]<class T>() { return !requires(T value) { value.hiddenComposedLayerPaths; }
        && !requires(T value) { value.disabledComposedLayerPaths; }; }
        .template operator()<PsdDepthImportSettings>(),
        "PSD depth import settings must not duplicate composed-layer visibility state");
    static_assert([]<class T>() { return !requires(T value) { value.composedSource; }; }
        .template operator()<PsdDepthImportResult>(),
        "PSD depth import result must not retain a second composed-layer copy");
    resetCase();
    std::uint64_t retainedPsdDepthBytes = 0;
    check(ngReservePsdDepthRetainedLayer(2048, 2048,
            PsdDepthRetainedDepthLayerBytesPerPixel, retainedPsdDepthBytes)
        && retainedPsdDepthBytes <= PsdDepthMaxRetainedBytes
        && !ngReservePsdDepthRetainedLayer(2048, 2048,
            PsdDepthRetainedDepthLayerBytesPerPixel, retainedPsdDepthBytes),
        "PSD Depth Map imports must enforce an aggregate retained-image budget across layers");
    std::uint64_t oversizedPsdDepthBytes = 0;
    check(!ngReservePsdDepthRetainedLayer(std::numeric_limits<std::int64_t>::max(),
            std::numeric_limits<std::int64_t>::max(), PsdDepthRetainedDepthLayerBytesPerPixel,
            oversizedPsdDepthBytes),
        "PSD Depth Map retained-image budgeting must reject overflowing dimensions before allocation");
    std::uint64_t retainedPsdColorBytes = 0;
    for (int i = 0; i < 10; ++i) {
        check(ngReservePsdDepthRetainedLayer(1024, 1024,
                PsdDepthRetainedColorLayerBytesPerPixel, retainedPsdColorBytes),
            "PSD Depth Map color composition should reserve its retained source and derived RGBA buffers");
    }
    check(!ngReservePsdDepthRetainedLayer(1024, 1024,
            PsdDepthRetainedColorLayerBytesPerPixel, retainedPsdColorBytes),
        "PSD Depth Map color composition must reject aggregate derived buffers beyond the retained budget");
    Layer smallLayerWithLargeMask;
    smallLayerWithLargeMask.right = 1;
    smallLayerWithLargeMask.bottom = 1;
    smallLayerWithLargeMask.layerMask.resize(1);
    smallLayerWithLargeMask.layerMask[0].right = 8192;
    smallLayerWithLargeMask.layerMask[0].bottom = 8192;
    smallLayerWithLargeMask.channels.resize(1);
    smallLayerWithLargeMask.channels[0].type = static_cast<std::int16_t>(ChannelType::LAYER_OR_VECTOR_MASK);
    std::uint64_t maskedLayerBytes = 0;
    check(!ngReservePsdLayerExtraction(smallLayerWithLargeMask,
            PsdDepthRetainedDepthLayerBytesPerPixel, PsdDepthMaxRetainedBytes, maskedLayerBytes)
        && maskedLayerBytes == 0,
        "PSD extraction budgeting must reject a document-sized mask before extracting a small layer");
    Layer emptyLayer;
    emptyLayer.bottom = 16;
    std::uint64_t emptyLayerBytes = 123;
    check(ngReservePsdLayerExtraction(emptyLayer,
            PsdDepthRetainedDepthLayerBytesPerPixel, PsdDepthMaxRetainedBytes, emptyLayerBytes)
        && emptyLayerBytes == 123,
        "PSD extraction budgeting should accept a zero-area layer without reserving memory");
    smallLayerWithLargeMask.layerMask[0].right = 16;
    smallLayerWithLargeMask.layerMask[0].bottom = 16;
    check(ngReservePsdLayerExtraction(smallLayerWithLargeMask,
            PsdDepthRetainedDepthLayerBytesPerPixel, PsdDepthMaxRetainedBytes, maskedLayerBytes)
        && maskedLayerBytes == PsdDepthRetainedDepthLayerBytesPerPixel
            + 16ULL * 16ULL * PsdMaskExtractionBytesPerPixel + 6ULL * 17ULL * sizeof(std::uint64_t),
        "PSD extraction budgeting should include mask decode, feather buffers, and prefix-sum storage");
    auto grid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    grid->setName(text("psd-depth-grid").toNullableString());
    auto child = nicxlive::core::nodes::Node::createSourceDeferred([&] { return grid; });
    child->setName(text("psd-depth-child").toNullableString());
    check(ngPsdDepthContainingGrid(child) == grid, "PSD depth helper should resolve parent GridDeformer");
    check(ngPsdDepthContainingGrid(grid) == grid, "PSD depth helper should resolve direct GridDeformer targets");
    grid->localTransform.translation = Vec3{10, -5, 0};
    grid->localTransform.update();
    nijigenerate::project::incActivePuppet()->root->build();
    auto docPoint = ngPsdDepthGridVertexDocumentPosition(grid, Vec2{2, 3}, 100, 80);
    check(near(docPoint.x, 62.0f) && near(docPoint.y, 38.0f),
        "PSD depth helper should map transformed grid vertices into document coordinates");
    Slice<std::uint8_t> pixels;
    const auto add = [&](std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
        pixels.appendRange(Slice<std::uint8_t>{r, g, b, a});
    };
    add(255, 255, 255, 255); add(255, 255, 255, 255); add(255, 255, 255, 255);
    add(255, 255, 255, 255); add(0, 0, 0, 255);       add(255, 255, 255, 255);
    add(255, 255, 255, 255); add(255, 255, 255, 255); add(255, 255, 255, 255);
    PsdDepthImportSettings settings;
    settings.backDepth = -1.0f;
    settings.frontDepth = 1.0f;
    settings.alphaThreshold = 0.01f;
    settings.convolution = PsdDepthConvolution::Nearest;
    auto nearest = ngPsdDepthSamplePixels(pixels, 3, 3, 1, 1, settings);
    check(nearest.valid && near(nearest.value, -1.0f), "PSD depth nearest sampling should read black as back by default");
    settings.invert = true;
    auto inverted = ngPsdDepthSamplePixels(pixels, 3, 3, 0, 0, settings);
    check(inverted.valid && near(inverted.value, -1.0f), "PSD depth invert should make white back");
    settings.invert = false;
    Slice<std::uint8_t> channelPixels{255, 0, 0, 255};
    settings.convolution = PsdDepthConvolution::Nearest;
    settings.channel = PsdDepthChannel::AverageRGB;
    auto averageRed = ngPsdDepthSamplePixels(channelPixels, 1, 1, 0, 0, settings);
    check(averageRed.valid && near(averageRed.value, -1.0f / 3.0f),
        "PSD depth AverageRGB channel should average color components");
    settings.channel = PsdDepthChannel::R;
    auto redChannel = ngPsdDepthSamplePixels(channelPixels, 1, 1, 0, 0, settings);
    check(redChannel.valid && near(redChannel.value, 1.0f), "PSD depth R channel should use red only");
    settings.channel = PsdDepthChannel::G;
    auto greenChannel = ngPsdDepthSamplePixels(channelPixels, 1, 1, 0, 0, settings);
    check(greenChannel.valid && near(greenChannel.value, -1.0f), "PSD depth G channel should use green only");
    settings.channel = PsdDepthChannel::Luminance;
    auto luminanceChannel = ngPsdDepthSamplePixels(channelPixels, 1, 1, 0, 0, settings);
    check(luminanceChannel.valid && near(luminanceChannel.value, -0.5748f),
        "PSD depth Luminance channel should use perceptual weights");
    check(near(ngDepthSamplePixelDepth01(std::span<const std::uint8_t>{channelPixels.data(), channelPixels.size()},
            0, DepthSampleChannel::Luminance, false),
            (0.2126f * 255.0f) / 255.0f),
        "common depth sample helper should expose the shared luminance depth conversion");
    check(near(ngDepthSamplePixelDepth(std::span<const std::uint8_t>{channelPixels.data(), channelPixels.size()},
            0, DepthSampleChannel::R, false, -1.0f, 1.0f, 1.0f),
            redChannel.value),
        "PSD depth sampling should use the common pixel depth conversion helper");
    check(near(ngDepthSamplePixelDepth(std::span<const std::uint8_t>{channelPixels.data(), channelPixels.size()},
            0, DepthSampleChannel::R, true, -1.0f, 1.0f, 1.0f), -1.0f),
        "common depth sample helper should preserve invert semantics");
    check(near(ngDepthSampleValueToDepth01(0.0f, -1.0f, 1.0f, 1.0f), 0.5f),
        "common depth sample helper should invert depth values back to normalized depth");
    check(near(ngDepthSampleOpacity01(128), static_cast<float>(128) / 255.0f),
        "common depth sample helper should expose opacity conversion");
    check(ngPsdDepthCompositionModeForCounts(1, 1) == PsdDepthCompositionMode::OneToOne
        && ngPsdDepthCompositionModeForCounts(2, 1) == PsdDepthCompositionMode::NToOne
        && ngPsdDepthCompositionModeForCounts(2, 2) == PsdDepthCompositionMode::NToN
        && ngPsdDepthCompositionModeForCounts(1, 2) == PsdDepthCompositionMode::UnsupportedOneToN,
        "PSD depth import composition classifier should reject depth-draw unsupported 1:N composition");
    check(ngPsdDepthNormalizeLayerName(text("/Group/Face.PNG")) == text("face")
        && ngPsdDepthLayerIdentityKey(text("/Group/Face.PNG"), 1, 2, 3, 4) == text("face:1:2:3:4"),
        "PSD depth import should expose depth-draw-style normalized layer identity keys");
    PsdDepthLayerMapping ignoredMapping;
    ignoredMapping.layerPath = text("/ignored");
    ignoredMapping.ignored = true;
    PsdDepthLayerMapping matchedMapping;
    matchedMapping.layerPath = text("/matched");
    matchedMapping.matched = true;
    PsdDepthLayerMapping unresolvedMapping;
    unresolvedMapping.layerPath = text("/unresolved");
    check(ngPsdDepthMappingAccountsForColorLayer(ignoredMapping)
        && ngPsdDepthMappingAccountsForColorLayer(matchedMapping)
        && !ngPsdDepthMappingAccountsForColorLayer(unresolvedMapping),
        "PSD depth import should not reclassify an ignored depth layer as a missing color-layer match");
    check(near(ngPsdDepthRectOverlapScore(0, 0, 10, 10, 5, 5, 10, 10), 25.0f / 175.0f)
        && near(ngPsdDepthRectOverlapScore(0, 0, 10, 10, 20, 20, 10, 10), 0.0f),
        "PSD depth import should expose rectangle-overlap scoring for depth-draw layer matching fallback");
    check(near(ngDepthSampleEffectiveAlpha(
            std::span<const std::uint8_t>{channelPixels.data(), channelPixels.size()}, 0, 0.5f), 0.5f),
        "common depth sample helper should multiply pixel alpha by opacity");
    check(!ngDepthSampleAcceptsAlpha(0.01f, 0.01f) && ngDepthSampleAcceptsAlpha(0.0101f, 0.01f),
        "common depth sample helper should preserve strict alpha-threshold missing semantics");
    check(!ngDepthSampleMissingPoint().valid,
        "common depth sample helper should provide a shared missing sample value");
    check(ngDepthSampleAlphaByte(0.5f) == 128,
        "common depth sample helper should preserve alpha byte rounding");
    settings.channel = PsdDepthChannel::AverageRGB;
    auto pngFixtureDir = paths::ngBuildPath(std::array<Text, 2>{paths::ngTempDirectory(),
        text("nijigenerate-psd-depth-png-source")});
    if (paths::ngFileExists(pngFixtureDir.view())) paths::ngRmdirRecurse(pngFixtureDir);
    paths::ngMkdirRecurse(pngFixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        auto pngDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("png-depth-grid.png")});
        writeRegressionPng(pngDepthPath, 255, 255, 255, 4, 4);
        auto pngGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        pngGrid->setName(text("png-depth-grid").toNullableString());
        pngGrid->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{0, 0}, Vec2{1, 1}});
        pngGrid->localTransform.update();
        nijigenerate::project::incActivePuppet()->root->build();
        PsdDepthImportSettings pngSettings;
        pngSettings.convolution = PsdDepthConvolution::Nearest;
        pngSettings.layerTargetGridUuidOverrides[text("/png-depth-grid")] = formatted("%s", pngGrid->getUUID());
        auto pngImported = ngBuildPsdDepthsFromSource(nijigenerate::project::incActivePuppet(), pngDepthPath, pngSettings);
        check(pngImported.mappings.size() == 1 && pngImported.mappings[0].matched
            && pngImported.grids.size() == 1 && pngImported.grids[0].grid == pngGrid,
            "PSD depth import dialog source builder should map PNG sources through the existing target mapping path");
        auto ambiguousDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("ambiguous-depth-target.png")});
        writeRegressionPng(ambiguousDepthPath, 255, 255, 255, 2, 2);
        auto ambiguousGridA = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        ambiguousGridA->setName(text("ambiguous-depth-target").toNullableString());
        ambiguousGridA->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}});
        auto ambiguousGridB = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        ambiguousGridB->setName(text("ambiguous-depth-target").toNullableString());
        ambiguousGridB->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}});
        nijigenerate::project::incActivePuppet()->root->build();
        PsdDepthImportSettings ambiguousDepthSettings;
        ambiguousDepthSettings.convolution = PsdDepthConvolution::Nearest;
        auto ambiguousDepthImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), ambiguousDepthPath, ambiguousDepthSettings);
        check(ambiguousDepthImported.mappings.size() == 1 && ambiguousDepthImported.mappings[0].ambiguous
            && !ambiguousDepthImported.mappings[0].matched && ambiguousDepthImported.mappings[0].targetGridUuid == 0
            && ambiguousDepthImported.grids.size() == 0 && ambiguousDepthImported.composedLayers.size() == 1
            && ambiguousDepthImported.composedLayers[0].targetGridUuid == 0
            && !ambiguousDepthImported.composedLayers[0].enabled,
            "PSD Depth Map imports must leave equally ranked distinct targets unbound until manually resolved");
        auto coloredPngDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("colored-depth-grid.png")});
        writeRegressionPng(coloredPngDepthPath, 255, 0, 0, 2, 2);
        auto coloredPngGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        coloredPngGrid->setName(text("colored-depth-grid").toNullableString());
        coloredPngGrid->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}});
        coloredPngGrid->localTransform.update();
        nijigenerate::project::incActivePuppet()->root->build();
        PsdDepthImportSettings coloredPngSettings;
        coloredPngSettings.channel = PsdDepthChannel::AverageRGB;
        coloredPngSettings.convolution = PsdDepthConvolution::Nearest;
        coloredPngSettings.layerTargetGridUuidOverrides[text("/colored-depth-grid")] = formatted("%s", coloredPngGrid->getUUID());
        auto coloredPngImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), coloredPngDepthPath, coloredPngSettings);
        coloredPngImported = composePsdDepthImportForRegression(coloredPngImported);
        check(coloredPngImported.grids.size() == 1 && coloredPngImported.grids[0].depths.size() == 1
            && near(coloredPngImported.grids[0].depths[0], -1.0f / 3.0f),
            "PNG depth import must preserve explicit AverageRGB sampling instead of silently using red");
        {
            const bool condition = pngImported.compositionMode == PsdDepthCompositionMode::OneToOne
                && pngImported.compositionModeName == text("1:1") && pngImported.compositionWidth == 4
                && pngImported.compositionHeight == 4 && pngImported.colorLayerCount == 1
                && pngImported.sourceDepthLayerCount == 1 && pngImported.composedLayerCount == 1
                && near(pngImported.globalDepthScale, 1.0f);
            require(condition, concatenateSourceStrings({text(
                "PSD depth import PNG manual/direct mapping should expose depth-draw 1:1 composition metadata: "),
                formatted("mode=%s width=%s height=%s color=%s depth=%s composed=%s scale=%s centroid=%s",
                    pngImported.compositionModeName, pngImported.compositionWidth, pngImported.compositionHeight,
                    pngImported.colorLayerCount, pngImported.sourceDepthLayerCount, pngImported.composedLayerCount,
                    pngImported.globalDepthScale, pngImported.globalDepthCentroid)}));
        }
        auto pngComposedSource = ngPsdDepthComposedSourceFromImportResult(pngImported);
        check(pngComposedSource.mode == PsdDepthCompositionMode::OneToOne
            && pngComposedSource.modeName == text("1:1") && pngComposedSource.width == 4 && pngComposedSource.height == 4
            && pngComposedSource.colorSource.kind == PsdDepthCompositeSourceKind::ActiveArtTargets
            && pngComposedSource.depthSource.kind == PsdDepthCompositeSourceKind::FlatImage
            && pngComposedSource.layers.size() == 1,
            "PSD depth import should expose a named ColorComposite/DepthComposite/ComposedSource model");
        check(pngImported.composedLayers.size() == 1 && pngImported.composedLayers[0].id == text("/png-depth-grid")
            && pngImported.composedLayers[0].sourcePath == pngDepthPath
            && pngImported.composedLayers[0].targetGridUuid == pngGrid->getUUID()
            && pngImported.composedLayers[0].depthRgba.size() == 4 * 4 * 4
            && pngImported.composedLayers[0].maskRgba.size() == 4 * 4 * 4
            && pngImported.composedLayers[0].puppetFitEnabled
            && pngImported.composedLayers[0].puppetBindingOverride == formatted("%s", pngGrid->getUUID()),
            "PSD depth import should expose a first-class composed layer before target sampling/apply");
        auto pngComposedSession = ngPsdDepthImportResultToDepthDrawSession(pngImported);
        check(pngComposedSession->layers.size() == 1 && pngComposedSession->documentWidth == pngComposedSource.width
            && pngComposedSession->documentHeight == pngComposedSource.height
            && pngComposedSession->layers[0].id == text("/png-depth-grid")
            && pngComposedSession->layers[0].sourcePath == pngDepthPath && pngComposedSession->layers[0].hasDepthPixels()
            && pngComposedSession->bindings.size() == 1
            && pngComposedSession->bindings[0].layerId == pngComposedSession->layers[0].id
            && pngComposedSession->bindings[0].targetGridUuid == pngGrid->getUUID(),
            "PSD depth import composed source should export to DepthDraw layer entries with target bindings");
        auto pngComposedManifest = ngDepthDrawSessionToManifest(pngComposedSession);
        check(objectMember(pngComposedManifest, "layers", 6405).sourceArrayValue().size() == 1
            && objectMember(pngComposedManifest, "bindings", 6406).sourceArrayValue().size() == 1,
            "PSD depth import composed source layer entries should be serializable through the DepthDraw manifest");
        auto psdDepthExportDir = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("psd-depth-composed-export")});
        auto psdDepthExportManifest = paths::ngBuildPath(std::array<Text, 2>{psdDepthExportDir, text("manifest.json")});
        auto psdDepthExport = ngExportPsdDepthComposedSourcePng(pngComposedSource,
            psdDepthExportDir, psdDepthExportManifest);
        check(psdDepthExport.succeeded && psdDepthExport.exportedLayers == 1
            && psdDepthExport.layerPaths.size() == 1 && paths::ngFileExists(psdDepthExport.layerPaths[0].view())
            && paths::ngFileExists(psdDepthExport.manifestPath.view()),
            "PSD depth import composed source should drive PNG layer export directly from the source model");
        {
            auto exportedManifest = nijigenerate::autorig::json::detail::parseJSON(
                nijigenerate::compat::RetainedStringSlice(paths::ngReadUtf8Text(psdDepthExport.manifestPath)));
            check(objectMember(exportedManifest, "layers", 6424).sourceArrayValue().size() == 1
                && objectMember(exportedManifest, "bindings", 6425).sourceArrayValue().size() == 1
                && objectMember(exportedManifest, "documentWidth", 6426).as_int64() == pngComposedSource.width
                && objectMember(exportedManifest, "documentHeight", 6427).as_int64() == pngComposedSource.height,
                "PSD depth import composed source PNG export should preserve canvas, layer, and binding manifest entries");
        }
        auto disabledPngImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), pngDepthPath, pngSettings);
        disabledPngImported.composedLayers[0].enabled = false;
        auto disabledPngComposed = composePsdDepthImportForRegression(disabledPngImported);
        check(disabledPngImported.composedLayers.size() == 1 && !disabledPngImported.composedLayers[0].enabled
            && disabledPngComposed.grids.size() == 1 && disabledPngComposed.grids[0].sampledVertices == 0,
            "PSD depth import should apply the sole composed-layer enabled state before target sampling");
        auto persistedPngSource = ngPsdDepthComposedSourceFromImportResult(pngImported);
        persistedPngSource.layers[0].visible = false;
        persistedPngSource.layers[0].enabled = false;
        persistedPngSource.layers[0].depthOffset = 0.42f;
        persistedPngSource.layers[0].depthScale = 0.75f;
        auto persistedPngImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), pngDepthPath, pngSettings);
        ngPsdDepthApplyPreviousComposedSourceState(persistedPngImported, persistedPngSource);
        check(persistedPngImported.composedLayers.size() == 1 && !persistedPngImported.composedLayers[0].visible
            && !persistedPngImported.composedLayers[0].enabled
            && near(persistedPngImported.composedLayers[0].depthOffset, 0.42f)
            && near(persistedPngImported.composedLayers[0].depthScale, 0.75f)
            && persistedPngImported.composedLayers[0].targetGridUuid == pngGrid->getUUID(),
            "PSD depth import should reload layer state from the persistent composed source model, not only dialog settings");
        auto pngImportSummary = ngPsdDepthImportSummaryToJson(pngImported, 0);
        check(objectMember(pngImportSummary, "compositionMode", 6453).sourceStringValue() == text("1:1")
            && objectMember(pngImportSummary, "compositionWidth", 6454).as_int64() == 4
            && objectMember(pngImportSummary, "compositionHeight", 6455).as_int64() == 4
            && objectMember(pngImportSummary, "colorSourceKind", 6456).sourceStringValue() == text("ActiveArtTargets")
            && objectMember(pngImportSummary, "depthSourceKind", 6457).sourceStringValue() == text("FlatImage")
            && objectMember(pngImportSummary, "composedLayerCount", 6458).as_int64() == 1
            && objectMember(pngImportSummary, "composedSourceMode", 6459).sourceStringValue() == text("1:1")
            && objectMember(pngImportSummary, "composedSourceWidth", 6460).as_int64() == 4
            && objectMember(pngImportSummary, "composedSourceHeight", 6461).as_int64() == 4
            && objectMember(pngImportSummary, "composedSourceLayerCount", 6462).as_int64() == 1
            && objectMember(pngImportSummary, "composedLayers", 6463).sourceArrayValue().size() == 1
            && objectMember(objectMember(pngImportSummary, "composedLayers", 6464).sourceArrayValue()[0],
                "layerPath", 6464).sourceStringValue() == text("/png-depth-grid")
            && objectMember(pngImportSummary, "compositionDiagnostics", 6465).sourceArrayValue().size() == 0,
            "PSD depth import apply summary should expose depth-draw composition/debug stats");
        check(pngImported.depthSource.kind == PsdDepthCompositeSourceKind::FlatImage
            && pngImported.depthSource.kindName == text("FlatImage") && pngImported.depthSource.sourcePath == pngDepthPath
            && pngImported.depthSource.width == 4 && pngImported.depthSource.height == 4
            && pngImported.depthSource.layers.size() == 1 && pngImported.depthSource.layers[0].rgba.size() == 4 * 4 * 4
            && pngImported.depthSource.layers[0].maskRgba.size() == 4 * 4 * 4
            && pngImported.colorSource.kind == PsdDepthCompositeSourceKind::ActiveArtTargets
            && pngImported.colorSource.sourcePath.size() == 0
            && pngImported.colorSource.width == pngImported.compositionWidth
            && pngImported.colorSource.height == pngImported.compositionHeight && pngImported.colorSource.layers.size() == 1
            && pngImported.colorSource.layers[0].rgba.size() == 4 * 4 * 4
            && pngImported.colorSource.layers[0].maskRgba.size() == 4 * 4 * 4,
            "PSD depth import should expose format-neutral color/depth source summaries with source pixels/masks");
        check(pngImported.composedLayers.size() == 1 && pngImported.composedLayers[0].colorRgba.size() == 4 * 4 * 4,
            "PSD depth import dialog source builder should expose PNG previews in the composed layer model");
        auto pngLayerPreview = pngImported.composedLayers[0];
        auto pngDebugSession = ngPsdDepthImportResultToDepthDrawSession(pngImported);
        auto pngDebugLayer = pngDebugSession->layers.size() ? pngDebugSession->layers[0] : DepthDrawLayer{};
        using nijigenerate::viewport::depth::common::targetview::DepthTargetView;
        auto pngDebugTarget = std::allocate_shared<DepthTargetView>(
            nicxlive::core::d_runtime::SourceAllocator<DepthTargetView>{}, pngGrid);
        auto pngDebugDoc0 = ngDepthDrawVertexDocumentPosition(pngDebugTarget,
            pngGrid->getVertices()[0], pngImported.grids[0].documentWidth, pngImported.grids[0].documentHeight);
        auto pngDebugLayer0 = ngDepthDrawLayerPixelFromDocument(pngDebugLayer, pngDebugDoc0);
        auto pngDebugSample0 = nijigenerate::io::depthimage::ngDepthImageSampleRgbaWithOpacitySource(
            pngDebugLayer.depthPixels, pngDebugLayer.width, pngDebugLayer.height, pngDebugLayer0.x, pngDebugLayer0.y,
            pngDebugLayer.opacity, pngDebugLayer.sampleSettings());
        auto sampledPngImported = composePsdDepthImportForRegression(pngImported);
        {
            const bool condition = sampledPngImported.grids[0].sampledVertices == pngGrid->getVertices().length()
                && sampledPngImported.grids[0].missingVertices == 0
                && sampledPngImported.grids[0].depths.size() == pngGrid->getVertices().length();
            require(condition, formatted(
                "PSD depth import composition should sample PNG sources into GridDeformer depths: "
                "sampled=%s missing=%s depths=%s vertices=%s doc=%sx%s layer=%s,%s %sx%s "
                "dd=%s,%s %sx%s enabled=%s visible=%s hasDepth=%s point=%s,%s layerPoint=%s,%s sample=%s/%s "
                "depthA=%s maskA=%s",
                sampledPngImported.grids[0].sampledVertices, sampledPngImported.grids[0].missingVertices,
                sampledPngImported.grids[0].depths.size(), pngGrid->getVertices().length(),
                sampledPngImported.grids[0].documentWidth, sampledPngImported.grids[0].documentHeight,
                pngLayerPreview.left, pngLayerPreview.top, pngLayerPreview.width, pngLayerPreview.height,
                pngDebugLayer.bounds.left, pngDebugLayer.bounds.top, pngDebugLayer.width, pngDebugLayer.height,
                pngDebugLayer.enabled, pngDebugLayer.visible, pngDebugLayer.hasDepthPixels(), pngDebugDoc0.x, pngDebugDoc0.y,
                pngDebugLayer0.x, pngDebugLayer0.y, pngDebugSample0.valid, pngDebugSample0.value,
                pngLayerPreview.depthRgba.size() > 3 ? pngLayerPreview.depthRgba[3] : 0,
                pngLayerPreview.maskRgba.size() > 3 ? pngLayerPreview.maskRgba[3] : 0));
        }
        check(pngLayerPreview.id == text("/png-depth-grid") && pngLayerPreview.sourcePath == pngDepthPath
            && pngLayerPreview.layerPath == text("/png-depth-grid") && pngLayerPreview.layerName == text("png-depth-grid"),
            "PSD depth import normalized PNG source session should expose stable id, source path, layer path, and display name");
        check(pngImported.compositionWidth == 4 && pngImported.compositionHeight == 4 && pngLayerPreview.left == 0
            && pngLayerPreview.top == 0 && pngLayerPreview.width == 4 && pngLayerPreview.height == 4,
            "PSD depth import normalized PNG source session should expose document and layer bounds");
        check(pngLayerPreview.colorRgba.size() == 4 * 4 * 4 && pngLayerPreview.maskRgba.size() == 4 * 4 * 4
            && pngLayerPreview.coverageMaskRgba.size() == 4 * 4 * 4 && pngLayerPreview.colorRgba[3] == 255,
            "PSD depth import normalized PNG source session should expose RGBA/depth pixels and alpha coverage");
        check(pngLayerPreview.visible && pngLayerPreview.enabled && pngLayerPreview.coverageMaskRgba[3] == 255,
            "PSD depth import normalized PNG source session should expose visibility, enabled state, coverage, and normal/art coverage count");
        check(near(pngLayerPreview.depthOffset, 0.0f) && near(pngLayerPreview.depthScale, 1.0f)
            && near(pngLayerPreview.backDepth, -1.0f) && near(pngLayerPreview.frontDepth, 1.0f)
            && near(pngLayerPreview.sourceDepthScale, 1.0f) && pngLayerPreview.convolution == PsdDepthConvolution::Nearest,
            "PSD depth import normalized PNG source session should expose transform and depth-draw-compatible sampling state");
        check(pngLayerPreview.depthStats.hasDepth && pngLayerPreview.depthStats.maskedPixels == 16
            && pngLayerPreview.depthStats.zeroPixels == 0 && near(pngLayerPreview.depthStats.minDepth01, 1.0f)
            && near(pngLayerPreview.depthStats.maxDepth01, 1.0f),
            "PSD depth import normalized PNG source session should expose composed-layer depth pixel stats");
        auto previousPngImportState = pngImported;
        previousPngImportState.composedLayers = previousPngImportState.composedLayers.duplicate();
        previousPngImportState.composedLayers[0].visible = false;
        previousPngImportState.composedLayers[0].enabled = false;
        previousPngImportState.composedLayers[0].depthEnabled = false;
        previousPngImportState.composedLayers[0].depthOffset = 0.25f;
        previousPngImportState.composedLayers[0].depthScale = 0.5f;
        previousPngImportState.composedLayers[0].outlierPruneEnabled = true;
        previousPngImportState.composedLayers[0].puppetFitEnabled = false;
        auto refreshedPngImport = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), pngDepthPath, pngSettings);
        ngPsdDepthApplyPreviousComposedLayerState(refreshedPngImport, previousPngImportState);
        check(refreshedPngImport.composedLayers.size() == 1 && !refreshedPngImport.composedLayers[0].visible
            && !refreshedPngImport.composedLayers[0].enabled && !refreshedPngImport.composedLayers[0].depthEnabled
            && near(refreshedPngImport.composedLayers[0].depthOffset, 0.25f)
            && near(refreshedPngImport.composedLayers[0].depthScale, 0.5f)
            && refreshedPngImport.composedLayers[0].outlierPruneEnabled && !refreshedPngImport.composedLayers[0].puppetFitEnabled,
            "PSD depth import should preserve depth-draw composed-layer state by source layer identity across reloads");
        auto previousNormalizedState = pngImported;
        previousNormalizedState.composedLayers = previousNormalizedState.composedLayers.duplicate();
        previousNormalizedState.composedLayers[0].id = Text{};
        previousNormalizedState.composedLayers[0].layerPath = Text{};
        previousNormalizedState.composedLayers[0].layerName = text("PNG Depth Grid.png");
        previousNormalizedState.composedLayers[0].depthOffset = 0.75f;
        auto refreshedNormalizedState = pngImported;
        refreshedNormalizedState.composedLayers = refreshedNormalizedState.composedLayers.duplicate();
        refreshedNormalizedState.composedLayers[0].id = Text{};
        refreshedNormalizedState.composedLayers[0].layerPath = Text{};
        refreshedNormalizedState.composedLayers[0].layerName = text("png-depth-grid");
        ngPsdDepthApplyPreviousComposedLayerState(refreshedNormalizedState, previousNormalizedState);
        check(near(refreshedNormalizedState.composedLayers[0].depthOffset, 0.0f),
            "PSD depth import must not transfer transforms through ambiguous normalized-name matching");
        PsdDepthImportResult duplicateIdPrevious;
        PsdDepthImportResult duplicateIdRefreshed;
        auto duplicateIdLayerA = pngImported.composedLayers[0];
        auto duplicateIdLayerB = pngImported.composedLayers[0];
        duplicateIdLayerA.id = text("shared-source");
        duplicateIdLayerA.layerPath = text("/part-a");
        duplicateIdLayerA.depthOffset = 0.25f;
        duplicateIdLayerA.depthScale = 0.5f;
        duplicateIdLayerB.id = text("shared-source");
        duplicateIdLayerB.layerPath = text("/part-b");
        duplicateIdLayerB.depthOffset = -0.75f;
        duplicateIdLayerB.depthScale = 1.5f;
        duplicateIdPrevious.composedLayers = Slice<PsdDepthComposedLayer>{duplicateIdLayerA, duplicateIdLayerB};
        duplicateIdLayerA.depthOffset = 0.0f;
        duplicateIdLayerA.depthScale = 1.0f;
        duplicateIdLayerB.depthOffset = 0.0f;
        duplicateIdLayerB.depthScale = 1.0f;
        duplicateIdRefreshed.composedLayers = Slice<PsdDepthComposedLayer>{duplicateIdLayerA, duplicateIdLayerB};
        ngPsdDepthApplyPreviousComposedLayerState(duplicateIdRefreshed, duplicateIdPrevious);
        check(near(duplicateIdRefreshed.composedLayers[0].depthOffset, 0.25f)
            && near(duplicateIdRefreshed.composedLayers[0].depthScale, 0.5f)
            && near(duplicateIdRefreshed.composedLayers[1].depthOffset, -0.75f)
            && near(duplicateIdRefreshed.composedLayers[1].depthScale, 1.5f),
            "PSD depth import must preserve transforms by exact layerPath when source ids are shared");
        auto replacementDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("png-depth-grid-replacement.png")});
        writeRegressionPng(replacementDepthPath, 128, 128, 128, 4, 4);
        auto previousForDepthReplacement = pngImported;
        previousForDepthReplacement.composedLayers = previousForDepthReplacement.composedLayers.duplicate();
        previousForDepthReplacement.composedLayers[0].depthOffset = 0.33f;
        previousForDepthReplacement.composedLayers[0].enabled = false;
        ngPsdDepthRefreshDerivedState(previousForDepthReplacement);
        auto replacedDepthImport = ngPsdDepthReplaceDepthSource(nijigenerate::project::incActivePuppet(),
            previousForDepthReplacement, replacementDepthPath, pngSettings);
        check(replacedDepthImport.depthSource.sourcePath == replacementDepthPath && replacedDepthImport.composedLayers.size() == 1
            && near(replacedDepthImport.composedLayers[0].depthOffset, 0.33f) && !replacedDepthImport.composedLayers[0].enabled,
            "PSD depth import should replace the depth source while preserving composed-layer state");
        auto partialDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("partial-depth-grid.png")});
        Slice<std::uint8_t> partialDepthPixels;
        partialDepthPixels.resize(4 * 4 * 4);
        for (auto& byte : partialDepthPixels) byte = 255;
        auto partialMissingOffset = static_cast<std::size_t>(1 * 4 + 1) * 4;
        auto partialMissing = partialDepthPixels.slice(partialMissingOffset, partialMissingOffset + 3);
        for (auto& byte : partialMissing) byte = 0;
        nicxlive::core::ShallowTexture(SharedArray<std::uint8_t>::retainedView(
            partialDepthPixels.retentionOwner(), partialDepthPixels.data(), partialDepthPixels.size()), 4, 4, 4)
            .save(partialDepthPath);
        pngGrid->replaceDepths(Slice<float>{0.1f, 0.2f, 0.3f});
        auto partialSettings = pngSettings;
        partialSettings.layerTargetGridUuidOverrides[text("/partial-depth-grid")] = formatted("%s", pngGrid->getUUID());
        partialSettings.missingPolicy = PsdDepthMissingPolicy::KeepExisting;
        auto partialKeepImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), partialDepthPath, partialSettings);
        check(partialKeepImported.grids.size() == 1 && partialKeepImported.grids[0].missingVertices > 0
            && partialKeepImported.grids[0].sampledVertices > 0,
            "partial SkipGrid fixture must contain both missing and covered vertices");
        PsdDepthComposedView partialKeepComposed;
        Text partialKeepError;
        {
            const bool condition = ngComposePsdDepthImportResult(partialKeepImported, partialKeepComposed, partialKeepError)
                && nearDepths(partialKeepImported.grids[0].depths, [] { return Slice<float>{1.0f, 1.0f, 1.0f}; });
            require(condition, concatenateSourceStrings({text(
                "Keep Existing must extrapolate partial coverage from sampled depths instead of retaining depth pits: "),
                partialKeepError}));
        }
        partialSettings.missingPolicy = PsdDepthMissingPolicy::SkipGrid;
        auto partialSkipImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), partialDepthPath, partialSettings);
        check(partialSkipImported.grids.size() == 1 && partialSkipImported.grids[0].skipped
            && nearDepths(partialSkipImported.grids[0].depths, [] { return Slice<float>{0.1f, 0.2f, 0.3f}; }),
            "SkipGrid must preserve every existing depth when any vertex is missing");
        auto blackPngDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("png-depth-zero-mask.png")});
        writeRegressionPng(blackPngDepthPath, 0, 0, 0, 4, 4);
        pngGrid->replaceDepths(Slice<float>{0.1f, 0.2f, 0.3f});
        auto blackPngSettings = pngSettings;
        blackPngSettings.layerTargetGridUuidOverrides[text("/png-depth-zero-mask")] = formatted("%s", pngGrid->getUUID());
        auto blackPngImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), blackPngDepthPath, blackPngSettings);
        check(blackPngImported.grids.size() == 1 && blackPngImported.grids[0].sampledVertices == 0
            && blackPngImported.grids[0].missingVertices == pngGrid->getVertices().length()
            && exactDepths(blackPngImported.grids[0].depths, [] { return Slice<float>{0.1f, 0.2f, 0.3f}; })
            && blackPngImported.composedLayers[0].depthStats.maskedPixels == 0,
            "PSD depth import PNG should treat zero depth pixels as missing like depth-draw createMaskFromDepthPixels");
        PsdDepthComposedView blackPngComposed;
        Text blackPngComposeError;
        {
            const bool condition = ngComposePsdDepthImportResult(blackPngImported, blackPngComposed, blackPngComposeError)
                && exactDepths(blackPngImported.grids[0].depths, [] { return Slice<float>{0.1f, 0.2f, 0.3f}; });
            require(condition, concatenateSourceStrings({text(
                "Keep Existing must preserve target depths for missing vertices through final PSD depth composition: "),
                blackPngComposeError}));
        }
        auto zeroMissingSettings = blackPngSettings;
        zeroMissingSettings.missingPolicy = PsdDepthMissingPolicy::SetZero;
        auto zeroMissingImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), blackPngDepthPath, zeroMissingSettings);
        PsdDepthComposedView zeroMissingComposed;
        Text zeroMissingError;
        {
            const bool condition = ngComposePsdDepthImportResult(zeroMissingImported, zeroMissingComposed, zeroMissingError)
                && zeroMissingImported.grids[0].missingVertices == pngGrid->getVertices().length()
                && exactDepths(zeroMissingImported.grids[0].depths, [] { return Slice<float>{0.0f, 0.0f, 0.0f}; });
            require(condition, concatenateSourceStrings({text(
                "Set Zero must set every missing vertex to zero through final PSD depth composition: "), zeroMissingError}));
        }
        auto backMissingSettings = blackPngSettings;
        backMissingSettings.missingPolicy = PsdDepthMissingPolicy::SetBack;
        backMissingSettings.backDepth = -2.0f;
        backMissingSettings.depthScale = 1.5f;
        auto backMissingImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), blackPngDepthPath, backMissingSettings);
        PsdDepthComposedView backMissingComposed;
        Text backMissingError;
        {
            const bool condition = ngComposePsdDepthImportResult(backMissingImported, backMissingComposed, backMissingError)
                && backMissingImported.grids[0].missingVertices == pngGrid->getVertices().length()
                && exactDepths(backMissingImported.grids[0].depths, [] { return Slice<float>{-3.0f, -3.0f, -3.0f}; });
            require(condition, concatenateSourceStrings({text(
                "Set Back must apply scaled back depth to every missing vertex through final PSD depth composition: "),
                backMissingError}));
        }
        auto unmatchedPngPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("unmatched-depth-source.png")});
        writeRegressionPng(unmatchedPngPath, 192, 192, 192, 4, 4);
        PsdDepthImportSettings unmatchedPngSettings;
        unmatchedPngSettings.convolution = PsdDepthConvolution::Nearest;
        unmatchedPngSettings.matchDirectGridName = false;
        auto unmatchedPngImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), unmatchedPngPath, unmatchedPngSettings);
        check(unmatchedPngImported.grids.size() == 0 && unmatchedPngImported.mappings.size() == 1
            && unmatchedPngImported.mappings[0].status == text("Unmatched")
            && unmatchedPngImported.composedLayers.size() == 1
            && unmatchedPngImported.composedLayers[0].layerPath == text("/unmatched-depth-source"),
            "PSD depth import dialog must keep unmatched PNG source layers visible so users can remap them");
        check(unmatchedPngImported.composedLayers.size() == 1 && unmatchedPngImported.composedLayers[0].targetGridUuid == 0
            && unmatchedPngImported.composedLayers[0].layerPath == text("/unmatched-depth-source")
            && unmatchedPngImported.composedLayers[0].depthRgba.size() == 4 * 4 * 4,
            "PSD depth import dialog must keep unmatched PNG source as an unbound composed layer");
        check(unmatchedPngImported.compositionMode == PsdDepthCompositionMode::Unknown
            && unmatchedPngImported.compositionDiagnostics.size() == 1
            && unmatchedPngImported.compositionDiagnostics[0].type == text("missing-color-source"),
            "PSD depth import unmatched PNG source should expose composition diagnostics instead of silently becoming an empty target view");
        auto flatColorPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("flat-color-source.png")});
        auto pairedFlatDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("paired-flat-depth.png")});
        writeRegressionPng(flatColorPath, 255, 255, 255, 4, 4, 0);
        writeRegressionPng(pairedFlatDepthPath, 255, 255, 255, 4, 4);
        PsdDepthImportSettings pairedFlatSettings;
        pairedFlatSettings.convolution = PsdDepthConvolution::Nearest;
        pairedFlatSettings.matchDirectGridName = false;
        pairedFlatSettings.colorSourcePath = flatColorPath;
        auto pairedFlatImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), pairedFlatDepthPath, pairedFlatSettings);
        check(pairedFlatImported.compositionMode == PsdDepthCompositionMode::OneToOne
            && pairedFlatImported.colorSource.kind == PsdDepthCompositeSourceKind::FlatImage
            && pairedFlatImported.colorSource.layers.size() == 1
            && pairedFlatImported.colorSource.layers[0].id == text("/flat-color-source")
            && pairedFlatImported.colorSource.layers[0].rgba.size() == 4 * 4 * 4
            && pairedFlatImported.depthSource.layers.size() == 1 && pairedFlatImported.composedLayers.size() == 1
            && pairedFlatImported.composedLayers[0].layerPath == text("/flat-color-source")
            && pairedFlatImported.composedLayers[0].depthStats.maskedPixels == 0
            && pairedFlatImported.composedLayers[0].coverageMaskRgba[3] == 0
            && pairedFlatImported.compositionDiagnostics.size() == 1
            && pairedFlatImported.compositionDiagnostics[0].type == text("unbound-composed-layer"),
            "PSD depth import should support depth-draw flat color PNG + flat depth PNG 1:1 composition using color alpha as mask");
        auto replacementFlatColorPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("flat-color-replacement.png")});
        writeRegressionPng(replacementFlatColorPath, 64, 64, 64, 4, 4);
        auto previousForColorReplacement = pairedFlatImported;
        previousForColorReplacement.composedLayers[0].depthScale = 0.25f;
        previousForColorReplacement.composedLayers[0].visible = false;
        ngPsdDepthRefreshDerivedState(previousForColorReplacement);
        auto replacedColorImport = ngPsdDepthReplaceColorSource(nijigenerate::project::incActivePuppet(),
            previousForColorReplacement, replacementFlatColorPath, pairedFlatSettings);
        check(replacedColorImport.depthSource.sourcePath == pairedFlatDepthPath
            && replacedColorImport.colorSource.sourcePath == replacementFlatColorPath
            && replacedColorImport.composedLayers.size() == 1 && near(replacedColorImport.composedLayers[0].depthScale, 0.25f)
            && !replacedColorImport.composedLayers[0].visible,
            "PSD depth import should replace the color source while preserving composed-layer state");
        auto mismatchedFlatColorPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("flat-color-mismatched.png")});
        writeRegressionPng(mismatchedFlatColorPath, 255, 255, 255, 2, 2);
        auto mismatchedFlatSettings = pairedFlatSettings;
        mismatchedFlatSettings.colorSourcePath = mismatchedFlatColorPath;
        {
            bool caught = false;
            try { (void)ngBuildPsdDepthsFromSource(nijigenerate::project::incActivePuppet(), pairedFlatDepthPath,
                mismatchedFlatSettings); }
            catch (const nijigenerate::compat::ThrowableException&) { caught = true; }
            check(caught, "PSD depth import should reject flat color/depth 1:1 sources with mismatched dimensions");
        }
        auto pairedColorPsdDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("paired-color-psd-depth.psd")});
        auto pairedColorPsdColorPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("paired-color-psd-color.png")});
        auto pairedColorPsdMismatchPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("paired-color-psd-color-mismatch.png")});
        writeRegressionPsdFixture(pairedColorPsdDepthPath);
        writeRegressionPng(pairedColorPsdColorPath, 255, 255, 255, 1, 1);
        writeRegressionPng(pairedColorPsdMismatchPath, 255, 255, 255, 2, 2);
        PsdDepthImportSettings pairedColorPsdSettings;
        pairedColorPsdSettings.colorSourcePath = pairedColorPsdColorPath;
        auto pairedColorPsdImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), pairedColorPsdDepthPath, pairedColorPsdSettings);
        check(pairedColorPsdImported.colorSource.kind == PsdDepthCompositeSourceKind::FlatImage
            && pairedColorPsdImported.colorSource.layers.size() == 1
            && pairedColorPsdImported.colorSource.layers[0].id == text("/paired-color-psd-color")
            && pairedColorPsdImported.colorSource.layers[0].rgba.size() == 4 && pairedColorPsdImported.colorLayerCount == 1,
            "PSD depth import should route explicit flat color PNG sources into PSD depth composition");
        auto pairedColorPsdMismatchSettings = pairedColorPsdSettings;
        pairedColorPsdMismatchSettings.colorSourcePath = pairedColorPsdMismatchPath;
        {
            bool caught = false;
            try { (void)ngBuildPsdDepthsFromSource(nijigenerate::project::incActivePuppet(), pairedColorPsdDepthPath,
                pairedColorPsdMismatchSettings); }
            catch (const nijigenerate::compat::ThrowableException&) { caught = true; }
            check(caught, "PSD depth import should reject paired flat color PNG sources whose dimensions differ from the PSD depth source");
        }
        auto pairedPsdColorPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("paired-color-source.psd")});
        auto pairedPsdColorMismatchPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("paired-color-source-mismatch.png")});
        writeRegressionPsdFixture(pairedPsdColorPath);
        PsdDepthImportSettings pairedPsdColorSettings;
        pairedPsdColorSettings.colorSourcePath = pairedPsdColorPath;
        auto pairedPsdColorImported = ngBuildPsdDepthsFromSource(nijigenerate::project::incActivePuppet(),
            pairedColorPsdDepthPath, pairedPsdColorSettings);
        check(pairedPsdColorImported.colorSource.kind == PsdDepthCompositeSourceKind::PsdLayers
            && pairedPsdColorImported.colorSource.width == 1 && pairedPsdColorImported.colorSource.height == 1,
            "PSD depth import should route explicit PSD color sources as PsdLayers color composites");
        PsdDepthCompositeSourceLayer offsetColorCoverage;
        offsetColorCoverage.left = 11;
        offsetColorCoverage.top = 20;
        offsetColorCoverage.width = 2;
        offsetColorCoverage.height = 1;
        offsetColorCoverage.visible = true;
        offsetColorCoverage.opacity = 1.0f;
        offsetColorCoverage.rgba = Slice<std::uint8_t>{255, 255, 255, 64, 255, 255, 255, 192};
        auto alignedPsdColorCoverage = ngPsdDepthAttachCompositeSourceCoverageForRegression(10, 20, 4, 1, offsetColorCoverage);
        check(alignedPsdColorCoverage.size() == 16 && alignedPsdColorCoverage[3] == 0
            && alignedPsdColorCoverage[7] == 64 && alignedPsdColorCoverage[11] == 192 && alignedPsdColorCoverage[15] == 0,
            "PSD depth import must align explicit PSD color pixels to depth-layer document bounds for coverage");
        PsdDepthCompositeSourceLayer similarColorA;
        similarColorA.id = text("/color-a");
        similarColorA.name = text("similar-layer");
        similarColorA.width = 4;
        similarColorA.height = 4;
        auto similarColorB = similarColorA;
        similarColorB.id = text("/color-b");
        PsdDepthCompositeSourceLayer similarDepthA;
        similarDepthA.id = text("/depth-a");
        similarDepthA.name = text("similar-layer");
        similarDepthA.width = 4;
        similarDepthA.height = 4;
        auto similarDepthB = similarDepthA;
        similarDepthB.id = text("/depth-b");
        Slice<std::ptrdiff_t> expectedReservedPairing{0, -1};
        const auto exactPairing = [](auto left, auto right) {
            return left.size() == right.size() && (left.size() == 0
                || std::memcmp(left.data(), right.data(), left.size() * sizeof(std::ptrdiff_t)) == 0);
        };
        check([&] {
                auto colors = Slice<PsdDepthCompositeSourceLayer>{similarColorA, similarColorB};
                auto depths = Slice<PsdDepthCompositeSourceLayer>{similarDepthA};
                return exactPairing(ngPsdDepthPairCompositeSourceLayersForRegression(colors, depths), expectedReservedPairing);
            }() && [&] {
                auto depths = Slice<PsdDepthCompositeSourceLayer>{similarDepthA, similarDepthB};
                auto colors = Slice<PsdDepthCompositeSourceLayer>{similarColorA};
                return exactPairing(ngPsdDepthPairCoverageSourceLayersForRegression(depths, colors), expectedReservedPairing);
            }(),
            "paired PSD matching must reserve consumed depth and color candidates in both directions");
        auto richColorPsdPath = localDepthDrawDataPath(text("Midori-20260621-color.psd"));
        auto richDepthPsdPath = localDepthDrawDataPath(text("Midori-20260621-color-psd-depth.psd"));
        if (richColorPsdPath.size() && richDepthPsdPath.size()) {
            PsdDepthImportSettings richPsdPairSettings;
            richPsdPairSettings.colorSourcePath = richColorPsdPath;
            richPsdPairSettings.matchDirectGridName = false;
            auto richPsdPairImported = ngBuildPsdDepthsFromSource(nijigenerate::project::incActivePuppet(),
                richDepthPsdPath, richPsdPairSettings);
            bool hasMatchedColorLayerIdentity = false;
            auto richPsdPairSource = ngPsdDepthComposedSourceFromImportResult(richPsdPairImported);
            const auto currentLayers = richPsdPairSource.layers;
            for (auto layer : currentLayers) {
                if (layer.colorLayerPath.size() && layer.colorLayerName.size()) {
                    hasMatchedColorLayerIdentity = true;
                    break;
                }
            }
            check(richPsdPairImported.compositionMode == PsdDepthCompositionMode::NToN
                && richPsdPairImported.colorSource.kind == PsdDepthCompositeSourceKind::PsdLayers
                && richPsdPairImported.depthSource.kind == PsdDepthCompositeSourceKind::PsdLayers
                && richPsdPairImported.colorSource.layers.size() > 1 && richPsdPairImported.depthSource.layers.size() > 1
                && richPsdPairSource.layers.size() > 1 && richPsdPairSource.width == richPsdPairImported.compositionWidth
                && richPsdPairSource.height == richPsdPairImported.compositionHeight && hasMatchedColorLayerIdentity,
                "PSD depth import should prove N:N source-domain pairing against the local depth-draw multi-layer PSD fixture");
        }
        auto pairedPsdColorPngDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("paired-psd-color-flat-depth.png")});
        writeRegressionPng(pairedPsdColorPngDepthPath, 255, 255, 255, 1, 1);
        auto pairedPsdColorPngImported = ngBuildPsdDepthsFromSource(nijigenerate::project::incActivePuppet(),
            pairedPsdColorPngDepthPath, pairedPsdColorSettings);
        check(pairedPsdColorPngImported.colorSource.kind == PsdDepthCompositeSourceKind::PsdLayers
            && pairedPsdColorPngImported.colorSource.width == 1 && pairedPsdColorPngImported.colorSource.height == 1
            && pairedPsdColorPngImported.grids.size() == 0 && pairedPsdColorPngImported.compositionDiagnostics.size() > 0
            && pairedPsdColorPngImported.compositionDiagnostics[pairedPsdColorPngImported.compositionDiagnostics.size() - 1].type
                == text("missing-color-source"),
            "PSD depth import should use explicit PSD color source for flat PNG depth N:1 instead of falling back to active targets");
        auto flatComposedDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("flat-composed-depth.png")});
        writeRegressionPng(flatComposedDepthPath, 255, 255, 255, 4, 4);
        auto composedGridA = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        composedGridA->setName(text("color-layer-a").toNullableString());
        composedGridA->zSort(1.0f);
        composedGridA->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}});
        auto composedGridB = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        composedGridB->setName(text("color-layer-b").toNullableString());
        composedGridB->zSort(-1.0f);
        composedGridB->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}});
        auto composedBareGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        composedBareGrid->setName(text("bare-depth-target").toNullableString());
        composedBareGrid->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}});
        auto composedTextureA = Texture::createSourceDeferred([] {
            return ShallowTexture(SharedArray<std::uint8_t>{255, 32, 16, 255}, 1, 1, 4, 4);
        }, false);
        auto composedTextureB = Texture::createSourceDeferred([] {
            return ShallowTexture(SharedArray<std::uint8_t>{16, 32, 255, 255}, 1, 1, 4, 4);
        }, false);
        MeshData composedCoverageMesh;
        composedCoverageMesh.vertices = ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-4, -4}, Vec2{4, -4}, Vec2{-4, 4}, Vec2{4, 4}});
        composedCoverageMesh.uvs = ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1}, Vec2{1, 1}});
        composedCoverageMesh.indices = SharedArray<std::uint16_t>{0, 1, 3, 0, 3, 2};
        auto composedPartA = Part::createDataSourceDeferred([&] { return composedCoverageMesh; },
            [&] { return SharedArray<std::shared_ptr<Texture>>{composedTextureA}; },
            [] { return Node::inCreateUUID(); }, [&] { return composedGridA; });
        composedPartA->setName(text("color-layer-a-art").toNullableString());
        auto composedPartB = Part::createDataSourceDeferred([&] { return composedCoverageMesh; },
            [&] { return SharedArray<std::shared_ptr<Texture>>{composedTextureB}; },
            [] { return Node::inCreateUUID(); }, [&] { return composedGridB; });
        composedPartB->setName(text("color-layer-b-art").toNullableString());
        nijigenerate::project::incActivePuppet()->root->build();
        PsdDepthImportSettings composedPngSettings;
        composedPngSettings.convolution = PsdDepthConvolution::Nearest;
        composedPngSettings.matchDirectGridName = false;
        auto composedPngImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), flatComposedDepthPath, composedPngSettings);
        check(composedPngImported.grids.size() == 2 && composedPngImported.composedLayers.size() == 2
            && composedPngImported.mappings.size() == 2 && composedPngImported.mappings[0].status == text("ComposedN1")
            && composedPngImported.mappings[1].status == text("ComposedN1"),
            "PSD depth import should split a flat PNG depth source into color-backed target layers instead of mapping every target");
        check(composedPngImported.compositionMode == PsdDepthCompositionMode::NToOne
            && composedPngImported.compositionModeName == text("N:1") && composedPngImported.compositionWidth == 4
            && composedPngImported.compositionHeight == 4 && composedPngImported.colorLayerCount == 2
            && composedPngImported.sourceDepthLayerCount == 1 && composedPngImported.composedLayerCount == 2,
            "PSD depth import flat PNG composition should expose depth-draw N:1 composition metadata");
        check(composedPngImported.composedLayers[0].targetGridUuid == composedGridA->getUUID()
            && composedPngImported.composedLayers[1].targetGridUuid == composedGridB->getUUID()
            && composedGridA->zSort() > composedGridB->zSort(),
            "PSD depth import N:1 must pass layers to depth-draw back-to-front so smaller zSort/front layers have larger indexes");
        check(composedPngImported.composedLayers.size() == 2
            && composedPngImported.composedLayers[0].layerPath != text("/flat-composed-depth")
            && composedPngImported.composedLayers[1].sourcePath == flatComposedDepthPath
            && composedPngImported.composedLayers[0].depthRgba.size() == 4 * 4 * 4
            && composedPngImported.colorSource.layers.size() == 2
            && composedPngImported.colorSource.layers[0].rgba.size() == 4 * 4 * 4
            && composedPngImported.colorSource.layers[0].maskRgba.size() == 4 * 4 * 4
            && composedPngImported.depthSource.layers.size() == 1
            && composedPngImported.depthSource.layers[0].rgba.size() == 4 * 4 * 4,
            "PSD depth import N:1 should materialize composed color/target layers separately from the flat depth source");
        {
            const auto currentLayers = composedPngImported.composedLayers;
            for (auto layerPreview : currentLayers) {
                check(layerPreview.maskRgba.size() == layerPreview.colorRgba.size(),
                    "PSD depth import surface mask must have the same local dimensions as its color layer");
                const auto count = layerPreview.colorRgba.size() / 4;
                for (std::size_t i = 0; i < count; ++i) {
                    auto index = i * 4;
                    auto colorAlpha = layerPreview.colorRgba[index + 3];
                    auto maskAlpha = layerPreview.maskRgba[index + 3];
                    check((colorAlpha == 0) == (maskAlpha == 0),
                        "PSD depth validity must not erase the color surface mask");
                    if (maskAlpha != 0) {
                        check(layerPreview.maskRgba[index + 0] == 255 && layerPreview.maskRgba[index + 1] == 255
                            && layerPreview.maskRgba[index + 2] == 255,
                            "PSD depth surface mask must not depend on color RGB or depth intensity");
                    }
                }
            }
        }
        bool hasUpperSuppressionDiagnostic = false;
        {
            const auto currentDiagnostics = composedPngImported.compositionDiagnostics;
            for (auto diagnostic : currentDiagnostics) {
                if (diagnostic.type == text("upper-layer-suppression") && diagnostic.count > 0) {
                    hasUpperSuppressionDiagnostic = true;
                    break;
                }
            }
        }
        {
            const bool condition = !hasUpperSuppressionDiagnostic
                && composedPngImported.composedLayers[0].depthStats.maskedPixels == 16
                && composedPngImported.composedLayers[1].depthStats.maskedPixels == 16;
            require(condition, concatenateSourceStrings({text(
                "PSD depth import N:1 must not shrink or prune any layer when contour repair is disabled: "),
                formatted("diag=%s layer0=%s:%s/%s layer1=%s:%s/%s z0=%s z1=%s",
                    hasUpperSuppressionDiagnostic, composedPngImported.composedLayers[0].layerName,
                    composedPngImported.composedLayers[0].depthStats.maskedPixels,
                    composedPngImported.composedLayers[0].depthRgba.size() > 3 ? composedPngImported.composedLayers[0].depthRgba[3] : 0,
                    composedPngImported.composedLayers[1].layerName, composedPngImported.composedLayers[1].depthStats.maskedPixels,
                    composedPngImported.composedLayers[1].depthRgba.size() > 3 ? composedPngImported.composedLayers[1].depthRgba[3] : 0,
                    composedGridA->zSort(), composedGridB->zSort())}));
        }
        auto repairedComposedPngSettings = composedPngSettings;
        repairedComposedPngSettings.repairContourBand = true;
        auto repairedComposedPngImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), flatComposedDepthPath, repairedComposedPngSettings);
        check(repairedComposedPngImported.composedLayers.size() == 2
            && repairedComposedPngImported.composedLayers[1].depthStats.maskedPixels == 16,
            "PSD depth contour repair must preserve a layer when its contour band has no interior seed pixels");
        bool foundComposedA = false;
        bool foundComposedB = false;
        bool foundBare = false;
        {
            const auto currentGrids = composedPngImported.grids;
            for (auto gridResult : currentGrids) {
                if (gridResult.grid == composedGridA) foundComposedA = true;
                if (gridResult.grid == composedGridB) foundComposedB = true;
                if (gridResult.grid == composedBareGrid) foundBare = true;
                const bool condition = gridResult.coverageSources > 0
                    && gridResult.depths.size() == gridResult.grid->getVertices().length();
                require(condition, formatted(
                    "PSD depth import N:1 composed PNG targets should use color/art coverage before target sampling target=%s coverage=%s depths=%s vertices=%s",
                    gridResult.grid->name, gridResult.coverageSources, gridResult.depths.size(),
                    gridResult.grid->getVertices().length()));
            }
        }
        check(foundComposedA && foundComposedB && !foundBare,
            "PSD depth import N:1 composed PNG import should include only targets backed by color/art layers");
        check(composedPngImported.composedLayers[0].layerPath != text("/flat-composed-depth")
            && composedPngImported.composedLayers[1].sourcePath == flatComposedDepthPath,
            "PSD depth import N:1 composed PNG layers should be keyed by color/target layer while retaining the flat depth source path");
        {
            const auto currentLayers = composedPngImported.composedLayers;
            for (auto layerPreview : currentLayers) {
                check(layerPreview.depthStats.maskedPixels <= static_cast<std::size_t>(layerPreview.width)
                    * static_cast<std::size_t>(layerPreview.height),
                    "PSD depth import N:1 composed PNG layers should expose bounded coverage-masked depth stats");
            }
        }
        bool foundComposedColorPreviewA = false;
        bool foundComposedColorPreviewB = false;
        {
            const auto currentLayers = composedPngImported.composedLayers;
            for (auto layerPreview : currentLayers) {
                if (layerPreview.layerPath == text("/color-layer-a-art") && layerPreview.colorRgba.size() >= 4
                    && layerPreview.colorRgba[0] == 255 && layerPreview.colorRgba[1] == 32
                    && layerPreview.colorRgba[2] == 16) foundComposedColorPreviewA = true;
                if (layerPreview.layerPath == text("/color-layer-b-art") && layerPreview.colorRgba.size() >= 4
                    && layerPreview.colorRgba[0] == 16 && layerPreview.colorRgba[1] == 32
                    && layerPreview.colorRgba[2] == 255) foundComposedColorPreviewB = true;
            }
        }
        check(foundComposedColorPreviewA && foundComposedColorPreviewB,
            "PSD depth import N:1 3D previews should use color/art texture pixels instead of the flat depth PNG pixels");
        auto localizedDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("localized-composed-depth.png")});
        writeRegressionPng(localizedDepthPath, 255, 255, 255, 16, 16);
        auto localizedGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        localizedGrid->setName(text("localized-color-layer").toNullableString());
        localizedGrid->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}});
        auto localizedTexture = Texture::createSourceDeferred([] {
            return ShallowTexture(SharedArray<std::uint8_t>{255, 255, 255, 255}, 1, 1, 4, 4);
        }, false);
        MeshData localizedCoverageMesh;
        localizedCoverageMesh.vertices = ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-1, -1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{1, 1}});
        localizedCoverageMesh.uvs = ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1}, Vec2{1, 1}});
        localizedCoverageMesh.indices = SharedArray<std::uint16_t>{0, 1, 3, 0, 3, 2};
        auto localizedPart = Part::createDataSourceDeferred([&] { return localizedCoverageMesh; },
            [&] { return SharedArray<std::shared_ptr<Texture>>{localizedTexture}; },
            [] { return Node::inCreateUUID(); }, [&] { return localizedGrid; });
        localizedPart->setName(text("localized-color-layer-art").toNullableString());
        nijigenerate::project::incActivePuppet()->root->build();
        PsdDepthImportSettings localizedSettings;
        localizedSettings.convolution = PsdDepthConvolution::Nearest;
        localizedSettings.matchDirectGridName = false;
        auto localizedImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), localizedDepthPath, localizedSettings);
        PsdDepthComposedLayer* localizedLayer = nullptr;
        PsdDepthGridResult* localizedGridResult = nullptr;
        {
            auto currentLayers = localizedImported.composedLayers;
            for (auto& layer : currentLayers) {
                if (layer.layerPath == text("/localized-color-layer-art")) localizedLayer = &layer;
            }
        }
        {
            auto currentGrids = localizedImported.grids;
            for (auto& gridResult : currentGrids) {
                if (gridResult.grid == localizedGrid) localizedGridResult = &gridResult;
            }
        }
        check(localizedLayer != nullptr && localizedLayer->width < localizedImported.compositionWidth
            && localizedLayer->height < localizedImported.compositionHeight && localizedLayer->left > 0 && localizedLayer->top > 0,
            "PSD depth import active-target N:1 should crop composed layers to target/art coverage bounds instead of using the full PNG rectangle");
        check(localizedGridResult != nullptr && localizedGridResult->previewWidth < localizedImported.compositionWidth
            && localizedGridResult->previewHeight < localizedImported.compositionHeight,
            "PSD depth import active-target N:1 preview should use localized composed-layer bounds");
        auto multiPartDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("multi-part-composed-depth.png")});
        writeRegressionPng(multiPartDepthPath, 32, 128, 224, 16, 16);
        auto multiPartGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        multiPartGrid->setName(text("multi-part-grid").toNullableString());
        multiPartGrid->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{-2, 0}, Vec2{2, 0}});
        auto multiTextureA = Texture::createSourceDeferred([] {
            return ShallowTexture(SharedArray<std::uint8_t>{255, 255, 255, 255}, 1, 1, 4, 4);
        }, false);
        auto multiTextureB = Texture::createSourceDeferred([] {
            return ShallowTexture(SharedArray<std::uint8_t>{255, 255, 255, 255}, 1, 1, 4, 4);
        }, false);
        MeshData multiMeshA;
        multiMeshA.vertices = ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-4, -2}, Vec2{0, -2}, Vec2{-4, 2}, Vec2{0, 2}});
        multiMeshA.uvs = ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1}, Vec2{1, 1}});
        multiMeshA.indices = SharedArray<std::uint16_t>{0, 1, 3, 0, 3, 2};
        MeshData multiMeshB = multiMeshA;
        multiMeshB.vertices = ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{0, -2}, Vec2{4, -2}, Vec2{0, 2}, Vec2{4, 2}});
        auto multiPartA = Part::createDataSourceDeferred([&] { return multiMeshA; },
            [&] { return SharedArray<std::shared_ptr<Texture>>{multiTextureA}; },
            [] { return Node::inCreateUUID(); }, [&] { return multiPartGrid; });
        multiPartA->setName(text("multi-art-left").toNullableString());
        auto multiPartB = Part::createDataSourceDeferred([&] { return multiMeshB; },
            [&] { return SharedArray<std::shared_ptr<Texture>>{multiTextureB}; },
            [] { return Node::inCreateUUID(); }, [&] { return multiPartGrid; });
        multiPartB->setName(text("multi-art-right").toNullableString());
        nijigenerate::project::incActivePuppet()->root->build();
        PsdDepthImportSettings multiPartSettings;
        multiPartSettings.convolution = PsdDepthConvolution::Nearest;
        multiPartSettings.channel = PsdDepthChannel::B;
        multiPartSettings.matchDirectGridName = false;
        auto multiPartImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), multiPartDepthPath, multiPartSettings);
        bool foundMultiLayerA = false;
        bool foundMultiLayerB = false;
        bool splitLayersUseSelectedBlueChannel = true;
        std::size_t selectedBluePixelCount = 0;
        PsdDepthGridResult* multiPartGridResult = nullptr;
        {
            auto currentLayers = multiPartImported.composedLayers;
            for (auto& layer : currentLayers) {
                if (layer.layerPath == text("/multi-art-left")) foundMultiLayerA = true;
                else if (layer.layerPath == text("/multi-art-right")) foundMultiLayerB = true;
                else continue;
                const auto count = layer.depthRgba.size() / 4;
                for (std::size_t pixelIndex = 0; pixelIndex < count; ++pixelIndex) {
                    auto byteIndex = pixelIndex * 4;
                    if (layer.depthRgba[byteIndex + 3] == 0) continue;
                    ++selectedBluePixelCount;
                    splitLayersUseSelectedBlueChannel = splitLayersUseSelectedBlueChannel
                        && layer.depthRgba[byteIndex] == 224 && layer.depthRgba[byteIndex + 1] == 224
                        && layer.depthRgba[byteIndex + 2] == 224;
                }
            }
        }
        {
            auto currentGrids = multiPartImported.grids;
            for (auto& gridResult : currentGrids) {
                if (gridResult.grid == multiPartGrid) multiPartGridResult = &gridResult;
            }
        }
        check(foundMultiLayerA && foundMultiLayerB,
            "PSD depth import active-target N:1 should preserve active art Part layers instead of collapsing them to the target Grid");
        check(multiPartGridResult != nullptr && multiPartGridResult->layerMasks.size() == 2,
            "PSD depth import active-target N:1 should combine multiple Part layers as layer masks on the same target Grid");
        check(selectedBluePixelCount > 0 && splitLayersUseSelectedBlueChannel,
            "PSD depth import active-target N:1 split layers must decode the selected B channel, not Average RGB");
        auto activeArtPsdDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("active-art-psd-depth.psd")});
        writeRegressionPsdFixture(activeArtPsdDepthPath);
        auto activeArtPsdGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        activeArtPsdGrid->setName(text("active-art-psd-grid").toNullableString());
        activeArtPsdGrid->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{0, 0}});
        auto activeArtPsdTextureA = Texture::createSourceDeferred([] {
            return ShallowTexture(SharedArray<std::uint8_t>{255, 255, 255, 255}, 1, 1, 4, 4);
        }, false);
        auto activeArtPsdTextureB = Texture::createSourceDeferred([] {
            return ShallowTexture(SharedArray<std::uint8_t>{255, 255, 255, 255}, 1, 1, 4, 4);
        }, false);
        auto activeArtPsdHiddenTexture = Texture::createSourceDeferred([] {
            return ShallowTexture(SharedArray<std::uint8_t>{255, 255, 255, 255}, 1, 1, 4, 4);
        }, false);
        auto activeArtPsdDynamicTexture = Texture::createSourceDeferred([] {
            return ShallowTexture(SharedArray<std::uint8_t>{255, 255, 255, 255}, 1, 1, 4, 4);
        }, false);
        MeshData activeArtPsdMesh;
        activeArtPsdMesh.vertices = ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{-1, -1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{1, 1}});
        activeArtPsdMesh.uvs = ngVecaFromVectorsSource(SharedArray<Vec2>{
            Vec2{0, 0}, Vec2{1, 0}, Vec2{0, 1}, Vec2{1, 1}});
        activeArtPsdMesh.indices = SharedArray<std::uint16_t>{0, 1, 3, 0, 3, 2};
        auto activeArtPsdPartA = Part::createDataSourceDeferred([&] { return activeArtPsdMesh; },
            [&] { return SharedArray<std::shared_ptr<Texture>>{activeArtPsdTextureA}; },
            [] { return Node::inCreateUUID(); }, [&] { return activeArtPsdGrid; });
        activeArtPsdPartA->setName(text("active-art-psd-left").toNullableString());
        auto activeArtPsdPartB = Part::createDataSourceDeferred([&] { return activeArtPsdMesh; },
            [&] { return SharedArray<std::shared_ptr<Texture>>{activeArtPsdTextureB}; },
            [] { return Node::inCreateUUID(); }, [&] { return activeArtPsdGrid; });
        activeArtPsdPartB->setName(text("active-art-psd-right").toNullableString());
        auto activeArtPsdHiddenPart = Part::createDataSourceDeferred([&] { return activeArtPsdMesh; },
            [&] { return SharedArray<std::shared_ptr<Texture>>{activeArtPsdHiddenTexture}; },
            [] { return Node::inCreateUUID(); }, [&] { return activeArtPsdGrid; });
        activeArtPsdHiddenPart->setName(text("active-art-psd-hidden").toNullableString());
        activeArtPsdHiddenPart->setEnabled(false);
        auto activeArtPsdHiddenParent = Node::createSourceDeferred([&] { return activeArtPsdGrid; });
        activeArtPsdHiddenParent->setName(text("active-art-psd-hidden-parent").toNullableString());
        activeArtPsdHiddenParent->setEnabled(false);
        auto activeArtPsdAncestorHiddenPart = Part::createDataSourceDeferred([&] { return activeArtPsdMesh; },
            [&] { return SharedArray<std::shared_ptr<Texture>>{activeArtPsdHiddenTexture}; },
            [] { return Node::inCreateUUID(); }, [&] { return activeArtPsdHiddenParent; });
        activeArtPsdAncestorHiddenPart->setName(text("active-art-psd-ancestor-hidden").toNullableString());
        auto activeArtPsdDynamic = nicxlive::core::nodes::DynamicComposite::createSourceDeferred(
            [&] { return activeArtPsdGrid; });
        activeArtPsdDynamic->setName(text("active-art-psd-dynamic-composite").toNullableString());
        activeArtPsdDynamic->textures = std::array<std::shared_ptr<Texture>, 3>{activeArtPsdDynamicTexture, nullptr, nullptr};
        nijigenerate::project::incActivePuppet()->root->build();
        PsdDepthImportSettings activeArtPsdSettings;
        activeArtPsdSettings.convolution = PsdDepthConvolution::Nearest;
        activeArtPsdSettings.matchDirectGridName = false;
        auto activeArtPsdImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), activeArtPsdDepthPath, activeArtPsdSettings);
        bool foundActiveArtPsdLeft = false;
        bool foundActiveArtPsdRight = false;
        bool foundActiveArtPsdGridLayer = false;
        bool foundActiveArtPsdDynamic = false;
        bool foundActiveArtPsdHidden = false;
        bool foundActiveArtPsdAncestorHidden = false;
        {
            const auto currentLayers = activeArtPsdImported.composedLayers;
            for (auto layer : currentLayers) {
                if (layer.layerPath == text("/active-art-psd-left")) foundActiveArtPsdLeft = true;
                if (layer.layerPath == text("/active-art-psd-right")) foundActiveArtPsdRight = true;
                if (layer.layerPath == text("/active-art-psd-grid")) foundActiveArtPsdGridLayer = true;
                if (layer.layerPath == text("/active-art-psd-dynamic-composite")) foundActiveArtPsdDynamic = true;
                if (layer.layerPath == text("/active-art-psd-hidden")) foundActiveArtPsdHidden = true;
                if (layer.layerPath == text("/active-art-psd-ancestor-hidden")) foundActiveArtPsdAncestorHidden = true;
            }
        }
        check(foundActiveArtPsdLeft && foundActiveArtPsdRight && !foundActiveArtPsdGridLayer
            && !foundActiveArtPsdDynamic && !foundActiveArtPsdHidden && !foundActiveArtPsdAncestorHidden,
            "PSD depth import active-target color source should preserve visible artwork Parts without adding hidden, Grid, or DynamicComposite outputs");
        auto pngSecondDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("png-depth-second.png")});
        writeRegressionPng(pngSecondDepthPath, 255, 255, 255, 4, 4);
        auto pngSecondGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        pngSecondGrid->setName(text("png-depth-second").toNullableString());
        pngSecondGrid->getVertices() = ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{0, 0}, Vec2{1, 1}});
        pngSecondGrid->localTransform.update();
        nijigenerate::project::incActivePuppet()->root->build();
        PsdDepthImportSettings pngSecondSettings;
        pngSecondSettings.convolution = PsdDepthConvolution::Nearest;
        pngSecondSettings.layerTargetGridUuidOverrides[text("/png-depth-second")] = formatted("%s", pngSecondGrid->getUUID());
        auto pngSecondImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), pngSecondDepthPath, pngSecondSettings);
        check(pngSecondImported.grids.size() == 1 && pngSecondImported.grids[0].grid == pngSecondGrid,
            "PSD depth import dialog source builder should map a second PNG source to a second target");
        PsdDepthImportResult combinedPngImport;
        combinedPngImport.grids.appendRange(pngImported.grids);
        combinedPngImport.grids.appendRange(pngSecondImported.grids);
        combinedPngImport.mappings.appendRange(pngImported.mappings);
        combinedPngImport.mappings.appendRange(pngSecondImported.mappings);
        combinedPngImport.composedLayers.appendRange(pngImported.composedLayers);
        combinedPngImport.composedLayers.appendRange(pngSecondImported.composedLayers);
        ngPsdDepthRefreshDerivedState(combinedPngImport);
        combinedPngImport.matchedLayers = pngImported.matchedLayers + pngSecondImported.matchedLayers;
        combinedPngImport.unmatchedLayers = pngImported.unmatchedLayers + pngSecondImported.unmatchedLayers;
        pngGrid->replaceDepths(Slice<float>{0.1f, 0.2f, 0.3f});
        pngSecondGrid->replaceDepths(Slice<float>{0.4f, 0.5f, 0.6f});
        auto invalidPngImport = combinedPngImport;
        invalidPngImport.grids = combinedPngImport.grids.duplicate();
        invalidPngImport.grids[0].depths = Slice<float>{0.7f, 0.8f, 0.9f};
        invalidPngImport.grids[1].depths = Slice<float>{0.1f};
        auto invalidFirstBefore = pngGrid->copyDepths();
        auto invalidSecondBefore = pngSecondGrid->copyDepths();
        auto invalidHistoryLength = nijigenerate::core::actionstack::incActionHistory().size();
        auto invalidPngView = ngPsdDepthComposedViewForRegression(invalidPngImport);
        auto invalidPngApplyResult = ngApplyPsdDepthImportResult(invalidPngView);
        check(!invalidPngApplyResult->succeeded
            && invalidPngApplyResult->message.view().find("depths length") != std::string_view::npos
            && exactDepths(pngGrid->copyDepths(), [&] { return invalidFirstBefore; })
            && exactDepths(pngSecondGrid->copyDepths(), [&] { return invalidSecondBefore; })
            && nijigenerate::core::actionstack::incActionHistory().size() == invalidHistoryLength,
            "PSD depth Apply should validate every target before mutating any target or action history");
        auto offsetBaseline = composePsdDepthImportForRegression(combinedPngImport);
        auto canonicalDepthDrawSession = ngPsdDepthImportResultToDepthDrawSession(combinedPngImport);
        auto canonicalFirstView = std::allocate_shared<DepthTargetView>(
            nicxlive::core::d_runtime::SourceAllocator<DepthTargetView>{}, pngGrid);
        canonicalFirstView->baseDepths = pngGrid->copyDepths();
        canonicalFirstView->depths = pngGrid->copyDepths();
        auto canonicalFirstResult = ngComposeDepthDrawTarget(canonicalDepthDrawSession, canonicalFirstView,
            combinedPngImport.grids[0].documentWidth, combinedPngImport.grids[0].documentHeight);
        Slice<float> baselineFirstDepths;
        Slice<float> baselineSecondDepths;
        {
            const auto currentGrids = offsetBaseline.grids;
            for (auto gridResult : currentGrids) {
                if (gridResult.grid == pngGrid) baselineFirstDepths = gridResult.depths.duplicate();
                if (gridResult.grid == pngSecondGrid) baselineSecondDepths = gridResult.depths.duplicate();
            }
        }
        check(exactDepths(baselineFirstDepths, [&] { return canonicalFirstResult.depths; }),
            "PSD import must preserve the exact DepthDraw composer output without Apply-only scaling");
        bool changedFirstLayerOffset = false;
        {
            auto currentLayers = combinedPngImport.composedLayers;
            for (auto& layer : currentLayers) {
                if (layer.targetGridUuid != pngGrid->getUUID()) continue;
                layer.depthOffset = 0.75f;
                changedFirstLayerOffset = true;
                break;
            }
        }
        check(changedFirstLayerOffset, "multi-target PSD fixture must expose the first target layer offset");
        auto offsetAdjusted = composePsdDepthImportForRegression(combinedPngImport);
        Slice<float> adjustedFirstDepths;
        Slice<float> adjustedSecondDepths;
        {
            const auto currentGrids = offsetAdjusted.grids;
            for (auto gridResult : currentGrids) {
                if (gridResult.grid == pngGrid) adjustedFirstDepths = gridResult.depths.duplicate();
                if (gridResult.grid == pngSecondGrid) adjustedSecondDepths = gridResult.depths.duplicate();
            }
        }
        check(exactDepths(adjustedSecondDepths, [&] { return baselineSecondDepths; }),
            "changing one PSD layer offset must not alter another target's composed depths");
        check(adjustedFirstDepths.size() == baselineFirstDepths.size() && adjustedFirstDepths.size() > 0,
            "offset-only PSD compose must preserve the target depth count");
        auto firstOffsetDelta = adjustedFirstDepths[0] - baselineFirstDepths[0];
        const auto adjustedCount = adjustedFirstDepths.size();
        for (std::size_t i = 1; i < adjustedCount; ++i) {
            check(near(adjustedFirstDepths[i] - baselineFirstDepths[i], firstOffsetDelta),
                "offset-only PSD compose must add one constant depth delta without changing scale");
        }
        auto targetScopedImport = combinedPngImport;
        targetScopedImport.composedLayers = targetScopedImport.composedLayers.duplicate();
        {
            auto currentLayers = targetScopedImport.composedLayers;
            for (auto& layer : currentLayers) layer.layerPath = text("/shared-depth-source");
        }
        targetScopedImport.composedLayers[0].depthOffset = 0.75f;
        targetScopedImport.composedLayers[1].depthOffset = 0.0f;
        targetScopedImport.grids = targetScopedImport.grids.duplicate();
        {
            auto currentGrids = targetScopedImport.grids;
            for (auto& gridResult : currentGrids) {
                gridResult.layerMasks = gridResult.layerMasks.duplicate();
                auto currentMasks = gridResult.layerMasks;
                for (auto& layerMask : currentMasks) layerMask.layerPath = text("/shared-depth-source");
            }
        }
        auto targetScopedFirst = composePsdDepthImportForRegression(targetScopedImport);
        Slice<float> targetScopedSecondDepths;
        {
            const auto currentGrids = targetScopedFirst.grids;
            for (auto gridResult : currentGrids) {
                if (gridResult.grid == pngSecondGrid) targetScopedSecondDepths = gridResult.depths.duplicate();
            }
        }
        check(exactDepths(targetScopedSecondDepths, [&] { return baselineSecondDepths; }),
            "same source layer paths must still keep per-target PSD transforms isolated during Apply");
        {
            auto currentLayers = combinedPngImport.composedLayers;
            for (auto& layer : currentLayers) {
                if (layer.targetGridUuid == pngGrid->getUUID()) layer.depthOffset = 0.0f;
            }
        }
        history::incActionClearHistory();
        auto combinedComposedForDebug = composePsdDepthImportForRegression(combinedPngImport);
        check(applyPsdDepthImportForRegression(combinedPngImport)->succeeded,
            "PSD depth import apply should update multiple source-builder targets in one operation");
        auto combinedPngAction = std::dynamic_pointer_cast<nijigenerate::actions::GroupAction>(history::incActionTop());
        check(combinedPngAction != nullptr && combinedPngAction->actions.size() == 2,
            "PSD depth import apply should group multiple source-builder target updates into one undo action");
        {
            const bool condition = exactDepths(pngGrid->copyDepths(), [] { return Slice<float>{1.0f, 1.0f, 1.0f}; })
                && exactDepths(pngSecondGrid->copyDepths(), [] { return Slice<float>{1.0f, 1.0f, 1.0f}; });
            require(condition, concatenateSourceStrings({text(
                "PSD depth import apply should write all enabled mapped targets, not only one selected binding: "),
                formatted("composed0=%s composed1=%s applied0=%s applied1=%s",
                    combinedComposedForDebug.grids.size() > 0 ? combinedComposedForDebug.grids[0].depths : Slice<float>{},
                    combinedComposedForDebug.grids.size() > 1 ? combinedComposedForDebug.grids[1].depths : Slice<float>{},
                    pngGrid->copyDepths(), pngSecondGrid->copyDepths())}));
        }
        history::incActionUndo();
        check(exactDepths(pngGrid->copyDepths(), [] { return Slice<float>{0.1f, 0.2f, 0.3f}; })
            && exactDepths(pngSecondGrid->copyDepths(), [] { return Slice<float>{0.4f, 0.5f, 0.6f}; }),
            "undo PSD depth import should restore every target from a multi-binding apply");
        history::incActionRedo();
        check(exactDepths(pngGrid->copyDepths(), [] { return Slice<float>{1.0f, 1.0f, 1.0f}; })
            && exactDepths(pngSecondGrid->copyDepths(), [] { return Slice<float>{1.0f, 1.0f, 1.0f}; }),
            "redo PSD depth import should reapply every target from a multi-binding apply");
        PsdDepthImportSettings pngGpuSettings;
        pngGpuSettings.convolution = PsdDepthConvolution::Nearest;
        pngGpuSettings.useGpuComposition = true;
        pngGpuSettings.layerTargetGridUuidOverrides[text("/png-depth-grid")] = formatted("%s", pngGrid->getUUID());
        pngGrid->replaceDepths(Slice<float>{0.1f, 0.2f, 0.3f});
        auto pngGpuImported = ngBuildPsdDepthsFromSource(
            nijigenerate::project::incActivePuppet(), pngDepthPath, pngGpuSettings);
        PsdDepthComposedView pngGpuComposed;
        Text pngGpuComposeError;
        check(!ngComposePsdDepthImportResult(pngGpuImported, pngGpuComposed, pngGpuComposeError)
            && pngGpuComposeError.view().find("CPU fallback is disabled") != std::string_view::npos
            && exactDepths(pngGrid->copyDepths(), [] { return Slice<float>{0.1f, 0.2f, 0.3f}; }),
            "PSD depth import GPU-selected preview composition should fail explicitly instead of falling back to CPU mutation");
        fakeDepthDrawGpuNextJobId = 1;
        fakeDepthDrawGpuSubmitCount = 0;
        fakeDepthDrawGpuPollCount = 0;
        fakeDepthDrawGpuNotReadyPolls = 0;
        fakeDepthDrawGpuReadbacks = nullptr;
        fakeDepthDrawGpuReadbacks[1] = DepthDrawGpuComposeReadback{pngGrid->getUUID(),
            Slice<float>{0.75f, 0.75f, 0.75f}, Slice<std::int32_t>{0, 0, 0},
            Slice<DepthDrawGpuLayerReadback>{DepthDrawGpuLayerReadback{0,
                Slice<std::uint8_t>{1, 1, 1}, Slice<float>{0.75f, 0.75f, 0.75f}}}};
        ngSetDepthDrawGpuTestHooks(&fakeDepthDrawGpuSupported, &fakeDepthDrawGpuSubmit, &fakeDepthDrawGpuPoll);
        nijigenerate::compat::ngRunWithScopeExit([&] {
            pngGrid->replaceDepths(Slice<float>{0.1f, 0.2f, 0.3f});
            auto pngGpuImportedWithBackend = ngBuildPsdDepthsFromSource(
                nijigenerate::project::incActivePuppet(), pngDepthPath, pngGpuSettings);
            history::incActionClearHistory();
            auto pngGpuApplyWithBackend = applyPsdDepthImportForRegression(pngGpuImportedWithBackend);
            check(pngGpuApplyWithBackend->succeeded && fakeDepthDrawGpuSubmitCount == 1
                && fakeDepthDrawGpuPollCount == 1
                && exactDepths(pngGrid->copyDepths(), [] { return Slice<float>{0.75f, 0.75f, 0.75f}; }),
                "PSD depth import GPU-selected apply should submit, poll, read back, and apply GPU depths without CPU fallback");
            history::incActionUndo();
            check(exactDepths(pngGrid->copyDepths(), [] { return Slice<float>{0.1f, 0.2f, 0.3f}; }),
                "undo PSD depth import GPU-selected apply should restore previous depths");
            fakeDepthDrawGpuNextJobId = 2;
            fakeDepthDrawGpuPollCount = 0;
            fakeDepthDrawGpuNotReadyPolls = 1;
            fakeDepthDrawGpuReadbacks[2] = DepthDrawGpuComposeReadback{pngGrid->getUUID(),
                Slice<float>{0.6f, 0.6f, 0.6f}, Slice<std::int32_t>{0, 0, 0},
                Slice<DepthDrawGpuLayerReadback>{DepthDrawGpuLayerReadback{0,
                    Slice<std::uint8_t>{1, 1, 1}, Slice<float>{0.6f, 0.6f, 0.6f}}}};
            auto incrementalGpuImported = ngBuildPsdDepthsFromSource(
                nijigenerate::project::incActivePuppet(), pngDepthPath, pngGpuSettings);
            auto incrementalDepthsBeforePoll = incrementalGpuImported.grids[0].depths.duplicate();
            PsdDepthGpuComposeWork incrementalGpuWork;
            Text incrementalGpuError;
            {
                const bool condition = ngSubmitPsdDepthImportGpu(incrementalGpuImported, incrementalGpuWork, incrementalGpuError)
                    && incrementalGpuWork.isActive() && ngPendingDepthDrawGpuComposeJobCount() == 1;
                require(condition, concatenateSourceStrings({text(
                    "PSD GPU preview should retain submitted work without waiting on the UI thread: "), incrementalGpuError}));
            }
            bool incrementalGpuReady = false;
            PsdDepthComposedView incrementalGpuComposed;
            check(ngPollPsdDepthImportGpu(incrementalGpuImported, incrementalGpuWork, incrementalGpuReady,
                    incrementalGpuComposed, incrementalGpuError)
                && !incrementalGpuReady && incrementalGpuWork.isActive() && fakeDepthDrawGpuPollCount == 1
                && exactDepths(incrementalGpuImported.grids[0].depths, [&] { return incrementalDepthsBeforePoll; }),
                "one PSD GPU preview poll must return control while unfinished and leave the prior preview transactional");
            check(ngPollPsdDepthImportGpu(incrementalGpuImported, incrementalGpuWork, incrementalGpuReady,
                    incrementalGpuComposed, incrementalGpuError)
                && incrementalGpuReady && !incrementalGpuWork.isActive() && fakeDepthDrawGpuPollCount == 2
                && exactDepths(incrementalGpuImported.grids[0].depths, [] { return Slice<float>{0.6f, 0.6f, 0.6f}; })
                && ngPendingDepthDrawGpuComposeJobCount() == 0,
                "a later PSD GPU preview poll should atomically publish the completed readback");
            fakeDepthDrawGpuNextJobId = 3;
            fakeDepthDrawGpuNotReadyPolls = 1;
            fakeDepthDrawGpuReadbacks[3] = DepthDrawGpuComposeReadback{pngGrid->getUUID(),
                Slice<float>{0.5f, 0.5f, 0.5f}, Slice<std::int32_t>{0, 0, 0},
                Slice<DepthDrawGpuLayerReadback>{DepthDrawGpuLayerReadback{0,
                    Slice<std::uint8_t>{1, 1, 1}, Slice<float>{0.5f, 0.5f, 0.5f}}}};
            auto canceledGpuImported = ngBuildPsdDepthsFromSource(
                nijigenerate::project::incActivePuppet(), pngDepthPath, pngGpuSettings);
            PsdDepthGpuComposeWork canceledGpuWork;
            Text canceledGpuError;
            check(ngSubmitPsdDepthImportGpu(canceledGpuImported, canceledGpuWork, canceledGpuError)
                && ngPendingDepthDrawGpuComposeJobCount() == 1,
                "PSD GPU preview cancellation fixture should retain one submitted job");
            ngCancelPsdDepthImportGpu(canceledGpuWork);
            check(!canceledGpuWork.isActive() && ngPendingDepthDrawGpuComposeJobCount() == 0,
                "canceling a PSD GPU preview must remove its retained backend jobs immediately");
            auto uiEditedPngImported = ngBuildPsdDepthsFromSource(
                nijigenerate::project::incActivePuppet(), pngDepthPath, pngSettings);
            check(uiEditedPngImported.composedLayers.size() == 1,
                "PSD depth import UI-edit regression fixture should have one composed layer");
            uiEditedPngImported.composedLayers[0].depthOffset = -0.25f;
            uiEditedPngImported.composedLayers[0].depthScale = 0.5f;
            ngPsdDepthRefreshDerivedState(uiEditedPngImported);
            PsdDepthComposedView uiEditedComposedView;
            Text uiEditedComposeError;
            {
                const bool condition = ngComposePsdDepthImportResult(
                    uiEditedPngImported, uiEditedComposedView, uiEditedComposeError);
                require(condition, concatenateSourceStrings({text(
                    "PSD depth import UI-edited composition should succeed: "), uiEditedComposeError}));
            }
            check(uiEditedPngImported.grids.size() == 1
                && exactDepths(uiEditedPngImported.grids[0].depths, [] { return Slice<float>{0.25f, 0.25f, 0.25f}; }),
                "PSD depth import Source/Mapping compose should use the same UI-edited Z transform as 3D Adjust preview");
            pngGrid->replaceDepths(Slice<float>{0.1f, 0.2f, 0.3f});
            history::incActionClearHistory();
            auto expectedUiEditedDepths = uiEditedPngImported.grids[0].depths.duplicate();
            auto uiEditedApply = ngApplyPsdDepthImportResult(uiEditedComposedView);
            check(uiEditedApply->succeeded && exactDepths(pngGrid->copyDepths(), [&] { return expectedUiEditedDepths; }),
                "PSD depth import apply should write the exact Source/Mapping composed UI-edited depths");
            auto pngPathDepthPath = paths::ngBuildPath(std::array<Text, 2>{pngFixtureDir, text("png-depth-path.png")});
            writeRegressionPng(pngPathDepthPath, 255, 255, 255, 4, 4);
            auto pngPath = ExPathDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
            pngPath->setName(text("png-depth-path").toNullableString());
            pngPath->rebuffer(ngVecaFromVectorsSource(SharedArray<Vec2>{Vec2{-1, -1}, Vec2{0, 0}, Vec2{1, 1}}));
            pngPath->replaceDepths(Slice<float>{0.25f, 0.25f, 0.25f});
            nijigenerate::project::incActivePuppet()->root->build();
            PsdDepthImportSettings pngPathSettings;
            pngPathSettings.convolution = PsdDepthConvolution::Nearest;
            pngPathSettings.layerTargetGridUuidOverrides[text("/png-depth-path")] = formatted("%s", pngPath->getUUID());
            auto pngPathImported = ngBuildPsdDepthsFromSource(
                nijigenerate::project::incActivePuppet(), pngPathDepthPath, pngPathSettings);
            check(pngPathImported.mappings.size() == 1 && pngPathImported.mappings[0].matched
                && pngPathImported.grids.size() == 1 && pngPathImported.grids[0].grid == pngPath,
                "PSD depth import dialog source builder should map PNG sources to PathDeformer targets");
            check(pngPathImported.grids[0].sampledVertices == pngPath->getVertices().length()
                && pngPathImported.grids[0].missingVertices == 0
                && pngPathImported.grids[0].depths.size() == pngPath->getVertices().length(),
                "PSD depth import dialog source builder should sample PNG sources into PathDeformer control depths");
            history::incActionClearHistory();
            check(applyPsdDepthImportForRegression(pngPathImported)->succeeded,
                "PSD depth import apply should accept PathDeformer target results");
            check(exactDepths(pngPath->copyDepths(), [] { return Slice<float>{1.0f, 1.0f, 1.0f}; }),
                "PSD depth import apply should write PathDeformer depth arrays");
            history::incActionUndo();
            check(exactDepths(pngPath->copyDepths(), [] { return Slice<float>{0.25f, 0.25f, 0.25f}; }),
                "undo PSD depth import should restore previous PathDeformer depths");
            history::incActionRedo();
            check(exactDepths(pngPath->copyDepths(), [] { return Slice<float>{1.0f, 1.0f, 1.0f}; }),
                "redo PSD depth import should restore imported PathDeformer depths");
            settings.convolution = PsdDepthConvolution::Gaussian3x3;
            check(ngDepthSampleConvolutionRadius(DepthSampleConvolution::Gaussian3x3, 9) == 1
                && ngDepthSampleConvolutionRadius(DepthSampleConvolution::Gaussian5x5, 9) == 2
                && ngDepthSampleConvolutionRadius(DepthSampleConvolution::GaussianCustom, 3) == 3,
                "common depth sample helper should preserve convolution radius selection");
            check(near(ngDepthSampleKernelWeight(DepthSampleConvolution::Gaussian3x3, 1, 0, 0), 4.0f)
                && near(ngDepthSampleKernelWeight(DepthSampleConvolution::Gaussian3x3, 1, 1, 1), 1.0f)
                && near(ngDepthSampleKernelWeight(DepthSampleConvolution::Gaussian5x5, 2, 0, 0), 36.0f)
                && near(ngDepthSampleKernelWeight(DepthSampleConvolution::Gaussian5x5, 2, 2, 2), 1.0f),
                "common depth sample helper should preserve fixed Gaussian kernel weights");
            check(ngDepthSampleConvolutionUsesWeightedAverage(DepthSampleConvolution::Box3x3)
                && ngDepthSampleConvolutionUsesWeightedAverage(DepthSampleConvolution::GaussianCustom)
                && ngDepthSampleConvolutionUsesMedian(DepthSampleConvolution::MedianCustom)
                && ngDepthSampleConvolutionUsesFrontmost(DepthSampleConvolution::Frontmost3x3)
                && ngDepthSampleConvolutionUsesBackmost(DepthSampleConvolution::BackmostCustom),
                "common depth sample helper should classify convolution modes for PSD import");
            auto weightedValues = Slice<float>{0.0f, 1.0f, -1.0f};
            auto weightedWeights = Slice<float>{1.0f, 2.0f, 0.0f};
            auto weighted = ngDepthSampleWeightedAverage(
                std::span<const float>{weightedValues.data(), weightedValues.size()},
                std::span<const float>{weightedWeights.data(), weightedWeights.size()});
            check(weighted.valid && near(weighted.value, 2.0f / 3.0f) && near(weighted.weight, 3.0f),
                "common depth sample helper should preserve weighted-average aggregation");
            DepthSampleWeightedAccumulator weightedAccumulator;
            weightedAccumulator.add(0.0f, 1.0f);
            weightedAccumulator.add(1.0f, 2.0f);
            weightedAccumulator.add(-1.0f, 0.0f);
            auto weightedAccumulated = weightedAccumulator.result();
            check(weightedAccumulated.valid && near(weightedAccumulated.value, weighted.value)
                && near(weightedAccumulated.weight, weighted.weight),
                "common depth sample helper should support allocation-free weighted-average aggregation");
            auto medianValues = Slice<float>{1.0f, -1.0f, 0.25f};
            auto median = ngDepthSampleMedian(std::span<const float>{medianValues.data(), medianValues.size()});
            check(median.valid && near(median.value, 0.25f),
                "common depth sample helper should preserve median aggregation");
            const auto extremeSample = [](Slice<float> values, bool frontmost) {
                return ngDepthSampleExtreme(std::span<const float>{values.data(), values.size()}, frontmost);
            };
            check(extremeSample(Slice<float>{0.2f, 0.9f, -0.5f}, true).value == 0.9f
                && extremeSample(Slice<float>{0.2f, 0.9f, -0.5f}, false).value == -0.5f,
                "common depth sample helper should preserve frontmost/backmost aggregation");
            DepthSampleExtremeAccumulator extremeAccumulator;
            extremeAccumulator.frontmost = false;
            extremeAccumulator.add(0.2f);
            extremeAccumulator.add(0.9f);
            extremeAccumulator.add(-0.5f);
            check(extremeAccumulator.result().value == -0.5f,
                "common depth sample helper should support allocation-free extreme aggregation");
            auto gaussian = ngPsdDepthSamplePixels(pixels, 3, 3, 1, 1, settings);
            {
                const bool condition = gaussian.valid && gaussian.value > 0.0f && gaussian.value < 1.0f;
                require(condition, concatenateSourceStrings({text(
                    "PSD depth Gaussian3x3 should smooth toward valid weighted neighbors; value="), formatted("%s", gaussian.value)}));
            }
            settings.convolution = PsdDepthConvolution::Frontmost3x3;
            auto front = ngPsdDepthSamplePixels(pixels, 3, 3, 1, 1, settings);
            check(front.valid && near(front.value, 1.0f), "PSD depth Frontmost3x3 should choose the closest value");
            settings.convolution = PsdDepthConvolution::Backmost3x3;
            auto back = ngPsdDepthSamplePixels(pixels, 3, 3, 1, 1, settings);
            check(back.valid && near(back.value, -1.0f), "PSD depth Backmost3x3 should choose the farthest value");
            Slice<std::uint8_t> transparentNeighbors;
            const auto addTransparent = [&](std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
                transparentNeighbors.appendRange(Slice<std::uint8_t>{r, g, b, a});
            };
            addTransparent(255, 255, 255, 0); addTransparent(255, 255, 255, 0); addTransparent(255, 255, 255, 0);
            addTransparent(255, 255, 255, 0); addTransparent(0, 0, 0, 255);     addTransparent(255, 255, 255, 0);
            addTransparent(255, 255, 255, 0); addTransparent(255, 255, 255, 0); addTransparent(255, 255, 255, 0);
            settings.convolution = PsdDepthConvolution::Box3x3;
            auto alphaFiltered = ngPsdDepthSamplePixels(transparentNeighbors, 3, 3, 1, 1, settings);
            check(alphaFiltered.valid && near(alphaFiltered.value, -1.0f),
                "PSD depth convolution should ignore transparent pixels instead of treating them as black");
            Slice<std::uint8_t> translucentEdge;
            const auto addTranslucent = [&](std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
                translucentEdge.appendRange(Slice<std::uint8_t>{r, g, b, a});
            };
            addTranslucent(0, 0, 0, 16);     addTranslucent(0, 0, 0, 16);       addTranslucent(0, 0, 0, 16);
            addTranslucent(0, 0, 0, 16);     addTranslucent(255, 255, 255, 255); addTranslucent(0, 0, 0, 16);
            addTranslucent(0, 0, 0, 16);     addTranslucent(0, 0, 0, 16);       addTranslucent(0, 0, 0, 16);
            auto translucentWeighted = ngPsdDepthSamplePixels(translucentEdge, 3, 3, 1, 1, settings);
            {
                const bool condition = translucentWeighted.valid && translucentWeighted.value > 0.25f;
                require(condition, concatenateSourceStrings({text(
                    "PSD depth convolution should weight translucent edge pixels by alpha; value="),
                    formatted("%s", translucentWeighted.value)}));
            }
            Slice<std::uint8_t> opaqueDepthWithBackEdges;
            Slice<std::uint8_t> normalCoverage;
            const auto addDepthCoverage = [&](std::uint8_t r, std::uint8_t g, std::uint8_t b,
                    std::uint8_t depthAlpha, std::uint8_t coverageAlpha) {
                opaqueDepthWithBackEdges.appendRange(Slice<std::uint8_t>{r, g, b, depthAlpha});
                normalCoverage.appendRange(Slice<std::uint8_t>{255, 255, 255, coverageAlpha});
            };
            addDepthCoverage(0, 0, 0, 255, 16);
            addDepthCoverage(0, 0, 0, 255, 16);
            addDepthCoverage(0, 0, 0, 255, 16);
            addDepthCoverage(0, 0, 0, 255, 16);
            addDepthCoverage(255, 255, 255, 255, 255);
            addDepthCoverage(0, 0, 0, 255, 16);
            addDepthCoverage(0, 0, 0, 255, 16);
            addDepthCoverage(0, 0, 0, 255, 16);
            addDepthCoverage(0, 0, 0, 255, 16);
            auto coverageWeighted = ngPsdDepthSamplePixelsWithCoverage(
                opaqueDepthWithBackEdges, 3, 3, normalCoverage, 3, 3, 1.0f, 1, 1, settings);
            {
                const bool condition = coverageWeighted.valid && coverageWeighted.value > 0.25f;
                require(condition, concatenateSourceStrings({text(
                    "PSD depth convolution should weight opaque depth pixels by matched normal-layer coverage; value="),
                    formatted("%s", coverageWeighted.value)}));
            }
            settings.convolution = PsdDepthConvolution::Nearest;
            auto coverageFiltered = ngPsdDepthSamplePixelsWithCoverage(
                opaqueDepthWithBackEdges, 3, 3, normalCoverage, 3, 3, 1.0f, 0, 0, settings);
            check(!coverageFiltered.valid,
                "PSD depth sampling should reject low normal-layer coverage instead of importing edge background depth");
            Slice<std::uint8_t> opaquePixel{255, 255, 255, 255};
            auto invisibleByLayerOpacity = ngPsdDepthSamplePixelsWithOpacity(opaquePixel, 1, 1, 0, 0, 0.005f, settings);
            check(!invisibleByLayerOpacity.valid,
                "PSD depth sampling should apply layer opacity before alpha thresholding");
            Slice<std::uint8_t> customRadiusPixels;
            for (int y = 0; y < 5; ++y) {
                for (int x = 0; x < 5; ++x) {
                    if (x == 2 && y == 2) customRadiusPixels.appendRange(Slice<std::uint8_t>{0, 0, 0, 255});
                    else if ((x == 0 || x == 4) && (y == 0 || y == 4))
                        customRadiusPixels.appendRange(Slice<std::uint8_t>{255, 255, 255, 255});
                    else customRadiusPixels.appendRange(Slice<std::uint8_t>{0, 0, 0, 0});
                }
            }
            settings.convolution = PsdDepthConvolution::BoxCustom;
            settings.customRadius = 1;
            auto customRadius1 = ngPsdDepthSamplePixels(customRadiusPixels, 5, 5, 2, 2, settings);
            settings.customRadius = 2;
            auto customRadius2 = ngPsdDepthSamplePixels(customRadiusPixels, 5, 5, 2, 2, settings);
            check(customRadius1.valid && customRadius2.valid && customRadius2.value > customRadius1.value,
                "PSD depth custom radius should include farther valid pixels when radius grows");
            auto composed = ngPsdDepthFrontmost(Slice<PsdDepthSampleResult>{
                PsdDepthSampleResult{false, -1.0f}, PsdDepthSampleResult{true, 0.25f}, PsdDepthSampleResult{true, 0.75f}});
            check(composed.valid && near(composed.value, 0.75f), "PSD depth layer compositing should choose frontmost depth");
            auto layerPath = text("/Depth/front");
            auto otherLayerPath = text("/Depth/back");
            check(ngPsdDepthGridEnabled(settings, grid->getUUID()),
                "PSD depth GridDeformer import switch should default to enabled");
            check(ngPsdDepthGridLayerEnabled(settings, grid->getUUID(), layerPath),
                "PSD depth per-grid layer switch should default to enabled");
            settings.disabledGridUuids[formatted("%s", grid->getUUID())] = true;
            check(!ngPsdDepthGridEnabled(settings, grid->getUUID()),
                "PSD depth GridDeformer import switch should disable a matching grid");
            auto otherGrid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
            otherGrid->setName(text("psd-depth-other-grid").toNullableString());
            check(ngPsdDepthGridEnabled(settings, otherGrid->getUUID()),
                "PSD depth GridDeformer import switch should not disable other grids");
            settings.disabledGridLayerKeys[ngPsdDepthGridLayerKey(grid->getUUID(), layerPath)] = true;
            check(!ngPsdDepthGridLayerEnabled(settings, grid->getUUID(), layerPath),
                "PSD depth per-grid layer switch should disable only the targeted layer");
            check(ngPsdDepthGridLayerEnabled(settings, grid->getUUID(), otherLayerPath),
                "PSD depth per-grid layer switch should leave other layers enabled");
            check(ngPsdDepthGridLayerEnabled(settings, otherGrid->getUUID(), layerPath),
                "PSD depth per-grid layer switch should be scoped by GridDeformer");
        }, [] { ngClearDepthDrawGpuTestHooks(); });
    }, [&] {
        if (paths::ngFileExists(pngFixtureDir.view())) paths::ngRmdirRecurse(pngFixtureDir);
    });
}
