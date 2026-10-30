#pragma once
#include "meta_source_binding.hpp"
#include "optimum.hpp"

namespace nijigenerate::viewport::vertex::automesh::meta {
template<> struct AMReflectionSource<optimum::OptimumAutoMeshProcessor> {
    using Host = optimum::OptimumAutoMeshProcessor;
    static constexpr bool hasPresetName = true;
    static constexpr bool hasPostParamWrite = false;
    static constexpr bool hasAlphaPreview = true;
    static constexpr auto declarations() {
        using namespace source_binding;
        using L = AutoMeshLevel;
        return std::tuple{
            ngMemberDeclaration<&Host::LARGE_THRESHOLD>("LARGE_THRESHOLD",
                ngParamDeclaration(L::Advanced, "large_threshold", "Large threshold",
                "Region area threshold", "drag", 50, 2000, 5)),
            ngMemberDeclaration<&Host::LENGTH_THRESHOLD>("LENGTH_THRESHOLD",
                ngParamDeclaration(L::Advanced, "length_threshold", "Length threshold",
                "Contour length threshold", "drag", 20, 2000, 5)),
            ngMemberDeclaration<&Host::RATIO_THRESHOLD>("RATIO_THRESHOLD",
                ngParamDeclaration(L::Advanced, "ratio_threshold", "Ratio threshold",
                "Width/length threshold", "drag", 0.01f, 1, 0.01f)),
            ngMemberDeclaration<&Host::SHARP_EXPANSION_FACTOR>("SHARP_EXPANSION_FACTOR",
                ngParamDeclaration(L::Advanced, "sharp_expand", "Sharp expand",
                "Edge expansion for sharp parts", "drag", 0, 0.2f, 0.005f)),
            ngMemberDeclaration<&Host::NONSHARP_EXPANSION_FACTOR>("NONSHARP_EXPANSION_FACTOR",
                ngParamDeclaration(L::Advanced, "unsharp_expand", "Unsharp expand",
                "Edge expansion for non-sharp", "drag", 0, 0.5f, 0.005f)),
            ngMemberDeclaration<&Host::NONSHARP_CONTRACTION_FACTOR>("NONSHARP_CONTRACTION_FACTOR",
                ngParamDeclaration(L::Advanced, "unsharp_contract", "Unsharp contract",
                "Edge contraction for non-sharp", "drag", 0, 0.5f, 0.005f)),
            ngMemberDeclaration<&Host::SCALES>("SCALES",
                ngParamDeclaration(L::Simple, "scales", "Scales", "Contour scales", "array"),
                AMArray{0, 2, 0.01f}),
            ngMemberDeclaration<&Host::MIN_DISTANCE>("MIN_DISTANCE",
                ngParamDeclaration(L::Simple, "min_distance", "Min distance",
                "Minimum vertex spacing", "drag", 1, 200, 1)),
            ngMemberDeclaration<&Host::MASK_THRESHOLD>("MASK_THRESHOLD",
                ngParamDeclaration(L::Simple, "mask_threshold", "Mask threshold",
                "Alpha binarize cutoff", "drag", 1, 200, 1)),
            ngMemberDeclaration<&Host::DIV_PER_PART>("DIV_PER_PART",
                ngParamDeclaration(L::Simple, "div_per_part", "Div per part",
                "Division per part", "drag", 4, 64, 0.5f))};
    }
    static auto members() { return source_binding::ngMembers(declarations()); }
    static auto presets() {
        using source_binding::ngPreset;
        using source_binding::ngOwner;
        return std::tuple{
            ngPreset("Normal parts", [](Host& host) { Host::presetNormal(ngOwner(host)); }),
            ngPreset("Detailed mesh", [](Host& host) { Host::presetDetailed(ngOwner(host)); }),
            ngPreset("Large parts", [](Host& host) { Host::presetLarge(ngOwner(host)); }),
            ngPreset("Small parts", [](Host& host) { Host::presetSmall(ngOwner(host)); }),
            ngPreset("Thin and minimum parts", [](Host& host) { Host::presetThin(ngOwner(host)); }),
            ngPreset("Preserve edges", [](Host& host) { Host::presetEdges(ngOwner(host)); })};
    }
    static auto typeName(Host& host) { return source_binding::ngTypeName(host); }
    static auto& presetName(Host& host) { return host.presetName; }
    template<class Actual> requires std::same_as<Actual, Host>
    static auto& alphaPreview(Actual& host) { return host._alphaPreview; }
};
} // namespace nijigenerate::viewport::vertex::automesh::meta
