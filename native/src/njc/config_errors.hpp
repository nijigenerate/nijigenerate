#pragma once

#include <nijigenerate/compat/phobos_unicode_exception.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <new>
#include <stdexcept>
#include <concepts>
#include <type_traits>

namespace njc::config {

namespace detail {
// Call only around the actual built-in string/vector memory operation.
template<class Operation> decltype(auto) ngConfigAllocation(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            std::source_location::current());
    } catch (const std::length_error&) {
        throw nijigenerate::compat::ngStaticError<nijigenerate::compat::OutOfMemoryError>(
            std::source_location::current());
    }
}
} // namespace detail

class ConfigException : public nijigenerate::compat::ThrowableException {
public:
    template<class Message> requires std::same_as<std::remove_cvref_t<Message>,
        nijigenerate::compat::RetainedString>
    explicit ConfigException(Message&& message,
        std::source_location source = std::source_location::current())
        : ThrowableException(std::move(message), {}, source) {}
    explicit ConfigException(std::string message,
        std::source_location source = std::source_location::current())
        : ThrowableException(detail::ngConfigAllocation([&] {
            return nijigenerate::compat::RetainedString(std::move(message));
        }), {}, source) {}
    ConfigException(nijigenerate::compat::RetainedString message,
        nijigenerate::compat::RetainedString sourceFile, std::size_t sourceLine)
        : ThrowableException(std::move(message),
            std::move(sourceFile), sourceLine) {}
};

class ConversionException : public ConfigException {
public:
    using ConfigException::ConfigException;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.conv.ConvException"; }
};
class ConversionOverflowException : public ConversionException {
public:
    using ConversionException::ConversionException;
    std::string_view ngThrowableTypeName() const noexcept override { return "std.conv.ConvOverflowException"; }
};

// Actual std.utf failure payload used by numeric reporting and OS string conversion.
class ConversionUtfException : public nijigenerate::compat::UTFException {
public:
    ConversionUtfException(std::string_view suffix, bool truncated,
        std::source_location source = std::source_location::current());
    ConversionUtfException(std::uint16_t unit, const char* message,
        std::source_location source = std::source_location::current());
};

#if defined(_WIN32)
class EnvironmentWindowsException : public ConfigException {
public:
    explicit EnvironmentWindowsException(std::uint32_t code);
    std::uint32_t code() const noexcept { return code_; }
    std::string_view ngThrowableTypeName() const noexcept override {
        return "std.windows.syserror.WindowsException";
    }
private:
    std::uint32_t code_;
};
#endif

} // namespace njc::config
