#pragma once

#include "slice.hpp"
#include "assert_error.hpp"
#include "retained_string.hpp"
#include "out_of_memory_error.hpp"
#include "native_access_fault.hpp"
#include "native_arithmetic_fault.hpp"
#include "phobos_ranges.hpp"
#include <compat/object_metadata.hpp>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <string>
#include <type_traits>

namespace nijigenerate::compat {

// Shared retained rank metadata/index kernel, extracted from DistanceOutput.
template<class T, std::size_t Rank, class Storage = Slice<T>> class ContiguousView {
public:
    ContiguousView() = default;
    ContiguousView(Storage storage, std::array<std::size_t, Rank> shape, std::size_t iteratorOffset = 0)
        : storage_(std::move(storage)), shape_(shape), iteratorOffset_(iteratorOffset) {}
    const auto& shape() const { return shape_; }
    // Storage retains the backing; iteratorOffset identifies the current iterator.
    const Storage& storage() const { return storage_; }
    Storage& storage() { return storage_; }
    auto sourceReferenceFront() requires (Rank == 1 && SourceByteReferenceElement<T>) {
        decltype(auto) cell = front();
        if constexpr (SourceReferenceType<std::remove_cvref_t<decltype(cell)>>::value) return cell;
        else return SourceReference<T>::fromRetainedObjectCell(storage_.retentionOwner(), cell);
    }
    auto sourceReferenceBack() requires (Rank == 1 && SourceByteReferenceElement<T>) {
        decltype(auto) cell = back();
        if constexpr (SourceReferenceType<std::remove_cvref_t<decltype(cell)>>::value) return cell;
        else return SourceReference<T>::fromRetainedObjectCell(storage_.retentionOwner(), cell);
    }
    template<class... Indices> requires (sizeof...(Indices) == Rank && SourceByteReferenceElement<T>)
    auto sourceReferenceAt(Indices... indices) {
        decltype(auto) cell = at(indices...);
        if constexpr (SourceReferenceType<std::remove_cvref_t<decltype(cell)>>::value) return cell;
        else return SourceReference<T>::fromRetainedObjectCell(storage_.retentionOwner(), cell);
    }
    std::size_t iteratorOffset() const { return iteratorOffset_; }
    decltype(auto) uncheckedElement(std::size_t index) {
        if constexpr (requires { storage_.unchecked(index); }) return storage_.unchecked(iteratorOffset_ + index);
        else return storage_.data()[iteratorOffset_ + index];
    }
    decltype(auto) uncheckedElement(std::size_t index) const {
        if constexpr (requires { storage_.unchecked(index); }) return storage_.unchecked(iteratorOffset_ + index);
        else return storage_.data()[iteratorOffset_ + index];
    }
    template<std::size_t OtherRank>
    auto reshaped(std::array<std::size_t, OtherRank> dimensions) {
        return ContiguousView<T, OtherRank, Storage>(storage_, dimensions, iteratorOffset_);
    }
    // Selected Mir topology.d1431: the actually used mutable byte-pointer
    // contiguous 1D-to-2D reshape. Failure keeps the shape and a null iterator.
    auto reshape(std::array<std::ptrdiff_t, 2> dimensions, std::int32_t& error)
        requires (Rank == 1 && std::same_as<T, std::uint8_t> && std::same_as<Storage, Slice<std::uint8_t>>) {
        std::array<std::size_t, 2> lengths{
            static_cast<std::size_t>(dimensions[0]), static_cast<std::size_t>(dimensions[1])};
        const std::size_t oldCount = elementCount();
        std::size_t newCount = lengths[0] * lengths[1];
        using Result = ContiguousView<T, 2, Storage>;
        if (oldCount == 0) { error = 1; return Result(Storage{}, lengths); }
        for (auto& length : lengths) {
            if (length != std::size_t(-1)) continue;
            newCount = std::size_t(0) - newCount;
            if (newCount == 0) ngFailIntegerDivideByZero();
            length = oldCount / newCount;
            newCount *= length;
            break;
        }
        if (oldCount != newCount) { error = 2; return Result(Storage{}, lengths); }
        error = 0;
        return Result(storage_, lengths, iteratorOffset_);
    }
    // Native path.hpp spells Source skeleton.length as size(), not elementCount.
    std::size_t size() const
        requires (Rank == 2 && std::same_as<T, std::uint8_t> && std::same_as<Storage, Slice<std::uint8_t>>) {
        return shape_[0];
    }
    decltype(auto) operator()(std::size_t row, std::size_t column)
        requires (Rank == 2 && std::same_as<T, std::uint8_t> && std::same_as<Storage, Slice<std::uint8_t>>) {
        checkBytePointer(row, column);
        return at(row, column);
    }
    decltype(auto) operator()(std::size_t row, std::size_t column) const
        requires (Rank == 2 && std::same_as<T, std::uint8_t> && std::same_as<Storage, Slice<std::uint8_t>>) {
        checkBytePointer(row, column);
        return at(row, column);
    }
    auto save() { return *this; }
    template<std::size_t Dimension = 0> requires (Dimension < Rank)
    std::size_t length() const { return shape_[Dimension]; }
    template<std::size_t Dimension = 0> requires (Dimension < Rank)
    bool empty() const { return shape_[Dimension] == 0; }
    bool anyEmpty() const {
        for (const auto dimension : shape_) if (dimension == 0) return true;
        return false;
    }
    template<std::size_t Dimension = 0> requires (Dimension < Rank)
    std::ptrdiff_t stride() const {
        std::size_t product = 1;
        for (std::size_t i = Rank; i > Dimension + 1;) product *= shape_[--i];
        return static_cast<std::ptrdiff_t>(product);
    }
    decltype(auto) front() requires (Rank == 1) {
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 1529);
#endif
        // Source dereferences the iterator directly after its own assertion.
        return uncheckedElement(0);
    }
    decltype(auto) front() const requires (Rank == 1) {
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 1537);
#endif
        return uncheckedElement(0);
    }
    auto front() requires (Rank > 1) {
        std::array<std::size_t, Rank - 1> dimensions;
        for (std::size_t i = 0; i < Rank - 1; ++i) dimensions[i] = shape_[i + 1];
        // The mutable multidimensional overload has no empty assertion and
        // retains the same iterator; only its shape descriptor loses an axis.
        return reshaped(dimensions);
    }
    decltype(auto) first() requires (Rank > 1) {
#ifndef NDEBUG
        if (anyEmpty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 1780);
#endif
        return uncheckedElement(0);
    }
    decltype(auto) first() requires (Rank == 1) { return front(); }
    decltype(auto) first() const requires (Rank == 1) { return front(); }
    decltype(auto) back() requires (Rank == 1) {
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 1619);
#endif
        return uncheckedElement(shape_[0] - 1);
    }
    auto back() requires (Rank > 1) {
#ifndef NDEBUG
        if (empty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 1627);
#endif
        auto result = front();
        result.iteratorOffset_ += static_cast<std::size_t>(stride()) * (shape_[0] - 1);
        return result;
    }
    decltype(auto) last() requires (Rank == 1) { return back(); }
    decltype(auto) last() requires (Rank > 1) {
#ifndef NDEBUG
        if (anyEmpty()) throw ngStaticError<AssertError>(
            RetainedString::fromStaticLiteral("Assertion failure"),
            RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 1807);
#endif
        return uncheckedElement(elementCount() - 1);
    }
    std::size_t elementCount() const {
        std::size_t count = 1;
        for (auto dimension : shape_) count *= dimension;
        return count;
    }
    template<class... Index> requires(sizeof...(Index) == Rank)
    decltype(auto) at(Index... index) {
        const std::array<std::size_t, Rank> indices{static_cast<std::size_t>(index)...};
        std::size_t flat = 0;
        std::size_t stride = 1;
        for (std::size_t i = Rank; i-- > 0;) {
#ifndef NDEBUG
            if (indices[i] >= shape_[i]) {
                throw ngStaticError<AssertError>(
                    indexMessage(i),
                    RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), i == Rank - 1 ? 788 : 794);
            }
#endif
            flat += indices[i] * stride;
            stride *= shape_[i];
        }
        // MIR dereferences its raw iterator after its own assertions; a second
        // checked array index would invent a release-only source bounds check.
        return uncheckedElement(flat);
    }
    template<class... Index> requires(sizeof...(Index) == Rank)
    decltype(auto) at(Index... index) const {
        const std::array<std::size_t, Rank> indices{static_cast<std::size_t>(index)...};
        std::size_t flat = 0;
        std::size_t stride = 1;
        for (std::size_t i = Rank; i-- > 0;) {
#ifndef NDEBUG
            if (indices[i] >= shape_[i]) {
                throw ngStaticError<AssertError>(
                    indexMessage(i),
                    RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), i == Rank - 1 ? 788 : 794);
            }
#endif
            flat += indices[i] * stride;
            stride *= shape_[i];
        }
        return uncheckedElement(flat);
    }
protected:
    void popFrontUnchecked() requires (Rank > 0) {
        --shape_[0];
        iteratorOffset_ += static_cast<std::size_t>(stride());
    }
private:
    template<class, std::size_t, class> friend class ContiguousView;
    void checkBytePointer(std::size_t row, std::size_t column) const
        requires (Rank == 2 && std::same_as<T, std::uint8_t> && std::same_as<Storage, Slice<std::uint8_t>>) {
        if (storage_.data()) return;
#ifndef NDEBUG
        if (column >= shape_[1]) throw ngStaticError<AssertError>(indexMessage(1),
            RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 788);
        if (row >= shape_[0]) throw ngStaticError<AssertError>(indexMessage(0),
            RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 794);
#endif
        // The path consumer reads the indexed byte after Source bounds checks.
        ngFailNullAccess(false);
    }
    struct IndexMessage { std::array<char, 128> bytes{}; std::size_t length = 0; };
    inline static constexpr std::string_view literalElementName_ = [] {
        if constexpr (std::same_as<T, std::uint8_t>) return std::string_view("ubyte");
        else if constexpr (std::same_as<T, std::uint16_t>) return std::string_view("ushort");
        else if constexpr (std::same_as<T, float>) return std::string_view("float");
        else if constexpr (std::same_as<T, std::int32_t>) return std::string_view("int");
        else if constexpr (std::same_as<T, double>) return std::string_view("double");
        else return std::string_view{};
    }();

    static RetainedString indexMessage(std::size_t dimension) {
        if constexpr (!literalElementName_.empty()) {
            return RetainedString::fromStaticView(
                {indexMessages_[dimension].bytes.data(), indexMessages_[dimension].length});
        } else {
            const auto element = nicxlive::core::d_runtime::ngSourceTypeSpelling<T>(
                nicxlive::core::d_runtime::SourceTypeSpelling::UnqualifiedElement);
            try {
                return RetainedString(std::to_string(Rank) + "D slice of " + std::string(element.view()) +
                    ": bounds check failed at " + std::to_string(dimension + 1) + " dimension");
            } catch (const std::bad_alloc&) {
                throw ngStaticError<OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw ngStaticError<OutOfMemoryError>(std::source_location::current());
            }
        }
    }
    // Keep the existing known literal cases as allocation-free Source enum bytes.
    inline static constexpr auto indexMessages_ = [] {
        std::array<IndexMessage, Rank> result{};
        if constexpr (literalElementName_.empty()) return result;
        for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
            auto& message = result[dimension];
            const auto append = [&](std::string_view text) {
                for (char value : text) message.bytes[message.length++] = value;
            };
            const auto number = [&](std::size_t value) {
                std::array<char, 32> digits{};
                std::size_t length = 0;
                do { digits[length++] = static_cast<char>('0' + value % 10); value /= 10; } while (value);
                while (length) message.bytes[message.length++] = digits[--length];
            };
            number(Rank);
            append("D slice of ");
            append(literalElementName_);
            append(": bounds check failed at ");
            number(dimension + 1);
            append(" dimension");
        }
        return result;
    }();
    Storage storage_;
    std::array<std::size_t, Rank> shape_{};
    std::size_t iteratorOffset_ = 0;
};

} // namespace nijigenerate::compat

namespace nijigenerate::compat::phobos {

namespace detail {
template<class View> struct PointerSliceValueOperations : DescriptorValueOperations<View> {
    static void assign(View& destination, const View& source) {
        // Hold every previous descriptor field until the complete replacement
        // is published, including shape/offset after retained storage assignment.
        [[maybe_unused]] const auto previous = destination;
        destination = source;
    }
};
}

// These kernel instantiations store the actual Source pointer iterator using
// a retained T[] descriptor. A different Storage needs its defining operations.
template<class T, std::size_t Rank>
    requires (Rank > 0 && Rank < 255)
struct SourceValueOperations<ContiguousView<T, Rank, Slice<T>>>
    : detail::PointerSliceValueOperations<ContiguousView<T, Rank, Slice<T>>> {};

template<class T, std::size_t Rank>
    requires (Rank > 0 && Rank < 255)
struct SourceValueOperations<ContiguousView<const T, Rank, ConstSlice<T>>>
    : detail::PointerSliceValueOperations<ContiguousView<const T, Rank, ConstSlice<T>>> {};

template<ReinterpretedByteElement T, std::size_t Rank>
    requires (Rank > 0 && Rank < 255)
struct SourceValueOperations<ContiguousView<T, Rank, ByteView<T>>>
    : detail::PointerSliceValueOperations<ContiguousView<T, Rank, ByteView<T>>> {};

} // namespace nijigenerate::compat::phobos

// Source identity is supplied by the actual defining view producer, not by
// this kernel's rank, storage implementation or native element spelling.
#include "source_slice_identity.hpp"

namespace nicxlive::core::d_runtime {

template<class T, std::size_t Rank, class Storage>
    requires requires {
        ::nijigenerate::compat::SourceSliceOrigin<
            ::nijigenerate::compat::ContiguousView<T, Rank, Storage>>::get();
    }
struct SourceTypeSpellings<::nijigenerate::compat::ContiguousView<T, Rank, Storage>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        const auto origin = ::nijigenerate::compat::SourceSliceOrigin<
            ::nijigenerate::compat::ContiguousView<T, Rank, Storage>>::get();
        return ::nijigenerate::compat::ngSourceSliceTypeSpellings(origin);
    }
};

} // namespace nicxlive::core::d_runtime
