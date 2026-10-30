#pragma once

#include <nijigenerate/api/acp/transport/mcp_stdio.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/commands/source_diagnostic_text.hpp>
#include <nijigenerate/providers/mcp/server.hpp>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <array>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::api::mcp::http_transport {
namespace detail {
template<class... T> void httpLog(T... args) {
#if defined(MCP_LOG) || defined(NIJIGENERATE_MCP_LOG)
    std::apply([](auto& format, auto&... values) {
        const auto pattern = [&] {
            if constexpr (requires { format.view(); }) return std::string_view(format.view());
            else return std::string_view(format);
        }();
        providers::mcp::stdio::standardOutput().withTextWriter(
            [&](providers::mcp::stdio::FileTextWriter& writer) {
                const std::array<core::translation::TypedFormatArgument, sizeof...(values)> arguments{
                    core::translation::ngFormatArgument(values)...};
                core::translation::ngFormatTypedTo(
                    [&](std::string_view bytes) { writer.put(bytes); }, pattern, arguments);
                writer.put(static_cast<unsigned char>('\n'));
            });
    }, std::forward_as_tuple(args...));
#else
    ((void)args, ...);
#endif
}
}

using Transport = providers::mcp::stdio::Transport;
using JsonValue = core::settings::SettingsJsonValue;
using String = core::settings::SettingsString;
class HttpTransport : public Transport, public std::enable_shared_from_this<HttpTransport> {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit HttpTransport(String host = compat::RetainedString::fromStaticLiteral("127.0.0.1"),
        std::uint16_t port = 8080);
    ~HttpTransport() override;
    virtual bool authEnabled();
    virtual void authEnabled(bool value);
    void setMessageHandler(MessageHandler handler) override;
    void handleMessage(JsonValue message) override;
    void sendMessage(JsonValue message) override;
    void run() override;
    void close() override;
    virtual bool hasExited() const noexcept;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<HttpTransport> other);
    virtual int opCmp(std::shared_ptr<HttpTransport> other);
protected:
    virtual void prepareSourceClassInfo();
};
bool ngHttpTransportObjectEquals(std::shared_ptr<HttpTransport> left, std::shared_ptr<HttpTransport> right);
std::shared_ptr<HttpTransport> createHttpTransport(
    String host = compat::RetainedString::fromStaticLiteral("127.0.0.1"), std::uint16_t port = 8080);
namespace detail {
// Construct this source field before the SDK base, as the D constructor's
// assignment preceding super() requires. This stores the actual shared SDK
// transport; it is not a second SDK/server implementation.
struct ExtMCPServerTransport {
    std::shared_ptr<Transport> _transport;
    explicit ExtMCPServerTransport(std::shared_ptr<Transport> transport) : _transport(std::move(transport)) {}
};
}
class ExtMCPServer : private detail::ExtMCPServerTransport, public providers::mcp::server::MCPServer,
    public std::enable_shared_from_this<ExtMCPServer> {
public:
    using detail::ExtMCPServerTransport::_transport;
    explicit ExtMCPServer(std::shared_ptr<Transport> transport,
        String name = compat::RetainedString::fromStaticLiteral("D MCP Server"),
        String version = compat::RetainedString::fromStaticLiteral("1.0.0"));
    ExtMCPServer(const ExtMCPServer&) = delete;
    ExtMCPServer& operator=(const ExtMCPServer&) = delete;
    ExtMCPServer(ExtMCPServer&&) = delete;
    ExtMCPServer& operator=(ExtMCPServer&&) = delete;
    virtual void stop();
    virtual bool transportExited() const;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<ExtMCPServer> other);
    virtual int opCmp(std::shared_ptr<ExtMCPServer> other);
protected:
    virtual void prepareSourceClassInfo();
};
bool ngExtMCPServerObjectEquals(std::shared_ptr<ExtMCPServer> left, std::shared_ptr<ExtMCPServer> right);
} // namespace nijigenerate::api::mcp::http_transport

namespace nicxlive::core::d_runtime {
#define NG_HTTP_TRANSPORT_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::api::mcp::http_transport::Type> { \
        inline static constexpr char className[] = "nijigenerate.api.mcp.http_transport." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.api.mcp.http_transport." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::api::mcp::http_transport::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.api.mcp.http_transport." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.api.mcp.http_transport." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
        NG_SOURCE_TYPE_DIAGNOSTIC_MEMBERS(#Type) \
    };
NG_HTTP_TRANSPORT_SOURCE_NAMES(HttpTransport)
NG_HTTP_TRANSPORT_SOURCE_NAMES(ExtMCPServer)
#undef NG_HTTP_TRANSPORT_SOURCE_NAMES
}
