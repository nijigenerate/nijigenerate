#pragma once

#include <nijigenerate/commands/automesh/dynamic_types_fwd.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>

namespace nijigenerate::commands::automesh::dynamic {

struct AutoMeshKey {
    ::nijigenerate::compat::RetainedStringSlice id;

    ::nijigenerate::compat::RetainedStringSlice toString() const;
    std::size_t toHash() const noexcept;
    bool opEquals(AutoMeshKey rhs) const noexcept;
};

}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::automesh::dynamic::AutoMeshKey> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.automesh.dynamic.AutoMeshKey)";
    inline static constexpr char unqualifiedArgument[] = "(AutoMeshKey)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.automesh.dynamic.AutoMeshKey";
    inline static constexpr char unqualifiedElement[] = "AutoMeshKey";
};
}
