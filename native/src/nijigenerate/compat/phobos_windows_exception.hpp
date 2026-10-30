#pragma once
// Used std.windows.syserror.WindowsException constructor/message bodies.
// Copyright The D Language Foundation 2006-2013. BSL-1.0;
// see port/licenses/phobos-psd-file.txt.
#include "phobos_file_exception.hpp"
#ifdef _WIN32
#include <charconv>

namespace nijigenerate::compat {
class WindowsException : public ThrowableException {
    std::uint32_t code_;
    static RetainedString message(std::uint32_t code, const RetainedString& prefix) {
        std::string output;
        if (!prefix.empty()) {
            output.append(prefix.data(), prefix.size());
            if (code) output.append(": ");
        }
        if (!code) return output.empty() ? RetainedString{} : RetainedString(std::move(output));
        const auto prefixLength = output.size();
        bool success = false;
        try {
            wchar_t* buffer = nullptr;
            const auto length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
            struct Release {
                wchar_t* pointer;
                ~Release() { if (pointer) LocalFree(pointer); }
            } release{buffer};
            if (buffer) {
                using Cursor = phobos::detail::UnicodeCursor<std::span<const wchar_t>, wchar_t>;
                std::size_t first = 0, last = length;
                while (first != last && file_exception_detail::white(buffer[first])) ++first;
                while (first != last && file_exception_detail::white(buffer[last - 1])) --last;
                Cursor values(std::span<const wchar_t>{buffer + first, last - first});
                while (!values.empty()) {
                    file_exception_detail::appendScalar(output, values.front());
                    values.popFront();
                }
                success = true;
            }
        } catch (const ThrowableException&) {
            output.resize(prefixLength);
        }
        std::array<char, 10> digits;
        const auto converted = std::to_chars(digits.data(), digits.data() + digits.size(), code);
        if (success) output.append(" (error ");
        else output.append("Error ");
        output.append(digits.data(), static_cast<std::size_t>(converted.ptr - digits.data()));
        if (success) output.push_back(')');
        return RetainedString(std::move(output));
    }
public:
    explicit WindowsException(std::uint32_t code, RetainedString prefix = {},
        RetainedString file = {}, std::size_t line = 0)
        : ThrowableException(message(code, prefix), std::move(file), line), code_(code) {}
    std::uint32_t code() const noexcept { return code_; }
    std::string_view ngThrowableTypeName() const noexcept override { return "std.windows.syserror.WindowsException"; }
};
} // namespace nijigenerate::compat
#endif
