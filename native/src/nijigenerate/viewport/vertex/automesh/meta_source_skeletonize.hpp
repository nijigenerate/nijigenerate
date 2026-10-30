#pragma once
#include "meta_source_binding.hpp"
#include "skeletonize.hpp"

namespace nijigenerate::viewport::vertex::automesh::meta {
template<> struct AMReflectionSource<skeletonize::SkeletonExtractor> {
    using Host = skeletonize::SkeletonExtractor;
    static constexpr bool hasPresetName = false;
    static constexpr bool hasPostParamWrite = false;
    static constexpr bool hasAlphaPreview = true;
    static constexpr auto declarations() {
        using namespace source_binding;
        return std::tuple{
            ngMemberDeclaration<&Host::maskThreshold>("maskThreshold",
                ngParamDeclaration(AutoMeshLevel::Simple, "mask_threshold", "Mask threshold",
                "Alpha binarize cutoff", "drag", 1, 200, 1)),
            ngMemberDeclaration<&Host::targetPointCount>("targetPointCount",
                ngParamDeclaration(AutoMeshLevel::Simple, "target_point_count", "Target point count",
                "Target number of path control points", "drag", 2, 128, 1))};
    }
    static auto members() { return source_binding::ngMembers(declarations()); }
    static auto presets() { return std::tuple{}; } // This class declares no AMPreset method.
    static auto typeName(Host& host) { return source_binding::ngTypeName(host); }
    template<class Actual> requires std::same_as<Actual, Host>
    static auto& alphaPreview(Actual& host) { return host._alphaPreview; }
};
} // namespace nijigenerate::viewport::vertex::automesh::meta
