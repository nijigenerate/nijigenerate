// Complete original regression.d21830-21882, included inside regression::detail.
// The existing source_strings.hpp definitions and regressionSourceRoot precede it.
void testCommandBrowserResourceArgumentParsingResolvesLiveResources() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::SimplePhysics;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::PathDeformer;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::viewport::control::AddFlipPairCommand;
    using nijigenerate::commands::viewport::control::ListFlipPairsCommand;
    using nijigenerate::commands::node::simplephysics::SetSimplePhysicsParameterCommand;
    using nijigenerate::commands::depth::bone::PreviewDepthBoneDeformCommand;
    using nijigenerate::ext::nodes::ExDepthRigRoot;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    const auto uuidText = [](std::uint32_t input) {
        std::array<char, 10> buffer;
        std::size_t first = buffer.size();
        do {
            std::uint32_t digit;
            if (input < 10) { digit = input; input = 0; }
            else { digit = input % 10; input /= 10; }
            buffer[--first] = static_cast<char>(digit + '0');
        } while (input);
        auto bytes = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(bytes.data(), buffer.data() + first, bytes.size());
        return Text::retainedView(bytes.retentionOwner(), bytes.data(), bytes.size());
    };
    auto source = nijigenerate::compat::phobos::ngReadUtf8Text(regressionSourceRoot(text("windows/command_browser.d")));
    const auto contains = [&](std::string_view value) { return source.view().find(value) != std::string_view::npos; };
    check(contains("} else static if (isNodeResource!T)") && contains("} else static if (isNodeResourceArray!T)"),
        "Command Browser should keep parseArgValue support for Node and all Node-derived resource types");
    for (auto resourceType : nijigenerate::compat::Slice<Text>{text("Parameter"), text("ParameterBinding"), text("Parameter[]"), text("ParameterBinding[]")}) {
        auto needle = concatenateSourceStrings({text("} else static if (is(T == "), resourceType, text("))")});
        const bool found = contains(needle.view());
        auto message = concatenateSourceStrings({text("Command Browser should keep parseArgValue support for "), resourceType});
        require(found, message);
    }
    check(contains("auto found = cast(T)p.find!(Node)(id);") && contains("auto found = p.find!(Parameter)(id);"),
        "Command Browser should resolve Node-derived and Parameter arguments from the active puppet by UUID");
    check(contains("findBindingByCommandBrowserId") && contains("foreach (param; puppet.parameters)") &&
        contains("foreach (binding; param.bindings)") && contains("ensureCommandBrowserBindingId(binding) == id"),
        "Command Browser should resolve ParameterBinding ids through active-puppet bindings, not stale cached links");
    check(contains("E[] result;") && contains("Parameter[] result;") && contains("ParameterBinding[] result;"),
        "Command Browser should keep array resource parsing for all resource argument families");
    check(contains("info.isNodeResource = isNodeResource!TParam;") &&
        contains("info.isNodeResourceArray = isNodeResourceArray!TParam;") &&
        contains("info.acceptsNodeResource = (Node n) { return cast(NR)n !is null; };"),
        "Command Browser metadata should classify and filter scalar and array Node-derived resource arguments");
    check(contains("ctxBindingsSel = ctx.hasActiveBindings ? ctx.activeBindings : ctx.hasBindings ? ctx.bindings : [];"),
        "Command Browser should capture the active binding context before falling back to all parameter bindings");
    check(contains("if (auto pv = fname in vals)") && contains("if (parseArgValue!TParam(*pv, v))") &&
        contains("mixin(\"inst.\"~fname~\" = v;\")"),
        "Command Browser applyArgs should parse and assign command arguments before run");
    check(contains("} else static if (isResourceArgument!TParam)") && contains("clearResourceArgValue!TParam(v);") &&
        contains("mixin(\"inst.\"~fname~\" = v;\")"),
        "Command Browser applyArgs should reset stale resource arguments to valid empty selections when the UI passes an empty value");
    check(contains("if (info.applyArgs !is null && selectedCmd !is null)") && contains("info.applyArgs(selectedCmd, argValues);") &&
        contains("res = ngRunCommand(selectedCmd, ctx);"),
        "Command Browser should apply parsed arguments before scope-aware command dispatch");
    auto firstApplier = countUntilSourceString(source, text("applier = (Command c, string[string] vals)"));
    auto firstCapture = countUntilSourceString(source, text("defCapturer = (Command c, ref string[string] vals)"));
    check(firstApplier >= 0 && firstCapture > firstApplier,
        "Command Browser should generate an argument applier before default capture logic");
    auto firstApplierBody = source.subslice(static_cast<std::size_t>(firstApplier), static_cast<std::size_t>(firstCapture));
    check(firstApplierBody.view().find("static if (!hidden)") != std::string_view::npos &&
        firstApplierBody.view().find("!is(TParam : Node)") == std::string_view::npos &&
        firstApplierBody.view().find("!is(TParam : Parameter)") == std::string_view::npos &&
        firstApplierBody.view().find("!is(TParam : ParameterBinding)") == std::string_view::npos,
        "Command Browser applyArgs must not filter out Node, Parameter, or ParameterBinding resource arguments");
    resetCase();
    auto puppet = nijigenerate::project::incActivePuppet();
    auto left = Node::createSourceDeferred([&] { return puppet->root; });
    left->setName(text("command-browser-left").toNullableString());
    auto right = Node::createSourceDeferred([&] { return puppet->root; });
    right->setName(text("command-browser-right").toNullableString());
    auto flipCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    flipCtx->puppet(puppet);
    auto addFlip = constructSourceCommand<AddFlipPairCommand>();
    addFlip->left = left;
    addFlip->right = right;
    check(addFlip->run(flipCtx)->succeeded, "AddFlipPairCommand should still run with live Node resource references");
    auto listedJson = nijigenerate::compat::ngJsonStringify(constructSourceCommand<ListFlipPairsCommand>()->run(flipCtx)->result);
    check(listedJson.view().find(uuidText(left->uuid).view()) != std::string_view::npos,
        "AddFlipPairCommand should preserve flip-pair result semantics");
    resetCase();
    auto physics = SimplePhysics::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    physics->setName(text("command-browser-physics").toNullableString());
    auto physicsParam = new2DParameter(text("CommandBrowserPhysicsParam"));
    auto physicsCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    physicsCtx->nodes(Context::Nodes{physics});
    auto setPhysicsParam = constructSourceCommand<SetSimplePhysicsParameterCommand>(physicsParam);
    check(setPhysicsParam->run(physicsCtx)->succeeded,
        "SetSimplePhysicsParameterCommand should still run with live Parameter resource references");
    check(physics->getParam() == physicsParam, "SetSimplePhysicsParameterCommand should preserve parameter assignment semantics");
    resetCase();
    auto rigRoot = ExDepthRigRoot::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    rigRoot->setName(text("command-browser-depth-root").toNullableString());
    auto targetA = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    targetA->setName(text("command-browser-target-a").toNullableString());
    auto targetB = PathDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    targetB->setName(text("command-browser-target-b").toNullableString());
    auto preview = constructSourceCommand<PreviewDepthBoneDeformCommand>();
    preview->root = rigRoot;
    preview->targets = Context::Nodes{targetA, targetB};
    check(preview->root == rigRoot, "PreviewDepthBoneDeformCommand should keep scalar Node resource assignment semantics");
    check(preview->targets.size() == 2 && preview->targets[0] == targetA && preview->targets[1] == targetB,
        "PreviewDepthBoneDeformCommand should keep Node[] resource assignment semantics");
}

void testCommandBrowserResourcePickerSelectionsAreInitialized() {
    using Text = nijigenerate::compat::RetainedString;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    auto source = nijigenerate::compat::phobos::ngReadUtf8Text(
        regressionSourceRoot(text("windows/command_browser.d")));
    const auto contains = [&](Text value) { return source.view().find(value.view()) != std::string_view::npos; };
    nijigenerate::compat::Slice<std::tuple<Text, Text>> entries{
        {text("Node"), text("argNodeSelections")},
        {text("Parameter"), text("argParamSelections")},
        {text("ParameterBinding"), text("argBindingSelections")}};
    for (const auto entry : entries) {
        auto typeName = std::get<0>(entry);
        auto selectionName = std::get<1>(entry);
        const bool initialized = contains(concatenateSourceStrings({text("if ("), selectionName,
                text(" is null || arg.name !in "), selectionName, text(")")})) &&
            contains(concatenateSourceStrings({selectionName, text("[arg.name] = [];")})) &&
            contains(concatenateSourceStrings({text("renderResourcePicker!"), typeName,
                text("(arg.name, "), selectionName, text("[arg.name]")}));
        const auto initializedMessage = concatenateSourceStrings({text("Command Browser should initialize "),
            typeName, text("[] resource picker selection before indexed AA access")});
        require(initialized, initializedMessage);
        const bool serialized = contains(concatenateSourceStrings({text("argValues[arg.name] = resourceSelectionIds("),
            selectionName, text("[arg.name]);")}));
        const auto serializedMessage = concatenateSourceStrings({text("Command Browser should serialize selected "),
            typeName, text("[] resources back into argument values")});
        require(serialized, serializedMessage);
        if (typeName.view() == "Node") {
            const bool single = contains(text("} else if (arg.isNodeResource)")) &&
                contains(text("renderResourcePicker!Node(arg.name, sel, incActivePuppet(), arg.acceptsNodeResource)")) &&
                contains(text("argValues[arg.name] = resourceSelectionIds(sel);"));
            require(single, text("Command Browser should use a filtered single-resource picker for Node-derived arguments and serialize it"));
        } else {
            const bool single = contains(concatenateSourceStrings({text("} else if (arg.typeName == \""), typeName, text("\")")})) &&
                contains(concatenateSourceStrings({text("renderResourcePicker!"), typeName,
                    text("(arg.name, sel, incActivePuppet())")})) &&
                contains(text("argValues[arg.name] = resourceSelectionIds(sel);"));
            const auto singleMessage = concatenateSourceStrings({text("Command Browser should use a single-resource picker for "),
                typeName, text(" arguments and serialize it")});
            require(single, singleMessage);
        }
    }
    require(contains(text("} else static if (isNodeResource!T)")) &&
        contains(text("} else static if (isNodeResourceArray!T)")),
        text("Command Browser should parse Node and all Node-derived resource argument values for command execution"));
    nijigenerate::compat::Slice<Text> resourceTypes{text("Parameter"), text("ParameterBinding"),
        text("Parameter[]"), text("ParameterBinding[]")};
    for (const auto resourceType : resourceTypes) {
        const bool parsed = contains(concatenateSourceStrings({text("} else static if (is(T == "), resourceType, text("))")}));
        const auto parsedMessage = concatenateSourceStrings({text("Command Browser should parse "), resourceType,
            text(" argument values for command execution")});
        require(parsed, parsedMessage);
    }
    require(!contains(text("!is(TParam : Node) && !is(TParam : Parameter))")),
        text("Command Browser applyArgs should not skip Node/Parameter resource arguments"));
    require(contains(text("clearResourceArgValue!TParam(v);")) && contains(text("mixin(\"inst.\"~fname~\" = v;\")")),
        text("Command Browser applyArgs should clear resource fields to valid empty selections when an empty picker value cannot be parsed"));
    require(contains(text("renderResourcePicker!Node(arg.name, argNodeSelections[arg.name], incActivePuppet(), arg.acceptsNodeResource);")) &&
        contains(text("renderResourcePicker!Parameter(arg.name, argParamSelections[arg.name], incActivePuppet());")) &&
        contains(text("renderResourcePicker!ParameterBinding(arg.name, argBindingSelections[arg.name], incActivePuppet());")),
        text("Command Browser should synchronize resource-array arg values every frame, including empty selections"));
    require(contains(text("if (contextDirty)")) && contains(text("ctx.parameters = paramsOverride;")) &&
        contains(text("ctx.armedParameters = armedOverride;")) && contains(text("ctx.nodes = nodesOverride;")) &&
        contains(text("ctx.bindings = bindingsOverride;")) && contains(text("ctx.activeBindings = bindingsOverride;")),
        text("Command Browser should explicitly apply manual context picker overrides, including empty selections"));
}
