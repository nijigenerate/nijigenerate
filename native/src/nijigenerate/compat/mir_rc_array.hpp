#pragma once

#include "mir_rc_context.hpp"
#include "mir_rc_element_definition.hpp"
#include "out_of_memory_error.hpp"
#include "retained_string.hpp"
#include <bit>
#include <concepts>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {
// Mapped aggregates provide Source size, initializer, elaborate flags and actual
// bodies. Native default construction/destruction traits do not substitute them.
template<class T, class = void> struct MirRCElementOperations : MirRCDefinedElementOperations<T> {};

template<class T> struct MirRCElementOperations<T, std::enable_if_t<
    std::is_integral_v<T> ||
    std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_same_v<T, long double>>> {
    inline static const MirRCTypeInfo typeInfo{nullptr, static_cast<std::int32_t>(sizeof(T))};
    inline static constexpr bool elaborateAssign = false;
    inline static constexpr bool elaborateDestructor = false;
private:
    static constexpr bool character = std::is_same_v<T, char> || std::is_same_v<T, char8_t> ||
        std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t> || std::is_same_v<T, wchar_t>;
    static constexpr T characterInitializer() {
        if constexpr (std::is_same_v<T, char32_t> || (std::is_same_v<T, wchar_t> && sizeof(T) == 4))
            return static_cast<T>(0xFFFFu);
        else return std::bit_cast<T>((std::numeric_limits<std::make_unsigned_t<T>>::max)());
    }
public:
    static const void* payload() noexcept {
        if constexpr (character) {
            static constexpr T initializer = characterInitializer();
            return &initializer;
        } else return nullptr;
    }
    static void fillDefault(T* values, std::size_t length) {
        for (std::size_t i = 0; i < length; ++i) {
            if constexpr (std::is_floating_point_v<T>)
                std::construct_at(values + i, std::numeric_limits<T>::quiet_NaN());
            else if constexpr (character)
                std::construct_at(values + i, characterInitializer());
            else std::construct_at(values + i, T{0});
        }
    }
    static void assignElement(T& destination, T& source) { destination = source; }
    static void emplaceInt(T* destination, std::int32_t value) {
        if constexpr (std::is_integral_v<T> &&
            (sizeof(T) < sizeof(std::int32_t) || std::is_same_v<T, bool>)) {
            static_assert(std::is_same_v<T, void>,
                "Source emplace cannot implicitly construct this type from a dynamic int");
        } else std::construct_at(destination, value);
    }
};

// The Source module owns one immutable OutOfMemoryError. Its single string
// argument is the real OutOfMemoryError file parameter, not a custom message.
inline const OutOfMemoryError mirRCAllocationError(
    RetainedString::fromStaticLiteral("mir_rcarray: out of memory error."), 17);

template<class T> class MirRCArray {
    template<class> friend class MirRCArray;
    template<class> friend struct MirRCPlainConstProjection;
    using Element = std::remove_cv_t<T>;
    T* payload_ = nullptr;
    MirRCContext& context() const {
        ngMirRCAssert(payload_ != nullptr, "mir/rc/array.d", 32);
        return *(reinterpret_cast<MirRCContext*>(const_cast<Element*>(payload_)) - 1);
    }
public:
    MirRCArray() = default;
    MirRCArray(std::nullptr_t) noexcept {}
    explicit MirRCArray(std::size_t length, bool initialize = true, bool deallocate = true) {
        if (!length) return;
        using Operations = MirRCElementOperations<Element>;
        auto* owner = ngMirRCCreate(Operations::typeInfo, length, Operations::payload(), initialize, deallocate);
        if (!owner) throw mirRCAllocationError;
        payload_ = reinterpret_cast<T*>(owner + 1);
        // Exactly the Source constructor's second, independent initialization pass.
        if (initialize || Operations::elaborateAssign)
            Operations::fillDefault(const_cast<Element*>(payload_), length);
    }
    MirRCArray(const MirRCArray& other) : payload_(other.payload_) {
        if (payload_) ngMirRCIncrease(context());
    }
    template<class Q> requires (std::same_as<std::remove_cv_t<Q>, Element> &&
        std::is_convertible_v<Q*, T*>)
    MirRCArray(const MirRCArray<Q>& other) : payload_(other.payload_) {
        if (payload_) ngMirRCIncrease(context());
    }
    MirRCArray(MirRCArray&& other) noexcept : payload_(std::exchange(other.payload_, nullptr)) {}
    ~MirRCArray() noexcept(false) { if (payload_) ngMirRCDecrease(context()); }
    MirRCArray& operator=(MirRCArray other) {
        proxySwap(other);
        return *this;
    }
    MirRCArray& operator=(std::nullptr_t) { return *this = MirRCArray{}; }
    void proxySwap(MirRCArray& other) noexcept { std::swap(payload_, other.payload_); }
    std::size_t size() const noexcept { return payload_ ? context().length : 0; }
    std::size_t length() const noexcept { return size(); }
    bool empty() const noexcept { return !size(); }
    explicit operator bool() const noexcept { return payload_ != nullptr; }
    T* ptr() noexcept { return payload_; }
    const T* ptr() const noexcept { return payload_; }
    T* data() noexcept { return payload_; }
    const T* data() const noexcept { return payload_; }
    std::size_t counter() const noexcept { return payload_ ? context().counter.load() : 0; }
    T& operator[](std::size_t index) {
        ngMirRCAssert(payload_ != nullptr, "mir/rc/array.d", 125);
        ngMirRCAssert(index < size(), "mir/rc/array.d", 126);
        return payload_[index];
    }
    const T& operator[](std::size_t index) const {
        ngMirRCAssert(payload_ != nullptr, "mir/rc/array.d", 125);
        ngMirRCAssert(index < size(), "mir/rc/array.d", 126);
        return payload_[index];
    }
    auto lightConst() const { return MirRCConstProjection<T>::retain(*this); }
    auto moveToConst() { return MirRCConstProjection<T>::move(*this); }
};

template<class T> struct MirRCPlainConstProjection {
    using Element = std::remove_cv_t<T>;
    static MirRCArray<const Element> retain(const MirRCArray<T>& value) {
        return MirRCArray<const Element>(value);
    }
    static MirRCArray<const Element> move(MirRCArray<T>& value) {
        MirRCArray<const Element> result;
        result.payload_ = std::exchange(value.payload_, nullptr);
        return result;
    }
};
template<class T> struct MirRCConstProjection<T, std::enable_if_t<std::is_arithmetic_v<T>>>
    : MirRCPlainConstProjection<T> {};
} // namespace nijigenerate::compat
