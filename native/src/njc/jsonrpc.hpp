#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace njc {
using nijigenerate::compat::RetainedString;
RetainedString escapeJson(std::string_view value);
RetainedString rpcRequest(std::int64_t id, std::string_view method, std::string_view paramsJson);
RetainedString ngEscapeJsonSource(RetainedString value);
RetainedString ngRpcRequestSource(std::int64_t id, RetainedString method, RetainedString paramsJson);
template<class T> requires std::is_same_v<std::remove_cvref_t<T>, RetainedString>
RetainedString escapeJson(T&& value) { return ngEscapeJsonSource(value); }
template<class M, class P>
    requires (std::is_same_v<std::remove_cvref_t<M>, RetainedString> &&
              std::is_same_v<std::remove_cvref_t<P>, RetainedString>)
RetainedString rpcRequest(std::int64_t id, M&& method, P&& paramsJson) {
    return ngRpcRequestSource(id, method, paramsJson);
}
namespace jsonrpc {
using ::njc::escapeJson;
using ::njc::rpcRequest;
using ::njc::ngEscapeJsonSource;
using ::njc::ngRpcRequestSource;
}
}
