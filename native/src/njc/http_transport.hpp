#pragma once
#include "config.hpp"
#include "http_transport_errors.hpp"
#include <nijigenerate/core/settings_value.hpp>
#include <cstddef>
#include <memory>
#include <string>
#include <compat/object_metadata.hpp>

namespace njc::http_transport {
// Transitive std.socket module initialization, including the help-only CLI path.
void ngInitializeHttpTransport();

class HttpTransport final {
public:
    explicit HttpTransport(config::Endpoint endpoint);
    HttpTransport(const HttpTransport&) = delete;
    HttpTransport& operator=(const HttpTransport&) = delete;
    HttpTransport(HttpTransport&&) = delete;
    HttpTransport& operator=(HttpTransport&&) = delete;
    virtual ~HttpTransport() = default;
    nijigenerate::core::settings::SettingsJsonValue postJson(config::Text body);
    virtual nijigenerate::compat::RetainedString toString();
    virtual std::size_t toHash() noexcept;
    virtual bool opEquals(std::shared_ptr<HttpTransport> other);
    virtual int opCmp(std::shared_ptr<HttpTransport> other);
private:
    config::Endpoint endpoint;
    std::string buildRequest(config::Text body) const;
};
} // namespace njc::http_transport

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::njc::http_transport::HttpTransport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(njc.http_transport.HttpTransport)";
    inline static constexpr char unqualifiedArgument[] = "(HttpTransport)";
    inline static constexpr char qualifiedElement[] = "njc.http_transport.HttpTransport";
    inline static constexpr char unqualifiedElement[] = "HttpTransport";
};
template<> struct SourceObjectTypeNames<::njc::http_transport::HttpTransport> {
    inline static constexpr char className[] = "njc.http_transport.HttpTransport";
    inline static constexpr char templateArgumentName[] = "(njc.http_transport.HttpTransport)";
};
}
