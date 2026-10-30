#pragma once
// std.base64.Base64Exception, Phobos BSL-1.0; see port/licenses/phobos-base64.txt.
#include "throwable.hpp"
#include "retained_string.hpp"
#include <cstddef>
#include <utility>
namespace nijigenerate::compat {
class Base64Exception : public ThrowableException {
public:
    Base64Exception(RetainedString message,std::size_t sourceLine)
        : ThrowableException(std::move(message),RetainedString::fromStaticLiteral("std/base64.d"),sourceLine) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.base64.Base64Exception"; }
};
}
