#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::automesh::config::detail {

struct AutoMeshConfigSnapshot {
    compat::RetainedStringSlice simple;
    compat::RetainedStringSlice advanced;
};

} // namespace nijigenerate::commands::automesh::config::detail

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::automesh::config::detail::AutoMeshConfigSnapshot> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.automesh.config.AutoMeshConfigSnapshot)";
    inline static constexpr char unqualifiedArgument[] = "(AutoMeshConfigSnapshot)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.automesh.config.AutoMeshConfigSnapshot";
    inline static constexpr char unqualifiedElement[] = "AutoMeshConfigSnapshot";
};
} // namespace nicxlive::core::d_runtime
