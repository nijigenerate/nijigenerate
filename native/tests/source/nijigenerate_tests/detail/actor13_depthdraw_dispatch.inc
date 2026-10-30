// Existing regression.d9915-10056; include once inside regression::detail.
std::size_t runDepthDrawAlphaMaskFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawAlphaDepthGapFillFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawDepthCleanupFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawDepthPruneFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawDepthSplitFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawLayerBoundsSamplingFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawPsdMaskExportFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawGlobalDepthScaleFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawGridConvolutionFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawArtifactFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);
std::size_t runDepthDrawLocalMergePoliciesFixtureFile(nijigenerate::compat::RetainedString, nijigenerate::core::settings::SettingsJsonObject);

std::size_t runDepthDrawFixtureFile(nijigenerate::compat::RetainedString path) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngFloatToInt32;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::vertex::define_mesh::DefineGridCommand;
    using nijigenerate::ext::nodes::ExGridDeformer;
    using nijigenerate::viewport::depth::common::targetview::DepthTargetView;
    using namespace nijigenerate::viewport::depth::draw;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::d_runtime::SourceAllocator;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto render = []<class... Values>(std::string_view pattern, const Values&... values) {
        const std::array<format::TypedFormatArgument, sizeof...(Values)> arguments{
            format::ngFormatArgument(values)...};
        auto output = format::ngFormatTyped(pattern, arguments);
        return nijigenerate::platform::detail::ngPathAllocation([&] { return RetainedString(std::move(output)); });
    };
    auto input = nijigenerate::compat::phobos::ngReadUtf8Text(path);
    auto root = nijigenerate::autorig::json::detail::parseJSON(RetainedStringSlice(input));
    require(root.is_object(), concatenateSourceStrings({text("depthdraw fixture root must be an object: "), path}));
    auto object = root.sourceObjectRef();
    auto generatedByPointer = object.if_contains("generatedBy");
    if (!generatedByPointer) return 0;
    auto generatedBy = fixtureString(*generatedByPointer, concatenateSourceStrings({path, text(".generatedBy")}));
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-alpha-mask-contract")
        return runDepthDrawAlphaMaskFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-alpha-depth-gap-fill-contract")
        return runDepthDrawAlphaDepthGapFillFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-depth-cleanup-contract")
        return runDepthDrawDepthCleanupFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-depth-prune-contract")
        return runDepthDrawDepthPruneFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-depth-split-contract")
        return runDepthDrawDepthSplitFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-depth-conversion-contract")
        return runDepthDrawDepthConversionFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-layer-bounds-sampling-contract")
        return runDepthDrawLayerBoundsSamplingFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-psd-mask-export-contract")
        return runDepthDrawPsdMaskExportFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-global-depth-scale-contract")
        return runDepthDrawGlobalDepthScaleFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-grid-convolution-contract")
        return runDepthDrawGridConvolutionFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && idOrEmpty(object).view() == "depthdraw-api-capabilities")
        return runDepthDrawApiCapabilitiesFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw" && object.if_contains("composite"))
        return runDepthDrawArtifactFixtureFile(path, object);
    if (generatedBy.view() == "depth-draw") return 0;
    if (generatedBy.view() == "local-contract" && idOrEmpty(object).view() == "local-merge-policies-smoke")
        return runDepthDrawLocalMergePoliciesFixtureFile(path, object);
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto document = fixtureRequired(object, text("document"), id);
    require(document.is_object(), concatenateSourceStrings({id, text(".document must be an object")}));
    auto documentWidthObject = document.sourceObjectRef();
    auto documentWidthOwner = concatenateSourceStrings({id, text(".document")});
    auto documentWidthValue = fixtureRequired(documentWidthObject, text("width"), documentWidthOwner);
    auto documentWidth = ngFloatToInt32(fixtureNumber(documentWidthValue,
        concatenateSourceStrings({id, text(".document.width")})));
    auto documentHeightObject = document.sourceObjectRef();
    auto documentHeightOwner = concatenateSourceStrings({id, text(".document")});
    auto documentHeightValue = fixtureRequired(documentHeightObject, text("height"), documentHeightOwner);
    auto documentHeight = ngFloatToInt32(fixtureNumber(documentHeightValue,
        concatenateSourceStrings({id, text(".document.height")})));
    auto gridObject = fixtureRequired(object, text("grid"), id);
    require(gridObject.is_object(), concatenateSourceStrings({id, text(".grid must be an object")}));
    auto axisXObject = gridObject.sourceObjectRef();
    auto axisXOwner = concatenateSourceStrings({id, text(".grid")});
    auto axisXValue = fixtureRequired(axisXObject, text("axisX"), axisXOwner);
    auto axisX = fixtureFloatArray(axisXValue, concatenateSourceStrings({id, text(".grid.axisX")}));
    auto axisYObject = gridObject.sourceObjectRef();
    auto axisYOwner = concatenateSourceStrings({id, text(".grid")});
    auto axisYValue = fixtureRequired(axisYObject, text("axisY"), axisYOwner);
    auto axisY = fixtureFloatArray(axisYValue, concatenateSourceStrings({id, text(".grid.axisY")}));
    auto baseObject = gridObject.sourceObjectRef();
    auto baseOwner = concatenateSourceStrings({id, text(".grid")});
    auto baseValue = fixtureRequired(baseObject, text("baseDepths"), baseOwner);
    auto baseDepths = fixtureFloatArray(baseValue, concatenateSourceStrings({id, text(".grid.baseDepths")}));
    resetCase();
    auto grid = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(nijigenerate::project::incActivePuppet())->root; });
    grid->setName(concatenateSourceStrings({text("depthdraw-fixture-"), id}).toNullableString());
    auto context = std::allocate_shared<Context>(SourceAllocator<Context>{});
    context->nodes(Context::Nodes{std::static_pointer_cast<Node>(grid)});
    struct X { nijigenerate::compat::Slice<float> values; operator nijigenerate::compat::Slice<float>() && { return values; } };
    struct Y { nijigenerate::compat::Slice<float> values; operator nijigenerate::compat::Slice<float>() && { return values; } };
    auto define = std::allocate_shared<DefineGridCommand>(SourceAllocator<DefineGridCommand>{}, X{axisX}, Y{axisY});
    const bool defined = ngRequireObject(define->run(context))->succeeded;
    require(defined, concatenateSourceStrings({id, text(" should define fixture grid")}));
    grid->localTransform.translation = Vec3{0, 0, 0};
    grid->localTransform.update();
    ngRequireObject(ngRequireObject(nijigenerate::project::incActivePuppet())->root)->build();
    grid->replaceDepths(baseDepths);
    auto session = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    auto layerValues = fixtureRequired(object, text("layers"), id);
    require(layerValues.is_array(), concatenateSourceStrings({id, text(".layers must be an array")}));
    auto layerEntries = layerValues.sourceArrayValue();
    for (std::size_t i = 0; i < layerEntries.size(); ++i) {
        auto value = layerEntries[i];
        const bool isObject = value.is_object();
        require(isObject, render("%s.layers[%s] must be an object", id, i));
        auto layerObject = value.sourceObjectRef();
        DepthDrawLayer layer;
        auto identifier = fixtureRequired(layerObject, text("id"), render("%s.layers[%s]", id, i));
        layer.id = fixtureString(identifier, render("%s.layers[%s].id", id, i));
        auto width = fixtureRequired(layerObject, text("width"), layer.id);
        layer.width = ngFloatToInt32(fixtureNumber(width, concatenateSourceStrings({layer.id, text(".width")})));
        auto height = fixtureRequired(layerObject, text("height"), layer.id);
        layer.height = ngFloatToInt32(fixtureNumber(height, concatenateSourceStrings({layer.id, text(".height")})));
        auto boundsValue = fixtureRequired(layerObject, text("bounds"), layer.id);
        auto bounds = fixtureFloatArray(boundsValue, concatenateSourceStrings({layer.id, text(".bounds")}));
        require(bounds.size() == 4, concatenateSourceStrings({layer.id, text(".bounds must have four values")}));
        layer.bounds.left = ngFloatToInt32(bounds[0]);
        layer.bounds.top = ngFloatToInt32(bounds[1]);
        layer.bounds.width = ngFloatToInt32(bounds[2]);
        layer.bounds.height = ngFloatToInt32(bounds[3]);
        auto pixels = fixtureRequired(layerObject, text("pixels"), layer.id);
        layer.depthPixels = fixtureByteArray(pixels, concatenateSourceStrings({layer.id, text(".pixels")}));
        if (auto* field = layerObject.if_contains("zScale")) layer.zScale = static_cast<float>(fixtureNumber(*field, concatenateSourceStrings({layer.id, text(".zScale")})));
        if (auto* field = layerObject.if_contains("zOffset")) layer.zOffset = static_cast<float>(fixtureNumber(*field, concatenateSourceStrings({layer.id, text(".zOffset")})));
        if (auto* field = layerObject.if_contains("backDepth")) layer.backDepth = static_cast<float>(fixtureNumber(*field, concatenateSourceStrings({layer.id, text(".backDepth")})));
        if (auto* field = layerObject.if_contains("frontDepth")) layer.frontDepth = static_cast<float>(fixtureNumber(*field, concatenateSourceStrings({layer.id, text(".frontDepth")})));
        session->layers.append(layer);
    }
    auto bindingValues = fixtureRequired(object, text("bindings"), id);
    require(bindingValues.is_array(), concatenateSourceStrings({id, text(".bindings must be an array")}));
    auto bindingEntries = bindingValues.sourceArrayValue();
    for (std::size_t i = 0; i < bindingEntries.size(); ++i) {
        auto value = bindingEntries[i];
        const bool isObject = value.is_object();
        require(isObject, render("%s.bindings[%s] must be an object", id, i));
        auto bindingObject = value.sourceObjectRef();
        DepthDrawBinding binding;
        auto identifier = fixtureRequired(bindingObject, text("layerId"), render("%s.bindings[%s]", id, i));
        binding.layerId = fixtureString(identifier, render("%s.bindings[%s].layerId", id, i));
        binding.targetGridUuid = grid->uuid;
        binding.targetNodeUuid = grid->uuid;
        if (auto* field = bindingObject.if_contains("order")) binding.order = ngFloatToInt32(fixtureNumber(*field, concatenateSourceStrings({binding.layerId, text(".order")})));
        if (auto* field = bindingObject.if_contains("mergePolicy")) binding.mergePolicy = fixtureMergePolicy(fixtureString(*field, concatenateSourceStrings({binding.layerId, text(".mergePolicy")})));
        session->bindings.append(binding);
    }
    auto view = std::allocate_shared<DepthTargetView>(SourceAllocator<DepthTargetView>{}, grid);
    auto vertices = view->getVertices();
    if (vertices.size() > 0) {
        auto point = coordinate::ngDepthDrawVertexDocumentPosition(view, vertices[0], documentWidth, documentHeight);
        const bool finite = std::isfinite(point.x) && std::isfinite(point.y);
        require(finite, render("%s first vertex document point must be finite: (%s,%s)", id, point.x, point.y));
    }
    auto result = ngComposeDepthDrawTarget(session, view, documentWidth, documentHeight);
    auto expectedValue = fixtureRequired(object, text("expectedDepths"), id);
    auto expectedDepths = fixtureFloatArray(expectedValue, concatenateSourceStrings({id, text(".expectedDepths")}));
    const bool sameLength = result.depths.size() == expectedDepths.size();
    require(sameLength, render("%s expected depth length mismatch actual=%s expected=%s", id, result.depths.size(), expectedDepths.size()));
    auto toleranceDefault = Json(0.00001);
    auto* tolerancePointer = object.if_contains("tolerance");
    auto toleranceValue = tolerancePointer ? *tolerancePointer : toleranceDefault;
    auto tolerance = fixtureNumber(toleranceValue, concatenateSourceStrings({id, text(".tolerance")}));
    for (std::size_t i = 0; i < expectedDepths.size(); ++i) {
        const float expected = expectedDepths[i];
        const bool close = std::abs(result.depths[i] - expected) <= tolerance;
        require(close, render("%s depth[%s] actual=%s expected=%s tolerance=%s", id, i, result.depths[i], expected, tolerance));
    }
    if (auto* winners = object.if_contains("expectedWinners")) {
        auto expected = fixtureStringArray(*winners, concatenateSourceStrings({id, text(".expectedWinners")}));
        const bool sameWinners = result.winningLayerIds.size() == expected.size();
        require(sameWinners, render("%s winner length mismatch actual=%s expected=%s", id, result.winningLayerIds.size(), expected.size()));
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto winner = expected[i];
            const bool same = result.winningLayerIds[i] == winner;
            require(same, render("%s winner[%s] actual=%s expected=%s", id, i, result.winningLayerIds[i], winner));
        }
    }
    return generatedBy.view() == "depth-draw" ? 1 : 0;
}
