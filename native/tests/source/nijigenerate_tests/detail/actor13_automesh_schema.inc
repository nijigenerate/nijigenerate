// Existing regression.d4612-4618,18736-18823; include inside regression::detail after helpers.
std::shared_ptr<nijigenerate::commands::ExCommandResult<nijigenerate::core::settings::SettingsJsonValue>>
currentCommandScope(std::shared_ptr<nijigenerate::commands::Context> ctx) {
    auto result = std::dynamic_pointer_cast<nijigenerate::commands::ExCommandResult<nijigenerate::core::settings::SettingsJsonValue>>(
        nijigenerate::commands::ngCmd<nijigenerate::commands::CommandScopeCommand::GetCurrentCommandScope>(ctx));
    require(result != nullptr && result->succeeded,
        nijigenerate::compat::RetainedString::fromStaticLiteral("current command scope query must return a JSON payload"));
    return result;
}

void testAutoMeshSchemaValuesPresetsAndActiveProcessor() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::ngRequireObject;
    using nijigenerate::commands::Context;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::viewport::vertex::automesh::AutoMeshProcessor;
    using nijigenerate::viewport::vertex::automesh::meta::IAutoMeshReflect;
    using nijigenerate::commands::automesh::dynamic::ngEnsureApplyAutoMeshCommand;
    using namespace nijigenerate::commands::automesh::config;
    using namespace nijigenerate::core::actionstack;
    using namespace nijigenerate::project;
    using namespace nijigenerate::viewport::vertex;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return RetainedString::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    struct ProcessorIdArgument {
        std::shared_ptr<AutoMeshProcessor> processor;
        operator RetainedString() && { return ngRequireObject(processor)->procId(); }
    };
    resetCase();
    incSetEditMode(EditMode::ModelEdit);
    auto processors = ngAutoMeshProcessors();
    check(processors.size() >= 4, "AutoMesh processor registry should include standard processors");
    bool sawPresetProcessor = false;
    for (auto processor : processors) {
        const auto processorCheck = [&]<std::size_t N>(bool condition, const char (&prefix)[N]) {
            require(condition, concatenateSourceStrings({RetainedString::fromStaticLiteral(prefix),
                ngRequireObject(processor)->procId()}));
        };
        check(ngRequireObject(processor)->procId().size() > 0, "AutoMesh processor should expose procId");
        check(processor->displayName().size() > 0, "AutoMesh processor should expose displayName");
        check(processor->icon().size() > 0, "AutoMesh processor should expose icon");
        auto applyCommand = ngEnsureApplyAutoMeshCommand(RetainedStringSlice(processor->procId()));
        processorCheck(applyCommand != nullptr, "AutoMesh Apply command should be registered: ");
        processorCheck(nijigenerate::commands::ngCommandAllowedInCurrentContext(applyCommand),
            "AutoMesh Apply command should be allowed in ModelEdit: ");
        auto reflect = std::dynamic_pointer_cast<IAutoMeshReflect>(processor);
        processorCheck(reflect != nullptr, "standard AutoMesh processor should be reflectable: ");
        auto schema = ngRequireObject(reflect)->schema();
        processorCheck(schema.view().find("\"type\"") != std::string_view::npos, "AutoMesh schema should include type: ");
        processorCheck(schema.view().find("\"Simple\"") != std::string_view::npos &&
            schema.view().find("\"Advanced\"") != std::string_view::npos, "AutoMesh schema should include config levels: ");
        processorCheck(schema.view().find("\"presets\"") != std::string_view::npos, "AutoMesh schema should include presets array: ");
        auto simple = reflect->values(text("Simple"));
        auto advanced = reflect->values(text("Advanced"));
        processorCheck(simple.size() > 0, "AutoMesh simple values should serialize: ");
        processorCheck(advanced.size() > 0, "AutoMesh advanced values should serialize: ");
        processorCheck(ngRequireObject(std::allocate_shared<AutoMeshGetSchemaCommand>(SourceAllocator<AutoMeshGetSchemaCommand>{},
            ProcessorIdArgument{processor})->run(std::allocate_shared<Context>(SourceAllocator<Context>{})))->succeeded,
            "AutoMeshGetSchemaCommand should succeed: ");
        processorCheck(ngRequireObject(std::allocate_shared<AutoMeshGetValuesCommand>(SourceAllocator<AutoMeshGetValuesCommand>{},
            ProcessorIdArgument{processor}, text("Simple"))->run(std::allocate_shared<Context>(SourceAllocator<Context>{})))->succeeded,
            "AutoMeshGetValuesCommand should succeed: ");
        if (schema.view().find("\"Normal parts\"") != std::string_view::npos) {
            sawPresetProcessor = true;
            if (schema.view().find("\"Detailed mesh\"") != std::string_view::npos) {
                processorCheck(ngRequireObject(std::allocate_shared<AutoMeshSetPresetCommand>(SourceAllocator<AutoMeshSetPresetCommand>{},
                    ProcessorIdArgument{processor}, text("Detailed mesh"))->run(std::allocate_shared<Context>(SourceAllocator<Context>{})))->succeeded,
                    "AutoMesh preset test should prepare a non-Normal preset state: ");
                incActionClearHistory();
            }
            auto beforeSimple = reflect->values(text("Simple"));
            auto beforeAdvanced = reflect->values(text("Advanced"));
            processorCheck(ngRequireObject(std::allocate_shared<AutoMeshSetPresetCommand>(SourceAllocator<AutoMeshSetPresetCommand>{},
                ProcessorIdArgument{processor}, text("Normal parts"))->run(std::allocate_shared<Context>(SourceAllocator<Context>{})))->succeeded,
                "AutoMeshSetPresetCommand should apply Normal parts: ");
            check(incActionHistory().size() > 0, "AutoMeshSetPresetCommand should push undoable config action");
            incActionUndo();
            check(jsonEquivalent(reflect->values(text("Simple")), beforeSimple), "undo AutoMesh preset should restore simple values");
            const bool sameAdvanced = jsonEquivalent(reflect->values(text("Advanced")), beforeAdvanced);
            const auto processorId = processor->procId();
            const auto currentAdvanced = reflect->values(text("Advanced"));
            namespace format = nijigenerate::core::translation;
            const std::array<format::TypedFormatArgument, 3> arguments{
                format::ngFormatArgument(processorId), format::ngFormatArgument(currentAdvanced), format::ngFormatArgument(beforeAdvanced)};
            auto rendered = format::ngFormatTyped("undo AutoMesh preset should restore advanced values for %s\nactual=%s\nexpected=%s", arguments);
            require(sameAdvanced, nijigenerate::platform::detail::ngPathAllocation([&] { return RetainedString(std::move(rendered)); }));
            incActionRedo();
            incActionClearHistory();
        }
    }
    check(sawPresetProcessor, "at least one AutoMesh processor should expose presets");
    auto normalScope = ngRequireObject(currentCommandScope(std::allocate_shared<Context>(SourceAllocator<Context>{})))->result;
    for (auto processor : processors) {
        auto availableCommands = normalScope.sourceIndexValue("availableCommands").sourceArrayValue();
        const auto toolName = concatenateSourceStrings({text("AutoMesh_Apply_"), ngRequireObject(processor)->procId()});
        const bool listed = scopeCommandListContains(availableCommands, toolName.view());
        const auto message = concatenateSourceStrings({text("ModelEdit command scope should expose AutoMesh Apply MCP tool: "), processor->procId()});
        require(listed, message);
    }
    auto dialogScope = nijigenerate::commands::ngPushCommandScope(
        nijigenerate::commands::ngCommandScope<nijigenerate::commands::depth::psd_dialog::PsdDepthDialogCommandScope>());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        for (auto processor : processors) {
            auto applyCommand = ngEnsureApplyAutoMeshCommand(RetainedStringSlice(ngRequireObject(processor)->procId()));
            const bool unavailable = !nijigenerate::commands::ngCommandAllowedInCurrentContext(applyCommand);
            require(unavailable, concatenateSourceStrings({text("AutoMesh Apply command should be unavailable in the PSD depth dialog: "), processor->procId()}));
        }
        ngRequireObject(dialogScope)->close();
        for (auto processor : processors) {
            auto applyCommand = ngEnsureApplyAutoMeshCommand(RetainedStringSlice(ngRequireObject(processor)->procId()));
            const bool available = nijigenerate::commands::ngCommandAllowedInCurrentContext(applyCommand);
            require(available, concatenateSourceStrings({text("AutoMesh Apply command should return after the PSD depth dialog closes: "), processor->procId()}));
        }
        auto original = ngActiveAutoMeshProcessor();
        auto target = processors[processors.size() - 1] == original ? processors[0] : processors[processors.size() - 1];
        struct TargetIdArgument {
            std::shared_ptr<AutoMeshProcessor> target;
            operator RetainedString() && { return ngRequireObject(target)->procId(); }
        };
        check(ngRequireObject(std::allocate_shared<AutoMeshSetActiveCommand>(SourceAllocator<AutoMeshSetActiveCommand>{},
            TargetIdArgument{target})->run(std::allocate_shared<Context>(SourceAllocator<Context>{})))->succeeded, "AutoMeshSetActiveCommand should succeed");
        check(ngActiveAutoMeshProcessor() == target, "AutoMeshSetActiveCommand should change active processor");
        ngActiveAutoMeshProcessor(original);
    }, [&] { ngRequireObject(dialogScope)->close(); });
}
