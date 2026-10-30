#pragma once

#include "slice.hpp"
#include "const_slice.hpp"
#include "retained_string_slice.hpp"
#include <common/shared_array.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {

// The native char representation may be signed; Source char is an unsigned
// UTF-8 code unit even when automatic decoding is disabled.
inline char8_t ngSourceCharCodeUnit(char value) noexcept {
    return static_cast<char8_t>(static_cast<unsigned char>(value));
}

template<class T> concept SourceByteStorageReferenceElement =
    std::same_as<std::remove_const_t<T>, std::uint16_t>
    || std::same_as<std::remove_const_t<T>, float>
    || std::same_as<std::remove_const_t<T>, char8_t>;

template<class T> concept SourceByteReferenceElement = !std::is_volatile_v<T>
    && (SourceByteStorageReferenceElement<T>
        || std::same_as<std::remove_const_t<T>, std::uint8_t>
        || std::same_as<std::remove_const_t<T>, std::int32_t>);

template<class T, class Object> concept SourceNativeObjectReference = SourceByteReferenceElement<T>
    && std::is_lvalue_reference_v<Object&&>
    && !std::is_volatile_v<std::remove_reference_t<Object>>
    && std::same_as<std::remove_cvref_t<Object>, std::remove_const_t<T>>
    && (std::is_const_v<T> || !std::is_const_v<std::remove_reference_t<Object>>);

// A Source scalar reference need not designate a live, aligned native T object.
// Its byte-backed form retains the original allocation and addresses that cell
// on every access, including accesses after another alias or callback writes it.
template<SourceByteReferenceElement T> class SourceReference {
    template<SourceByteReferenceElement> friend class SourceReference;
    std::shared_ptr<const void> byteOwner_;
    const void* byteAddress_ = nullptr;
    void* writableByteAddress_ = nullptr;
    T* object_ = nullptr;
    bool byteBacked_ = false;

    SourceReference(std::shared_ptr<const void> owner, const void* address, void* writableAddress) noexcept
        : byteOwner_(std::move(owner)), byteAddress_(address), writableByteAddress_(writableAddress),
          byteBacked_(true) {}
    SourceReference(std::shared_ptr<const void> owner, T& object) noexcept
        : byteOwner_(std::move(owner)), object_(std::addressof(object)) {}

public:
    using element_type = T;
    using value_type = std::remove_const_t<T>;
    SourceReference() = delete;
    template<class Object> requires SourceNativeObjectReference<T, Object>
    explicit SourceReference(Object&& object) noexcept : object_(std::addressof(object)) {}
    SourceReference(value_type&&) = delete;
    SourceReference(const value_type&&) = delete;
    SourceReference(const SourceReference&) noexcept = default;
    // Moving a lowered reference preserves the original alias as well. This is
    // a reference binding, not ownership transfer of a Source array variable.
    SourceReference(SourceReference&& other) noexcept
        : byteOwner_(other.byteOwner_), byteAddress_(other.byteAddress_),
          writableByteAddress_(other.writableByteAddress_), object_(other.object_), byteBacked_(other.byteBacked_) {}

    template<SourceByteReferenceElement U>
        requires (std::is_const_v<T> && !std::is_const_v<U>
            && std::same_as<std::remove_const_t<T>, U>)
    SourceReference(const SourceReference<U>& other) noexcept
        : byteOwner_(other.byteOwner_), byteAddress_(other.byteAddress_), writableByteAddress_(nullptr),
          object_(other.object_), byteBacked_(other.byteBacked_) {}

    // The defining cursor supplies the actual allocation owner and live typed
    // object. Ownership transport does not create an object or allocate a box.
    template<class Object> requires SourceNativeObjectReference<T, Object>
    static SourceReference fromRetainedObjectCell(std::shared_ptr<const void> owner, Object&& object) noexcept {
        return SourceReference(std::move(owner), object);
    }
    static SourceReference fromRetainedObjectCell(std::shared_ptr<const void>, value_type&&) = delete;
    static SourceReference fromRetainedObjectCell(std::shared_ptr<const void>, const value_type&&) = delete;

    // The declaring iterator performs its own Source check before dereference.
    static SourceReference fromBytesUnchecked(Slice<std::uint8_t> bytes, std::size_t offset) noexcept
        requires SourceByteStorageReferenceElement<T> {
        auto* cell = bytes.data() + offset;
        return SourceReference(bytes.retentionOwner(), cell, cell);
    }
    // Select only for an actual Source char declaration. Native char signedness
    // is irrelevant: all code-unit reads/writes use the unsigned representation.
    static SourceReference fromCodeUnitsUnchecked(Slice<char> units, std::size_t index) noexcept
        requires std::same_as<value_type, char8_t> {
        auto* cell = units.data() + index;
        return SourceReference(units.retentionOwner(), cell, cell);
    }
    static SourceReference fromCodeUnitsUnchecked(ConstSlice<char> units, std::size_t index) noexcept
        requires (std::same_as<value_type, char8_t> && std::is_const_v<T>) {
        return SourceReference(units.retentionOwner(), units.data() + index, nullptr);
    }
    static SourceReference fromCodeUnitsUnchecked(Slice<char8_t> units, std::size_t index) noexcept
        requires std::same_as<value_type, char8_t> {
        auto* cell = units.data() + index;
        return SourceReference(units.retentionOwner(), cell, cell);
    }
    static SourceReference fromCodeUnitsUnchecked(ConstSlice<char8_t> units, std::size_t index) noexcept
        requires (std::same_as<value_type, char8_t> && std::is_const_v<T>) {
        return SourceReference(units.retentionOwner(), units.data() + index, nullptr);
    }
    static SourceReference fromCodeUnitsUnchecked(RetainedString units, std::size_t index) noexcept
        requires (std::same_as<value_type, char8_t> && std::is_const_v<T>) {
        return SourceReference(units.retentionOwner(), units.data() + index, nullptr);
    }
    static SourceReference fromCodeUnitsUnchecked(const RetainedStringSlice& units, std::size_t index)
        requires (std::same_as<value_type, char8_t> && std::is_const_v<T>) {
        return fromCodeUnitsUnchecked(units.retainedString(), index);
    }
    template<class Unit>
        requires (std::same_as<value_type, char8_t>
            && (std::same_as<Unit, char> || std::same_as<Unit, char8_t>))
    static SourceReference fromCodeUnitsUnchecked(nicxlive::core::common::SharedArray<Unit>& units,
        std::size_t index) noexcept {
        auto* cell = units.data() + index;
        return SourceReference(units.retentionOwner(), cell, cell);
    }
    template<class Unit>
        requires (std::same_as<value_type, char8_t> && std::is_const_v<T>
            && (std::same_as<Unit, char> || std::same_as<Unit, char8_t>))
    static SourceReference fromCodeUnitsUnchecked(const nicxlive::core::common::SharedArray<Unit>& units,
        std::size_t index) noexcept {
        return SourceReference(units.retentionOwner(), units.data() + index, nullptr);
    }
    // Read-only defining ranges provide their genuine existing retention token;
    // the token is not synthesized from a borrowed pointer or a native type.
    static SourceReference fromRetainedCharCell(std::shared_ptr<const void> owner, const char* cell) noexcept
        requires (std::same_as<value_type, char8_t> && std::is_const_v<T>) {
        return SourceReference(std::move(owner), cell, nullptr);
    }
    static SourceReference fromRetainedCharCell(std::shared_ptr<void> owner, char* cell) noexcept
        requires std::same_as<value_type, char8_t> {
        return SourceReference(std::move(owner), cell, cell);
    }
    static SourceReference fromCharCell(char& cell) noexcept requires std::same_as<value_type, char8_t> {
        return SourceReference({}, std::addressof(cell), std::addressof(cell));
    }
    static SourceReference fromCharCell(const char& cell) noexcept
        requires (std::same_as<value_type, char8_t> && std::is_const_v<T>) {
        return SourceReference({}, std::addressof(cell), nullptr);
    }
    static SourceReference fromCharCell(char&&) requires std::same_as<value_type, char8_t> = delete;
    static SourceReference fromCharCell(const char&&) requires std::same_as<value_type, char8_t> = delete;
    value_type load() const {
        if (!byteBacked_) return *object_;
        value_type value;
        std::memcpy(&value, byteAddress_, sizeof(value));
        return value;
    }
    void store(value_type value) const requires (!std::is_const_v<T>) {
        if (!byteBacked_) *object_ = value;
        else std::memcpy(writableByteAddress_, &value, sizeof(value));
    }
    operator value_type() const { return load(); }
    const SourceReference& operator=(value_type value) const requires (!std::is_const_v<T>) {
        store(value);
        return *this;
    }
    const SourceReference& operator=(const SourceReference& other) const requires (!std::is_const_v<T>) {
        store(other.load());
        return *this;
    }
    const SourceReference& operator=(const SourceReference&) const requires std::is_const_v<T> = delete;

    auto asConst() const noexcept {
        return SourceReference<const value_type>(*this);
    }
    const void* address() const noexcept {
        return byteBacked_ ? byteAddress_
            : static_cast<const void*>(object_);
    }
    template<SourceByteReferenceElement U> requires std::same_as<value_type, std::remove_const_t<U>>
    bool sameLocation(const SourceReference<U>& other) const noexcept { return address() == other.address(); }
};

template<class T> struct SourceReferenceType : std::false_type {};
template<SourceByteReferenceElement T>
struct SourceReferenceType<SourceReference<T>> : std::true_type { using element_type = T; };

template<class Value, class Argument> struct SourceReferenceValueMatches : std::false_type {};
template<class Value, SourceByteReferenceElement T>
struct SourceReferenceValueMatches<Value, SourceReference<T>>
    : std::bool_constant<std::same_as<Value, std::remove_const_t<T>>> {};

template<class Argument> struct SourcePrimitiveArgumentType {
    using type = std::conditional_t<std::same_as<Argument, char>, char8_t, Argument>;
};
template<SourceByteReferenceElement T> struct SourcePrimitiveArgumentType<SourceReference<T>> {
    using type = std::remove_const_t<T>;
};

// D 2.111.0 type.html: integer types widen by width and implicitly convert to
// floating point. This matrix covers only the actual five declared families;
// it does not infer conversions from C++ constructors, enums or range analysis.
template<class To, class From> inline constexpr bool sourcePrimitiveValueConversion =
    (SourceByteReferenceElement<From> && std::same_as<To, From>)
    || ((std::same_as<From, char8_t> || std::same_as<From, std::uint8_t>)
        && (std::same_as<To, char8_t> || std::same_as<To, std::uint8_t>
            || std::same_as<To, std::uint16_t> || std::same_as<To, std::int32_t> || std::same_as<To, float>))
    || (std::same_as<From, std::uint16_t> && (std::same_as<To, std::int32_t> || std::same_as<To, float>))
    || (std::same_as<From, std::int32_t> && std::same_as<To, float>);

template<SourceByteReferenceElement Value, class Argument>
    requires (!std::is_const_v<Value>
        && sourcePrimitiveValueConversion<Value,
            typename SourcePrimitiveArgumentType<std::remove_cvref_t<Argument>>::type>)
Value ngBindSourcePrimitiveValue(Argument&& argument) {
    if constexpr (SourceReferenceType<std::remove_cvref_t<Argument>>::value)
        return static_cast<Value>(argument.load());
    else if constexpr (std::same_as<std::remove_cvref_t<Argument>, char>)
        return static_cast<Value>(ngSourceCharCodeUnit(argument));
    else return static_cast<Value>(std::forward<Argument>(argument));
}

// Use in the actual call's braced argument frame, so Source binding happens in
// argument order. A native scalar rvalue cannot bind; byte cells stay cells.
template<SourceByteReferenceElement T, class Argument>
    requires std::constructible_from<SourceReference<T>, Argument&&>
SourceReference<T> ngBindSourceReference(Argument&& argument) {
    return SourceReference<T>(std::forward<Argument>(argument));
}

} // namespace nijigenerate::compat
