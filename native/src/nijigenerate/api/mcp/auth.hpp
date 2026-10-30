#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <tuple>

namespace nijigenerate::api::mcp::auth {

struct ApprovalRequest {
    compat::RetainedStringSlice reqId;
    compat::RetainedStringSlice clientId;
    compat::RetainedStringSlice scopeId;
    compat::RetainedStringSlice resource;
    compat::RetainedStringSlice state;
    compat::RetainedStringSlice redirectUri;
    bool operator==(const ApprovalRequest&) const = default;
};

compat::RetainedStringSlice ngSimpleAuth(ApprovalRequest request);

} // namespace nijigenerate::api::mcp::auth

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::api::mcp::auth::ApprovalRequest> {
    using Value=::nijigenerate::api::mcp::auth::ApprovalRequest;
    inline static constexpr std::string_view typeName="ApprovalRequest";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("reqId",offsetof(Value,reqId),&Value::reqId),
            ngFormatAggregateMember("clientId",offsetof(Value,clientId),&Value::clientId),
            ngFormatAggregateMember("scopeId",offsetof(Value,scopeId),&Value::scopeId),
            ngFormatAggregateMember("resource",offsetof(Value,resource),&Value::resource),
            ngFormatAggregateMember("state",offsetof(Value,state),&Value::state),
            ngFormatAggregateMember("redirectUri",offsetof(Value,redirectUri),&Value::redirectUri)};
    }
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::api::mcp::auth::ApprovalRequest> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.api.mcp.auth.ApprovalRequest)";
    inline static constexpr char unqualifiedArgument[] = "(ApprovalRequest)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.api.mcp.auth.ApprovalRequest";
    inline static constexpr char unqualifiedElement[] = "ApprovalRequest";
};
}
