#pragma once
#include "meta_source_binding.hpp"
#include "contours.hpp"

namespace nijigenerate::viewport::vertex::automesh::meta {
template<> struct AMReflectionSource<contours::ContourAutoMeshProcessor> {
    using Host = contours::ContourAutoMeshProcessor;
    static constexpr bool hasPresetName = true;
    static constexpr bool hasPostParamWrite = false;
    static constexpr bool hasAlphaPreview = true;
    static constexpr auto declarations() {
        using namespace source_binding;
        using L = AutoMeshLevel;
        return std::tuple{
            ngMemberDeclaration<&Host::SAMPLING_STEP>("SAMPLING_STEP",
                ngParamDeclaration(L::Simple, "sampling_step", "Sampling rate",
                "Contour sampling rate", "drag", 1, 200, 1)),
            ngMemberDeclaration<&Host::maskThreshold>("maskThreshold",
                ngParamDeclaration(L::Simple, "mask_threshold", "Mask threshold",
                "Alpha binarize cutoff", "drag", 1, 200, 1)),
            ngMemberDeclaration<&Host::MIN_DISTANCE>("MIN_DISTANCE",
                ngParamDeclaration(L::Advanced, "min_distance", "Minimum distance",
                "Minimum distance between vertices", "drag", 1, 200, 1)),
            ngMemberDeclaration<&Host::MAX_DISTANCE>("MAX_DISTANCE",
                ngParamDeclaration(L::Advanced, "max_distance", "Maximum distance",
                "Maximum distance between vertices", "drag", -1, 200, 1)),
            ngMemberDeclaration<&Host::SCALES>("SCALES",
                ngParamDeclaration(L::Advanced, "scales", "Scales", "Contour scales", "array"),
                AMArray{0, 2, 0.01f})};
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
