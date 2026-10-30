#pragma once
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/api/acp/transport/mcp_stdio.hpp>
#include <array>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::api::mcp::server::detail {
template<std::size_t N> std::string_view mcpLogPattern(const char (&value)[N]) noexcept {
    return {value,N-1};
}
inline std::string_view mcpLogPattern(std::string_view value) noexcept { return value; }
inline std::string_view mcpLogPattern(const compat::RetainedStringSlice& value) noexcept { return value.view(); }
inline std::string_view mcpLogPattern(const compat::RetainedString& value) noexcept { return value.view(); }

template<class... T> void mcpLog(T&&... values) {
#if defined(MCP_LOG) || defined(NIJIGENERATE_MCP_LOG)
    static_assert(sizeof...(T)>0,"Enabled mcpLog requires a format argument");
    if constexpr(sizeof...(T)>0) {
        auto tuple=std::forward_as_tuple(std::forward<T>(values)...);
        const auto format=mcpLogPattern(std::get<0>(tuple));
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            const std::array<core::translation::TypedFormatArgument,sizeof...(I)> arguments{
                core::translation::ngFormatArgument(std::get<I+1>(tuple))...};
            providers::mcp::stdio::standardOutput().withTextWriter(
                [&](providers::mcp::stdio::FileTextWriter& writer) {
                    core::translation::ngFormatTypedTo(
                        [&writer](std::string_view bytes) { writer.put(bytes); },format,arguments);
                    writer.put(static_cast<unsigned char>('\n'));
                });
        }(std::make_index_sequence<sizeof...(T)-1>{});
    }
#else
    ((void)values,...);
#endif
}
}
