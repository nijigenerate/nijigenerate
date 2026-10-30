// Existing regression.d10058-10162. Include once inside detail after fixture helpers.
std::size_t runDepthDrawLocalMergePoliciesFixtureFile(nijigenerate::compat::RetainedString path,
    nijigenerate::core::settings::SettingsJsonObject object) {
    using Text = nijigenerate::compat::RetainedString;
    using Bytes = nijigenerate::compat::Slice<std::uint8_t>;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::compat::ngFloatToInt32;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::vertex::define_mesh::DefineGridCommand;
    using nijigenerate::ext::nodes::ExGridDeformer;
    using nijigenerate::viewport::depth::common::targetview::DepthTargetView;
    using namespace nijigenerate::viewport::depth::draw;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto formatted = []<class... Args>(std::string_view pattern, Args&&... args) {
        const std::array<format::TypedFormatArgument, sizeof...(Args)> arguments{
            format::ngFormatArgument(std::forward<Args>(args))...};
        auto rendered = format::ngFormatTyped(pattern, arguments);
        return nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(rendered)); });
    };
    auto idValue = fixtureRequired(object, text("id"), path);
    auto id = fixtureString(idValue, concatenateSourceStrings({path, text(".id")}));
    auto document = fixtureRequired(object, text("document"), id);
    require(document.is_object(), concatenateSourceStrings({id, text(".document must be an object")}));
    auto widthObject = document.sourceObjectRef();
    auto widthOwner = concatenateSourceStrings({id, text(".document")});
    auto widthValue = fixtureRequired(widthObject, text("width"), widthOwner);
    auto documentWidth = ngFloatToInt32(fixtureNumber(widthValue,
        concatenateSourceStrings({id, text(".document.width")})));
    auto heightObject = document.sourceObjectRef();
    auto heightOwner = concatenateSourceStrings({id, text(".document")});
    auto heightValue = fixtureRequired(heightObject, text("height"), heightOwner);
    auto documentHeight = ngFloatToInt32(fixtureNumber(heightValue,
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
    auto baseDepthsValue = fixtureRequired(baseObject, text("baseDepths"), baseOwner);
    auto baseDepths = fixtureFloatArray(baseDepthsValue, concatenateSourceStrings({id, text(".grid.baseDepths")}));
    resetCase();
    auto grid = ExGridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    grid->setName(concatenateSourceStrings({text("depthdraw-fixture-"), id}).toNullableString());
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{grid});
    const bool defined = constructSourceCommand<DefineGridCommand>(axisX, axisY)->run(ctx)->succeeded;
    require(defined, concatenateSourceStrings({id, text(" should define fixture grid")}));
    grid->localTransform.translation = nicxlive::core::math::Vec3{0, 0, 0};
    grid->localTransform.update();
    nijigenerate::project::incActivePuppet()->root->build();
    grid->replaceDepths(baseDepths);
    const auto fullVertexPixels = [](std::uint8_t gray) {
        Bytes pixels;
        for (std::size_t i = 0; i < 16; ++i) pixels.appendRange(Bytes{0, 0, 0, 0});
        const auto setPixel = [&](int x, int y) {
            auto index = (static_cast<std::size_t>(y) * 4 + static_cast<std::size_t>(x)) * 4;
            pixels[index + 0] = gray;
            pixels[index + 1] = gray;
            pixels[index + 2] = gray;
            pixels[index + 3] = 255;
        };
        setPixel(1, 1);
        setPixel(3, 1);
        setPixel(1, 3);
        setPixel(3, 3);
        return pixels;
    };
    auto casesValue = fixtureRequired(object, text("cases"), id);
    require(casesValue.is_array(), concatenateSourceStrings({id, text(".cases must be an array")}));
    auto cases = casesValue.sourceArrayValue();
    for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex) {
        auto caseValue = cases[caseIndex];
        const bool isObject = caseValue.is_object();
        require(isObject, formatted("%s.cases[%s] must be an object", id, caseIndex));
        auto caseObject = caseValue.sourceObjectRef();
        auto policyValue = fixtureRequired(caseObject, text("policy"), formatted("%s.cases[%s]", id, caseIndex));
        auto policyName = fixtureString(policyValue, formatted("%s.cases[%s].policy", id, caseIndex));
        auto policy = fixtureMergePolicy(policyName);
        DepthDrawLayer lowLayer;
        lowLayer.id = text("low");
        lowLayer.layerPath = text("/low");
        lowLayer.displayName = text("Low");
        lowLayer.width = 4;
        lowLayer.height = 4;
        lowLayer.bounds.width = 4;
        lowLayer.bounds.height = 4;
        lowLayer.convolution = nijigenerate::io::depthimage::DepthImageConvolution::Nearest;
        lowLayer.depthPixels = fullVertexPixels(0);
        DepthDrawLayer highLayer = lowLayer;
        highLayer.id = text("high");
        highLayer.layerPath = text("/high");
        highLayer.displayName = text("High");
        highLayer.depthPixels = fullVertexPixels(255);
        DepthDrawBinding lowBinding;
        lowBinding.layerId = text("low");
        lowBinding.targetGridUuid = grid->getUUID();
        lowBinding.targetNodeUuid = grid->getUUID();
        lowBinding.order = 0;
        lowBinding.mergePolicy = policy;
        DepthDrawBinding highBinding = lowBinding;
        highBinding.layerId = text("high");
        highBinding.order = 1;
        auto session = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
        session->layers.append(lowLayer);
        session->layers.append(highLayer);
        session->bindings.append(lowBinding);
        session->bindings.append(highBinding);
        auto view = std::allocate_shared<DepthTargetView>(SourceAllocator<DepthTargetView>{}, grid);
        auto result = ngComposeDepthDrawTarget(session, view, documentWidth, documentHeight);
        auto expectedDepthsValue = fixtureRequired(caseObject, text("expectedDepths"),
            formatted("%s.cases[%s]", id, caseIndex));
        auto expectedDepths = fixtureFloatArray(expectedDepthsValue,
            concatenateSourceStrings({policyName, text(".expectedDepths")}));
        auto expectedWinnersValue = fixtureRequired(caseObject, text("expectedWinners"),
            formatted("%s.cases[%s]", id, caseIndex));
        auto expectedWinners = fixtureStringArray(expectedWinnersValue,
            concatenateSourceStrings({policyName, text(".expectedWinners")}));
        const bool depthLengthMatches = result.depths.size() == expectedDepths.size();
        require(depthLengthMatches, formatted("%s.%s expected depth length mismatch actual=%s expected=%s",
            id, policyName, result.depths.size(), expectedDepths.size()));
        const bool winnerLengthMatches = result.winningLayerIds.size() == expectedWinners.size();
        require(winnerLengthMatches, formatted("%s.%s expected winner length mismatch actual=%s expected=%s",
            id, policyName, result.winningLayerIds.size(), expectedWinners.size()));
        for (std::size_t i = 0; i < expectedDepths.size(); ++i) {
            auto expected = expectedDepths[i];
            const bool depthMatches = near(result.depths[i], expected);
            require(depthMatches, formatted("%s.%s depth[%s] actual=%s expected=%s",
                id, policyName, i, result.depths[i], expected));
            const bool winnerMatches = result.winningLayerIds[i] == expectedWinners[i];
            require(winnerMatches, formatted("%s.%s winner[%s] actual=%s expected=%s",
                id, policyName, i, result.winningLayerIds[i], expectedWinners[i]));
        }
    }
    return 0;
}
