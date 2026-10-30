#pragma once
#include "meta_source_binding.hpp"
#include "grid.hpp"

namespace nijigenerate::viewport::vertex::automesh::meta {
template<> struct AMReflectionSource<grid::GridAutoMeshProcessor> {
    using Host = grid::GridAutoMeshProcessor;
    static constexpr bool hasPresetName = false;
    static constexpr bool hasPostParamWrite = true;
    static constexpr bool hasAlphaPreview = true;
    static constexpr auto declarations() {
        using namespace source_binding;
        using L = AutoMeshLevel;
        return std::tuple{
            ngMemberDeclaration<&Host::scaleX>("scaleX",
                ngParamDeclaration(L::Advanced, "scale_x", "X Scale", "Axis scale factors (X)", "array"),
                AMArray{0, 2, 0.01f}),
            ngMemberDeclaration<&Host::scaleY>("scaleY",
                ngParamDeclaration(L::Advanced, "scale_y", "Y Scale", "Axis scale factors (Y)", "array"),
                AMArray{0, 2, 0.01f}),
            ngMemberDeclaration<&Host::maskThreshold>("maskThreshold",
                ngParamDeclaration(L::Simple, "mask_threshold", "Mask threshold",
                "Alpha binarize cutoff", "drag", 1, 200, 1)),
            ngMemberDeclaration<&Host::xSegments>("xSegments",
                ngParamDeclaration(L::Simple, "x_segments", "X Segments", "Grid density X", "drag", 2, 20, 1)),
            ngMemberDeclaration<&Host::ySegments>("ySegments",
                ngParamDeclaration(L::Simple, "y_segments", "Y Segments", "Grid density Y", "drag", 2, 20, 1)),
            ngMemberDeclaration<&Host::margin>("margin",
                ngParamDeclaration(L::Simple, "margin", "Margin", "Outer margin factor", "drag", 0, 1, 0.1f))};
    }
    static auto members() { return source_binding::ngMembers(declarations()); }
    static auto presets() { return std::tuple{}; } // This class declares no AMPreset method.
    static auto typeName(Host& host) { return source_binding::ngTypeName(host); }
    static void postParamWrite(Host& host, compat::RetainedString id) { host.ngPostParamWrite(id); }
    template<class Actual> requires std::same_as<Actual, Host>
    static auto& alphaPreview(Actual& host) { return host._alphaPreview; }
};
} // namespace nijigenerate::viewport::vertex::automesh::meta
