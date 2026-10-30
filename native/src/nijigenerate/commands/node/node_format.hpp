#pragma once
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <cstddef>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
namespace nijigenerate::commands::node::detail {
inline compat::RetainedString retainNodeFormatText(std::string text) {
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline compat::RetainedString tr(std::string_view s) {
    return retainNodeFormatText(core::translation::ngTranslate(s));
}
template<std::size_t N> compat::RetainedString tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
template<class... T> compat::RetainedString format(std::string_view pattern,T&&... value) {
    const auto translated = core::translation::ngTranslate(pattern);
    const std::array args{core::translation::ngFormatArgument(std::forward<T>(value))...};
    return retainNodeFormatText(core::translation::ngFormatTyped(translated,args));
}
template<std::size_t N, class... T>
compat::RetainedString format(const char (&pattern)[N], T&&... value) {
    const auto translated = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(pattern));
    const std::array args{core::translation::ngFormatArgument(std::forward<T>(value))...};
    return retainNodeFormatText(core::translation::ngFormatTyped(translated.view(), args));
}
}
