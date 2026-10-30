#pragma once

#include <math/types.hpp>
#include <math/mat4.hpp>
#include "nijigenerate/compat/slice.hpp"
#include <common/shared_array.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format_vec2.hpp>
#include <nijigenerate/core/typed_format_shared_array.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include <tuple>

namespace nijigenerate::core::math::welding {
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Mat4;
using nijigenerate::compat::Slice;
struct WeldingMeshRefinement {
    Slice<Vec2> vertices;
    Slice<std::uint16_t> indices;
    std::size_t addedVertices{};
};
struct WeldingIndexPair {
    nicxlive::core::common::SharedArray<std::ptrdiff_t> forward;
    nicxlive::core::common::SharedArray<std::ptrdiff_t> reverse;
};
WeldingIndexPair ngRemapWeldingLinks(std::span<const std::ptrdiff_t> forward,
    std::span<const std::ptrdiff_t> reverse, std::span<const std::ptrdiff_t> oldToNew, std::size_t newCount);
WeldingMeshRefinement ngRefineWeldingMesh(std::span<const Vec2> fineWorld,
    std::span<const std::uint16_t> fineIndices, std::span<const Vec2> coarseWorld,
    std::span<const Vec2> coarseLocal, std::span<const std::uint16_t> coarseIndices,
    Mat4 worldToCoarse, float distanceLimit = 4);
// A missing slice admits every target; an engaged empty slice admits none.
Slice<std::ptrdiff_t> ngMatchWeldingVertices(std::span<const Vec2> source,
    std::span<const Vec2> target,
    std::optional<std::span<const std::ptrdiff_t>> previous = std::nullopt, float distanceLimit = 4);
}

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::core::math::welding::WeldingMeshRefinement> {
    using Value = ::nijigenerate::core::math::welding::WeldingMeshRefinement;
    inline static constexpr std::string_view typeName = "WeldingMeshRefinement";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("vertices", offsetof(Value, vertices), &Value::vertices),
            ngFormatAggregateMember("indices", offsetof(Value, indices), &Value::indices),
            ngFormatAggregateMember("addedVertices", offsetof(Value, addedVertices), &Value::addedVertices)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::core::math::welding::WeldingIndexPair> {
    using Value = ::nijigenerate::core::math::welding::WeldingIndexPair;
    inline static constexpr std::string_view typeName = "WeldingIndexPair";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("forward", offsetof(Value, forward), &Value::forward),
            ngFormatAggregateMember("reverse", offsetof(Value, reverse), &Value::reverse)};
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::core::math::welding::WeldingMeshRefinement> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.core.math.welding.WeldingMeshRefinement)"),
            RetainedString::fromStaticLiteral("(WeldingMeshRefinement)"),
            RetainedString::fromStaticLiteral("nijigenerate.core.math.welding.WeldingMeshRefinement"),
            RetainedString::fromStaticLiteral("WeldingMeshRefinement")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::core::math::welding::WeldingIndexPair> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.core.math.welding.WeldingIndexPair)"),
            RetainedString::fromStaticLiteral("(WeldingIndexPair)"),
            RetainedString::fromStaticLiteral("nijigenerate.core.math.welding.WeldingIndexPair"),
            RetainedString::fromStaticLiteral("WeldingIndexPair")};
    }
};
} // namespace nicxlive::core::d_runtime
