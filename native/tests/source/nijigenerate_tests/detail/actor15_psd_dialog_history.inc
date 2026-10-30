// Complete existing regression.d4627-4774 only.
void testPsdDepthDialogCommandsUndoRedo() {
    using Text = nijigenerate::compat::RetainedString;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::ngCmd;
    using nijigenerate::commands::ngRunCommand;
    using nijigenerate::commands::ngCommandAllowedInCurrentContext;
    using nijigenerate::commands::node::dynamic::ensureAddNodeCommand;
    using nijigenerate::commands::depth::psd_dialog::PsdDepthDialogCommand;
    using nijigenerate::windows::psddepthmap::PSDDepthMapWindow;
    using nijigenerate::windows::psddepthmap::PsdDepthDialogLayerState;
    namespace edits = nijigenerate::commands::puppet::edit;
    namespace actions = nijigenerate::core::actionstack;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto isTrue = [](Json value) { return value.is_bool() && value.as_bool(); };
    const auto objectMember = [](Json value, std::string_view key, std::uint32_t line) -> Json {
        const auto& object = value.sourceObjectRef();
        const auto* member = object.if_contains(key);
        if (!member) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
            Text::fromStaticLiteral("source/nijigenerate_tests/regression.d"), line);
        return *member;
    };
    resetCase();
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    auto dialog = nijigenerate::windows::base::ngCreateWindowDeferred<PSDDepthMapWindow>([] {
        return Text::fromStaticLiteral("psd-depth-dialog-command-regression.png");
    });
    auto normalScope = currentCommandScope(ctx)->result;
    check(objectMember(normalScope.sourceIndexValue("scope"), "id", 4633).sourceStringValue().view() == "normal" &&
        normalScope.sourceIndexValue("scopeStack").sourceArrayValue().size() == 1 &&
        objectMember(normalScope.sourceIndexValue("lifecycle"), "finish", 4635).sourceArrayValue().empty() &&
        objectMember(normalScope.sourceIndexValue("lifecycle"), "cancel", 4636).sourceArrayValue().empty(),
        "normal scope query must report no dialog lifecycle commands");
    auto outsideResult = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogInvert>(ctx, true);
    check(!outsideResult->succeeded, "PSD depth dialog commands must reject calls while the dialog is not displayed");
    check(actions::ngActionStackLevel() == 0 && actions::incActionHistory().empty(),
        "rejected PSD depth dialog commands must not open or mutate action history");
    dialog->beginDialogCommandSessionForRegression();
    check(dialog->dialogCommandsAvailable() && actions::ngActionStackLevel() == 1,
        "displayed PSD depth dialog must open one nested action stack");
    check(!ngCmd<edits::EditCommand::ShowSettingsWindow>(ctx)->succeeded,
        "non-dialog commands must be unavailable while the PSD depth dialog is displayed");
    auto directAddContext = std::allocate_shared<Context>(SourceAllocator<Context>{});
    directAddContext->puppet(nijigenerate::project::incActivePuppet());
    directAddContext->nodes(Context::Nodes{nijigenerate::project::incActivePuppet()->root});
    auto childCountBeforeDirectAdd = nijigenerate::project::incActivePuppet()->root->children.size();
    check(!ngRunCommand(ensureAddNodeCommand(text("Node")), directAddContext)->succeeded &&
        nijigenerate::project::incActivePuppet()->root->children.size() == childCountBeforeDirectAdd,
        "scope-aware direct command dispatch must reject node-panel mutations in the PSD depth dialog");
    auto dialogCommands = nijigenerate::commands::viewport::palette::filterCommands(text(""));
    check(dialogCommands.size() == format::FormatEnumTraits<PsdDepthDialogCommand>::members.size() + 3,
        "PSD depth dialog context must expose only its dialog commands, scope query, Undo, and Redo");
    for (auto command : dialogCommands) {
        check(ngCommandAllowedInCurrentContext(command),
            "command lists must not expose commands outside the active PSD depth dialog context");
    }
    auto dialogScope = currentCommandScope(ctx)->result;
    check(objectMember(dialogScope.sourceIndexValue("scope"), "id", 4666).sourceStringValue().view() == "psd-depth-dialog" &&
        dialogScope.sourceIndexValue("scopeStack").sourceArrayValue().size() == 2 &&
        objectMember(dialogScope.sourceIndexValue("lifecycle"), "finish", 4668).sourceArrayValue().size() == 1 &&
        objectMember(objectMember(dialogScope.sourceIndexValue("lifecycle"), "finish", 4669).sourceArrayValue()[0],
            "tool", 4669).sourceStringValue().view() == "PsdDepthDialogCommand_ApplyPsdDepthDialog" &&
        isTrue(objectMember(objectMember(dialogScope.sourceIndexValue("lifecycle"), "finish", 4671).sourceArrayValue()[0], "available", 4671)) &&
        objectMember(dialogScope.sourceIndexValue("lifecycle"), "cancel", 4672).sourceArrayValue().size() == 1 &&
        objectMember(objectMember(dialogScope.sourceIndexValue("lifecycle"), "cancel", 4673).sourceArrayValue()[0],
            "tool", 4673).sourceStringValue().view() == "PsdDepthDialogCommand_CancelPsdDepthDialog" &&
        isTrue(objectMember(objectMember(dialogScope.sourceIndexValue("lifecycle"), "cancel", 4675).sourceArrayValue()[0], "available", 4675)),
        "PSD dialog scope query must report its Apply and Cancel transition commands");
    check(scopeCommandListContains(dialogScope.sourceIndexValue("availableCommands").sourceArrayValue(),
            "PsdDepthDialogCommand_SetPsdDepthDialogInvert") &&
        scopeCommandListContains(dialogScope.sourceIndexValue("availableCommands").sourceArrayValue(),
            "CommandScopeCommand_GetCurrentCommandScope") &&
        !scopeCommandListContains(dialogScope.sourceIndexValue("availableCommands").sourceArrayValue(), "Node_Add_Node"),
        "scope query must list allowed MCP tools and exclude normal-scope tools");
    auto updateSettings = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogInvert>(ctx, true);
    check(updateSettings->succeeded && dialog->captureDialogSettingsState().settings.invert,
        "PSD depth dialog settings command must update the in-progress dialog state");
    check(actions::incActionHistory().size() == 1, "PSD depth dialog settings command must push to the nested action stack");
    check(ngCmd<edits::EditCommand::Undo>(ctx)->succeeded, "dialog context must allow Undo");
    check(!dialog->captureDialogSettingsState().settings.invert,
        "undo in the PSD depth dialog stack must restore the previous settings");
    check(ngCmd<edits::EditCommand::Redo>(ctx)->succeeded, "dialog context must allow Redo");
    check(dialog->captureDialogSettingsState().settings.invert,
        "redo in the PSD depth dialog stack must restore the changed settings");
    auto contextTarget = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    PsdDepthDialogLayerState initialLayer;
    initialLayer.layerPath = text("/dialog-layer");
    initialLayer.targetGridUuid = contextTarget->uuid;
    initialLayer.visible = true;
    initialLayer.enabled = true;
    initialLayer.depthEnabled = true;
    initialLayer.depthOffset = 0.0f;
    initialLayer.depthScale = 1.0f;
    dialog->setDialogLayerStateForRegression(initialLayer);
    auto updateMapping = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingIgnored>(ctx, text("/dialog-layer"));
    check(updateMapping->succeeded && dialog->captureDialogSettingsState().settings.ignoredLayerPaths.contains("/dialog-layer"),
        "Source / Mapping ignore command must update the selected source layer mapping");
    check(actions::incActionHistory().size() == 2, "Source / Mapping command must push to the nested action stack");
    check(ngCmd<edits::EditCommand::Undo>(ctx)->succeeded &&
        !dialog->captureDialogSettingsState().settings.ignoredLayerPaths.contains("/dialog-layer"),
        "Undo must restore the previous Source / Mapping state");
    check(ngCmd<edits::EditCommand::Redo>(ctx)->succeeded &&
        dialog->captureDialogSettingsState().settings.ignoredLayerPaths.contains("/dialog-layer"),
        "Redo must restore the changed Source / Mapping state");
    auto contextWithoutLayer = std::allocate_shared<Context>(SourceAllocator<Context>{});
    check(!ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthScale>(contextWithoutLayer, text("/dialog-layer"), 2.5f)->succeeded,
        "PSD depth layer command must require a selected layer in Context");
    ctx = nijigenerate::core::shortcut::ngBuildExecutionContext();
    check(ctx->hasNodes() && ctx->nodes().size() == 1 && ctx->nodes()[0] == contextTarget,
        "dialog command Context must use the layer selected in the dialog instead of the normal node selection");
    auto updateLayer = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthScale>(ctx, text("/dialog-layer"), 2.5f);
    PsdDepthDialogLayerState capturedLayer;
    const bool layerUpdated = updateLayer->succeeded &&
        dialog->captureDialogLayerState(text("/dialog-layer"), initialLayer.targetGridUuid, capturedLayer) &&
        capturedLayer.depthScale == 2.5f && capturedLayer.depthOffset == 0.0f;
    auto layerMessage = concatenateSourceStrings({text("PSD depth dialog layer command must update layer state: "), updateLayer->message});
    require(layerUpdated, layerMessage);
    check(actions::incActionHistory().size() == 3, "PSD depth dialog layer command must add a nested history action");
    check(ngCmd<edits::EditCommand::Undo>(ctx)->succeeded, "dialog context must allow layer Undo");
    const bool layerUndone = dialog->captureDialogLayerState(text("/dialog-layer"), initialLayer.targetGridUuid, capturedLayer) &&
        capturedLayer.depthScale == 1.0f && capturedLayer.depthOffset == 0.0f;
    const std::array<format::TypedFormatArgument, 2> undoArguments{
        format::ngFormatArgument(capturedLayer.depthScale), format::ngFormatArgument(capturedLayer.depthOffset)};
    auto rendered = format::ngFormatTyped("undo in the PSD depth dialog stack must restore layer state: scale=%s offset=%s", undoArguments);
    auto undoMessage = nijigenerate::platform::detail::ngPathAllocation([&] { return Text(std::move(rendered)); });
    require(layerUndone, undoMessage);
    check(ngCmd<edits::EditCommand::Redo>(ctx)->succeeded, "dialog context must allow layer Redo");
    check(dialog->captureDialogLayerState(text("/dialog-layer"), initialLayer.targetGridUuid, capturedLayer) &&
        capturedLayer.depthScale == 2.5f && capturedLayer.depthOffset == 0.0f,
        "redo in the PSD depth dialog stack must restore changed layer state");
    auto updateVisibility = ngCmd<PsdDepthDialogCommand::SetPsdDepthDialogLayerVisible>(ctx, text("/dialog-layer"), false);
    check(updateVisibility->succeeded && dialog->captureDialogLayerState(text("/dialog-layer"), initialLayer.targetGridUuid, capturedLayer) &&
        capturedLayer.enabled && !capturedLayer.visible, "Show must be commandized independently from the layer Enable state");
    auto cancelDialog = ngCmd<PsdDepthDialogCommand::CancelPsdDepthDialog>(ctx);
    check(cancelDialog->succeeded, "Cancel PSD depth dialog command must run while the dialog is displayed");
    check(!dialog->dialogCommandsAvailable() && actions::ngActionStackLevel() == 0,
        "closing the PSD depth dialog must discard its nested action stack");
    check(actions::incActionHistory().empty(), "PSD depth dialog edit history must not leak into the root action stack");
    check(!ngCmd<PsdDepthDialogCommand::CancelPsdDepthDialog>(ctx)->succeeded,
        "PSD depth dialog commands must reject calls after the dialog closes");
    const auto found = edits::commands.find(edits::EditCommand::ShowSettingsWindow);
    if (found == edits::commands.end()) throw nijigenerate::compat::ngStaticError<nijigenerate::compat::RangeError>(
        text("source/nijigenerate_tests/regression.d"), 4770);
    check(ngCommandAllowedInCurrentContext(found->second), "closing the PSD depth dialog must restore the normal command set");
    check(objectMember(currentCommandScope(ctx)->result.sourceIndexValue("scope"), "id", 4772).sourceStringValue().view() == "normal",
        "scope query must return to normal after the dialog closes");
}
