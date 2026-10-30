// Complete original regression.d18963-19044, same module-private namespace.
void testMcpResourceListingAndContextHelpers() {
    using Text = nijigenerate::compat::RetainedString;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::d_runtime::SourceAllocator;
    using Json = nijigenerate::core::settings::SettingsJsonValue;
    using Object = nijigenerate::core::settings::SettingsJsonObject;
    using Array = nijigenerate::core::settings::SettingsJsonArray;
    using Key = nijigenerate::core::settings::SettingsString;
    namespace resourcesApi = nijigenerate::api::mcp::resource_listing;
    namespace helpersApi = nijigenerate::api::mcp::helpers;
    const auto text = []<std::size_t N>(const char (&value)[N]) { return Text::fromStaticLiteral(value); };
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, Text::fromStaticLiteral(message));
    };
    // std.conv.toChars!10(uint).array and unsignedToTempString!10(char).
    const auto uuidText = [](std::uint32_t input) {
        std::array<char, 10> buffer;
        std::size_t first = buffer.size();
        std::uint64_t value = input;
        do {
            std::uint32_t digit;
            if (value < 10) { digit = static_cast<std::uint32_t>(value); value = 0; }
            else { digit = static_cast<std::uint32_t>(value % 10); value /= 10; }
            buffer[--first] = static_cast<char>(digit + '0');
        } while (value);
        auto result = allocateSourceStringBytes(buffer.size() - first);
        std::memcpy(result.data(), buffer.data() + first, result.size());
        return Text::retainedView(result.retentionOwner(), result.data(), result.size());
    };
    resetCase();
    auto puppet = nijigenerate::project::incActivePuppet();
    auto node = Node::createSourceDeferred([&] { return puppet->root; });
    node->setName(NullableString::fromStaticLiteral("MCPNode"));
    auto child = Node::createSourceDeferred([&] { return node; });
    child->setName(NullableString::fromStaticLiteral("MCPChild"));
    auto param = nijigenerate::ext::ExParameter::createSourceDeferred(
        [] { return NullableString::fromStaticLiteral("MCPParam"); }, [] { return false; });
    puppet->parameters.push_back(param);
    auto binding = newValueBinding(param, child, text("transform.t.x"));
    binding->setValue({1, 0}, 12.5f);
    auto resources = resourcesApi::buildCurrentResourceList();
    check(resources.is_object(), "MCP resources should return an object");
    check(resources.sourceIndexValue("resources").is_array(), "MCP resources should contain resources array");
    bool hasRootGuide = false, hasNode = false, hasParam = false, hasBinding = false;
    for (auto entry : resources.sourceIndexValue("resources").sourceArrayValue()) {
        auto uri = entry.sourceIndexValue("uri").as_string();
        auto name = entry.sourceIndexValue("name").as_string();
        if (uri.view() == "resource://nijigenerate/resources/find?selector=*") hasRootGuide = true;
        if (uri.view().ends_with(concatenateSourceStrings({text("/"), uuidText(node->getUUID())}).view()) &&
            name.view() == "MCPNode") hasNode = true;
        if (uri.view().ends_with(concatenateSourceStrings({text("/"), uuidText(param->getUUID())}).view()) &&
            name.view() == "MCPParam") hasParam = true;
        if (uri.view().starts_with("resource://nijigenerate/bindings/get?") &&
            uri.view().find(concatenateSourceStrings({text("parameter="), uuidText(param->getUUID())}).view()) != std::string_view::npos &&
            uri.view().find(concatenateSourceStrings({text("target="), uuidText(child->getUUID())}).view()) != std::string_view::npos &&
            name.view().find("MCPParam -> MCPChild.transform.t.x") != std::string_view::npos) hasBinding = true;
    }
    check(hasRootGuide, "MCP resource listing should include selector guide entry");
    check(hasNode, "MCP resource listing should include model nodes");
    check(hasParam, "MCP resource listing should include parameters");
    check(hasBinding, "MCP resource listing should include parameter bindings");
    Object responseObj, resultObj;
    resultObj[Key("resources")] = Json(Array{});
    responseObj[Key("result")] = Json(resultObj);
    auto response = Json(responseObj);
    Object requestObj;
    requestObj[Key("method")] = Json("resources/list");
    auto request = Json(requestObj);
    resourcesApi::rewriteResourcesListResponse(response, request);
    check(response.sourceIndexValue("result").sourceIndexValue("resources").sourceArrayValue().size() >=
        resources.sourceIndexValue("resources").sourceArrayValue().size(),
        "resources/list response should be rewritten with current resource list");
    auto resultJson = helpersApi::commandResultToJsonRuntime(
        std::allocate_shared<nijigenerate::commands::CommandResult>(SourceAllocator<nijigenerate::commands::CommandResult>{},
            true, text("done")));
    check(resultJson.sourceIndexValue("status").as_string().view() == "ok", "MCP command result JSON should report ok status");
    const auto success = resultJson.sourceIndexValue("succeeded");
    check(success.is_bool() && success.as_bool(), "MCP command result JSON should preserve success flag");
    check(resultJson.sourceIndexValue("message").as_string().view() == "done", "MCP command result JSON should preserve messages");
    Array nodeIds{Json(static_cast<std::int64_t>(node->getUUID()))};
    Array paramIds{Json(static_cast<std::int64_t>(param->getUUID()))};
    Array paramValue{Json(0.25), Json(-0.5)};
    Object bindingDesc;
    bindingDesc[Key("target")] = Json(static_cast<std::int64_t>(child->getUUID()));
    bindingDesc[Key("name")] = Json("transform.t.x");
    Array bindings{Json(bindingDesc)};
    Object contextObj;
    contextObj[Key("nodes")] = Json(nodeIds);
    contextObj[Key("parameters")] = Json(paramIds);
    contextObj[Key("parameterValue")] = Json(paramValue);
    contextObj[Key("bindings")] = Json(bindings);
    Object payloadObj;
    payloadObj[Key("context")] = Json(contextObj);
    auto context = helpersApi::buildContextFromPayload(Json(payloadObj));
    check(context->hasNodes() && context->nodes().size() == 1 && context->nodes()[0] == node,
        "MCP context payload should select explicit nodes");
    check(context->hasParameters() && context->parameters().size() == 1 && context->parameters()[0] == param,
        "MCP context payload should select explicit parameters");
    check(context->hasActiveBindings() && context->activeBindings().size() == 1 && context->activeBindings()[0] == binding,
        "MCP context payload should resolve binding descriptors");
    check(near(context->parameterValue().x, 0.25f) && near(context->parameterValue().y, -0.5f),
        "MCP context payload should parse parameterValue as parameter-axis values");
    check(!context->hasExplicitKeyPoint(), "MCP context payload should avoid stale keypoint context when parameterValue is used");
}
