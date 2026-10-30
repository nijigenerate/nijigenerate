// Existing regression.d19152-19212, included once inside regression::detail.
// Required headers: api/mcp/auth.hpp, http_transport.hpp, server.hpp;
// compat/phobos_build_path.hpp, phobos_file_read_text.hpp, and <array>.
// Existing module-private regressionSourceRoot and require precede this body.
void testApiTransportAndServerContracts() {
    using nijigenerate::compat::RetainedString;
    // The authored escaping delegate retains this same captured local cell.
    const auto handledCell = std::allocate_shared<bool>(
        nicxlive::core::d_runtime::SourceAllocator<bool>{}, false);
    namespace transportApi = nijigenerate::api::mcp::http_transport;
    namespace serverApi = nijigenerate::api::mcp::server;
    namespace settingsApi = nijigenerate::core::settings;
    const auto check = []<std::size_t N>(bool condition, const char (&message)[N]) {
        require(condition, RetainedString::fromStaticLiteral(message));
    };
    nijigenerate::api::mcp::auth::ApprovalRequest req{
        RetainedString::fromStaticLiteral("req-1"),
        RetainedString::fromStaticLiteral("client-a"),
        RetainedString::fromStaticLiteral("mcp:use"),
        RetainedString::fromStaticLiteral("http://127.0.0.1:8088/mcp"),
        RetainedString::fromStaticLiteral("state-a"),
        RetainedString::fromStaticLiteral("http://127.0.0.1/callback")};
    check(req.reqId.view() == "req-1", "MCP approval request should preserve request id");
    check(req.clientId.view() == "client-a", "MCP approval request should preserve client id");
    check(req.scopeId.view() == "mcp:use", "MCP approval request should preserve scope");
    check(req.redirectUri.view().ends_with("/callback"), "MCP approval request should preserve redirect uri");

    auto transport = transportApi::createHttpTransport(
        RetainedString::fromStaticLiteral("127.0.0.1"), 0);
    check(transport->authEnabled(), "MCP HTTP transport should default to auth enabled");
    transport->authEnabled(false);
    check(!transport->authEnabled(), "MCP HTTP transport auth toggle should disable auth");
    transport->authEnabled(true);
    check(transport->authEnabled(), "MCP HTTP transport auth toggle should enable auth");
    bool& handled = *handledCell;
    transport->setMessageHandler([handledCell](settingsApi::SettingsJsonValue message) {
        *handledCell = message.is_object() && message.sourceObjectRef().if_contains("method") &&
            message.sourceIndexValue("method").as_string().view() == "ping";
    });
    settingsApi::SettingsJsonObject pingObj;
    pingObj[settingsApi::SettingsString("method")] = settingsApi::SettingsJsonValue("ping");
    transport->handleMessage(settingsApi::SettingsJsonValue(pingObj));
    check(handled, "MCP HTTP transport should dispatch messages to its handler");
    transport->close();

    serverApi::ngMcpApplySettings(false, RetainedString::fromStaticLiteral("127.0.0.1"), 0);
    serverApi::ngMcpAuthEnabled(true);
    check(!serverApi::ngMcpAuthEnabled(), "MCP auth getter should stay false when server transport is not running");
    serverApi::ngMcpStop();

    const std::array httpSegments{regressionSourceRoot(RetainedString::fromStaticLiteral("api")),
        RetainedString::fromStaticLiteral("mcp"), RetainedString::fromStaticLiteral("http_transport.d")};
    auto httpSource = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(httpSegments));
    check(httpSource.view().find("router.post(\"/mcp\"") != std::string_view::npos,
        "MCP HTTP transport should register POST /mcp");
    check(httpSource.view().find("router.get (\"/events\"") != std::string_view::npos,
        "MCP HTTP transport should register GET /events");
    check(httpSource.view().find("/.well-known/oauth-protected-resource/mcp") != std::string_view::npos,
        "MCP HTTP transport should expose protected resource metadata");
    check(httpSource.view().find("/auth/token") != std::string_view::npos,
        "MCP HTTP transport should expose token endpoint");
    check(httpSource.view().find("rewriteResourcesListResponse") != std::string_view::npos,
        "MCP HTTP transport should rewrite resources/list responses");

    const std::array authSegments{regressionSourceRoot(RetainedString::fromStaticLiteral("api")),
        RetainedString::fromStaticLiteral("mcp"), RetainedString::fromStaticLiteral("auth.d")};
    auto authSource = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(authSegments));
    check(authSource.view().find("approvalTimeout = 120.seconds") != std::string_view::npos,
        "MCP auth should keep a bounded approval timeout");
    check(authSource.view().find("ngRunInMainThread") != std::string_view::npos,
        "MCP auth UI should be marshalled to the main thread");
    check(authSource.view().find("Approve") != std::string_view::npos &&
        authSource.view().find("Deny") != std::string_view::npos,
        "MCP auth UI should expose approve and deny decisions");

    const std::array stdioSegments{regressionSourceRoot(RetainedString::fromStaticLiteral("api")),
        RetainedString::fromStaticLiteral("acp"), RetainedString::fromStaticLiteral("transport"),
        RetainedString::fromStaticLiteral("stdio.d")};
    auto stdioSource = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(stdioSegments));
    check(stdioSource.view().find("public alias StdioTransport") != std::string_view::npos,
        "ACP stdio adapter should expose StdioTransport alias");
    check(stdioSource.view().find("createStdioTransport") != std::string_view::npos,
        "ACP stdio adapter should expose transport factory");
    check(stdioSource.view().find("mcp.transport.stdio.createStdioTransport") != std::string_view::npos,
        "ACP stdio adapter should delegate to mcp stdio transport");

    const std::array echoSegments{regressionSourceRoot(RetainedString::fromStaticLiteral("api")),
        RetainedString::fromStaticLiteral("acp"), RetainedString::fromStaticLiteral("echo_agent.d")};
    auto echoSource = nijigenerate::compat::phobos::ngReadUtf8Text(
        nijigenerate::compat::phobos::ngBuildPath(echoSegments));
    check(echoSource.view().find("version (ACP_ECHO_TOOL)") != std::string_view::npos,
        "ACP echo agent should stay build-gated");
    check(echoSource.view().find("Content-Length") != std::string_view::npos,
        "ACP echo agent should support Content-Length framing");
    check(echoSource.view().find("method == \"initialize\"") != std::string_view::npos,
        "ACP echo agent should handle initialize");
    check(echoSource.view().find("ACP_PROTOCOL_VERSION") != std::string_view::npos,
        "ACP echo agent should report protocol version");
}
