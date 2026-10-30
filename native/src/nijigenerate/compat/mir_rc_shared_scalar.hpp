#pragma once

#include "mir_rc_array.hpp"
#include "mir_rc_plain_value.hpp"
#include "slice.hpp"
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {
// D shared is a separate type qualifier, not native volatile or atomic storage.
// These bodies implement the selected ordinary shared-access profile for basic
// values and declared scalar-only vectors. Synchronization remains the caller's
// Source responsibility; no unrelated native aggregate is accepted.
template<class T> concept MirSharedScalar = std::is_arithmetic_v<T> &&
    !std::is_same_v<T, bool> && !std::is_const_v<T> && !std::is_volatile_v<T>;

template<MirRCPlainSharedValue T, bool ReadOnly = false> class MirSharedScalarPointer {
    T* address_ = nullptr;
    explicit MirSharedScalarPointer(T* address) noexcept : address_(address) {}
public:
    class Reference {
        T* address_;
        friend class MirSharedScalarPointer;
        explicit Reference(T* address) noexcept : address_(address) {}
    public:
        Reference(const Reference&) = default;
        operator T() const noexcept { return *address_; }
        Reference& operator=(T value) noexcept requires (!ReadOnly) {
            *address_ = value;
            return *this;
        }
        Reference& operator=(const Reference& source) noexcept requires (!ReadOnly) {
            return *this = static_cast<T>(source);
        }
        Reference& operator=(T) requires ReadOnly = delete;
        Reference& operator=(const Reference&) requires ReadOnly = delete;
    };
    MirSharedScalarPointer() = default;
    // An explicit trusted Source pointer cast, never an implicit qualifier change.
    static MirSharedScalarPointer fromTrustedAddress(T* address) noexcept {
        return MirSharedScalarPointer(address);
    }
    template<bool OtherReadOnly> requires (ReadOnly && !OtherReadOnly)
    MirSharedScalarPointer(MirSharedScalarPointer<T, OtherReadOnly> other) noexcept
        : address_(other.template trustedAddressForQualification<ReadOnly>()) {}
    Reference operator[](std::size_t index) const noexcept { return Reference(address_ + index); }
    explicit operator bool() const noexcept { return address_ != nullptr; }
private:
    template<MirRCPlainSharedValue, bool> friend class MirSharedScalarPointer;
    template<bool> T* trustedAddressForQualification() const noexcept { return address_; }
};

template<MirRCPlainSharedValue T> class SourceSharedValueSlice {
    Slice<T> values_;
public:
    SourceSharedValueSlice() = default;
    // The defining producer asserts an explicit @system shared cast here. The
    // descriptor keeps the original backing, offset and length; no copy is made.
    static SourceSharedValueSlice fromTrustedDescriptor(Slice<T> values) {
        SourceSharedValueSlice result;
        result.values_ = std::move(values);
        return result;
    }
    std::size_t size() const noexcept { return values_.size(); }
    MirSharedScalarPointer<T> ptr() noexcept {
        return MirSharedScalarPointer<T>::fromTrustedAddress(values_.data());
    }
    MirSharedScalarPointer<T, true> ptr() const noexcept {
        // The trusted cast changes access type, never the actual storage bytes.
        return MirSharedScalarPointer<T, true>::fromTrustedAddress(const_cast<T*>(values_.data()));
    }
    auto operator[](std::size_t index) {
        return MirSharedScalarPointer<T>::fromTrustedAddress(&values_[index])[0];
    }
    auto operator[](std::size_t index) const {
        return MirSharedScalarPointer<T, true>::fromTrustedAddress(const_cast<T*>(&values_[index]))[0];
    }
};

template<MirRCPlainSharedValue T> class SourceSharedValueRCArray {
    MirRCArray<T> values_;
public:
    SourceSharedValueRCArray() = default;
    explicit SourceSharedValueRCArray(std::size_t length, bool initialize = true, bool deallocate = true)
        : values_(length, initialize, deallocate) {}
    std::size_t size() const noexcept { return values_.size(); }
    std::size_t length() const noexcept { return values_.length(); }
    std::size_t counter() const noexcept { return values_.counter(); }
    explicit operator bool() const noexcept { return static_cast<bool>(values_); }
    MirSharedScalarPointer<T> ptr() noexcept {
        return MirSharedScalarPointer<T>::fromTrustedAddress(values_.ptr());
    }
    MirSharedScalarPointer<T, true> ptr() const noexcept {
        return MirSharedScalarPointer<T, true>::fromTrustedAddress(const_cast<T*>(values_.ptr()));
    }
    auto operator[](std::size_t index) {
        return MirSharedScalarPointer<T>::fromTrustedAddress(&values_[index])[0];
    }
    auto operator[](std::size_t index) const {
        return MirSharedScalarPointer<T, true>::fromTrustedAddress(const_cast<T*>(&values_[index]))[0];
    }
    // Initialization is the same explicit Unqual!T[] step as mir_rcarray.this.
    T* trustedInitializationAddress() noexcept { return values_.ptr(); }
};
template<MirSharedScalar T> using SourceSharedScalarSlice = SourceSharedValueSlice<T>;
template<MirSharedScalar T> using SourceSharedScalarRCArray = SourceSharedValueRCArray<T>;

template<MirRCPlainSharedValue T> requires MirRCPlainReadOnlyValue<T> class SourceSharedConstValueSlice {
    SourceSharedValueSlice<T> values_;
public:
    SourceSharedConstValueSlice() = default;
    static SourceSharedConstValueSlice fromTrustedDescriptor(Slice<T> values) {
        SourceSharedConstValueSlice result;
        result.values_ = SourceSharedValueSlice<T>::fromTrustedDescriptor(std::move(values));
        return result;
    }
    std::size_t size() const noexcept { return values_.size(); }
    MirSharedScalarPointer<T, true> ptr() const noexcept { return values_.ptr(); }
    auto operator[](std::size_t index) const { return values_[index]; }
};

template<MirRCPlainSharedValue T> requires MirRCPlainReadOnlyValue<T> class SourceSharedConstValueRCArray {
    SourceSharedValueRCArray<T> values_;
public:
    SourceSharedConstValueRCArray() = default;
    explicit SourceSharedConstValueRCArray(std::size_t length, bool initialize = true, bool deallocate = true)
        : values_(length, initialize, deallocate) {}
    std::size_t size() const noexcept { return values_.size(); }
    std::size_t counter() const noexcept { return values_.counter(); }
    MirSharedScalarPointer<T, true> ptr() const noexcept { return values_.ptr(); }
    auto operator[](std::size_t index) const { return values_[index]; }
};
} // namespace nijigenerate::compat
