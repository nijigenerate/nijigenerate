// Existing regression.d4776-5250 only.
void requirePsdDepthDialogCommandRoundTrip(std::shared_ptr<nijigenerate::commands::Context> ctx,
    std::shared_ptr<nijigenerate::commands::CommandResult> result,
    nijigenerate::compat::SharedDelegate<bool()> changed,
    nijigenerate::compat::SharedDelegate<bool()> restored, nijigenerate::compat::RetainedString commandName) {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::puppet::edit::EditCommand;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const bool succeeded = ngRequireObject(result)->succeeded;
    require(succeeded, concatenateSourceStrings({commandName,
        text(" must run while the PSD depth dialog is displayed: "), result->message}));
    const bool firstChanged = changed();
    require(firstChanged, concatenateSourceStrings({commandName, text(" must update the dialog state")}));
    const bool undone = ngRequireObject(ngCmd<EditCommand::Undo>(ctx))->succeeded;
    require(undone, concatenateSourceStrings({commandName, text(" must support Undo")}));
    const bool wasRestored = restored();
    require(wasRestored, concatenateSourceStrings({commandName, text(" Undo must restore the previous dialog state")}));
    const bool redone = ngRequireObject(ngCmd<EditCommand::Redo>(ctx))->succeeded;
    require(redone, concatenateSourceStrings({commandName, text(" must support Redo")}));
    const bool changedAgain = changed();
    require(changedAgain, concatenateSourceStrings({commandName, text(" Redo must restore the changed dialog state")}));
}

void testAllPsdDepthDialogCommands() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::CommandResult;
    using nijigenerate::commands::ExCommandResult;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::puppet::edit::EditCommand;
    using nijigenerate::windows::psddepthmap::PSDDepthMapWindow;
    using nijigenerate::windows::psddepthmap::PsdDepthDialogLayerState;
    using nijigenerate::windows::base::ngCreateWindow;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using namespace nijigenerate::commands::depth::psd_dialog;
    using namespace nijigenerate::io::depthmap_psd;
    using namespace nijigenerate::core::actionstack;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    using JsonType = nijigenerate::core::settings::SettingsJsonType;
    const auto objectMember = [](Json value, std::string_view key, std::uint32_t line) -> Json {
        const auto& object = value.sourceObjectRef();
        const auto* member = object.if_contains(key);
        if (!member) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            RetainedString::fromStaticLiteral("source/nijigenerate_tests/regression.d"), line);
        return *member;
    };
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    // std.json.get!ulong, json.d465-499; the same-width signed conversion uses conv.d547-572.
    const auto unsignedValue = [&](Json value) -> std::uint64_t {
        if (value.is_uint64()) return value.as_uint64();
        if (value.is_int64()) {
            const auto integer = value.as_int64();
            if (integer < 0) throw nijigenerate::compat::PhobosConvOverflowException(
                text("Conversion negative overflow"), text("std/conv.d"), 568);
            return static_cast<std::uint64_t>(integer);
        }
        throw nijigenerate::core::settings::SettingsJsonError::fromStaticLiteral("JSONValue is not a an integral type");
    };
    resetCase();
    auto grid = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    grid->setName(NullableString::fromStaticLiteral("PSD Dialog Command Target"));
    auto dialog = ngCreateWindow<PSDDepthMapWindow>(text("psd-depth-dialog-all-commands-regression.png"));
    dialog->beginDialogCommandSessionForRegression();
    PsdDepthDialogLayerState initialLayer;
    initialLayer.layerPath = text("/all-commands-layer");
    initialLayer.targetGridUuid = grid->uuid;
    initialLayer.visible = true;
    initialLayer.enabled = true;
    initialLayer.depthEnabled = true;
    initialLayer.depthOffset = 0.0f;
    initialLayer.depthScale = 1.0f;
    dialog->setDialogLayerStateForRegression(initialLayer);
    auto ctx = nijigenerate::core::shortcut::ngBuildExecutionContext();
    check(ctx->hasNodes() && ctx->nodes().size() == 1 && ctx->nodes()[0] == grid,
        "PSD dialog layer selection must populate the standard Context.nodes target");
    auto inspect = ngCmd<PsdDepthDialogCommand::InspectPsdDepthDialog>(ctx);
    auto inspected = std::dynamic_pointer_cast<ExCommandResult<Json>>(inspect);
    check(inspected != nullptr && inspected->succeeded && inspected->result.sourceIndexValue("layers").sourceArrayValue().size() == 1 &&
        inspected->result.sourceIndexValue("layers").sourceArrayValue()[0].sourceIndexValue("layerPath").as_string().view() == initialLayer.layerPath.view() &&
        unsignedValue(inspected->result.sourceIndexValue("layers").sourceArrayValue()[0].sourceIndexValue("targetGridUuid")) == grid->uuid &&
        incActionHistory().size() == 0, "InspectPsdDepthDialog must expose layer identity without mutating dialog history");
    grid->vertices.resize(2);
    grid->vertices[0] = Vec2{0, 0};
    grid->vertices[1] = Vec2{1, 0};
    Slice<std::uint8_t> colors{255, 0, 0, 255, 0, 0, 255, 255};
    Slice<std::uint8_t> depths{64, 64, 64, 255, 192, 192, 192, 255};
    Slice<std::uint8_t> previews{80, 80, 80, 255, 200, 200, 200, 255};
    check(dialog->setDialogPartDataForRegression(grid, 2, 1, colors, depths, previews, Slice<float>{-0.5f, 0.75f}),
        "PSD dialog part data regression fixture must be installed");
    auto noPart = std::allocate_shared<Context>(SourceAllocator<Context>{});
    check(!ngCmd<PsdDepthDialogCommand::GetPsdDepthDialogPartData>(noPart)->succeeded,
        "GetPsdDepthDialogPartData must require a target selected through Context.nodes");
    auto part = std::dynamic_pointer_cast<ExCommandResult<Json>>(ngCmd<PsdDepthDialogCommand::GetPsdDepthDialogPartData>(ctx));
    check(part != nullptr && part->succeeded && part->result.sourceIndexValue("mcpDirectToolResult").sourceType() == JsonType::True &&
        part->result.sourceIndexValue("_meta").sourceIndexValue("imageCount").as_int64() == 4 &&
        part->result.sourceIndexValue("_meta").sourceIndexValue("resourceCount").as_int64() == 1 &&
        part->result.sourceIndexValue("content").sourceArrayValue().size() == 6,
        "GetPsdDepthDialogPartData must return metadata, overall preview/depth, and part images");
    auto metadataText = part->result.sourceIndexValue("content").sourceArrayValue()[0].sourceIndexValue("text").as_string();
    auto metadata = nijigenerate::autorig::json::detail::parseJSON(RetainedStringSlice(metadataText));
    auto entry = metadata.sourceIndexValue("parts").sourceArrayValue()[0];
    auto layerEntry = entry.sourceIndexValue("layers").sourceArrayValue()[0];
    check(metadata.sourceIndexValue("parts").sourceArrayValue().size() == 1 &&
        unsignedValue(entry.sourceIndexValue("targetGridUuid")) == grid->uuid &&
        entry.sourceIndexValue("depth").sourceIndexValue("values").sourceArrayValue().size() == 2 &&
        near(static_cast<float>(entry.sourceIndexValue("depth").sourceIndexValue("values").sourceArrayValue()[0].as_double()), -0.5f) &&
        near(static_cast<float>(entry.sourceIndexValue("depth").sourceIndexValue("values").sourceArrayValue()[1].as_double()), 0.75f) &&
        entry.sourceIndexValue("depth").sourceIndexValue("vertexPositions").sourceArrayValue().size() == 2 &&
        metadata.sourceIndexValue("overallPreviewImage").sourceIndexValue("contentIndex").as_int64() == 1 &&
        metadata.sourceIndexValue("overallPreviewImage").sourceIndexValue("width").as_int64() == 640 &&
        metadata.sourceIndexValue("overallPreviewImage").sourceIndexValue("height").as_int64() == 480 &&
        metadata.sourceIndexValue("overallDepth").sourceIndexValue("contentIndex").as_int64() == 2 &&
        metadata.sourceIndexValue("overallDepth").sourceIndexValue("encoding").as_string().view() == "float32-le" &&
        metadata.sourceIndexValue("overallDepth").sourceIndexValue("layout").as_string().view() == "row-major-top-left" &&
        metadata.sourceIndexValue("overallDepth").sourceIndexValue("valueSpace").as_string().view() == "composed-depth" &&
        entry.sourceIndexValue("previewImage").sourceIndexValue("contentIndex").as_int64() == 3 &&
        layerEntry.sourceIndexValue("colorImage").sourceIndexValue("contentIndex").as_int64() == 4 &&
        layerEntry.sourceIndexValue("depthImage").sourceIndexValue("contentIndex").as_int64() == 5 &&
        layerEntry.sourceIndexValue("colorStatistics").sourceIndexValue("averageRgba8").sourceArrayValue()[0].as_int64() == 128 &&
        layerEntry.sourceIndexValue("colorStatistics").sourceIndexValue("averageRgba8").sourceArrayValue()[2].as_int64() == 128,
        "GetPsdDepthDialogPartData must expose target depth geometry and image/content mappings");
    auto resource = part->result.sourceIndexValue("content").sourceArrayValue()[2];
    check(objectMember(resource, "type", 4880).as_string().view() == "resource" &&
        objectMember(objectMember(resource, "resource", 4881), "mimeType", 4881).as_string().view() == "application/vnd.nijigenerate.depth-map.f32le" &&
        objectMember(objectMember(resource, "resource", 4883), "blob", 4883).as_string().size() > 0,
        "GetPsdDepthDialogPartData must expose exact overall float32 depth data");
    auto bytes = nijigenerate::compat::phobos::ngDecodeBase64(
        objectMember(objectMember(resource, "resource", 4887), "blob", 4887).as_string());
    check(bytes.size() == 640 * 480 * sizeof(float),
        "GetPsdDepthDialogPartData overall depth data must contain one float32 value per pixel");
    auto previewContent = part->result.sourceIndexValue("content").sourceArrayValue().slice(1, 2);
    for (auto image : previewContent) {
        check(objectMember(image, "type", 4891).as_string().view() == "image" &&
            objectMember(image, "mimeType", 4892).as_string().view() == "image/png" && objectMember(image, "data", 4893).as_string().size() > 0,
            "GetPsdDepthDialogPartData overall preview content must contain encoded PNG data");
    }
    auto imageContent = part->result.sourceIndexValue("content").sourceArrayValue();
    for (auto image : imageContent.slice(3, imageContent.size())) {
        check(objectMember(image, "type", 4897).as_string().view() == "image" &&
            objectMember(image, "mimeType", 4898).as_string().view() == "image/png" && objectMember(image, "data", 4899).as_string().size() > 0,
            "GetPsdDepthDialogPartData image content must contain encoded PNG data");
    }
    check(incActionHistory().size() == 0, "GetPsdDepthDialogPartData must not mutate dialog history");
    dialog->clearDialogPartDataForRegression();
    const auto trip = [&]<PsdDepthDialogCommand Id>(auto changed, auto restored, RetainedString name, auto&&... args) {
        auto result = ngCmd<Id>(ctx, std::forward<decltype(args)>(args)...);
        nijigenerate::compat::SharedDelegate<bool()> changeDelegate(std::move(changed));
        nijigenerate::compat::SharedDelegate<bool()> restoreDelegate(std::move(restored));
        requirePsdDepthDialogCommandRoundTrip(ctx, result, changeDelegate, restoreDelegate, name);
    };
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogColorSource>(
        [&] { return dialog->captureDialogSettingsState().settings.colorSourcePath == text("command-color-source.png"); },
        [&] { return dialog->captureDialogSettingsState().settings.colorSourcePath.size() == 0; },
        text("SetPsdDepthDialogColorSource"), text("command-color-source.png"));
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogInvert>(
        [&] { return dialog->captureDialogSettingsState().settings.invert; },
        [&] { return !dialog->captureDialogSettingsState().settings.invert; }, text("SetPsdDepthDialogInvert"), true);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogBackDepth>(
        [&] { return near(dialog->captureDialogSettingsState().settings.backDepth, -2.0f); },
        [&] { return near(dialog->captureDialogSettingsState().settings.backDepth, -1.0f); }, text("SetPsdDepthDialogBackDepth"), -2.0f);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogFrontDepth>(
        [&] { return near(dialog->captureDialogSettingsState().settings.frontDepth, 2.0f); },
        [&] { return near(dialog->captureDialogSettingsState().settings.frontDepth, 1.0f); }, text("SetPsdDepthDialogFrontDepth"), 2.0f);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogDepthScale>(
        [&] { return near(dialog->captureDialogSettingsState().settings.depthScale, 2.0f); },
        [&] { return near(dialog->captureDialogSettingsState().settings.depthScale, 1.0f); }, text("SetPsdDepthDialogDepthScale"), 2.0f);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogChannel>(
        [&] { return dialog->captureDialogSettingsState().settings.channel == PsdDepthChannel::R; },
        [&] { return dialog->captureDialogSettingsState().settings.channel == PsdDepthChannel::AverageRGB; },
        text("SetPsdDepthDialogChannel"), PsdDepthChannel::R);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogSampling>(
        [&] { return dialog->captureDialogSettingsState().settings.convolution == PsdDepthConvolution::Nearest; },
        [&] { return dialog->captureDialogSettingsState().settings.convolution == PsdDepthConvolution::Gaussian3x3; },
        text("SetPsdDepthDialogSampling"), PsdDepthConvolution::Nearest);
    auto unsupported = dialog->captureDialogSettingsState();
    unsupported.settings.convolution = PsdDepthConvolution::MedianCustom;
    unsupported.settings.useGpuComposition = true;
    dialog->applyDialogSettingsState(unsupported);
    auto normalized = dialog->captureDialogSettingsState();
    check(normalized.settings.useGpuComposition && normalized.settings.convolution == PsdDepthConvolution::Median3x3,
        "PSD depth dialog GPU mode must normalize unsupported Median Custom sampling to Median 3x3");
    unsupported.settings.convolution = PsdDepthConvolution::Nearest;
    unsupported.settings.useGpuComposition = false;
    dialog->applyDialogSettingsState(unsupported);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogCustomRadius>(
        [&] { return dialog->captureDialogSettingsState().settings.customRadius == 5; },
        [&] { return dialog->captureDialogSettingsState().settings.customRadius == 3; }, text("SetPsdDepthDialogCustomRadius"), 5);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogAlphaThreshold>(
        [&] { return near(dialog->captureDialogSettingsState().settings.alphaThreshold, 0.25f); },
        [&] { return near(dialog->captureDialogSettingsState().settings.alphaThreshold, 0.01f); }, text("SetPsdDepthDialogAlphaThreshold"), 0.25f);
    auto historyBeforeInvalid = incActionHistory().size();
    auto invalidScale = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogDepthScale>(ctx, -0.1f);
    auto invalidRadius = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogCustomRadius>(ctx, 65);
    auto invalidAlpha = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogAlphaThreshold>(ctx, 1.1f);
    auto afterInvalid = dialog->captureDialogSettingsState().settings;
    check(!invalidScale->succeeded && !invalidRadius->succeeded && !invalidAlpha->succeeded &&
        near(afterInvalid.depthScale, 2.0f) && afterInvalid.customRadius == 5 && near(afterInvalid.alphaThreshold, 0.25f) &&
        incActionHistory().size() == historyBeforeInvalid,
        "invalid PSD sampling settings must be rejected before changing dialog state or history");
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogMissingPolicy>(
        [&] { return dialog->captureDialogSettingsState().settings.missingPolicy == PsdDepthMissingPolicy::SetZero; },
        [&] { return dialog->captureDialogSettingsState().settings.missingPolicy == PsdDepthMissingPolicy::KeepExisting; },
        text("SetPsdDepthDialogMissingPolicy"), PsdDepthMissingPolicy::SetZero);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogContourRepair>(
        [&] { return dialog->captureDialogSettingsState().settings.repairContourBand; },
        [&] { return !dialog->captureDialogSettingsState().settings.repairContourBand; }, text("SetPsdDepthDialogContourRepair"), true);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogSurfaceSmoothing>(
        [&] { return dialog->captureDialogSettingsState().settings.smoothWavySurface; },
        [&] { return !dialog->captureDialogSettingsState().settings.smoothWavySurface; }, text("SetPsdDepthDialogSurfaceSmoothing"), true);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogGpuComposition>(
        [&] { return dialog->captureDialogSettingsState().settings.useGpuComposition; },
        [&] { return !dialog->captureDialogSettingsState().settings.useGpuComposition; }, text("SetPsdDepthDialogGpuComposition"), true);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogDirectGridMatch>(
        [&] { return !dialog->captureDialogSettingsState().settings.matchDirectGridName; },
        [&] { return dialog->captureDialogSettingsState().settings.matchDirectGridName; }, text("SetPsdDepthDialogDirectGridMatch"), false);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogProblemFilter>(
        [&] { return dialog->captureDialogSettingsState().onlyProblemLayers; },
        [&] { return !dialog->captureDialogSettingsState().onlyProblemLayers; }, text("SetPsdDepthDialogProblemFilter"), true);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingIgnored>(
        [&] { auto state = dialog->captureDialogSettingsState(); return state.settings.ignoredLayerPaths.contains(initialLayer.layerPath.view()); },
        [&] { auto state = dialog->captureDialogSettingsState(); return !state.settings.ignoredLayerPaths.contains(initialLayer.layerPath.view()); },
        text("SetPsdDepthDialogLayerMappingIgnored"), initialLayer.layerPath);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingAuto>(
        [&] { auto state = dialog->captureDialogSettingsState(); return !state.settings.ignoredLayerPaths.contains(initialLayer.layerPath.view()); },
        [&] { auto state = dialog->captureDialogSettingsState(); return state.settings.ignoredLayerPaths.contains(initialLayer.layerPath.view()); },
        text("SetPsdDepthDialogLayerMappingAuto"), initialLayer.layerPath);
    const auto uuidText = [](std::uint32_t value) {
        char buffer[10];
        auto first = sizeof(buffer);
        do { const auto digit = value % 10; value /= 10; buffer[--first] = static_cast<char>('0' + digit); } while (value);
        auto output = allocateSourceStringBytes(sizeof(buffer) - first);
        std::memcpy(output.data(), buffer + first, output.size());
        return RetainedString::retainedView(output.retentionOwner(), output.data(), output.size());
    };
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingTarget>(
        [&] {
            auto state = dialog->captureDialogSettingsState();
            auto target = state.settings.layerTargetGridUuidOverrides.find(initialLayer.layerPath.view());
            return target && *target == uuidText(grid->uuid);
        },
        [&] { auto state = dialog->captureDialogSettingsState(); return !state.settings.layerTargetGridUuidOverrides.contains(initialLayer.layerPath.view()); },
        text("SetPsdDepthDialogLayerMappingTarget"), initialLayer.layerPath, std::static_pointer_cast<Node>(grid));
    auto historyBeforeUnknown = incActionHistory().size();
    auto unknownIgnored = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingIgnored>(ctx, text("/unknown-layer"));
    auto unknownAuto = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingAuto>(ctx, text("/unknown-layer"));
    auto unknownTarget = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingTarget>(ctx, text("/unknown-layer"), std::static_pointer_cast<Node>(grid));
    auto unknownState = dialog->captureDialogSettingsState();
    check(!unknownIgnored->succeeded && !unknownAuto->succeeded && !unknownTarget->succeeded &&
        !unknownState.settings.ignoredLayerPaths.contains("/unknown-layer") &&
        !unknownState.settings.layerTargetGridUuidOverrides.contains("/unknown-layer") && incActionHistory().size() == historyBeforeUnknown,
        "PSD mapping commands must reject unknown layer paths without changing state or history");
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogTargetEnabled>(
        [&] { auto state = dialog->captureDialogSettingsState(); auto key = uuidText(grid->uuid); return state.settings.disabledGridUuids.contains(key.view()); },
        [&] { auto state = dialog->captureDialogSettingsState(); auto key = uuidText(grid->uuid); return !state.settings.disabledGridUuids.contains(key.view()); },
        text("SetPsdDepthDialogTargetEnabled"), false);
    const auto layerState = [&] {
        PsdDepthDialogLayerState state;
        check(dialog->captureDialogLayerState(initialLayer.layerPath, grid->uuid, state), "all-command regression layer must remain available");
        return state;
    };
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogLayerEnabled>(
        [&] { return !layerState().enabled && !layerState().visible; }, [&] { return layerState().enabled && layerState().visible; },
        text("SetPsdDepthDialogLayerEnabled"), initialLayer.layerPath, false);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogLayerVisible>(
        [&] { return !layerState().enabled && layerState().visible; }, [&] { return !layerState().enabled && !layerState().visible; },
        text("SetPsdDepthDialogLayerVisible"), initialLayer.layerPath, true);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthEnabled>(
        [&] { return !layerState().depthEnabled; }, [&] { return layerState().depthEnabled; },
        text("SetPsdDepthDialogLayerDepthEnabled"), initialLayer.layerPath, false);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthInverted>(
        [&] { return layerState().invert; }, [&] { return !layerState().invert; },
        text("SetPsdDepthDialogLayerDepthInverted"), initialLayer.layerPath, true);
    trip.template operator()<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthScale>(
        [&] { return near(layerState().depthScale, 2.5f); }, [&] { return near(layerState().depthScale, 1.0f); },
        text("SetPsdDepthDialogLayerDepthScale"), initialLayer.layerPath, 2.5f);
    auto offset = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthOffset>(ctx, initialLayer.layerPath, -0.75f);
    check(offset->succeeded && near(layerState().depthOffset, -0.75f), "SetPsdDepthDialogLayerDepthOffset must update the dialog state");
    check(ngCmd<EditCommand::Undo>(ctx)->succeeded, "SetPsdDepthDialogLayerDepthOffset must support Undo");
    auto offsetUndo = layerState();
    const bool offsetRestored = near(offsetUndo.depthOffset, 0.0f);
    const std::array<nijigenerate::core::translation::TypedFormatArgument, 1> offsetArguments{
        nijigenerate::core::translation::ngFormatArgument(offsetUndo.depthOffset)};
    auto offsetMessage = nijigenerate::core::translation::ngFormatTyped(
        "SetPsdDepthDialogLayerDepthOffset Undo must restore the previous offset; actual=%s", offsetArguments);
    require(offsetRestored, nijigenerate::platform::detail::ngPathAllocation([&] { return RetainedString(std::move(offsetMessage)); }));
    check(ngCmd<EditCommand::Redo>(ctx)->succeeded && near(layerState().depthOffset, -0.75f),
        "SetPsdDepthDialogLayerDepthOffset Redo must restore the changed offset");
    trip.template operator()<PsdDepthDialogCommand::ResetPsdDepthDialogLayerDepthTransform>(
        [&] { return !layerState().invert && near(layerState().depthScale, 1.0f) && near(layerState().depthOffset, 0.0f); },
        [&] { return layerState().invert && near(layerState().depthScale, 2.5f) && near(layerState().depthOffset, -0.75f); },
        text("ResetPsdDepthDialogLayerDepthTransform"), initialLayer.layerPath);
    Slice<std::uint8_t> gapDepth{
        80, 80, 80, 80, 80, 80, 10, 0, 30, 80, 80, 80, 20, 80, 80,
        80, 80, 0, 80, 80, 80, 80, 80, 80, 80};
    Slice<std::uint8_t> depthRgba;
    Slice<std::uint8_t> maskRgba;
    for (auto depth : gapDepth) {
        depthRgba.appendRange(Slice<std::uint8_t>{depth, depth, depth, 255});
        maskRgba.appendRange(Slice<std::uint8_t>{255, 0, 0, 255});
    }
    check(dialog->setDialogLayerPixelsForRegression(5, 5, depthRgba, maskRgba), "alpha-depth gap regression fixture must compose");
    auto beforeFill = dialog->captureDialogLayerPixels();
    auto fill = ngCmd<PsdDepthDialogCommand::FillPsdDepthDialogAlphaDepthGaps>(ctx);
    auto afterFill = dialog->captureDialogLayerPixels();
    check(fill->succeeded && !PSDDepthMapWindow::dialogLayerPixelsEqual(beforeFill, afterFill),
        "FillPsdDepthDialogAlphaDepthGaps must change the detected gap pixels");
    check(ngCmd<EditCommand::Undo>(ctx)->succeeded && PSDDepthMapWindow::dialogLayerPixelsEqual(beforeFill, dialog->captureDialogLayerPixels()),
        "FillPsdDepthDialogAlphaDepthGaps Undo must restore source pixels");
    check(ngCmd<EditCommand::Redo>(ctx)->succeeded && PSDDepthMapWindow::dialogLayerPixelsEqual(afterFill, dialog->captureDialogLayerPixels()),
        "FillPsdDepthDialogAlphaDepthGaps Redo must restore filled pixels");
    check(dialog->replayDialogCleanupAfterRebuildForRegression(beforeFill[0].depthRgba) &&
        PSDDepthMapWindow::dialogLayerPixelsEqual(afterFill, dialog->captureDialogLayerPixels()),
        "PSD dialog preview rebuilds must replay alpha-depth gap cleanup on rebuilt source pixels");
    check(ngCmd<EditCommand::Undo>(ctx)->succeeded && PSDDepthMapWindow::dialogLayerPixelsEqual(beforeFill, dialog->captureDialogLayerPixels()),
        "alpha-depth gap regression setup must restore the unfilled pixels");
    dialog->failNextPreviewCompositionForRegressionTest();
    auto failedFill = ngCmd<PsdDepthDialogCommand::FillPsdDepthDialogAlphaDepthGaps>(ctx);
    check(!failedFill->succeeded && PSDDepthMapWindow::dialogLayerPixelsEqual(beforeFill, dialog->captureDialogLayerPixels()),
        "failed alpha-depth gap recomposition must roll back every pixel mutation");
    check(ngCmd<EditCommand::Redo>(ctx)->succeeded && PSDDepthMapWindow::dialogLayerPixelsEqual(afterFill, dialog->captureDialogLayerPixels()),
        "failed alpha-depth gap fill must not consume or replace the existing Redo entry");
    auto cancel = ngCmd<PsdDepthDialogCommand::CancelPsdDepthDialog>(ctx);
    check(cancel->succeeded && !dialog->dialogCommandsAvailable(), "CancelPsdDepthDialog must close the active command context");
    nijigenerate::commands::depth::psd_dialog::commands.forEachRetained([&](PsdDepthDialogCommand, std::shared_ptr<nijigenerate::commands::Command> command) {
        const bool unavailable = !ngRequireObject(command)->runnable(std::allocate_shared<Context>(SourceAllocator<Context>{}));
        require(unavailable, concatenateSourceStrings({text("every PSD dialog command must be unavailable after Cancel: "), command->label()}));
    });
    auto applyDialog = ngCreateWindow<PSDDepthMapWindow>(text("psd-depth-dialog-apply-command-regression.png"));
    applyDialog->beginDialogCommandSessionForRegression();
    check(applyDialog->prepareDialogApplyForRegression(), "Apply command regression fixture must prepare an empty composed import");
    auto applyCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    auto apply = ngCmd<PsdDepthDialogCommand::ApplyPsdDepthDialog>(applyCtx);
    check(apply->succeeded && !applyDialog->dialogCommandsAvailable(), "ApplyPsdDepthDialog must apply and close the active command context");
    check(!ngCmd<PsdDepthDialogCommand::ApplyPsdDepthDialog>(applyCtx)->succeeded, "ApplyPsdDepthDialog must reject calls after the dialog closes");
    auto failedDialog = ngCreateWindow<PSDDepthMapWindow>(text("psd-depth-dialog-failed-apply-regression.png"));
    failedDialog->beginDialogCommandSessionForRegression();
    failedDialog->setDialogLayerStateForRegression(initialLayer);
    auto failedCtx = nijigenerate::core::shortcut::ngBuildExecutionContext();
    check(ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogLayerVisible>(failedCtx, initialLayer.layerPath, false)->succeeded,
        "failed Apply regression must create dialog-local Undo history");
    check(failedDialog->setDialogPartDataForRegression(grid, 2, 1, colors, depths, previews, Slice<float>{-0.5f}),
        "failed Apply regression fixture must install an invalid vertex-depth count");
    failedDialog->useCurrentPreviewForDialogApplyRegression();
    auto failedApply = ngCmd<PsdDepthDialogCommand::ApplyPsdDepthDialog>(failedCtx);
    check(!failedApply->succeeded && failedDialog->dialogCommandsAvailable() && failedDialog->dialogActionScopeActiveForRegression(),
        "failed Apply must keep the dialog and its isolated action stack active");
    check(ngCmd<EditCommand::Undo>(failedCtx)->succeeded, "failed Apply must preserve the dialog-local Undo history");
    PsdDepthDialogLayerState failedLayer;
    check(failedDialog->captureDialogLayerState(initialLayer.layerPath, grid->uuid, failedLayer) && failedLayer.visible,
        "Undo after failed Apply must restore the preceding dialog edit");
    failedDialog->endDialogCommandSessionForRegression();
    auto firstDialog = ngCreateWindow<PSDDepthMapWindow>(text("first-overlapping-psd-depth-dialog.png"));
    auto secondDialog = ngCreateWindow<PSDDepthMapWindow>(text("second-overlapping-psd-depth-dialog.png"));
    firstDialog->beginDialogCommandSessionForRegression();
    secondDialog->beginDialogCommandSessionForRegression();
    check(firstDialog->dialogCommandScopeActiveForRegression() && secondDialog->dialogCommandScopeActiveForRegression() &&
        firstDialog->dialogActionScopeActiveForRegression() && secondDialog->dialogActionScopeActiveForRegression() && ngActionStackLevel() == 2,
        "overlapping PSD depth dialogs must each own command and action-scope registrations");
    firstDialog->endDialogCommandSessionForRegression();
    check(!firstDialog->dialogCommandScopeActiveForRegression() && secondDialog->dialogCommandScopeActiveForRegression() &&
        !secondDialog->dialogActionScopeActiveForRegression() && ngActionStackLevel() == 0,
        "closing an outer PSD depth dialog must invalidate action scopes unwound above it");
    check(secondDialog->dialogCommandsAvailable() && secondDialog->dialogActionScopeActiveForRegression() && ngActionStackLevel() == 1,
        "the remaining active PSD depth dialog must recreate its invalidated isolated action stack");
    secondDialog->endDialogCommandSessionForRegression();
    check(!secondDialog->dialogCommandScopeActiveForRegression() && !secondDialog->dialogActionScopeActiveForRegression() && ngActionStackLevel() == 0,
        "closing the active PSD depth dialog must close its command and action scopes");
}
