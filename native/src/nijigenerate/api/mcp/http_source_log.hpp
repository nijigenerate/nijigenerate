#pragma once

#include "http_task_runtime.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::api::mcp::http_transport::detail {
template<class Prepare>
void httpSourceLog(SourceLogLevel level, std::string_view pattern, Prepare&& prepare) {
    ngSourceLog(level, [&] {
        auto text = std::apply([&](const auto&... values) {
            const std::array<core::translation::TypedFormatArgument, sizeof...(values)> arguments{
                core::translation::ngFormatArgument(values)...};
            return core::translation::ngFormatTyped(pattern, arguments);
        }, std::forward<Prepare>(prepare)());
        return [text = std::move(text)](SourceLogSink sink) { sink(text); };
    });
}
inline void httpSourceLog(SourceLogLevel level, std::string_view message) {
    ngSourceLog(level, [message] {
        return [message](SourceLogSink sink) { sink(message); };
    });
}
}
