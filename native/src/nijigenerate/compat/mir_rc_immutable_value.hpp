#pragma once
#include "mir_rc_plain_value.hpp"
#include "slice.hpp"
#include <utility>

namespace nijigenerate::compat {
// An immutable Source value is a distinct type, even where a const native view
// suffices for its proven scalar-only fields. Trusted casts retain the backing;
// they do not establish that external aliases obey immutable lifetime rules.
template<MirRCPlainReadOnlyValue T> class SourceImmutableValueSlice {
    Slice<T> values_;
public:
    SourceImmutableValueSlice() = default;
    static SourceImmutableValueSlice fromTrustedDescriptor(Slice<T> values) {
        SourceImmutableValueSlice result;
        result.values_ = std::move(values);
        return result;
    }
    std::size_t size() const noexcept { return values_.size(); }
    const T* ptr() const noexcept { return values_.data(); }
    const T& operator[](std::size_t index) const { return values_[index]; }
};

template<MirRCPlainReadOnlyValue T> class SourceImmutableValueRCArray {
    MirRCArray<const T> values_;
public:
    SourceImmutableValueRCArray() = default;
    explicit SourceImmutableValueRCArray(std::size_t length, bool initialize = true, bool deallocate = true)
        : values_(length, initialize, deallocate) {}
    static SourceImmutableValueRCArray fromTrustedArray(const MirRCArray<T>& values) {
        SourceImmutableValueRCArray result;
        result.values_ = values.lightConst();
        return result;
    }
    std::size_t size() const noexcept { return values_.size(); }
    std::size_t length() const noexcept { return values_.length(); }
    std::size_t counter() const noexcept { return values_.counter(); }
    const T* ptr() const noexcept { return values_.ptr(); }
    const T& operator[](std::size_t index) const { return values_[index]; }
    explicit operator bool() const noexcept { return static_cast<bool>(values_); }
};
} // namespace nijigenerate::compat
