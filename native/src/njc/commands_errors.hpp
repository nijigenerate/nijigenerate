#pragma once

#include <nijigenerate/compat/throwable.hpp>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace njc::commands {

class CommandException : public nijigenerate::compat::ThrowableException {
public:
    explicit CommandException(nijigenerate::compat::RetainedString message)
        : ThrowableException(std::move(message)) {}
    CommandException(nijigenerate::compat::RetainedString message,
        nijigenerate::compat::RetainedString sourceFile, std::size_t sourceLine)
        : ThrowableException(std::move(message), std::move(sourceFile), sourceLine) {}
};

} // namespace njc::commands
