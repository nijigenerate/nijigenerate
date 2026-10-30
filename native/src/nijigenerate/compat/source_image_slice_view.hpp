#pragma once

// Mir slice traversal follows the selected Mir sources and retained licenses
// referenced by mir_any.hpp. Source identity comes from the actual Image path.
#include "mir_any.hpp"
#include "phobos_ranges.hpp"
#include "source_function_origin.hpp"
#include "source_static_function_origin.hpp"
#include "source_image_slice_identity.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat {

template<core::cv::image::ImageDataElement T, std::size_t Rank, KnownSourceMirAbi SourceAbi>
    requires (Rank > 0 && Rank < 255)
struct ImageSliceTraversalOrigin {
    using Identity = SourceSliceDeclarationIdentity<ImageSliceTraversalOrigin>;
    using StaticDeclaration = StaticMirImageSliceDeclaration<T, Rank, SourceAbi>;
    template<std::size_t OtherRank>
    using Rebind = ImageSliceTraversalOrigin<T, OtherRank, SourceAbi>;

    static SourceSliceTypeOrigin get() {
        // Image starts at rank3. Its contiguous front and flattened operations
        // preserve the actual pointer/kind and select the resulting Source rank.
        using Values = SourceMirSliceValueOrigin<Rank, SourceMirSliceKind::contiguous, SourceAbi>;
        return ImageSliceSourceOrigin<T, SourceAbi, Values>::get();
    }
    static RetainedString popFrontMessage() noexcept {
        return ngStaticMirSlicePopFrontErrorMessage<StaticDeclaration>();
    }
};

template<class Origin, class T, std::size_t Rank, class Storage>
class SourceImageSliceView : public ContiguousView<T, Rank, Storage> {
    using Base = ContiguousView<T, Rank, Storage>;
public:
    using SourceIdentity = SourceSliceDeclarationIdentity<Origin>;
    SourceImageSliceView() = default;
    explicit SourceImageSliceView(Base input) : Base(std::move(input)) {}

    auto save() { return *this; }
    template<std::size_t OtherRank>
    auto reshaped(std::array<std::size_t, OtherRank> dimensions) {
        using Result = SourceImageSliceView<typename Origin::template Rebind<OtherRank>, T, OtherRank, Storage>;
        return Result(Base::template reshaped<OtherRank>(dimensions));
    }
    auto reshape(std::array<std::ptrdiff_t, 2> dimensions, std::int32_t& error)
        requires (Rank == 1 && std::same_as<T, std::uint8_t> && std::same_as<Storage, Slice<std::uint8_t>>) {
        using Result = SourceImageSliceView<typename Origin::template Rebind<2>, T, 2, Storage>;
        return Result(Base::reshape(dimensions, error));
    }
    decltype(auto) front() requires (Rank == 1) { return Base::front(); }
    decltype(auto) front() const requires (Rank == 1) { return Base::front(); }
    auto sourceReferenceFront() requires (Rank == 1 && SourceByteReferenceElement<T>) {
        decltype(auto) cell = front();
        if constexpr (SourceReferenceType<std::remove_cvref_t<decltype(cell)>>::value) return cell;
        else return SourceReference<T>::fromRetainedObjectCell(this->storage().retentionOwner(), cell);
    }
    auto front() requires (Rank > 1) {
        using Result = SourceImageSliceView<typename Origin::template Rebind<Rank - 1>, T, Rank - 1, Storage>;
        return Result(Base::front());
    }
    decltype(auto) back() requires (Rank == 1) { return Base::back(); }
    auto back() requires (Rank > 1) {
        using Result = SourceImageSliceView<typename Origin::template Rebind<Rank - 1>, T, Rank - 1, Storage>;
        return Result(Base::back());
    }
    auto fuseShape() const requires (mir::detail::ContiguousFuseDimensions<Base>::available) {
        return mir::detail::sourceFuseShape(static_cast<const Base&>(*this));
    }
    void popFront() {
#ifndef NDEBUG
        if (this->empty()) throw ngStaticError<AssertError>(Origin::popFrontMessage(),
            RetainedString::fromStaticLiteral("mir/ndslice/slice.d"), 1670);
#endif
        // The Source assertion precedes mutation. Contiguous stride excludes
        // dimension0, so decrementing its length does not change the advance.
        this->popFrontUnchecked();
    }
};

// Source ABI is mandatory. The actual Image operation establishes the origin;
// an arbitrary bare view cannot acquire Image identity through this factory.
template<KnownSourceMirAbi SourceAbi, core::cv::image::ImageDataElement T = std::uint8_t, class Receiver>
    requires (!std::is_volatile_v<Receiver>
        && std::derived_from<std::remove_cv_t<Receiver>, core::cv::image::Image>)
auto ngSourceImageSlice(Receiver& image) {
    // Select the actual defining operation even if a derived class hides it.
    using Base = std::conditional_t<std::is_const_v<Receiver>,
        const core::cv::image::Image, core::cv::image::Image>;
    auto input = static_cast<Base&>(image).template sliced<T>();
    using Storage = std::remove_cvref_t<decltype(input.storage())>;
    using Origin = ImageSliceTraversalOrigin<T, 3, SourceAbi>;
    return SourceImageSliceView<Origin, T, 3, Storage>(std::move(input));
}

// Selected slice.d309: ubyte[].sliced is the same real Mir Slice!(ubyte*, 1,
// Contiguous) declaration used by an Image byte-pointer slice. Reuse its exact
// four-context pointer/kind/empty-label records; no Image receiver is fabricated.
template<KnownSourceMirAbi SourceAbi>
    requires ((std::same_as<SourceAbi, SourceMirAbi64> && std::numeric_limits<std::size_t>::digits == 64)
        || (std::same_as<SourceAbi, SourceMirAbi32> && std::numeric_limits<std::size_t>::digits == 32))
auto ngSourceMutableByteArraySlice(Slice<std::uint8_t> input) {
    using Storage = Slice<std::uint8_t>;
    using Origin = ImageSliceTraversalOrigin<std::uint8_t, 1, SourceAbi>;
    using Base = ContiguousView<std::uint8_t, 1, Storage>;
    return SourceImageSliceView<Origin, std::uint8_t, 1, Storage>(Base(input, {input.size()}));
}

} // namespace nijigenerate::compat

namespace nijigenerate::compat::phobos {

// Image.sliced is a Mir Slice with an actual T* iterator and empty labels.
// Its lengths/strides and pointer have no postblit, destructor or user opAssign.
// Native retained storage preserves that pointer's backing without copying cells.
template<core::cv::image::ImageDataElement T, std::size_t Rank, KnownSourceMirAbi Abi, class Storage>
    requires ((std::same_as<T, std::uint8_t> && std::same_as<Storage, Slice<std::uint8_t>>)
        || (std::same_as<T, std::uint16_t> && std::same_as<Storage, ByteView<std::uint16_t>>)
        || (std::same_as<T, float> && std::same_as<Storage, ByteView<float>>))
struct SourceValueOperations<SourceImageSliceView<ImageSliceTraversalOrigin<T, Rank, Abi>, T, Rank, Storage>>
    : detail::PointerSliceValueOperations<
        SourceImageSliceView<ImageSliceTraversalOrigin<T, Rank, Abi>, T, Rank, Storage>> {};

template<core::cv::image::ImageDataElement T, std::size_t Rank, KnownSourceMirAbi Abi, class Storage>
    requires ((std::same_as<T, std::uint8_t> && std::same_as<Storage, Slice<std::uint8_t>>)
        || (std::same_as<T, std::uint16_t> && std::same_as<Storage, ByteView<std::uint16_t>>)
        || (std::same_as<T, float> && std::same_as<Storage, ByteView<float>>))
struct SourceArrayElementSpelling<
    SourceImageSliceView<ImageSliceTraversalOrigin<T, Rank, Abi>, T, Rank, Storage>> {
    inline static constexpr auto name = StaticMirImageSliceDeclaration<T, Rank, Abi>::unqualifiedName();
    static consteval std::string_view get() { return name.view(); }
};

} // namespace nijigenerate::compat::phobos

namespace nijigenerate::compat::mir {

template<class Origin, class T, std::size_t Rank, class Storage>
    requires requires { typename Origin::StaticDeclaration; }
struct StaticSliceDeclaration<SourceImageSliceView<Origin, T, Rank, Storage>> {
    using type = typename Origin::StaticDeclaration;
};

template<class Origin, class T, std::size_t Rank, class Storage>
struct SliceDeclaration<SourceImageSliceView<Origin, T, Rank, Storage>> {
    inline static constexpr bool contiguous = true;
    inline static constexpr std::size_t rank = Rank;
    static auto flattened(SourceImageSliceView<Origin, T, Rank, Storage> input) {
        if constexpr (Rank == 1) return input;
        else return input.template reshaped<1>({input.elementCount()});
    }
};

} // namespace nijigenerate::compat::mir

namespace nijigenerate::compat::phobos::detail {

// The declared Image element remains T even when byte storage uses an
// unaligned-access proxy. Materializing a range copies values, not proxy cells.
template<class Origin, class T, class Storage>
struct FrontValue<::nijigenerate::compat::SourceImageSliceView<Origin, T, 1, Storage>> {
    using type = T;
};

} // namespace nijigenerate::compat::phobos::detail

namespace nicxlive::core::d_runtime {

template<class Origin, class T, std::size_t Rank, class Storage>
struct SourceTypeSpellings<::nijigenerate::compat::SourceImageSliceView<Origin, T, Rank, Storage>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::compat::ngSourceSliceTypeSpellings(Origin::get());
    }
};

} // namespace nicxlive::core::d_runtime
