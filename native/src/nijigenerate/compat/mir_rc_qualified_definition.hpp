#pragma once
#include "mir_rc_shared_scalar.hpp"
#include <cstring>
#include <span>
#include <utility>

namespace nijigenerate::compat {
struct MirRCSourceUnqualified {};
struct MirRCSourceConst {};
struct MirRCSourceShared {};
struct MirRCSourceImmutable {};
struct MirRCSourceSharedConst {};

// A deep qualified type's defining producer supplies actual SliceStorage and
// ArrayStorage descriptors, retention/move/swap, pointer/index access overloads,
// createIntegerValues(span<int>), sliceLength and assignInputElement bodies.
// Storage descriptors are passive native transport of those Source owners;
// their native destruction must not invent Source user callbacks. No field
// pointer, qualifier conversion or method overload is inferred by this layer.
template<class T, class Qualifier> struct MirRCQualifiedTypeDefinition;
namespace detail { struct MirRCQualifiedDescriptorAccess; }

template<class T, class Qualifier> class MirRCQualifiedSlice {
    using Definition = MirRCQualifiedTypeDefinition<T, Qualifier>;
    typename Definition::SliceStorage storage_;
    friend struct detail::MirRCQualifiedDescriptorAccess;
    explicit MirRCQualifiedSlice(typename Definition::SliceStorage storage)
        : storage_(Definition::moveSlice(storage)) {}
public:
    MirRCQualifiedSlice() : storage_(Definition::emptySlice()) {}
    static MirRCQualifiedSlice fromDeclaredDescriptor(typename Definition::SliceStorage storage) {
        return MirRCQualifiedSlice(Definition::moveSlice(storage));
    }
    MirRCQualifiedSlice(const MirRCQualifiedSlice& other)
        : storage_(Definition::retainSlice(other.storage_)) {}
    MirRCQualifiedSlice(MirRCQualifiedSlice&& other)
        : storage_(Definition::moveSlice(other.storage_)) {}
    MirRCQualifiedSlice& operator=(MirRCQualifiedSlice other) {
        Definition::swapSlices(storage_, other.storage_);
        return *this;
    }
    std::size_t size() const { return Definition::sliceLength(storage_); }
    decltype(auto) ptr() { return Definition::slicePointer(storage_); }
    decltype(auto) ptr() const { return Definition::slicePointer(storage_); }
    decltype(auto) operator[](std::size_t i) { return Definition::sliceElement(storage_, i); }
    decltype(auto) operator[](std::size_t i) const { return Definition::sliceElement(storage_, i); }
};

template<class T, class Qualifier> class MirRCQualifiedArray {
    using Definition = MirRCQualifiedTypeDefinition<T, Qualifier>;
    typename Definition::ArrayStorage storage_;
    friend struct detail::MirRCQualifiedDescriptorAccess;
    explicit MirRCQualifiedArray(typename Definition::ArrayStorage storage)
        : storage_(Definition::moveArray(storage)) {}
public:
    MirRCQualifiedArray() : storage_(Definition::emptyArray()) {}
    static MirRCQualifiedArray fromProducedDescriptor(typename Definition::ArrayStorage storage) {
        return MirRCQualifiedArray(Definition::moveArray(storage));
    }
    MirRCQualifiedArray(const MirRCQualifiedArray& other)
        : storage_(Definition::retainArray(other.storage_)) {}
    MirRCQualifiedArray(MirRCQualifiedArray&& other)
        : storage_(Definition::moveArray(other.storage_)) {}
    MirRCQualifiedArray& operator=(MirRCQualifiedArray other) {
        Definition::swapArrays(storage_, other.storage_);
        return *this;
    }
    std::size_t size() const { return Definition::arrayLength(storage_); }
    std::size_t length() const { return size(); }
    std::size_t counter() const { return Definition::arrayCounter(storage_); }
    void proxySwap(MirRCQualifiedArray& other) { Definition::swapArrays(storage_, other.storage_); }
    decltype(auto) ptr() { return Definition::arrayPointer(storage_); }
    decltype(auto) ptr() const { return Definition::arrayPointer(storage_); }
    decltype(auto) operator[](std::size_t i) { return Definition::arrayElement(storage_, i); }
    decltype(auto) operator[](std::size_t i) const { return Definition::arrayElement(storage_, i); }
};

namespace detail {
// Internal typed bridge used by the real converter, never an implicit cast.
struct MirRCQualifiedDescriptorAccess {
    template<class T, class Q> static auto& slice(MirRCQualifiedSlice<T, Q>& value) {
        return value.storage_;
    }
    template<class T, class Q> static auto takeArray(MirRCQualifiedArray<T, Q>& value) {
        return MirRCQualifiedTypeDefinition<T, Q>::moveArray(value.storage_);
    }
};
} // namespace detail

// Default raw-compatible unqualified mapping. A defining producer with a
// different native storage representation specializes this binding with its
// actual descriptors and typed access bodies, retaining Source type identity.
template<class T> struct MirRCQualifiedTypeDefinition<T, MirRCSourceUnqualified> {
    using SliceStorage = Slice<T>;
    using ArrayStorage = MirRCArray<T>;
    static SliceStorage emptySlice() { return {}; }
    static ArrayStorage emptyArray() { return {}; }
    static SliceStorage retainSlice(const SliceStorage& value) { return value; }
    static SliceStorage moveSlice(SliceStorage& value) { return std::move(value); }
    static ArrayStorage retainArray(const ArrayStorage& value) { return value; }
    static ArrayStorage moveArray(ArrayStorage& value) { return std::move(value); }
    static void swapSlices(SliceStorage& a, SliceStorage& b) { std::swap(a, b); }
    static void swapArrays(ArrayStorage& a, ArrayStorage& b) { a.proxySwap(b); }
    static std::size_t sliceLength(const SliceStorage& value) { return value.size(); }
    static std::size_t arrayLength(const ArrayStorage& value) { return value.size(); }
    static std::size_t arrayCounter(const ArrayStorage& value) { return value.counter(); }
    static auto slicePointer(SliceStorage& value) { return value.data(); }
    static auto slicePointer(const SliceStorage& value) { return value.data(); }
    static auto arrayPointer(ArrayStorage& value) { return value.ptr(); }
    static auto arrayPointer(const ArrayStorage& value) { return value.ptr(); }
    static decltype(auto) sliceElement(SliceStorage& value, std::size_t i) { return value[i]; }
    static decltype(auto) sliceElement(const SliceStorage& value, std::size_t i) { return value[i]; }
    static decltype(auto) arrayElement(ArrayStorage& value, std::size_t i) { return value[i]; }
    static decltype(auto) arrayElement(const ArrayStorage& value, std::size_t i) { return value[i]; }
    static ArrayStorage createIntegerValues(std::span<const std::int32_t> arguments) {
        if constexpr (std::is_same_v<typename MirRCIntConstruction<std::remove_cv_t<T>>::SourceCase,
            MirRCInvalidSourceIntBody>) {
            MirRCIntConstruction<std::remove_cv_t<T>>::emplace(nullptr, 0);
            return {};
        } else if constexpr (std::is_enum_v<T> || (std::is_integral_v<T> &&
            (sizeof(T) < sizeof(std::int32_t) || std::is_same_v<T, bool>))) {
            static_assert(std::is_same_v<T, void>,
                "Source emplace cannot implicitly construct this type from a dynamic int");
            return {};
        } else {
            using Operations = MirRCElementOperations<std::remove_cv_t<T>>;
            ArrayStorage result(arguments.size(), Operations::elaborateDestructor);
            if constexpr (std::is_same_v<T, std::int32_t> && !Operations::elaborateAssign)
                std::memcpy(result.ptr(), arguments.data(), arguments.size_bytes());
            else for (std::size_t i = 0; i < arguments.size(); ++i)
                MirRCIntConstruction<T>::emplace(result.ptr() + i, arguments[i]);
            return result;
        }
    }
    static void assignInputElement(ArrayStorage& output, std::size_t destination,
        SliceStorage& input, std::size_t source) {
        MirRCElementOperations<std::remove_cv_t<T>>::assignElement(output.ptr()[destination], input[source]);
    }
};

// This real binding consumes the already implemented canonical owners. Other
// deep types provide their actual qualified operations, not a native T cast.
template<MirRCPlainSharedValue T> struct MirRCQualifiedTypeDefinition<T, MirRCSourceShared> {
    using SliceStorage = SourceSharedValueSlice<T>;
    using ArrayStorage = SourceSharedValueRCArray<T>;
    static SliceStorage emptySlice() { return {}; }
    static ArrayStorage emptyArray() { return {}; }
    static SliceStorage retainSlice(const SliceStorage& value) { return value; }
    static SliceStorage moveSlice(SliceStorage& value) { return std::move(value); }
    static ArrayStorage retainArray(const ArrayStorage& value) { return value; }
    static ArrayStorage moveArray(ArrayStorage& value) { return std::move(value); }
    static void swapSlices(SliceStorage& a, SliceStorage& b) { std::swap(a, b); }
    static void swapArrays(ArrayStorage& a, ArrayStorage& b) { std::swap(a, b); }
    static std::size_t sliceLength(const SliceStorage& value) { return value.size(); }
    static std::size_t arrayLength(const ArrayStorage& value) { return value.size(); }
    static std::size_t arrayCounter(const ArrayStorage& value) { return value.counter(); }
    static auto slicePointer(SliceStorage& value) { return value.ptr(); }
    static auto slicePointer(const SliceStorage& value) { return value.ptr(); }
    static auto arrayPointer(ArrayStorage& value) { return value.ptr(); }
    static auto arrayPointer(const ArrayStorage& value) { return value.ptr(); }
    static auto sliceElement(SliceStorage& value, std::size_t i) { return value[i]; }
    static auto sliceElement(const SliceStorage& value, std::size_t i) { return value[i]; }
    static auto arrayElement(ArrayStorage& value, std::size_t i) { return value[i]; }
    static auto arrayElement(const ArrayStorage& value, std::size_t i) { return value[i]; }
    static ArrayStorage createIntegerValues(std::span<const std::int32_t> arguments) {
        if constexpr (std::is_same_v<typename MirRCIntConstruction<T>::SourceCase, MirRCInvalidSourceIntBody>) {
            MirRCIntConstruction<T>::emplace(nullptr, 0);
            return {};
        } else {
            using Operations = MirRCElementOperations<T>;
            ArrayStorage result(arguments.size(), Operations::elaborateDestructor);
            auto* initialization = result.trustedInitializationAddress();
            if constexpr (std::is_same_v<T, std::int32_t> && !Operations::elaborateAssign)
                std::memcpy(initialization, arguments.data(), arguments.size_bytes());
            else for (std::size_t i = 0; i < arguments.size(); ++i)
                MirRCIntConstruction<T>::emplace(initialization + i, arguments[i]);
            return result;
        }
    }
    static void assignInputElement(ArrayStorage& output, std::size_t destination,
        SliceStorage& input, std::size_t source) {
        output.ptr()[destination] = static_cast<T>(input[source]);
    }
};
} // namespace nijigenerate::compat
