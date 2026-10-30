#pragma once
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/compat/throwable.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <cstdint>
#include <functional>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::platform {
namespace detail {
template<class Operation> decltype(auto) allocateLocaleExceptionStorage(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
class LocaleScanFileError : public compat::ThrowableException {
public:
    LocaleScanFileError(std::uint32_t code, std::string message)
        : compat::ThrowableException(detail::allocateLocaleExceptionStorage([&] {
              return compat::RetainedString(message);
          })),
          nativeCode(code), messageBytes(std::move(message)) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.file.FileException"; }
    std::uint32_t nativeCode;
    std::string messageBytes;
};

class LocalePathWindowsError : public compat::ThrowableException {
public:
    LocalePathWindowsError(std::uint32_t code, compat::RetainedString message)
        : compat::ThrowableException(std::move(message)), nativeCode(code) {}
    std::uint32_t code() const noexcept { return nativeCode; }
    std::string_view ngThrowableTypeName() const noexcept override {
        return "std.windows.syserror.WindowsException";
    }
    const std::uint32_t nativeCode;
};

class LocalePathErrnoError : public compat::ThrowableException {
public:
    LocalePathErrnoError(std::uint32_t code, std::string error)
        : compat::ThrowableException(detail::allocateLocaleExceptionStorage([&] {
              return compat::RetainedString(" (" + error + ")");
          })),
          nativeCode(code), errnoMessage(std::move(error)) {}
    std::string_view ngThrowableTypeName() const noexcept override { return "std.exception.ErrnoException"; }
    const std::uint32_t nativeCode;
    const std::string errnoMessage;
};
[[noreturn]] void throwLocaleScanFileError(compat::RetainedString name, std::uint32_t code);
// Calls the consumer before fetching the next real shallow directory entry.
void visitLocaleCatalogEntries(compat::RetainedString path,
    const std::function<void(compat::RetainedString)>& consumer);
compat::RetainedString localeExecutableDirectory();
// Null means this target has no additional executable-relative scan location.
compat::RetainedString localePlatformScanDirectory();
}
