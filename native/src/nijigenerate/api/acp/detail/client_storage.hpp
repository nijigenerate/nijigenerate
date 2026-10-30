#pragma once

#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <initializer_list>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::api::acp::client::detail {

// Call only around builtin storage operations, after evaluating real providers.
template<class Operation> auto allocateClientStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline std::string copyClientText(std::string_view text) {
    return allocateClientStorage([&] { return std::string(text); });
}
inline compat::RetainedString retainClientText(std::string text) {
    return allocateClientStorage([&] { return compat::RetainedString(std::move(text)); });
}
inline compat::RetainedString duplicateClientText(std::string_view text) {
    if (text.empty()) return {};
    return allocateClientStorage([&] { return compat::RetainedString(std::string(text)); });
}
inline std::string concatenateClientBytes(std::initializer_list<std::string_view> parts) {
    return allocateClientStorage([&] {
        std::string result;
        for (const auto part : parts) result.append(part);
        return result;
    });
}
inline compat::RetainedString concatenateClientText(std::initializer_list<std::string_view> parts) {
    return retainClientText(concatenateClientBytes(parts));
}
inline void appendClientText(std::string& text, std::string_view suffix) {
    allocateClientStorage([&] { text.append(suffix); });
}

} // namespace nijigenerate::api::acp::client::detail
