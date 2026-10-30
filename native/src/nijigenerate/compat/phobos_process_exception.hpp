#pragma once
// std.process.ProcessException, Phobos, BSL-1.0; see phobos-videoexport.txt.
#include "throwable.hpp"
#include "retained_string.hpp"
#include <cstddef>
#include <utility>

namespace nijigenerate::compat {
class ProcessException : public ThrowableException {
public:
    ProcessException(RetainedString message, std::size_t sourceLine)
        : ThrowableException(std::move(message), RetainedString::fromStaticLiteral("std/process.d"), sourceLine) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.process.ProcessException"; }
};
}
