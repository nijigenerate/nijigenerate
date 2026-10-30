#pragma once

#include "retained_string.hpp"
#include "array_range_error.hpp"
#include <compat/object_metadata.hpp>
#include <serialization_fields.hpp>
#include <cstddef>
#include <source_location>
#include <string_view>
#include <utility>

namespace nijigenerate::compat {

// Immutable byte-slice descriptor: copies retain the actual owning storage;
// subviews preserve its address relation and extent, including empty subviews.
class RetainedStringSlice {
public:
    RetainedStringSlice() noexcept = default;
    RetainedStringSlice(const RetainedStringSlice&) = default;
    RetainedStringSlice(RetainedStringSlice&&) noexcept = default;
    RetainedStringSlice& operator=(const RetainedStringSlice& other) noexcept {
        if (this == &other) return *this;
        auto previous = owner_;
        owner_ = other.owner_;
        offset_ = other.offset_;
        length_ = other.length_;
        return *this;
    }
    RetainedStringSlice& operator=(RetainedStringSlice&& other) noexcept {
        return *this = static_cast<const RetainedStringSlice&>(other);
    }
    RetainedStringSlice(RetainedString owner) noexcept : owner_(std::move(owner)), length_(owner_.size()) {}

    bool isNull() const noexcept { return owner_.isNull(); }
    bool empty() const noexcept { return length_ == 0; }
    std::size_t size() const noexcept { return length_; }
    const char* data() const noexcept {
        const auto base = owner_.data();
        return base ? base + offset_ : nullptr;
    }
    std::string_view view() const noexcept {
        return isNull() ? std::string_view{} : std::string_view(data(), length_);
    }
    // Extract the same immutable descriptor while retaining its actual backing.
    // No byte copy or C-string scan; null and empty subview identity survive.
    RetainedString retainedString() const {
        return owner_.subslice(offset_, offset_ + length_);
    }
    RetainedStringSlice slice(std::size_t first, std::size_t last) const {
        if (first > last || last > length_)
            throw ngStaticError<ArraySliceError>(first, last, length_, std::source_location::current());
        auto result = *this;
        result.offset_ += first;
        result.length_ = last - first;
        return result;
    }
    void reset() noexcept {
        auto previous = owner_;
        owner_.reset(); offset_ = 0; length_ = 0;
    }
    friend bool operator==(const RetainedStringSlice& left, const RetainedStringSlice& right) noexcept {
        return left.view() == right.view();
    }

private:
    RetainedString owner_;
    std::size_t offset_ = 0;
    std::size_t length_ = 0;
};

} // namespace nijigenerate::compat

namespace nicxlive::core::serde {

template<> struct SourceJsonString<::nijigenerate::compat::RetainedStringSlice> {
    static d_runtime::RetainedString read(const ::nijigenerate::compat::RetainedStringSlice& value) {
        return value.retainedString();
    }
};

} // namespace nicxlive::core::serde

namespace nicxlive::core::d_runtime {
// The retained byte descriptor represents the Source string declaration.
template<> struct SourceTypeSpellings<::nijigenerate::compat::RetainedStringSlice> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        const auto name=RetainedString::fromStaticLiteral("string");
        return {name,name,name,name};
    }
};
} // namespace nicxlive::core::d_runtime
