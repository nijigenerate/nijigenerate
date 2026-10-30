#pragma once

#include "exception_message.hpp"
#include "retained_string.hpp"
#include "throwable.hpp"
#include "out_of_memory_error.hpp"
#include <exception>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::compat {

inline RetainedString ngRetainExceptionMessage(const std::exception& error) {
    // The actual Throwable field is the original immutable descriptor, including
    // null/empty/subview identity; its virtual formatted message is a different API.
    if (const auto* throwable = dynamic_cast<const Throwable*>(&error)) return throwable->msg;
    // Legacy native adapters expose bytes only. Preserve their existing fallback
    // without claiming original descriptor identity for these exception types.
    const auto bytes = [&] {
        if (const auto* message = dynamic_cast<const ExceptionMessage*>(&error))
            return message->ngExceptionMessage();
        return std::string_view(error.what());
    }();
    try {
        return RetainedString(bytes);
    } catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}

} // namespace nijigenerate::compat
