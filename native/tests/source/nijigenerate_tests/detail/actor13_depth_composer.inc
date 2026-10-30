// Existing regression.d8556-9712; aggregation remains pending while the original whole body is lowered.
void testDepthDrawComposerContracts() {
    using Text = nijigenerate::compat::RetainedString;
    using Bytes = nijigenerate::compat::Slice<std::uint8_t>;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::compat::Vec2u;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::vertex::define_mesh::DefineGridCommand;
    using nijigenerate::ext::ExParameter;
    using namespace nijigenerate::ext::nodes;
    using namespace nijigenerate::commands::depth::bone;
    using namespace nijigenerate::commands::depth::bone_gpu_async;
    using namespace nijigenerate::viewport::depth::draw;
    using nijigenerate::viewport::depth::common::targetview::DepthTargetView;
    using namespace nijigenerate::core::actionstack;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    using nicxlive::core::nodes::MeshData;
    using nicxlive::core::param::DeformationParameterBinding;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::io::depthimage::DepthImageConvolution;
    using std::min;
    using std::max;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto readText = nijigenerate::compat::phobos::ngReadUtf8Text;
    const auto buildPath = []<class... Pieces>(Pieces... pieces) {
        const std::array<Text, sizeof...(Pieces)> values{pieces...};
        return nijigenerate::compat::phobos::ngBuildPath(values);
    };
    const auto defineGrid = [] {
        struct Axes { Slice<float> x, y; } axes;
        struct Axis {
            Axes& axes;
            bool second;
            operator Slice<float>() && {
                if (axes.x.isNull()) axes.x = Slice<float>{-1.0f, 1.0f};
                if (second && axes.y.isNull()) axes.y = Slice<float>{-1.0f, 1.0f};
                return second ? axes.y : axes.x;
            }
        };
        return std::allocate_shared<DefineGridCommand>(SourceAllocator<DefineGridCommand>{},
            Axis{axes, false}, Axis{axes, true});
    };
    const auto equalArrays = [](auto&& produceLeft, auto&& produceRight) {
        auto left = produceLeft();
        auto right = produceRight();
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) if (!(left[i] == right[i])) return false;
        return true;
    };
    const auto formatted = []<class... Values>(std::string_view pattern, const Values&... values) {
        const std::array<nijigenerate::core::translation::TypedFormatArgument, sizeof...(Values)> arguments{
            nijigenerate::core::translation::ngFormatArgument(values)...};
        auto output = nijigenerate::core::translation::ngFormatTyped(pattern, arguments);
        return nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(output)); });
    };

    resetCase();

    auto frontmostFixture = loadDepthDrawFixture(text("local-frontmost-smoke.json"));
    auto expectedFrontmostDepths = fixtureFloatArray(frontmostFixture.sourceIndexValue("expectedDepths"),
        text("local-frontmost-smoke.expectedDepths"));
    auto expectedFrontmostWinners = fixtureStringArray(frontmostFixture.sourceIndexValue("expectedWinners"),
        text("local-frontmost-smoke.expectedWinners"));

    auto grid = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(nijigenerate::project::incActivePuppet())->root; });
    grid->setName(text("depthdraw-compose-grid").toNullableString());
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{grid});
    require(ngRequireObject(defineGrid()->run(ctx))->succeeded,
        text("DepthDraw composer fixture should define a 2x2 grid"));
    grid->replaceDepths(Slice<float>{0.25f, 0.25f, 0.25f, 0.25f});

    auto nonDepthPart = newMeshPart(text("depthdraw-non-depth-target"));
    auto nonDepthTarget = std::allocate_shared<DepthTargetView>(SourceAllocator<DepthTargetView>{}, nonDepthPart);
    DepthDrawComposeResult nonDepthResult;
    nonDepthResult.targetGridUuid = nonDepthPart->uuid;
    nonDepthResult.depths.resize(nonDepthPart->getVertices().size(), std::numeric_limits<float>::quiet_NaN());
    auto nonDepthContext = std::allocate_shared<Context>(SourceAllocator<Context>{});
    auto nonDepthSummary = ngApplyDepthDrawTargetResultWithSummary(nonDepthContext, nonDepthTarget, nonDepthResult);
    require(!nonDepthSummary.succeeded && nonDepthSummary.changedTargets == 0,
        text("DepthDraw apply should reject deformables that do not implement DepthMappedNode"));

    Bytes layerA;
    Bytes layerB;
    for (std::size_t i = 0; i < 16; ++i) {
        layerA.appendRange(Bytes{0, 0, 0, 0});
        layerB.appendRange(Bytes{0, 0, 0, 0});
    }
    const auto setPixel = [](Bytes& pixels, int x, int y, std::uint8_t gray, std::uint8_t alpha) {
        auto index = (static_cast<std::size_t>(y) * 4 + static_cast<std::size_t>(x)) * 4;
        pixels[index + 0] = gray;
        pixels[index + 1] = gray;
        pixels[index + 2] = gray;
        pixels[index + 3] = alpha;
    };

    setPixel(layerA, 1, 1, 0, 255);
    setPixel(layerB, 1, 1, 255, 255);
    setPixel(layerB, 3, 1, 255, 255);

    DepthDrawLayer backLayer;
    backLayer.id = text("back");
    backLayer.layerPath = text("/back");
    backLayer.displayName = text("Back");
    backLayer.width = 4;
    backLayer.height = 4;
    backLayer.bounds.width = 4;
    backLayer.bounds.height = 4;
    backLayer.rgba = layerA.duplicate();
    backLayer.depthPixels = layerA;
    backLayer.normalCoverage = layerA.duplicate();

    DepthDrawLayer frontLayer;
    frontLayer.id = text("front");
    frontLayer.layerPath = text("/front");
    frontLayer.displayName = text("Front");
    frontLayer.width = 4;
    frontLayer.height = 4;
    frontLayer.bounds.width = 4;
    frontLayer.bounds.height = 4;
    frontLayer.rgba = layerB.duplicate();
    frontLayer.depthPixels = layerB;
    frontLayer.normalCoverage = layerB.duplicate();
    frontLayer.zScale = 0.5f;
    frontLayer.zOffset = 0.25f;

    DepthDrawBinding backBinding;
    backBinding.layerId = text("back");
    backBinding.targetGridUuid = grid->uuid;
    backBinding.targetNodeUuid = grid->uuid;
    backBinding.order = 0;
    backBinding.mergePolicy = DepthMergePolicy::Frontmost;

    DepthDrawBinding frontBinding = backBinding;
    frontBinding.layerId = text("front");
    frontBinding.order = 1;

    auto session = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    session->layers.append(backLayer);
    session->layers.append(frontLayer);
    session->bindings.append(backBinding);
    session->bindings.append(frontBinding);
    session->selectedLayerId = text("front");
    session->selectedGridUuid = grid->uuid;

    auto view = std::allocate_shared<DepthTargetView>(SourceAllocator<DepthTargetView>{}, grid);
    auto result = ngComposeDepthDrawTarget(session, view, 4, 4);
    require(result.depths.size() == grid->getVertices().size(), text("DepthDraw composer should produce one depth per target vertex"));
    require(result.layerStats.size() == 2, text("DepthDraw composer should report stats for each composed layer"));
    require(result.depths.size() == expectedFrontmostDepths.size() &&
        result.winningLayerIds.size() == expectedFrontmostWinners.size(),
        text("DepthDraw frontmost fixture should match the composed target vertex count"));

    {
        auto sourceRange1 = view->getVertices();
        const auto sourceCount1 = sourceRange1.size();
        for (std::size_t i = 0; i < sourceCount1; ++i) {
            auto vertex = sourceRange1[i];
        auto depth = result.depths[i];
        require(near(depth, expectedFrontmostDepths[i]),
            text("DepthDraw composer final target depths should match the fixture expectedDepths"));
        require(result.winningLayerIds[i] == expectedFrontmostWinners[i],
            text("DepthDraw composer winning layer ids should match the fixture expectedWinners"));
        if (near(vertex.y, -1.0f)) {
            require(near(depth, 0.75f), text("front layer should win top-row vertices after z transform"));
            require(result.winningLayerIds[i] == text("front"), text("front layer should be recorded as the winning layer"));
        } else {
            require(near(depth, 0.25f), text("missing layer samples should preserve base depth"));
            require(result.winningLayerIds[i].size() == 0, text("missing samples should not record a winning layer"));
        }
    }
    }

    require(result.layerStats[0].sampledVertices == 1 && result.layerStats[0].missingVertices == 3 &&
        result.layerStats[0].winningVertices == 0,
        text("back layer stats should reflect one sampled vertex superseded by frontmost composition"));
    require(result.layerStats[1].sampledVertices == 2 && result.layerStats[1].missingVertices == 2 &&
        result.layerStats[1].winningVertices == 2,
        text("front layer stats should reflect two winning sampled vertices"));
    require(result.sampledVertices == 3 && result.missingVertices == 5,
        text("DepthDraw composer aggregate stats should include all layer sample attempts"));
    require(result.hasDepthRange && near(result.minDepth, 0.25f) && near(result.maxDepth, 0.75f),
        text("DepthDraw composer should report final depth range"));
    auto opacitySession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    auto lowOpacityLayer = backLayer;
    lowOpacityLayer.opacity = 0.005f;
    auto lowOpacityBinding = backBinding;
    lowOpacityBinding.layerId = lowOpacityLayer.id;
    opacitySession->layers.append(lowOpacityLayer);
    opacitySession->bindings.append(lowOpacityBinding);
    auto opacityResult = ngComposeDepthDrawTarget(opacitySession, view, 4, 4);
    require(opacityResult.sampledVertices == 0 && opacityResult.missingVertices == grid->getVertices().size() &&
        equalArrays([&] { return opacityResult.depths; }, [&] { return Slice<float>{0.25f, 0.25f, 0.25f, 0.25f}; }),
        text("DepthDraw composer should apply normal depth layer opacity before accepting samples"));
    session->layers[1].sampleDepthScale = 0.75f;
    auto gpuPacket = ngBuildDepthDrawGpuComposePacket(session, view, 4, 4);
    require(gpuPacket.targetGridUuid == grid->uuid && gpuPacket.vertices.size() == grid->getVertices().size() &&
        gpuPacket.documentPositions.size() == grid->getVertices().size() && gpuPacket.baseDepths.size() == grid->getVertices().size(),
        text("DepthDraw GPU compose packet should carry the selected target geometry and base depth arrays"));
    require(gpuPacket.layers.size() == 2 && gpuPacket.bindings.size() == 2 &&
        gpuPacket.layers[0].layerId == text("back") && gpuPacket.layers[1].layerId == text("front") &&
        gpuPacket.bindings[0].layerIndex == 0 && gpuPacket.bindings[1].layerIndex == 1,
        text("DepthDraw GPU compose packet should preserve the CPU composer layer and binding order"));
    require(gpuPacket.layers[0].depthPixelOffset == 0 &&
        gpuPacket.layers[1].depthPixelOffset == static_cast<std::uint32_t>(backLayer.depthPixels.size()) &&
        gpuPacket.depthPixels.size() == backLayer.depthPixels.size() + frontLayer.depthPixels.size(),
        text("DepthDraw GPU compose packet should pack layer depth pixels with deterministic offsets"));
    require(near(gpuPacket.layers[0].boundsLeft, backLayer.bounds.left) &&
        near(gpuPacket.layers[0].boundsTop, backLayer.bounds.top) &&
        near(gpuPacket.layers[0].boundsWidth, backLayer.bounds.width) &&
        near(gpuPacket.layers[0].boundsHeight, backLayer.bounds.height),
        text("DepthDraw GPU compose packet should carry layer document bounds for GPU coordinate conversion"));
    require(gpuPacket.layers[0].normalCoverageOffset == 0 &&
        gpuPacket.layers[1].normalCoverageOffset == static_cast<std::uint32_t>(backLayer.normalCoverage.size()) &&
        gpuPacket.normalCoveragePixels.size() == backLayer.normalCoverage.size() + frontLayer.normalCoverage.size(),
        text("DepthDraw GPU compose packet should pack normal coverage pixels with deterministic offsets"));
    auto frontSamplePacket = ngBuildDepthDrawGpuLayerSamplePacket(gpuPacket, 1);
    require(frontSamplePacket.targetGridUuid == grid->uuid &&
        frontSamplePacket.layerIndex == 1 &&
        frontSamplePacket.layer.layerId == text("front") &&
        frontSamplePacket.layer.depthPixelOffset == 0 &&
        frontSamplePacket.layer.normalCoverageOffset == 0 &&
        frontSamplePacket.binding.layerIndex == 0 &&
        equalArrays([&] { return frontSamplePacket.depthPixels; }, [&] { return frontLayer.depthPixels; }) &&
        equalArrays([&] { return frontSamplePacket.normalCoveragePixels; }, [&] { return frontLayer.normalCoverage; }) &&
        equalArrays([&] { return frontSamplePacket.documentPositions; }, [&] { return gpuPacket.documentPositions; }),
        text("DepthDraw GPU layer sample packet should slice one layer into zero-offset shader inputs"));
    ngValidateDepthDrawGpuLayerSamplePacket(frontSamplePacket);
    auto flatSamplePositions = ngFlattenDepthDrawGpuDocumentPositions(frontSamplePacket.documentPositions);
    auto sampleUpload = ngBuildDepthDrawGpuLayerSampleUpload(frontSamplePacket);
    require(flatSamplePositions.size() == frontSamplePacket.documentPositions.size() * DepthDrawGpuDocumentPositionStride &&
        near(flatSamplePositions[0], frontSamplePacket.documentPositions[0].x) &&
        near(flatSamplePositions[1], frontSamplePacket.documentPositions[0].y) &&
        near(flatSamplePositions[2], frontSamplePacket.documentPositions[1].x) &&
        near(flatSamplePositions[3], frontSamplePacket.documentPositions[1].y),
        text("DepthDraw GPU document-position upload buffer should expose xy pairs in vertex order"));
    require(equalArrays([&] { return sampleUpload.documentPositions; }, [&] { return flatSamplePositions; }) &&
        equalArrays([&] { return sampleUpload.depthPixels; }, [&] { return frontSamplePacket.depthPixels; }) &&
        sampleUpload.depthPixels.data() == frontSamplePacket.depthPixels.data() &&
        sampleUpload.normalCoveragePixels.size() == frontLayer.normalCoverage.size() &&
        sampleUpload.normalCoveragePixels.data() == frontSamplePacket.normalCoveragePixels.data() &&
        sampleUpload.layer.size() == DepthDrawGpuLayerStride &&
        sampleUpload.binding.size() == DepthDrawGpuBindingStride &&
        near(sampleUpload.layer[static_cast<std::size_t>(DepthDrawGpuLayerField::DepthPixelOffset)], 0.0f) &&
        near(sampleUpload.layer[static_cast<std::size_t>(DepthDrawGpuLayerField::NormalCoverageOffset)], 0.0f) &&
        near(sampleUpload.binding[static_cast<std::size_t>(DepthDrawGpuBindingField::LayerIndex)], 0.0f),
        text("DepthDraw GPU layer sample upload should retain compact byte pixels without staging copies"));
    auto gpuLayerSampleSource = readText(buildPath(
        text("source"), text("nijigenerate"), text("viewport"), text("depth"), text("draw"), text("gpu.d")));
    require((gpuLayerSampleSource.view().find("uniform usamplerBuffer depthPixels;") != std::string_view::npos) &&
        (gpuLayerSampleSource.view().find("glTexBuffer(GL_TEXTURE_BUFFER, GL_R8UI, buffer);") != std::string_view::npos) &&
        (gpuLayerSampleSource.view().find("ngFlattenDepthDrawGpuRgbaBytes") == std::string_view::npos),
        text("DepthDraw GPU layer sampling must upload compact RGBA bytes instead of float-expanded staging arrays"));
    require((gpuLayerSampleSource.view().find("sampledDepth = rawDepth;") != std::string_view::npos) &&
        (gpuLayerSampleSource.view().find("outDepth = applyZTransform(sampleDepth);") != std::string_view::npos) &&
        (gpuLayerSampleSource.view().find("outDepth = applyZTransform(medianValues[medianCount / 2]);") != std::string_view::npos),
        text("DepthDraw GPU sampling must apply layer Z transforms after convolution like the CPU composer"));
    require(ngDepthDrawGpuLayerSampleSupportsConvolution(static_cast<std::int32_t>(DepthImageConvolution::Nearest)) &&
        ngDepthDrawGpuLayerSampleSupportsConvolution(static_cast<std::int32_t>(DepthImageConvolution::Box3x3)) &&
        ngDepthDrawGpuLayerSampleSupportsConvolution(static_cast<std::int32_t>(DepthImageConvolution::Gaussian5x5)) &&
        ngDepthDrawGpuLayerSampleSupportsConvolution(static_cast<std::int32_t>(DepthImageConvolution::FrontmostCustom)) &&
        ngDepthDrawGpuLayerSampleSupportsConvolution(static_cast<std::int32_t>(DepthImageConvolution::BackmostCustom)) &&
        ngDepthDrawGpuLayerSampleSupportsConvolution(static_cast<std::int32_t>(DepthImageConvolution::Median3x3)) &&
        !ngDepthDrawGpuLayerSampleSupportsConvolution(static_cast<std::int32_t>(DepthImageConvolution::MedianCustom)) &&
        !ngDepthDrawGpuLayerSampleSupportsConvolution(999),
        text("DepthDraw GPU layer sample backend should advertise supported weighted/extreme/fixed median convolution modes and reject custom median/unknown modes"));
    if (!ngDepthDrawGpuLayerSampleSupported()) {
        std::uint32_t layerSampleJobId{};
        Text layerSampleError;
        DepthDrawGpuLayerSamplePollResult layerSamplePoll;
        require(ngDepthDrawGpuLayerSampleMissingRequirements().size() > 0 &&
            ngDepthDrawGpuLayerSampleSupportDiagnostic().size() > 0,
            text("DepthDraw GPU layer sample backend should expose missing OpenGL requirements"));
        require(!ngSubmitDepthDrawGpuLayerSample(frontSamplePacket, layerSampleJobId, layerSampleError) &&
            layerSampleError.size() > 0 &&
            ngPendingDepthDrawGpuLayerSampleJobCount() == 0,
            text("DepthDraw GPU layer sample submit should fail explicitly without CPU fallback when backend is unavailable"));
        require(!ngPollDepthDrawGpuLayerSample(1, layerSamplePoll, layerSampleError) &&
            layerSampleError.size() > 0,
            text("DepthDraw GPU layer sample poll should fail explicitly without fabricating CPU readback"));
    }
    require(near(gpuPacket.layers[1].zScale, frontLayer.zScale) &&
        near(gpuPacket.layers[1].zOffset, frontLayer.zOffset) &&
        near(gpuPacket.layers[1].sampleDepthScale, session->layers[1].sampleDepthScale) &&
        gpuPacket.bindings[1].mergePolicy == static_cast<std::uint32_t>(DepthMergePolicy::Frontmost) &&
        gpuPacket.bindings[1].flags == 1,
        text("DepthDraw GPU compose packet should carry Z transform, merge policy, and coverage-use state"));
    require(near(gpuPacket.documentPositions[0].x, 1.0f) && near(gpuPacket.documentPositions[0].y, 1.0f) &&
        near(gpuPacket.documentPositions[1].x, 3.0f) && near(gpuPacket.documentPositions[1].y, 1.0f),
        text("DepthDraw GPU compose packet should precompute the same document positions used by the CPU sampler"));
    ngValidateDepthDrawGpuComposePacket(gpuPacket);
    auto flatGpuLayers = ngFlattenDepthDrawGpuLayers(gpuPacket.layers);
    auto flatGpuBindings = ngFlattenDepthDrawGpuBindings(gpuPacket.bindings);
    require(flatGpuLayers.size() == gpuPacket.layers.size() * DepthDrawGpuLayerStride &&
        flatGpuBindings.size() == gpuPacket.bindings.size() * DepthDrawGpuBindingStride,
        text("DepthDraw GPU flatten helpers should produce fixed-stride upload buffers"));
    require(near(flatGpuLayers[static_cast<std::size_t>(DepthDrawGpuLayerField::Width)], backLayer.width) &&
        near(flatGpuLayers[static_cast<std::size_t>(DepthDrawGpuLayerField::Height)], backLayer.height) &&
        near(flatGpuLayers[static_cast<std::size_t>(DepthDrawGpuLayerField::DepthPixelOffset)], 0.0f) &&
        near(flatGpuLayers[static_cast<std::size_t>(DepthDrawGpuLayerField::BoundsWidth)], backLayer.bounds.width) &&
        near(flatGpuLayers[static_cast<std::size_t>(DepthDrawGpuLayerField::Opacity)], backLayer.opacity),
        text("DepthDraw GPU flattened layer buffer should expose shader-readable layer fields at stable offsets"));
    auto flatFrontBase = DepthDrawGpuLayerStride;
    require(near(flatGpuLayers[flatFrontBase + static_cast<std::size_t>(DepthDrawGpuLayerField::DepthPixelOffset)], backLayer.depthPixels.size()) &&
        near(flatGpuLayers[flatFrontBase + static_cast<std::size_t>(DepthDrawGpuLayerField::ZScale)], frontLayer.zScale) &&
        near(flatGpuLayers[flatFrontBase + static_cast<std::size_t>(DepthDrawGpuLayerField::ZOffset)], frontLayer.zOffset) &&
        near(flatGpuLayers[flatFrontBase + static_cast<std::size_t>(DepthDrawGpuLayerField::SampleDepthScale)],
            session->layers[1].sampleDepthScale),
        text("DepthDraw GPU flattened layer buffer should preserve per-layer offsets and depth transforms"));
    require(near(flatGpuBindings[static_cast<std::size_t>(DepthDrawGpuBindingField::LayerIndex)], 0.0f) &&
        near(flatGpuBindings[static_cast<std::size_t>(DepthDrawGpuBindingField::MergePolicy)], static_cast<float>(DepthMergePolicy::Frontmost)) &&
        near(flatGpuBindings[static_cast<std::size_t>(DepthDrawGpuBindingField::Flags)], 1.0f) &&
        near(flatGpuBindings[static_cast<std::size_t>(DepthDrawGpuBindingField::CoverageThreshold)], backBinding.coverageThreshold),
        text("DepthDraw GPU flattened binding buffer should expose shader-readable binding fields at stable offsets"));
    DepthDrawGpuComposeReadback gpuReadback;
    gpuReadback.targetGridUuid = grid->uuid;
    gpuReadback.depths = result.depths.duplicate();
    gpuReadback.winningLayerIndices = {1, 1, -1, -1};
    DepthDrawGpuLayerReadback backReadback;
    backReadback.layerIndex = 0;
    backReadback.validSamples = {1, 0, 0, 0};
    backReadback.sampleDepths = {-1.0f, 0.0f, 0.0f, 0.0f};
    DepthDrawGpuLayerReadback frontReadback;
    frontReadback.layerIndex = 1;
    frontReadback.validSamples = {1, 1, 0, 0};
    frontReadback.sampleDepths = {0.75f, 0.75f, 0.0f, 0.0f};
    gpuReadback.layers = {backReadback, frontReadback};
    ngValidateDepthDrawGpuComposeReadback(gpuPacket, gpuReadback);
    auto builtGpuReadback = ngBuildDepthDrawGpuComposeReadback(gpuPacket, Slice<DepthDrawGpuLayerReadback>{backReadback, frontReadback});
    require(builtGpuReadback.targetGridUuid == gpuPacket.targetGridUuid &&
        equalArrays([&] { return builtGpuReadback.depths; }, [&] { return gpuReadback.depths; }) &&
        equalArrays([&] { return builtGpuReadback.winningLayerIndices; }, [&] { return gpuReadback.winningLayerIndices; }) &&
        builtGpuReadback.layers.size() == gpuReadback.layers.size(),
        text("DepthDraw GPU compose readback builder should merge layer readbacks into final depth and winner arrays"));
    auto builtGpuResult = ngDepthDrawComposeResultFromGpuReadback(gpuPacket, builtGpuReadback);
    auto builtGpuComparison = ngCompareDepthDrawComposeReadback(result, builtGpuResult);
    require(builtGpuComparison.ok,
        text("DepthDraw GPU compose readback builder should produce a CPU-comparable composed result"));
    ngClearDepthDrawGpuTestHooks();
    std::uint32_t depthDrawGpuJobId{};
    Text depthDrawGpuError;
    DepthDrawGpuDispatchPollResult depthDrawGpuPoll;
    if (!ngDepthDrawGpuSupported()) {
        require(ngDepthDrawGpuMissingRequirements().size() > 0 &&
            ngDepthDrawGpuSupportDiagnostic().size() > 0,
            text("DepthDraw GPU dispatch should expose a diagnostic for missing OpenGL backend requirements"));
        require(!ngSubmitDepthDrawGpuCompose(gpuPacket, depthDrawGpuJobId, depthDrawGpuError) &&
            depthDrawGpuError.size() > 0,
            text("DepthDraw GPU dispatch should fail explicitly instead of falling back to CPU when no backend is connected"));
        require(!ngPollDepthDrawGpuCompose(1, depthDrawGpuPoll, depthDrawGpuError) &&
            depthDrawGpuError.size() > 0,
            text("DepthDraw GPU poll should fail explicitly instead of fabricating CPU readback when no backend is connected"));
    } else {
        auto medianGpuPacket = gpuPacket;
        medianGpuPacket.layers = gpuPacket.layers.duplicate();
        medianGpuPacket.layers[0].convolution = static_cast<std::int32_t>(DepthImageConvolution::MedianCustom);
        require(!ngSubmitDepthDrawGpuCompose(medianGpuPacket, depthDrawGpuJobId, depthDrawGpuError) &&
            depthDrawGpuError.size() > 0 && (depthDrawGpuError.view().find("median") != std::string_view::npos),
            text("DepthDraw GPU dispatch should fail explicitly for unsupported custom median convolution instead of falling back to CPU"));
    }
    fakeDepthDrawGpuNextJobId = 1;
    fakeDepthDrawGpuSubmitCount = 0;
    fakeDepthDrawGpuPollCount = 0;
    fakeDepthDrawGpuNotReadyPolls = 0;
    fakeDepthDrawGpuReadbacks = nullptr;
    fakeDepthDrawGpuReadbacks.insertOrAssign(1, gpuReadback);
    ngSetDepthDrawGpuTestHooks(&fakeDepthDrawGpuSupported, &fakeDepthDrawGpuSubmit, &fakeDepthDrawGpuPoll);
    nijigenerate::compat::ngRunWithScopeExit([&] {
    require(ngDepthDrawGpuSupported(), text("DepthDraw GPU dispatch should use the configured backend support hook"));
    auto cleanGpuPacket = gpuPacket;
    cleanGpuPacket.layers = gpuPacket.layers.duplicate();
    cleanGpuPacket.bindings = gpuPacket.bindings.duplicate();
    auto invalidGpuPacket = gpuPacket;
    invalidGpuPacket.baseDepths = invalidGpuPacket.baseDepths.slice(0, invalidGpuPacket.baseDepths.size() - 1).duplicate();
    require(!ngSubmitDepthDrawGpuCompose(invalidGpuPacket, depthDrawGpuJobId, depthDrawGpuError) &&
        depthDrawGpuError.size() > 0 && (depthDrawGpuError.view().find("base-depth") != std::string_view::npos),
        text("DepthDraw GPU dispatch should reject invalid packets before backend hooks can submit them"));
    invalidGpuPacket = cleanGpuPacket;
    invalidGpuPacket.layers = cleanGpuPacket.layers.duplicate();
    invalidGpuPacket.layers[0].channel = 999;
    require(!ngSubmitDepthDrawGpuCompose(invalidGpuPacket, depthDrawGpuJobId, depthDrawGpuError) &&
        depthDrawGpuError.size() > 0 && (depthDrawGpuError.view().find("channel") != std::string_view::npos),
        text("DepthDraw GPU dispatch should reject unknown sampling channels before backend hooks can submit them"));
    invalidGpuPacket = cleanGpuPacket;
    invalidGpuPacket.layers = cleanGpuPacket.layers.duplicate();
    invalidGpuPacket.layers[0].convolution = 999;
    require(!ngSubmitDepthDrawGpuCompose(invalidGpuPacket, depthDrawGpuJobId, depthDrawGpuError) &&
        depthDrawGpuError.size() > 0 && (depthDrawGpuError.view().find("convolution") != std::string_view::npos),
        text("DepthDraw GPU dispatch should reject unknown convolution modes before backend hooks can submit them"));
    invalidGpuPacket = cleanGpuPacket;
    invalidGpuPacket.layers = cleanGpuPacket.layers.duplicate();
    invalidGpuPacket.layers[0].customRadius = 65;
    require(!ngSubmitDepthDrawGpuCompose(invalidGpuPacket, depthDrawGpuJobId, depthDrawGpuError) &&
        depthDrawGpuError.size() > 0 && (depthDrawGpuError.view().find("custom radius") != std::string_view::npos),
        text("DepthDraw GPU dispatch should reject unsupported custom radii before backend hooks can submit them"));
    invalidGpuPacket = cleanGpuPacket;
    invalidGpuPacket.layers = cleanGpuPacket.layers.duplicate();
    invalidGpuPacket.layers[0].convolution = static_cast<std::int32_t>(DepthImageConvolution::MedianCustom);
    require(!ngSubmitDepthDrawGpuCompose(invalidGpuPacket, depthDrawGpuJobId, depthDrawGpuError) &&
        depthDrawGpuError.size() > 0 && (depthDrawGpuError.view().find("median") != std::string_view::npos) &&
        fakeDepthDrawGpuSubmitCount == 0,
        text("DepthDraw GPU dispatch should reject custom median convolution before backend hooks can submit it"));
    require(ngSubmitDepthDrawGpuCompose(gpuPacket, depthDrawGpuJobId, depthDrawGpuError) &&
        depthDrawGpuJobId == 1 && fakeDepthDrawGpuSubmitCount == 1,
        text("DepthDraw GPU dispatch should submit through the configured backend hook"));
    require(ngPollDepthDrawGpuCompose(depthDrawGpuJobId, depthDrawGpuPoll, depthDrawGpuError) &&
        depthDrawGpuPoll.ready && fakeDepthDrawGpuPollCount == 1,
        text("DepthDraw GPU dispatch should poll readback through the configured backend hook"));
    auto validDepthDrawGpuPoll = depthDrawGpuPoll;
    auto invalidGpuReadback = gpuReadback;
    invalidGpuReadback.depths = invalidGpuReadback.depths.slice(0, invalidGpuReadback.depths.size() - 1).duplicate();
    fakeDepthDrawGpuReadbacks.insertOrAssign(2, invalidGpuReadback);
    require(ngSubmitDepthDrawGpuCompose(gpuPacket, depthDrawGpuJobId, depthDrawGpuError) &&
        depthDrawGpuJobId == 2,
        text("DepthDraw GPU dispatch should submit another packet for invalid-readback validation"));
    require(!ngPollDepthDrawGpuCompose(depthDrawGpuJobId, depthDrawGpuPoll, depthDrawGpuError) &&
        depthDrawGpuError.size() > 0 && (depthDrawGpuError.view().find("readback depth") != std::string_view::npos),
        text("DepthDraw GPU dispatch should reject invalid backend readback before reconstruction"));
    auto reconstructedGpuResult = ngDepthDrawComposeResultFromGpuReadback(gpuPacket, gpuReadback);
    auto gpuReadbackComparison = ngCompareDepthDrawComposeReadback(result, reconstructedGpuResult);
    require(gpuReadbackComparison.ok,
        text("DepthDraw GPU readback reconstruction should produce a CPU-comparable compose result"));
    auto hookedGpuResult = ngDepthDrawComposeResultFromGpuReadback(gpuPacket, validDepthDrawGpuPoll.readback);
    gpuReadbackComparison = ngCompareDepthDrawComposeReadback(result, hookedGpuResult);
    require(gpuReadbackComparison.ok,
        text("DepthDraw GPU dispatch hook readback should remain CPU-comparable after reconstruction"));
    fakeDepthDrawGpuReadbacks.insertOrAssign(3, gpuReadback);
    DepthDrawGpuTargetComposeJob targetGpuJob;
    DepthDrawGpuTargetComposePollResult targetGpuPoll;
    require(ngSubmitDepthDrawGpuTargetCompose(session, view, 4, 4, targetGpuJob, depthDrawGpuError) &&
        targetGpuJob.jobId == 3 && fakeDepthDrawGpuSubmitCount == 3,
        text("DepthDraw GPU target compose wrapper should submit a packet built from the session and target view"));
    require(ngPollDepthDrawGpuTargetCompose(targetGpuJob, targetGpuPoll, depthDrawGpuError) &&
        targetGpuPoll.ready && fakeDepthDrawGpuPollCount == 3,
        text("DepthDraw GPU target compose wrapper should poll and reconstruct a compose result"));
    gpuReadbackComparison = ngCompareDepthDrawComposeReadback(result, targetGpuPoll.result);
    require(gpuReadbackComparison.ok,
        text("DepthDraw GPU target compose wrapper should return a CPU-comparable compose result"));
    session->layers[1].sampleDepthScale = 1.0f;
    auto sameOrderSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    sameOrderSession->layers = session->layers.duplicate();
    auto sameOrderBack = backBinding;
    sameOrderBack.mergePolicy = DepthMergePolicy::Replace;
    sameOrderBack.order = 0;
    auto sameOrderFront = frontBinding;
    sameOrderFront.mergePolicy = DepthMergePolicy::Replace;
    sameOrderFront.order = 0;
    sameOrderSession->bindings = {sameOrderBack, sameOrderFront};
    auto sameOrderResult = ngComposeDepthDrawTarget(sameOrderSession, view, 4, 4);
    require(sameOrderResult.winningLayerIds[0] == text("front") && near(sameOrderResult.depths[0], 0.75f),
        text("DepthDraw composer should preserve session binding order when binding order values are equal"));

    Bytes lowerPixels;
    Bytes transparentUpperPixels;
    for (std::size_t i = 0; i < 16; ++i) {
        lowerPixels.appendRange(Bytes{0, 0, 0, 0});
        transparentUpperPixels.appendRange(Bytes{0, 0, 0, 0});
    }
    setPixel(lowerPixels, 1, 3, 255, 255);
    setPixel(lowerPixels, 3, 3, 255, 255);
    setPixel(transparentUpperPixels, 1, 1, 0, 255);
    setPixel(transparentUpperPixels, 3, 1, 0, 255);
    setPixel(transparentUpperPixels, 1, 3, 255, 0);
    setPixel(transparentUpperPixels, 3, 3, 255, 0);

    auto lowerLayer = backLayer;
    lowerLayer.id = text("lower");
    lowerLayer.layerPath = text("/lower");
    lowerLayer.displayName = text("Lower");
    lowerLayer.rgba = lowerPixels.duplicate();
    lowerLayer.depthPixels = lowerPixels;
    lowerLayer.normalCoverage = lowerPixels.duplicate();

    auto upperLayer = frontLayer;
    upperLayer.id = text("upper");
    upperLayer.layerPath = text("/upper");
    upperLayer.displayName = text("Upper");
    upperLayer.rgba = transparentUpperPixels.duplicate();
    upperLayer.depthPixels = transparentUpperPixels;
    upperLayer.normalCoverage = transparentUpperPixels.duplicate();
    upperLayer.zScale = 1.0f;
    upperLayer.zOffset = 0.0f;

    DepthDrawBinding lowerBinding = backBinding;
    lowerBinding.layerId = text("lower");
    lowerBinding.order = 0;
    DepthDrawBinding upperBinding = backBinding;
    upperBinding.layerId = text("upper");
    upperBinding.order = 1;

    auto transparentSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    transparentSession->layers = {lowerLayer, upperLayer};
    transparentSession->bindings = {lowerBinding, upperBinding};
    auto transparentResult = ngComposeDepthDrawTarget(transparentSession, view, 4, 4);
    require(transparentResult.depths.size() == 4 &&
        near(transparentResult.depths[0], -1.0f) &&
        near(transparentResult.depths[1], -1.0f) &&
        near(transparentResult.depths[2], 1.0f) &&
        near(transparentResult.depths[3], 1.0f),
        text("DepthDraw composer should allow lower layers to contribute where the upper layer is transparent"));
    require(equalArrays([&] { return transparentResult.winningLayerIds; }, [&] { return Slice<Text>{text("upper"), text("upper"), text("lower"), text("lower")}; }),
        text("DepthDraw composer should record lower winners under transparent upper-layer pixels"));

    auto disabledLayer = upperLayer;
    disabledLayer.id = text("disabled-layer");
    DepthDrawBinding disabledLayerBinding = upperBinding;
    disabledLayerBinding.layerId = text("disabled-layer");
    disabledLayerBinding.order = 2;
    auto disabledBindingLayer = upperLayer;
    disabledBindingLayer.id = text("disabled-binding-layer");
    DepthDrawBinding disabledBinding = upperBinding;
    disabledBinding.layerId = text("disabled-binding-layer");
    disabledBinding.order = 3;
    disabledBinding.enabled = false;
    auto invisibleLayer = upperLayer;
    invisibleLayer.id = text("invisible-layer");
    invisibleLayer.visible = false;
    DepthDrawBinding invisibleBinding = upperBinding;
    invisibleBinding.layerId = text("invisible-layer");
    invisibleBinding.order = 4;
    auto disabledSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    disabledSession->layers = {lowerLayer, disabledLayer, disabledBindingLayer, invisibleLayer};
    disabledSession->layers[1].enabled = false;
    disabledSession->bindings = {lowerBinding, disabledLayerBinding, disabledBinding, invisibleBinding};
    auto disabledResult = ngComposeDepthDrawTarget(disabledSession, view, 4, 4);
    require(disabledResult.layerStats.size() == 1 && disabledResult.layerStats[0].layerId == text("lower"),
        text("DepthDraw composer should skip disabled layers, invisible layers, and disabled bindings"));
    require(equalArrays([&] { return disabledResult.winningLayerIds; }, [&] { return Slice<Text>{text(""), text(""), text("lower"), text("lower")}; }) &&
        near(disabledResult.depths[2], 1.0f) && near(disabledResult.depths[3], 1.0f),
        text("DepthDraw disabled composer inputs should not contribute sampled depths"));

    auto additiveBottomLayer = lowerLayer;
    additiveBottomLayer.id = text("additive-bottom");
    auto additiveTopLayer = lowerLayer;
    additiveTopLayer.id = text("additive-top");
    DepthDrawBinding additiveBottomBinding = lowerBinding;
    additiveBottomBinding.layerId = text("additive-bottom");
    additiveBottomBinding.mergePolicy = DepthMergePolicy::Add;
    additiveBottomBinding.order = 0;
    DepthDrawBinding additiveTopBinding = additiveBottomBinding;
    additiveTopBinding.layerId = text("additive-top");
    additiveTopBinding.order = 1;
    auto additiveSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    additiveSession->layers = {additiveBottomLayer, additiveTopLayer};
    additiveSession->bindings = {additiveBottomBinding, additiveTopBinding};
    auto additiveResult = ngComposeDepthDrawTarget(additiveSession, view, 4, 4);
    require(additiveResult.layerStats.size() == 2 &&
        additiveResult.layerStats[0].contributedVertices == 2 &&
        additiveResult.layerStats[1].contributedVertices == 2 &&
        additiveResult.layerStats[0].winningVertices == 0 &&
        additiveResult.layerStats[1].winningVertices == 2,
        text("DepthDraw composer should count additive contributions even when a later additive layer owns the final winner id"));

    auto drawViewport = std::allocate_shared<DepthDrawViewport>(SourceAllocator<DepthDrawViewport>{}, session);
    drawViewport->setDocumentSize(4, 4);
    drawViewport->selectionChanged(Slice<std::shared_ptr<Node>>{grid});
    require(drawViewport->depthViewSession()->targets.size() == 1 &&
        drawViewport->depthViewSession()->selectedTarget()->getTarget() == grid &&
        drawViewport->depthDrawSession() == session &&
        drawViewport->targetRenderer() != nullptr,
        text("DepthDrawViewport should be a sibling viewport owning DepthViewSession and DepthTargetRenderer"));
    session->markAllPreviewDirty();
    require(drawViewport->composeDirtyPreviews() == 1 && !session->isTargetPreviewDirty(grid->uuid),
        text("DepthDrawViewport should compose dirty previews through DepthDrawSession"));
    auto viewportPreview = drawViewport->previewResult(grid->uuid);
    require(viewportPreview.depths.size() == result.depths.size() &&
        viewportPreview.winningLayerIds.size() == result.winningLayerIds.size(),
        text("DepthDrawViewport should retain composed preview results per target"));
    {
        auto sourceRange2 = result.depths;
        const auto sourceCount2 = sourceRange2.size();
        for (std::size_t i = 0; i < sourceCount2; ++i) {
            auto expectedDepth = sourceRange2[i];
        require(near(viewportPreview.depths[i], expectedDepth) &&
            viewportPreview.winningLayerIds[i] == result.winningLayerIds[i],
            text("DepthDrawViewport preview result should match direct DepthDraw composition"));
    }
    }
    require(equalArrays([&] { return drawViewport->depthViewSession()->selectedTarget()->copyWorkingDepths(); }, [&] { return viewportPreview.depths; }),
        text("DepthDrawViewport preview composition should update common target working depths only"));
    auto gpuDisplay = session->display;
    gpuDisplay.useGpuPreview = true;
    require(session->updateDisplayOptions(gpuDisplay), text("DepthDrawSession should dirty previews when GPU preview is enabled"));
    fakeDepthDrawGpuReadbacks.insertOrAssign(4, gpuReadback);
    fakeDepthDrawGpuNotReadyPolls = 1;
    require(drawViewport->composeDirtyPreviews() == 1 &&
        drawViewport->pendingGpuPreviewJobCount() == 1 &&
        session->isTargetPreviewDirty(grid->uuid),
        text("DepthDrawViewport GPU preview should submit dirty previews without clearing them before readback"));
    require(drawViewport->composeDirtyPreviews() == 0 &&
        drawViewport->pendingGpuPreviewJobCount() == 1 &&
        session->isTargetPreviewDirty(grid->uuid) &&
        drawViewport->gpuPreviewError().size() == 0,
        text("DepthDrawViewport GPU preview should keep pending and dirty state while GPU readback is not ready"));
    require(!drawViewport->composeSelectedPreviewForUpdate(),
        text("DepthDrawViewport selected-preview update helper should not compose CPU preview while GPU preview is enabled"));
    require(drawViewport->pendingGpuPreviewJobCount() == 1 &&
        session->isTargetPreviewDirty(grid->uuid),
        text("DepthDrawViewport update should not run CPU preview composition while GPU preview is pending"));
    require(drawViewport->composeDirtyPreviews() == 1 &&
        drawViewport->pendingGpuPreviewJobCount() == 0 &&
        !session->isTargetPreviewDirty(grid->uuid) &&
        drawViewport->gpuPreviewError().size() == 0,
        text("DepthDrawViewport GPU preview should poll ready readback and clear dirty state without CPU fallback"));
    auto gpuViewportPreview = drawViewport->previewResult(grid->uuid);
    auto gpuViewportComparison = ngCompareDepthDrawComposeReadback(result, gpuViewportPreview);
    require(gpuViewportComparison.ok &&
        equalArrays([&] { return drawViewport->depthViewSession()->selectedTarget()->copyWorkingDepths(); }, [&] { return gpuViewportPreview.depths; }),
        text("DepthDrawViewport GPU preview should reconstruct a CPU-comparable preview and update working depths"));
    auto staleReadback = gpuReadback;
    for (float& depth : staleReadback.depths) depth = 99.0f;
    auto staleJobId = fakeDepthDrawGpuNextJobId;
    fakeDepthDrawGpuReadbacks.insertOrAssign(staleJobId, staleReadback);
    fakeDepthDrawGpuReadbacks.insertOrAssign(staleJobId + 1, gpuReadback);
    session->markTargetPreviewDirty(grid->uuid);
    require(drawViewport->composeDirtyPreviews() == 1 && drawViewport->pendingGpuPreviewJobCount() == 1,
        text("DepthDrawViewport should submit a GPU preview for the current session revision"));
    require(session->updateLayerXYTransform(text("front"), Vec2(0.5f, 0.0f), Vec2(1.0f, 1.0f)),
        text("DepthDrawViewport stale preview fixture should change a composition input while readback is pending"));
    require(drawViewport->composeDirtyPreviews() == 1 &&
        drawViewport->pendingGpuPreviewJobCount() == 1 &&
        session->isTargetPreviewDirty(grid->uuid) &&
        equalArrays([&] { return drawViewport->previewResult(grid->uuid).depths; }, [&] { return gpuViewportPreview.depths; }),
        text("DepthDrawViewport must discard a stale GPU completion and immediately submit the latest revision"));
    require(drawViewport->composeDirtyPreviews() == 1 &&
        drawViewport->pendingGpuPreviewJobCount() == 0 &&
        !session->isTargetPreviewDirty(grid->uuid),
        text("DepthDrawViewport should accept the replacement GPU preview for the latest revision"));
    require(session->updateLayerXYTransform(text("front"), Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f)),
        text("DepthDrawViewport stale preview fixture should restore the identity XY transform"));
    gpuDisplay.useGpuPreview = false;
    require(session->updateDisplayOptions(gpuDisplay), text("DepthDrawSession should dirty previews when GPU preview is disabled"));
    require(drawViewport->composeDirtyPreviews() == 1 && !session->isTargetPreviewDirty(grid->uuid),
        text("DepthDrawViewport should return to CPU preview composition when GPU preview is disabled"));
    session->clearSelection();
    require(drawViewport->selectLayerPlaneAtDocumentPoint(Vec2(1.5f, 1.5f)) &&
        session->selectedLayerId == text("front") && session->selectedGridUuid == grid->uuid &&
        drawViewport->depthViewSession()->selectedTarget()->getTarget() == grid,
        text("DepthDrawViewport should select a layer plane at a document point and synchronize the selected target view"));
    session->clearSelection();
    require(drawViewport->depthViewPointToDocumentPoint(Vec2(-0.5f, -0.5f)) == Vec2(1.5f, 1.5f) &&
        drawViewport->selectLayerPlaneAtDepthViewPoint(Vec2(-0.5f, -0.5f)) &&
        session->selectedLayerId == text("front") && session->selectedGridUuid == grid->uuid,
        text("DepthDrawViewport should convert depth-view points into document-space layer-plane selection"));
    session->selectedGridUuid = 0;
    drawViewport->depthViewSession()->selectedGridUuid = 0;
    require(drawViewport->selectTargetAtDepthViewPoint(Vec2(-1.0f, -1.0f), 0.25f) &&
        session->selectedGridUuid == grid->uuid &&
        drawViewport->depthViewSession()->selectedTarget()->getTarget() == grid,
        text("DepthDrawViewport should hit-test target depth geometry and synchronize selected target ids"));
    session->selectedGridUuid = 0;
    drawViewport->depthViewSession()->selectedGridUuid = 0;
    require(!drawViewport->selectTargetAtDepthViewPoint(Vec2(1000.0f, 1000.0f), 0.25f) &&
        session->selectedGridUuid == 0 && drawViewport->depthViewSession()->selectedGridUuid == 0,
        text("DepthDrawViewport target hit-test should reject misses without changing selection"));
    require(session->selectTargetGrid(grid->uuid) && drawViewport->depthViewSession()->selectTarget(grid->uuid),
        text("DepthDrawViewport target hit-test test should restore the selected target for later checks"));
    MeshData previewChildMesh;
    previewChildMesh.vertices = nicxlive::core::math::ngVecaFromVectorsSource(nicxlive::core::common::SharedArray<Vec2>{
        Vec2(-1.0f, -1.0f),
        Vec2(1.0f, -1.0f),
        Vec2(-1.0f, 1.0f),
        Vec2(1.0f, 1.0f),
    });
    previewChildMesh.uvs = nicxlive::core::math::ngVecaFromVectorsSource(nicxlive::core::common::SharedArray<Vec2>{
        Vec2(0.0f, 0.0f),
        Vec2(1.0f, 0.0f),
        Vec2(0.0f, 1.0f),
        Vec2(1.0f, 1.0f),
    });
    previewChildMesh.indices = {static_cast<std::uint16_t>(0), 1, 3, 0, 3, 2};
    previewChildMesh.origin = Vec2(0.0f, 0.0f);
    auto previewChildPart = Part::createDataSourceDeferred([&] { return previewChildMesh; },
        [] { return nicxlive::core::common::SharedArray<std::shared_ptr<nicxlive::core::Texture>>{}; },
        [] { return Node::inCreateUUID(); }, [&] { return grid; });
    session->display.showWinningLayer = true;
    auto viewportGeometry = drawViewport->collectRenderGeometry();
    auto meshRenderPlans = drawViewport->collectMeshRenderPlan(viewportGeometry);
    require(meshRenderPlans.size() == 1 && meshRenderPlans[0].targetGridUuid == grid->uuid &&
        meshRenderPlans[0].hasTarget && meshRenderPlans[0].hasMesh &&
        meshRenderPlans[0].drawableChildren == 1 && meshRenderPlans[0].willAttemptTexture,
        text("DepthDrawViewport render plan should route target meshes through the shared offscreen texture path"));
    auto viewportStats = viewportGeometry.stats();
    require(viewportStats.targetMeshes == 1 && viewportStats.targetMeshGridUuids == 1 &&
        viewportStats.targetSurfaceTriangles == 2 && viewportStats.targetLines == 4 &&
        viewportStats.layerPlaneLines == 8 && viewportStats.selectedLayerLines == 4 &&
        viewportStats.depthRangeLines == 8 && viewportStats.gapLines == 1 &&
        viewportStats.overlapGapLines == 0 && viewportStats.gapHandles == 1 &&
        viewportStats.textLabels == 1 &&
        viewportStats.missingPoints == 2 && viewportStats.winningPoints == 2 &&
        viewportStats.winningPointGroups == 1 && viewportStats.normalImageLines == 8 &&
        viewportStats.rawDepthLines == 0 && viewportStats.coverageLines == 0 &&
        viewportStats.coveragePoints == 0,
        text("DepthDrawViewport render-geometry stats should summarize enabled depth-space groups"));
    require(viewportGeometry.targetMeshes.size() == 1 &&
        viewportGeometry.targetMeshes[0].positions.size() == grid->getVertices().size() &&
        viewportGeometry.targetMeshes[0].uvs.size() == grid->getVertices().size() &&
        viewportGeometry.targetMeshes[0].indices.size() > 0,
        text("DepthDrawViewport should expose composed target depth surface mesh geometry"));
    require(viewportGeometry.targetLines.size() == 4,
        text("DepthDrawViewport should expose target grid lines for the depth-space viewport"));
    require(viewportGeometry.layerPlaneLines.size() == 8 && viewportGeometry.depthRangeLines.size() == 8,
        text("DepthDrawViewport should expose layer planes and depth ranges for multiple source layers"));
    float planeMinX = std::numeric_limits<float>::max();
    float planeMaxX = -std::numeric_limits<float>::max();
    float planeMinY = std::numeric_limits<float>::max();
    float planeMaxY = -std::numeric_limits<float>::max();
    for (auto line : viewportGeometry.layerPlaneLines) {
        for (auto point : nijigenerate::compat::Slice<std::remove_cvref_t<decltype(line.p0)>>{line.p0, line.p1}) {
            planeMinX = min(planeMinX, point.x);
            planeMaxX = max(planeMaxX, point.x);
            planeMinY = min(planeMinY, point.y);
            planeMaxY = max(planeMaxY, point.y);
        }
    }
    require(near(planeMinX, -2.0f) && near(planeMaxX, 2.0f) &&
        near(planeMinY, -2.0f) && near(planeMaxY, 2.0f),
        text("DepthDrawViewport should center top-left document-space planes on the model origin"));
    require(session->updateLayerXYTransform(text("front"), Vec2(1.0f, -1.0f), Vec2(0.5f, 2.0f)),
        text("DepthDrawViewport transformed-plane fixture should update the selected layer XY transform"));
    auto transformedGeometry = drawViewport->collectRenderGeometry();
    float selectedMinX = std::numeric_limits<float>::max();
    float selectedMaxX = -std::numeric_limits<float>::max();
    float selectedMinY = std::numeric_limits<float>::max();
    float selectedMaxY = -std::numeric_limits<float>::max();
    for (auto line : transformedGeometry.selectedLayerLines) {
        for (auto point : nijigenerate::compat::Slice<std::remove_cvref_t<decltype(line.p0)>>{line.p0, line.p1}) {
            selectedMinX = min(selectedMinX, point.x);
            selectedMaxX = max(selectedMaxX, point.x);
            selectedMinY = min(selectedMinY, point.y);
            selectedMaxY = max(selectedMaxY, point.y);
        }
    }
    require(near(selectedMinX, -1.0f) && near(selectedMaxX, 1.0f) &&
        near(selectedMinY, -3.0f) && near(selectedMaxY, 5.0f) &&
        session->selectLayerPlaneAtDocumentPoint(Vec2(2.0f, 6.0f)),
        text("DepthDrawViewport guides and hit testing should use the same transformed layer bounds as composition"));
    require(session->updateLayerXYTransform(text("front"), Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f)),
        text("DepthDrawViewport transformed-plane fixture should restore the identity transform"));
    require(viewportGeometry.selectedLayerLines.size() == 4 && viewportGeometry.gapLines.size() == 1 &&
        viewportGeometry.overlapGapLines.size() == 0,
        text("DepthDrawViewport should expose selected layer bounds and valid depth-space gap markers"));
    require(viewportGeometry.gapHandles.size() == 1 &&
        viewportGeometry.gapHandles[0].backLayerId == text("back") &&
        viewportGeometry.gapHandles[0].frontLayerId == text("front") &&
        !viewportGeometry.gapHandles[0].overlap &&
        near(viewportGeometry.gapHandles[0].documentPoint.x, 2.0f) &&
        near(viewportGeometry.gapHandles[0].documentPoint.y, 2.0f) &&
        near(viewportGeometry.gapHandles[0].backDepth, -1.0f) &&
        near(viewportGeometry.gapHandles[0].frontDepth, 0.75f) &&
        near(viewportGeometry.gapHandles[0].renderPoint.size, 7.0f),
        text("DepthDrawViewport should expose visible gap handle metadata for labels and interaction"));
    require(viewportGeometry.textLabels.size() == 1 &&
        viewportGeometry.textLabels[0].text == text("Gap: back -> front") &&
        viewportGeometry.textLabels[0].backLayerId == text("back") &&
        viewportGeometry.textLabels[0].frontLayerId == text("front") &&
        !viewportGeometry.textLabels[0].overlap &&
        near(viewportGeometry.textLabels[0].point.x, viewportGeometry.gapHandles[0].renderPoint.point.x) &&
        near(viewportGeometry.textLabels[0].point.y, viewportGeometry.gapHandles[0].renderPoint.point.y),
        text("DepthDrawViewport should expose gap handle text labels next to visible handles"));
    require(viewportGeometry.missingPoints.size() == 2 && viewportGeometry.winningPoints.size() == 2,
        text("DepthDrawViewport should expose missing and winning target vertices from the composite preview"));
    auto frontWinnerColor = ngDepthDrawWinningLayerColor(text("front"));
    require(viewportGeometry.winningPointGroups.size() == 1 &&
        viewportGeometry.winningPointGroups[0].layerId == text("front") &&
        viewportGeometry.winningPointGroups[0].points.size() == 2 &&
        viewportGeometry.winningPointGroups[0].color == frontWinnerColor,
        text("DepthDrawViewport should group winning target vertices by layer color"));
    require(viewportGeometry.normalImageLines.size() == 8 && viewportGeometry.rawDepthLines.size() == 0 &&
        viewportGeometry.coverageLines.size() == 0 && viewportGeometry.coveragePoints.size() == 0,
        text("DepthDrawViewport should expose source-image plane geometry through display toggles"));
    session->display.showRawDepth = true;
    session->display.showCoverage = true;
    session->display.showComposite = false;
    session->display.showLayerPlanes = false;
    session->display.showDepthRanges = false;
    session->display.showMissingVertices = false;
    session->display.showWinningLayer = false;
    viewportGeometry = drawViewport->collectRenderGeometry();
    viewportStats = viewportGeometry.stats();
    require(viewportStats.targetMeshes == 0 && viewportStats.targetMeshGridUuids == 0 &&
        viewportStats.targetSurfaceTriangles == 0 && viewportStats.targetLines == 0 &&
        viewportStats.layerPlaneLines == 0 && viewportStats.selectedLayerLines == 0 &&
        viewportStats.depthRangeLines == 0 && viewportStats.gapLines == 0 &&
        viewportStats.overlapGapLines == 0 && viewportStats.gapHandles == 0 &&
        viewportStats.textLabels == 0 &&
        viewportStats.missingPoints == 0 && viewportStats.winningPoints == 0 &&
        viewportStats.winningPointGroups == 0 && viewportStats.normalImageLines == 8 &&
        viewportStats.rawDepthLines == 8 && viewportStats.coverageLines == 8 &&
        viewportStats.coveragePoints == 3,
        text("DepthDrawViewport render-geometry stats should reflect display toggles"));
    require(viewportGeometry.targetMeshes.size() == 0 && viewportGeometry.targetLines.size() == 0 &&
        viewportGeometry.missingPoints.size() == 0 &&
        viewportGeometry.winningPoints.size() == 0 && viewportGeometry.layerPlaneLines.size() == 0 &&
        viewportGeometry.depthRangeLines.size() == 0 && viewportGeometry.gapLines.size() == 0,
        text("DepthDrawViewport display toggles should disable composite and depth-space geometry groups"));
    require(viewportGeometry.normalImageLines.size() == 8 && viewportGeometry.rawDepthLines.size() == 8 &&
        viewportGeometry.coverageLines.size() == 8 && viewportGeometry.coveragePoints.size() == 3,
        text("DepthDrawViewport display toggles should enable normal, raw-depth, and coverage source overlays"));
    require(viewportGeometry.coveragePoints[0].layerId == text("back") &&
        near(viewportGeometry.coveragePoints[0].documentPoint.x, 1.5f) &&
        near(viewportGeometry.coveragePoints[0].documentPoint.y, 1.5f) &&
        near(viewportGeometry.coveragePoints[0].alpha, 1.0f),
        text("DepthDrawViewport coverage overlay should expose normal-layer coverage alpha points"));
    auto closingDisplay = session->display;
    closingDisplay.useGpuPreview = true;
    require(session->updateDisplayOptions(closingDisplay) && drawViewport->composeDirtyPreviews() == 1 &&
        drawViewport->pendingGpuPreviewJobCount() == 1 && ngPendingDepthDrawGpuComposeJobCount() == 1,
        text("DepthDrawViewport close fixture should leave one backend GPU preview pending"));
    drawViewport->withdraw();
    require(drawViewport->depthViewSession()->targets.size() == 0 &&
        drawViewport->pendingGpuPreviewJobCount() == 0 && ngPendingDepthDrawGpuComposeJobCount() == 0,
        text("DepthDrawViewport withdraw should cancel backend GPU previews and clear common target views"));
    auto composedStackRows = ngDepthDrawLayerStackRows(session, &result);
    require(composedStackRows.size() == 2 && composedStackRows[0].sampledVertices == 1 &&
        composedStackRows[0].missingVertices == 3 && composedStackRows[0].winningVertices == 0,
        text("DepthDraw layer stack rows should expose sampled, missing, and winning counts from composition"));
    require(composedStackRows[1].hasDepthRange && near(composedStackRows[1].minDepth, 0.75f) &&
        near(composedStackRows[1].maxDepth, 0.75f),
        text("DepthDraw layer stack rows should expose per-layer composed depth range"));
    auto previewSummary = ngDepthDrawCompositePreviewSummary(result);
    require(previewSummary.targetGridUuid == grid->uuid && previewSummary.vertexCount == result.depths.size(),
        text("DepthDraw composite preview summary should identify the target and vertex count"));
    require(previewSummary.sampledVertices == result.sampledVertices &&
        previewSummary.missingVertices == result.missingVertices &&
        previewSummary.finalSampledVertices == 2 && previewSummary.finalMissingVertices == 2,
        text("DepthDraw composite preview summary should expose sampled and missing counts"));
    require(previewSummary.points.size() == result.depths.size() &&
        previewSummary.points[0].sampled && !previewSummary.points[0].missing &&
        previewSummary.points[0].winningLayerId == text("front") &&
        !previewSummary.points[2].sampled && previewSummary.points[2].missing,
        text("DepthDraw composite preview summary should expose per-vertex winner and missing state"));
    require(previewSummary.points[0].winningLayerColor == frontWinnerColor,
        text("DepthDraw composite preview summary should assign stable colors to winning layer vertices"));
    require(previewSummary.hasDepthRange && near(previewSummary.minDepth, result.minDepth) &&
        near(previewSummary.maxDepth, result.maxDepth),
        text("DepthDraw composite preview summary should preserve final depth range"));
    auto readbackComparison = ngCompareDepthDrawComposeReadback(result, result);
    require(readbackComparison.ok && readbackComparison.depthMismatches == 0 &&
        readbackComparison.winnerMismatches == 0 && readbackComparison.missingFlagMismatches == 0 &&
        readbackComparison.targetMismatches == 0,
        text("DepthDraw readback equivalence should accept identical CPU/readback compose results"));
    readbackComparison = ngCompareDepthDrawComposeReadback(result, result, -0.1f);
    require(!readbackComparison.ok && readbackComparison.error.size() > 0,
        text("DepthDraw readback equivalence should reject negative tolerance"));
    readbackComparison = ngCompareDepthDrawComposeReadback(result, result, std::numeric_limits<float>::quiet_NaN());
    require(!readbackComparison.ok && readbackComparison.error.size() > 0,
        text("DepthDraw readback equivalence should reject NaN tolerance"));
    auto lengthMismatch = result;
    lengthMismatch.depths = result.depths.slice(0, result.depths.size() - 1).duplicate();
    readbackComparison = ngCompareDepthDrawComposeReadback(result, lengthMismatch);
    require(!readbackComparison.ok && readbackComparison.error.size() > 0,
        text("DepthDraw readback equivalence should reject depth array length mismatches"));
    auto depthMismatch = result;
    depthMismatch.depths = result.depths.duplicate();
    depthMismatch.winningLayerIds = result.winningLayerIds.duplicate();
    depthMismatch.depths[0] += 0.1f;
    readbackComparison = ngCompareDepthDrawComposeReadback(result, depthMismatch);
    require(!readbackComparison.ok && readbackComparison.depthMismatches == 1,
        text("DepthDraw readback equivalence should reject mismatched depth arrays"));
    auto winnerMismatch = result;
    winnerMismatch.depths = result.depths.duplicate();
    winnerMismatch.winningLayerIds = result.winningLayerIds.duplicate();
    winnerMismatch.winningLayerIds[0] = text("back");
    readbackComparison = ngCompareDepthDrawComposeReadback(result, winnerMismatch);
    require(!readbackComparison.ok && readbackComparison.winnerMismatches == 1,
        text("DepthDraw readback equivalence should reject mismatched layer winners"));
    auto missingMismatch = result;
    missingMismatch.depths = result.depths.duplicate();
    missingMismatch.winningLayerIds = result.winningLayerIds.duplicate();
    missingMismatch.winningLayerIds[0] = text("");
    readbackComparison = ngCompareDepthDrawComposeReadback(result, missingMismatch);
    require(!readbackComparison.ok && readbackComparison.missingFlagMismatches == 1,
        text("DepthDraw readback equivalence should reject mismatched missing flags"));
    auto targetMismatch = result;
    targetMismatch.depths = result.depths.duplicate();
    targetMismatch.winningLayerIds = result.winningLayerIds.duplicate();
    targetMismatch.targetGridUuid = result.targetGridUuid + 1;
    readbackComparison = ngCompareDepthDrawComposeReadback(result, targetMismatch);
    require(!readbackComparison.ok && readbackComparison.targetMismatches == 1 &&
        readbackComparison.winnerMismatches == 0,
        text("DepthDraw readback equivalence should report target identity mismatches separately from winner mismatches"));
    auto aggregateStatsMismatch = result;
    aggregateStatsMismatch.depths = result.depths.duplicate();
    aggregateStatsMismatch.winningLayerIds = result.winningLayerIds.duplicate();
    aggregateStatsMismatch.layerStats = result.layerStats.duplicate();
    aggregateStatsMismatch.sampledVertices++;
    readbackComparison = ngCompareDepthDrawComposeReadback(result, aggregateStatsMismatch);
    require(!readbackComparison.ok && readbackComparison.aggregateStatMismatches == 1,
        text("DepthDraw readback equivalence should reject mismatched aggregate compose stats"));
    auto layerStatsMismatch = result;
    layerStatsMismatch.depths = result.depths.duplicate();
    layerStatsMismatch.winningLayerIds = result.winningLayerIds.duplicate();
    layerStatsMismatch.layerStats = result.layerStats.duplicate();
    layerStatsMismatch.layerStats[0].sampledVertices++;
    readbackComparison = ngCompareDepthDrawComposeReadback(result, layerStatsMismatch);
    require(!readbackComparison.ok && readbackComparison.layerStatMismatches == 1,
        text("DepthDraw readback equivalence should reject mismatched per-layer compose stats"));
    auto depthSpace = ngDepthDrawDepthSpaceSummary(session, &result);
    require(depthSpace.selectedGridUuid == grid->uuid && depthSpace.selectedLayerId == text("front"),
        text("DepthDraw depth-space summary should preserve selected target and layer ids"));
    require(depthSpace.layers.size() == 2 && !depthSpace.layers[0].selected && depthSpace.layers[1].selected,
        text("DepthDraw depth-space summary should expose selected layer plane state"));
    require(depthSpace.layers[1].bounds.width == 4 && depthSpace.layers[1].bounds.height == 4 &&
        depthSpace.layers[1].hasDepthRange && near(depthSpace.layers[1].representativeDepth, 0.75f),
        text("DepthDraw depth-space summary should expose layer bounds and representative depth"));
    require(depthSpace.gaps.size() == 1 && depthSpace.gaps[0].valid && !depthSpace.gaps[0].overlap &&
        depthSpace.gaps[0].backLayerId == text("back") && depthSpace.gaps[0].frontLayerId == text("front") &&
        near(depthSpace.gaps[0].backDepth, -1.0f) && near(depthSpace.gaps[0].frontDepth, 0.75f),
        text("DepthDraw depth-space summary should expose adjacent gap and overlap markers"));

    const auto fullVertexPixels = [&](std::uint8_t gray) {
        Bytes pixels;
        for (std::size_t i = 0; i < 16; ++i) pixels.appendRange(Bytes{0, 0, 0, 0});
        setPixel(pixels, 1, 1, gray, 255);
        setPixel(pixels, 3, 1, gray, 255);
        setPixel(pixels, 1, 3, gray, 255);
        setPixel(pixels, 3, 3, gray, 255);
        return pixels;
    };

    auto mergeFixture = loadDepthDrawFixture(text("local-merge-policies-smoke.json"));
    {
        auto sourceRange3 = mergeFixture.sourceIndexValue("cases").sourceArrayValue();
        const auto sourceCount3 = sourceRange3.size();
        for (std::size_t caseIndex = 0; caseIndex < sourceCount3; ++caseIndex) {
            auto caseValue = sourceRange3[caseIndex];
        auto policyObject = caseValue.sourceObjectRef();
        auto policyOwner = formatted("local-merge-policies-smoke.cases[%s]", caseIndex);
        auto policyValue = fixtureRequired(policyObject, text("policy"), policyOwner);
        auto policyName = fixtureString(policyValue, text("merge policy"));
        auto depthsObject = caseValue.sourceObjectRef();
        auto depthsOwner = formatted("local-merge-policies-smoke.cases[%s]", caseIndex);
        auto depthsValue = fixtureRequired(depthsObject, text("expectedDepths"), depthsOwner);
        auto expectedDepths = fixtureFloatArray(depthsValue, concatenateSourceStrings({policyName, text(".expectedDepths")}));
        auto winnersObject = caseValue.sourceObjectRef();
        auto winnersOwner = formatted("local-merge-policies-smoke.cases[%s]", caseIndex);
        auto winnersValue = fixtureRequired(winnersObject, text("expectedWinners"), winnersOwner);
        auto expectedWinners = fixtureStringArray(winnersValue, concatenateSourceStrings({policyName, text(".expectedWinners")}));

        DepthDrawLayer lowLayer;
        lowLayer.id = text("low");
        lowLayer.layerPath = text("/low");
        lowLayer.displayName = text("Low");
        lowLayer.width = 4;
        lowLayer.height = 4;
        lowLayer.bounds.width = 4;
        lowLayer.bounds.height = 4;
        lowLayer.convolution = DepthImageConvolution::Nearest;
        lowLayer.depthPixels = fullVertexPixels(0);

        DepthDrawLayer highLayer = lowLayer;
        highLayer.id = text("high");
        highLayer.layerPath = text("/high");
        highLayer.displayName = text("High");
        highLayer.depthPixels = fullVertexPixels(255);

        DepthDrawBinding lowBinding;
        lowBinding.layerId = text("low");
        lowBinding.targetGridUuid = grid->uuid;
        lowBinding.targetNodeUuid = grid->uuid;
        lowBinding.order = 0;
        lowBinding.mergePolicy = fixtureMergePolicy(policyName);

        DepthDrawBinding highBinding = lowBinding;
        highBinding.layerId = text("high");
        highBinding.order = 1;

        auto mergeSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
        mergeSession->layers.append(lowLayer);
        mergeSession->layers.append(highLayer);
        mergeSession->bindings.append(lowBinding);
        mergeSession->bindings.append(highBinding);

        auto mergeResult = ngComposeDepthDrawTarget(mergeSession, view, 4, 4);
        const bool mergeLengths = mergeResult.depths.size() == expectedDepths.size() &&
            mergeResult.winningLayerIds.size() == expectedWinners.size();
        require(mergeLengths,
            concatenateSourceStrings({text("DepthDraw merge policy fixture should match the composed target vertex count for "), policyName}));
        {
        auto sourceRange4 = expectedDepths;
        const auto sourceCount4 = sourceRange4.size();
        for (std::size_t i = 0; i < sourceCount4; ++i) {
            auto expectedDepth = sourceRange4[i];
            const bool sameDepth = near(mergeResult.depths[i], expectedDepth);
            require(sameDepth,
                concatenateSourceStrings({text("DepthDraw merge policy "), policyName, text(" should produce fixture expectedDepths")}));
            const bool sameWinner = mergeResult.winningLayerIds[i] == expectedWinners[i];
            require(sameWinner,
                concatenateSourceStrings({text("DepthDraw merge policy "), policyName, text(" should produce fixture expectedWinners")}));
        }
    }

        auto mergeGpuPacket = ngBuildDepthDrawGpuComposePacket(mergeSession, view, 4, 4);
        DepthDrawGpuLayerReadback lowMergeReadback;
        lowMergeReadback.layerIndex = 0;
        lowMergeReadback.validSamples.resize(grid->getVertices().size());
        lowMergeReadback.sampleDepths.resize(grid->getVertices().size(), std::numeric_limits<float>::quiet_NaN());
        DepthDrawGpuLayerReadback highMergeReadback;
        highMergeReadback.layerIndex = 1;
        highMergeReadback.validSamples.resize(grid->getVertices().size());
        highMergeReadback.sampleDepths.resize(grid->getVertices().size(), std::numeric_limits<float>::quiet_NaN());
        for (std::size_t i = 0, count = grid->getVertices().size(); i < count; ++i) {
            lowMergeReadback.validSamples[i] = 1;
            lowMergeReadback.sampleDepths[i] = -1.0f;
            highMergeReadback.validSamples[i] = 1;
            highMergeReadback.sampleDepths[i] = 1.0f;
        }
        auto mergeGpuReadback = ngBuildDepthDrawGpuComposeReadback(
            mergeGpuPacket,
            Slice<DepthDrawGpuLayerReadback>{lowMergeReadback, highMergeReadback});
        auto mergeGpuResult = ngDepthDrawComposeResultFromGpuReadback(mergeGpuPacket, mergeGpuReadback);
        auto mergeGpuComparison = ngCompareDepthDrawComposeReadback(mergeResult, mergeGpuResult);
        require(mergeGpuComparison.ok,
            concatenateSourceStrings({text("DepthDraw GPU readback merge policy "), policyName, text(" should match CPU composition")}));
    }
    }

    auto transformGrid = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(nijigenerate::project::incActivePuppet())->root; });
    transformGrid->setName(text("depthdraw-transform-grid").toNullableString());
    auto transformCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    transformCtx->nodes(Context::Nodes{transformGrid});
    require(ngRequireObject(defineGrid()->run(transformCtx))->succeeded,
        text("DepthDraw transform fixture should define a 2x2 grid"));
    transformGrid->replaceDepths(Slice<float>{0.0f, 0.0f, 0.0f, 0.0f});

    Bytes transformPixels;
    for (std::size_t i = 0; i < 16; ++i) transformPixels.appendRange(Bytes{0, 0, 0, 0});
    setPixel(transformPixels, 0, 0, 0, 255);
    setPixel(transformPixels, 1, 0, 64, 255);
    setPixel(transformPixels, 0, 1, 128, 255);
    setPixel(transformPixels, 1, 1, 255, 255);

    DepthDrawLayer transformLayer;
    transformLayer.id = text("transform");
    transformLayer.layerPath = text("/transform");
    transformLayer.displayName = text("Transform");
    transformLayer.width = 4;
    transformLayer.height = 4;
    transformLayer.bounds.width = 4;
    transformLayer.bounds.height = 4;
    transformLayer.xyOffset = Vec2(1, 1);
    transformLayer.xyScale = Vec2(2, 2);
    transformLayer.convolution = DepthImageConvolution::Nearest;
    transformLayer.depthPixels = transformPixels;

    DepthDrawBinding transformBinding;
    transformBinding.layerId = text("transform");
    transformBinding.targetGridUuid = transformGrid->uuid;
    transformBinding.targetNodeUuid = transformGrid->uuid;
    transformBinding.order = 0;
    transformBinding.mergePolicy = DepthMergePolicy::Replace;

    auto transformSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    transformSession->layers.append(transformLayer);
    transformSession->bindings.append(transformBinding);

    auto transformView = std::allocate_shared<DepthTargetView>(SourceAllocator<DepthTargetView>{}, transformGrid);
    auto topLeftDocumentPoint = ngDepthDrawVertexDocumentPosition(transformView, Vec2(-1, -1), 4, 4);
    require(nearVec2(topLeftDocumentPoint, Vec2(1, 1)),
        text("DepthDraw coordinate helper should convert local vertices to document space"));
    auto topLeftLayerPoint = ngDepthDrawLayerPixelFromDocument(transformLayer, topLeftDocumentPoint);
    require(nearVec2(topLeftLayerPoint, Vec2(0, 0)),
        text("DepthDraw coordinate helper should apply bounds, XY offset, and XY scale in one shared path"));
    auto transformGpuPacket = ngBuildDepthDrawGpuComposePacket(transformSession, transformView, 4, 4);
    require(transformGpuPacket.layers.size() == 1 &&
        near(transformGpuPacket.layers[0].xyOffsetX, transformLayer.xyOffset.x) &&
        near(transformGpuPacket.layers[0].xyOffsetY, transformLayer.xyOffset.y) &&
        near(transformGpuPacket.layers[0].xyScaleX, transformLayer.xyScale.x) &&
        near(transformGpuPacket.layers[0].xyScaleY, transformLayer.xyScale.y),
        text("DepthDraw GPU compose packet should carry XY transform values used by the CPU sampler"));
    auto gpuTopLeftLayerPoint = ngDepthDrawGpuLayerPixelFromDocument(
        transformGpuPacket.layers[0],
        transformGpuPacket.documentPositions[0]);
    require(nearVec2(gpuTopLeftLayerPoint, topLeftLayerPoint),
        text("DepthDraw GPU coordinate helper should match the CPU layer-pixel coordinate helper"));
    auto transformResult = ngComposeDepthDrawTarget(transformSession, transformView, 4, 4);
    require(transformResult.depths.size() == transformGrid->getVertices().size(),
        text("DepthDraw transform composer should produce one depth per target vertex"));
    {
        auto sourceRange5 = transformView->getVertices();
        const auto sourceCount5 = sourceRange5.size();
        for (std::size_t i = 0; i < sourceCount5; ++i) {
            auto vertex = sourceRange5[i];
        float expected = std::numeric_limits<float>::quiet_NaN();
        if (near(vertex.x, -1.0f) && near(vertex.y, -1.0f)) {
            expected = -1.0f;
        } else if (near(vertex.x, 1.0f) && near(vertex.y, -1.0f)) {
            expected = static_cast<float>(64.0 / 255.0 * 2.0 - 1.0);
        } else if (near(vertex.x, -1.0f) && near(vertex.y, 1.0f)) {
            expected = static_cast<float>(128.0 / 255.0 * 2.0 - 1.0);
        } else if (near(vertex.x, 1.0f) && near(vertex.y, 1.0f)) {
            expected = 1.0f;
        } else {
            require(false, text("DepthDraw transform fixture should only contain the expected 2x2 vertices"));
        }
        require(near(transformResult.depths[i], expected),
            text("DepthDraw composer should apply layer bounds, XY offset, and XY scale before sampling"));
        require(transformResult.winningLayerIds[i] == text("transform"),
            text("DepthDraw transform fixture should sample every vertex from the transform layer"));
    }
    }

    auto pathTarget = ExPathDeformer::createSourceDeferred([] { return ngRequireObject(nijigenerate::project::incActivePuppet())->root; });
    pathTarget->setName(text("depthdraw-path-target").toNullableString());
    pathTarget->rebuffer(nicxlive::core::math::ngVecaFromVectorsSource(nicxlive::core::common::SharedArray<Vec2>{
        Vec2(-1, -1),
        Vec2(1, -1),
        Vec2(1, 1),
    }));
    pathTarget->replaceDepths(Slice<float>{0.1f, 0.2f, 0.3f});

    DepthDrawLayer pathLayer;
    pathLayer.id = text("path-layer");
    pathLayer.layerPath = text("/path-layer");
    pathLayer.displayName = text("Path Layer");
    pathLayer.width = 4;
    pathLayer.height = 4;
    pathLayer.bounds.width = 4;
    pathLayer.bounds.height = 4;
    pathLayer.convolution = DepthImageConvolution::Nearest;
    pathLayer.depthPixels = fullVertexPixels(255);

    DepthDrawBinding pathBinding;
    pathBinding.layerId = text("path-layer");
    pathBinding.targetGridUuid = pathTarget->uuid;
    pathBinding.targetNodeUuid = pathTarget->uuid;
    pathBinding.mergePolicy = DepthMergePolicy::Replace;

    auto pathSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    pathSession->layers.append(pathLayer);
    pathSession->bindings.append(pathBinding);
    auto pathView = std::allocate_shared<DepthTargetView>(SourceAllocator<DepthTargetView>{}, pathTarget);
    auto pathResult = ngComposeDepthDrawTarget(pathSession, pathView, 4, 4);
    require(pathResult.targetGridUuid == pathTarget->uuid && pathResult.depths.size() == pathTarget->getVertices().size(),
        text("DepthDraw composer should support PathDeformer targets"));
    for (auto depth : pathResult.depths) {
        require(near(depth, 1.0f), text("DepthDraw PathDeformer composition should sample one depth per path control point"));
    }
    auto pathContext = std::allocate_shared<Context>(SourceAllocator<Context>{});
    auto pathSummary = ngApplyDepthDrawTargetResultWithSummary(pathContext, pathView, pathResult);
    require(pathSummary.succeeded && pathSummary.changedTargets == 1 && equalArrays([&] { return pathTarget->copyDepths(); }, [&] { return pathResult.depths; }),
        text("DepthDraw apply should write composed depths to PathDeformer targets"));
    auto pathGpuPacket = ngBuildDepthDrawGpuComposePacket(pathSession, pathView, 4, 4);
    require(pathGpuPacket.targetGridUuid == pathTarget->uuid &&
        equalArrays([&] { return pathGpuPacket.vertices; }, [&] { return pathTarget->getVertices().toArray(); }) &&
        pathGpuPacket.documentPositions.size() == pathTarget->getVertices().size() &&
        equalArrays([&] { return pathGpuPacket.baseDepths; }, [&] { return pathView->baseDepths; }) &&
        pathGpuPacket.layers.size() == 1 &&
        pathGpuPacket.bindings.size() == 1,
        text("DepthDraw GPU compose packet should support PathDeformer targets through the shared target view"));
    DepthDrawGpuLayerReadback pathLayerReadback;
    pathLayerReadback.layerIndex = 0;
    pathLayerReadback.validSamples = {1, 1, 1};
    pathLayerReadback.sampleDepths = pathResult.depths.duplicate();
    auto builtPathGpuReadback = ngBuildDepthDrawGpuComposeReadback(pathGpuPacket, Slice<DepthDrawGpuLayerReadback>{pathLayerReadback});
    require(builtPathGpuReadback.targetGridUuid == pathTarget->uuid &&
        builtPathGpuReadback.depths.size() == pathTarget->getVertices().size() &&
        equalArrays([&] { return builtPathGpuReadback.winningLayerIndices; }, [&] { return Slice<std::int32_t>{0, 0, 0}; }),
        text("DepthDraw GPU readback builder should merge PathDeformer control-point samples"));
    auto builtPathGpuResult = ngDepthDrawComposeResultFromGpuReadback(pathGpuPacket, builtPathGpuReadback);
    auto builtPathGpuComparison = ngCompareDepthDrawComposeReadback(pathResult, builtPathGpuResult);
    require(builtPathGpuComparison.ok,
        text("DepthDraw GPU readback result should match CPU composition for PathDeformer targets"));
    incActionUndo();
    require(equalArrays([&] { return pathTarget->copyDepths(); }, [&] { return Slice<float>{0.1f, 0.2f, 0.3f}; }),
        text("DepthDraw PathDeformer apply undo should restore previous target depths"));
    incActionRedo();
    require(equalArrays([&] { return pathTarget->copyDepths(); }, [&] { return pathResult.depths; }),
        text("DepthDraw PathDeformer apply redo should restore composed target depths"));

    session->markTargetPreviewDirty(grid->uuid);
    session->markTargetPreviewDirty(transformGrid->uuid);
    auto preview = ngPreviewDepthDrawTarget(session, view, 4, 4);
    require(equalArrays([&] { return preview.depths; }, [&] { return view->copyWorkingDepths(); }),
        text("DepthDraw preview should write composed depths into the shared target view only"));
    require(equalArrays([&] { return grid->copyDepths(); }, [&] { return Slice<float>{0.25f, 0.25f, 0.25f, 0.25f}; }),
        text("DepthDraw preview should not apply composed depths to the model target"));
    require(!session->isTargetPreviewDirty(grid->uuid) && session->isTargetPreviewDirty(transformGrid->uuid),
        text("DepthDraw preview should clear dirty state only for the recomposed target"));

    fakeDepthBoneGpuNextJobId = 1;
    fakeDepthBoneGpuJobVertexCounts = nullptr;
    fakeDepthBoneGpuSubmitCount = 0;
    fakeDepthBoneGpuPollCount = 0;
    fakeDepthBoneGpuSubmitFailAfter = 0;
    fakeDepthBoneGpuNotReadyPolls = 0;
    ngSetDepthBoneGpuAsyncTestHooks(&fakeDepthBoneGpuSupported, &fakeDepthBoneGpuSubmit, &fakeDepthBoneGpuPoll);
    nijigenerate::compat::ngRunWithScopeExit([&] {

    auto rigRoot = ExDepthRigRoot::createSourceDeferred([] { return ngRequireObject(nijigenerate::project::incActivePuppet())->root; });
    rigRoot->setName(text("depthdraw-refresh-root").toNullableString());
    auto bone = ngCreateDepthBone(rigRoot, text("DepthDrawRefreshBone"), Vec3(0, 0, 0), Vec3(0, 100, 0));
    ExDepthRigBinding rigBinding;
    rigBinding.targetUuid = grid->uuid;
    rigBinding.targetKind = ExDepthTargetKind::Grid;
    rigBinding.sourceBoneUuids = {static_cast<std::uint64_t>(bone->uuid)};
    rigBinding.influenceRule.maxInfluences = 1;
    rigRoot->bindings.append(rigBinding);

    auto param = ExParameter::createSourceDeferred([&] { return text("DepthDrawRefreshParam").toNullableString(); }, [] { return false; });
    param->min = Vec2(0, 0);
    param->max = Vec2(1, 0);
    param->value = Vec2(1, 0);
    ngRequireObject(nijigenerate::project::incActivePuppet())->parameters.push_back(param);
    auto tx = newValueBinding(param, bone, text("transform.t.x"));
    tx->setValue(Vec2u(1, 0), 5.0f);

    auto applyCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    auto applySummary = ngApplyDepthDrawTargetResultWithSummary(applyCtx, view, result);
    require(applySummary.succeeded,
        text("DepthDraw apply helper should apply through the shared depth map change path"));
    require(applySummary.changedTargets == 1 && applySummary.changedVertices == 2,
        text("DepthDraw apply summary should count changed target and vertices"));
    require(applySummary.layersApplied == 1 && applySummary.layersSkipped == 1,
        text("DepthDraw apply summary should count applied and skipped layers"));
    require(applySummary.missingSamples == result.missingVertices,
        text("DepthDraw apply summary should report missing sample warnings"));
    require(equalArrays([&] { return grid->copyDepths(); }, [&] { return result.depths; }), text("DepthDraw apply should write composed depths to the target"));
    ngFlushDepthBoneDirtyImmediate();
    require(fakeDepthBoneGpuSubmitCount >= 2,
        text("DepthDraw apply should submit DepthBone refresh work for the changed target and keypoints"));
    auto deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(param->getBinding(grid, text("deform").toNullableString()));
    require(deformBinding != nullptr, text("DepthDraw apply should refresh DepthBone deform binding for the changed target"));
    require(ngRequireObject(deformBinding)->getValue(Vec2u(1, 0)).vertexOffsets.size() == grid->getVertices().size(),
        text("DepthDraw apply DepthBone refresh should write every target vertex offset"));
    incActionUndo();
    require(equalArrays([&] { return grid->copyDepths(); }, [&] { return Slice<float>{0.25f, 0.25f, 0.25f, 0.25f}; }),
        text("DepthDraw apply undo should restore previous target depths"));
    incActionRedo();
    require(equalArrays([&] { return grid->copyDepths(); }, [&] { return result.depths; }),
        text("DepthDraw apply redo should restore composed target depths"));

    auto additiveContext = std::allocate_shared<Context>(SourceAllocator<Context>{});
    auto additiveApplySummary = ngApplyDepthDrawTargetResultWithSummary(additiveContext, view, additiveResult);
    require(additiveApplySummary.succeeded &&
        additiveApplySummary.layersApplied == 2 &&
        additiveApplySummary.layersSkipped == 0,
        text("DepthDraw apply summary should count additive layers that contributed to final depths"));

    }, [] {
        ngClearDepthBoneGpuAsyncTestHooks();
        fakeDepthBoneGpuJobVertexCounts = nullptr;
        fakeDepthBoneGpuSubmitCount = 0;
        fakeDepthBoneGpuPollCount = 0;
        fakeDepthBoneGpuSubmitFailAfter = 0;
        fakeDepthBoneGpuNotReadyPolls = 0;
    
    });

    }, [] {
        ngClearDepthDrawGpuTestHooks();
        fakeDepthDrawGpuNotReadyPolls = 0;
        fakeDepthDrawGpuReadbacks = nullptr;
    
    });
}
