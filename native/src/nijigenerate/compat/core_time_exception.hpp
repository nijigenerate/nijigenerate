#pragma once
// core.time.TimeException / std.datetime.date.DateTimeException used shape.
// Jonathan M Davis; BSL-1.0: port/licenses/phobos-monotime.txt.
#include "throwable.hpp"

namespace nijigenerate::compat {
class TimeException : public ThrowableException {
public:
    using ThrowableException::ThrowableException;
    std::string_view ngThrowableTypeName() const noexcept override { return "core.time.TimeException"; }
};
using DateTimeException = TimeException;
} // namespace nijigenerate::compat
