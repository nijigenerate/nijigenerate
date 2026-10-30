#pragma once
// Independent Phobos std.format/std.conv exception protocol. BSL-1.0;
// source formatter notices are retained by typed_format_layout.hpp.
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::core::translation {
class FormatException : public compat::ThrowableException {
public:
    FormatException() : ThrowableException(compat::RetainedString::fromStaticLiteral("format error"),
        compat::RetainedString::fromStaticLiteral("std/format/package.d"),659) {}
    FormatException(compat::RetainedString message, compat::RetainedString file, std::size_t line,
        std::shared_ptr<compat::Throwable> tail={})
        : ThrowableException(std::move(message),std::move(file),line,std::move(tail)) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.format.FormatException"; }
};
namespace detail {
inline compat::RetainedString retainFormatMessage(std::string message) {
    try {
        return compat::RetainedString(std::move(message));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t M,std::size_t N> [[noreturn]] void formatFailure(const char (&message)[M],
        const char (&file)[N],std::size_t line) {
    throw FormatException(compat::RetainedString::fromStaticLiteral(message),
        compat::RetainedString::fromStaticLiteral(file),line);
}
template<std::size_t N> [[noreturn]] void formatFailure(std::string message,
        const char (&file)[N],std::size_t line) {
    throw FormatException(retainFormatMessage(std::move(message)),
        compat::RetainedString::fromStaticLiteral(file),line);
}
[[noreturn]] inline void formatConversionOverflow(std::string message,std::size_t line) {
    throw compat::PhobosConvOverflowException(retainFormatMessage(std::move(message)),
        compat::RetainedString::fromStaticLiteral("std/conv.d"),line);
}
template<std::size_t N> [[noreturn]] void formatConversionOverflow(const char (&message)[N],std::size_t line) {
    throw compat::PhobosConvOverflowException(compat::RetainedString::fromStaticLiteral(message),
        compat::RetainedString::fromStaticLiteral("std/conv.d"),line);
}
[[noreturn]] inline void formatParseFailure(std::string_view input,std::string_view target) {
    std::string message;
    char32_t scalar = 0;
    if (!input.empty()) {
        using Cursor=compat::phobos::detail::UnicodeCursor<std::string_view,char>;
        scalar=Cursor(input).front();
    }
    try {
        if (input.empty()) message="Unexpected end of input";
        else {
            const std::size_t length=scalar<0x80?1:scalar<0x800?2:scalar<0x10000?3:4;
            message="Unexpected '";
            message+=scalar=='\n'?std::string("\\n"):std::string(input.substr(0,length));
            message+="'";
        }
        message+=" when converting from type const(char)[] to type "; message+=target;
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::PhobosConvException(retainFormatMessage(std::move(message)),
        compat::RetainedString::fromStaticLiteral("std/conv.d"),2547);
}
} // namespace detail
} // namespace nijigenerate::core::translation
