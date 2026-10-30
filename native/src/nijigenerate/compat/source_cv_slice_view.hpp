#pragma once

#include "source_cv_slice_identity.hpp"
#include "source_image_slice_view.hpp"
#include <nijigenerate/core/cv/distancetransform.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace nijigenerate::compat {
namespace source_cv_detail {

template<class Element, std::size_t Rank, class SourceAbi> struct StaticOutputDeclaration;
template<std::size_t Rank, class SourceAbi> struct StaticOutputDeclaration<float, Rank, SourceAbi> {
    using type = StaticDistanceTransformDistanceSliceDeclaration<SourceAbi, Rank>;
};
template<std::size_t Rank, class SourceAbi> struct StaticOutputDeclaration<std::int32_t, Rank, SourceAbi> {
    using type = StaticDistanceTransformNearestSliceDeclaration<SourceAbi, Rank>;
};

} // namespace source_cv_detail

// These are the two pointer element types declared by the real M106 outputs.
// Front and flattening preserve their pointer/kind and change the Source rank.
template<class Element, std::size_t Rank, KnownSourceMirAbi SourceAbi>
    requires (Rank > 0 && ((std::same_as<Element, float> && Rank <= 2)
        || (std::same_as<Element, std::int32_t> && Rank <= 3)))
struct DistanceTransformSliceTraversalOrigin {
    using Identity = SourceSliceDeclarationIdentity<DistanceTransformSliceTraversalOrigin>;
    using StaticDeclaration = typename source_cv_detail::StaticOutputDeclaration<Element, Rank, SourceAbi>::type;
    template<std::size_t OtherRank>
    using Rebind = DistanceTransformSliceTraversalOrigin<Element, OtherRank, SourceAbi>;

    static SourceSliceTypeOrigin get() {
        return DeclaredPointerSliceSourceOrigin<Element, Rank, SourceMirSliceKind::contiguous, SourceAbi>::get();
    }
    static RetainedString popFrontMessage() noexcept {
        return ngStaticMirSlicePopFrontErrorMessage<StaticDeclaration>();
    }
};

template<KnownSourceMirAbi SourceAbi> struct SourceDistanceTransformResult {
    using DistanceOrigin = DistanceTransformSliceTraversalOrigin<float, 2, SourceAbi>;
    using NearestOrigin = DistanceTransformSliceTraversalOrigin<std::int32_t, 3, SourceAbi>;
    using Distance = SourceImageSliceView<DistanceOrigin, float, 2, Slice<float>>;
    using Nearest = SourceImageSliceView<NearestOrigin, std::int32_t, 3, Slice<std::int32_t>>;
    Distance dist;
    Nearest nearest;
};

// Forward the actual output objects themselves. If the provider throws, its
// writes to either descriptor have already reached the caller's same fields.
template<KnownSourceMirAbi SourceAbi, class Input>
void ngSourceDistanceTransform(const Input& input,
    typename SourceDistanceTransformResult<SourceAbi>::Distance& dist,
    typename SourceDistanceTransformResult<SourceAbi>::Nearest& nearest) {
    core::cv::distancetransform::distanceTransform(input,
        static_cast<core::cv::distancetransform::DistanceView&>(dist),
        static_cast<core::cv::distancetransform::NearestView&>(nearest));
}

// Publish declared views of the actual provider's successful result.
template<KnownSourceMirAbi SourceAbi, class Input>
auto ngSourceDistanceTransform(const Input& input) {
    using Result = SourceDistanceTransformResult<SourceAbi>;
    Result result;
    ngSourceDistanceTransform<SourceAbi>(input, result.dist, result.nearest);
    return result;
}

} // namespace nijigenerate::compat

namespace nijigenerate::compat::phobos {

// The real distance-transform outputs declare float* and int* Slice iterators.
// The commented RCArray alternative in the D implementation is not their type.
template<class Element, std::size_t Rank, KnownSourceMirAbi Abi>
    requires ((std::same_as<Element, float> && Rank > 0 && Rank <= 2)
        || (std::same_as<Element, std::int32_t> && Rank > 0 && Rank <= 3))
struct SourceValueOperations<SourceImageSliceView<
    DistanceTransformSliceTraversalOrigin<Element, Rank, Abi>, Element, Rank, Slice<Element>>>
    : detail::PointerSliceValueOperations<SourceImageSliceView<
        DistanceTransformSliceTraversalOrigin<Element, Rank, Abi>, Element, Rank, Slice<Element>>> {};

template<class Element, std::size_t Rank, KnownSourceMirAbi Abi>
    requires ((std::same_as<Element, float> && Rank > 0 && Rank <= 2)
        || (std::same_as<Element, std::int32_t> && Rank > 0 && Rank <= 3))
struct SourceArrayElementSpelling<SourceImageSliceView<
    DistanceTransformSliceTraversalOrigin<Element, Rank, Abi>, Element, Rank, Slice<Element>>> {
    using Declaration = typename source_cv_detail::StaticOutputDeclaration<Element, Rank, Abi>::type;
    inline static constexpr auto name = Declaration::unqualifiedName();
    static consteval std::string_view get() { return name.view(); }
};

} // namespace nijigenerate::compat::phobos
