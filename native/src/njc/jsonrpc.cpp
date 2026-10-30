#include "jsonrpc.hpp"
#include <nijigenerate/compat/druntime_utf8.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace njc {
namespace {
template<class Operation> decltype(auto) withSourceAllocation(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            std::source_location::current());
    } catch (const std::length_error&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            std::source_location::current());
    }
}

std::string_view sourceView(std::string_view value) { return value; }
std::string_view sourceView(const RetainedString& value) { return value.view(); }

template<class Text> RetainedString escapeJsonBody(Text value) {
    const auto input = sourceView(value);
    std::string output;
    const auto append = [&](std::string_view bytes) {
        withSourceAllocation([&] { output.append(bytes); });
    };
    nijigenerate::compat::DruntimeUtf8Cursor cursor(input);
    while (!cursor.empty()) {
        const auto first = cursor.offset();
        const auto ch = cursor.take();
        switch (ch) {
        case U'"': append("\\\""); break;
        case U'\\': append("\\\\"); break;
        case U'\b': append("\\b"); break;
        case U'\f': append("\\f"); break;
        case U'\n': append("\\n"); break;
        case U'\r': append("\\r"); break;
        case U'\t': append("\\t"); break;
        default:
            if (ch < 0x20) {
                const std::array arguments{
                    nijigenerate::core::translation::ngFormatArgument(static_cast<std::uint32_t>(ch))};
                const auto escaped = nijigenerate::core::translation::ngFormatTyped("\\u%04x", arguments);
                append(escaped);
            } else {
                append(input.substr(first, cursor.offset() - first));
            }
            break;
        }
    }
    if (output.empty()) return {};
    return withSourceAllocation([&] { return RetainedString(std::move(output)); });
}
template<class Method, class Params>
RetainedString rpcRequestBody(std::int64_t id, Method method, Params paramsJson) {
    const std::array arguments{nijigenerate::core::translation::ngFormatArgument(id)};
    const auto digits = nijigenerate::core::translation::ngFormatTyped("%s", arguments);
    auto output = withSourceAllocation([] { return std::string("{\"jsonrpc\":\"2.0\",\"id\":"); });
    const auto append = [&](std::string_view bytes) {
        withSourceAllocation([&] { output.append(bytes); });
    };
    append(digits);
    append(",\"method\":\"");
    {
        const auto escaped = escapeJsonBody(method);
        append(escaped.view());
    }
    append("\",\"params\":");
    append(sourceView(paramsJson));
    append("}");
    return withSourceAllocation([&] { return RetainedString(std::move(output)); });
}
}
RetainedString escapeJson(std::string_view value) {
    return escapeJsonBody(value);
}
RetainedString rpcRequest(std::int64_t id, std::string_view method, std::string_view paramsJson) {
    return rpcRequestBody(id, method, paramsJson);
}
RetainedString ngEscapeJsonSource(RetainedString value) {
    return escapeJsonBody(value);
}
RetainedString ngRpcRequestSource(std::int64_t id, RetainedString method, RetainedString paramsJson) {
    return rpcRequestBody(id, method, paramsJson);
}
}
