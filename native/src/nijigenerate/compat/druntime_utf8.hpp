#pragma once
// Port of the installed independent core.internal.utf.decode(char[], ref size_t).
// Copyright Digital Mars 2003-2016. BSL-1.0; see port/licenses/druntime-utf8.txt.
#include "array_range_error.hpp"
#include "assert_error.hpp"
#include "phobos_unicode_exception.hpp"
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace nijigenerate::compat {

// The caller retains the immutable input descriptor for the cursor's lifetime.
class DruntimeUtf8Cursor {
public:
    explicit DruntimeUtf8Cursor(std::string_view input) : input_(input) {}
    bool empty() const noexcept { return offset_ == input_.size(); }
    std::size_t offset() const noexcept { return offset_; }
    char32_t take() {
        if (empty()) {
#ifndef NDEBUG
            throw AssertError{"Assertion failure", __FILE__, __LINE__};
#else
            throw ArrayIndexError(offset_, input_.size());
#endif
        }
        const auto head = unit(offset_);
        std::uint32_t scalar = head;
        std::size_t width = 1;
        if (head & 0x80) {
            for (;; ++width) {
                if (width > 4) invalid();
                if (((head << width) & 0x80) == 0) {
                    if (width == 1) invalid();
                    break;
                }
            }
            scalar = head & ((1u << (7 - width)) - 1);
            if (width - 1 >= input_.size() - offset_) invalid();
            const auto second = unit(offset_ + 1);
            if ((head & 0xfe) == 0xc0 || (head == 0xe0 && (second & 0xe0) == 0x80) ||
                (head == 0xf0 && (second & 0xf0) == 0x80)) invalid();
            for (std::size_t j = 1; j < width; ++j) {
                const auto next = unit(offset_ + j);
                if ((next & 0xc0) != 0x80) invalid();
                scalar = (scalar << 6) | (next & 0x3f);
            }
            if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) invalid();
        }
        // Failed decoding leaves the original byte index unchanged.
        offset_ += width;
        return static_cast<char32_t>(scalar);
    }
private:
    std::uint32_t unit(std::size_t index) const {
        return static_cast<unsigned char>(input_[index]);
    }
    [[noreturn]] void invalid() const {
        throw UnicodeException(RetainedString::fromStaticLiteral("invalid UTF-8 sequence"), offset_,
            RetainedString::fromStaticLiteral("core/internal/utf.d"), 294);
    }
    std::string_view input_;
    std::size_t offset_ = 0;
};

} // namespace nijigenerate::compat
