#pragma once

#include "throwable.hpp"

namespace nijigenerate::compat {

// The independent Phobos std.uri.URIException is an ordinary Exception subtype.
class URIException : public ThrowableException {
public:
    using ThrowableException::ThrowableException;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.uri.URIException"; }
};

} // namespace nijigenerate::compat
