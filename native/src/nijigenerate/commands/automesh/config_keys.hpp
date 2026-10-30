#pragma once

#include "nijigenerate/compat/retained_string.hpp"
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::automesh::config {

struct AutoMeshGetConfigKey {
    compat::RetainedString id;
    friend bool operator==(const AutoMeshGetConfigKey&, const AutoMeshGetConfigKey&) = default;
};

struct AutoMeshSetConfigKey {
    compat::RetainedString id;
    friend bool operator==(const AutoMeshSetConfigKey&, const AutoMeshSetConfigKey&) = default;
};

struct AutoMeshSetSimpleKey {
    compat::RetainedString id;
    friend bool operator==(const AutoMeshSetSimpleKey&, const AutoMeshSetSimpleKey&) = default;
};

struct AutoMeshSetAdvancedKey {
    compat::RetainedString id;
    friend bool operator==(const AutoMeshSetAdvancedKey&, const AutoMeshSetAdvancedKey&) = default;
};

struct AutoMeshSetPresetKey {
    compat::RetainedString id;
    friend bool operator==(const AutoMeshSetPresetKey&, const AutoMeshSetPresetKey&) = default;
};

} // namespace nijigenerate::commands::automesh::config

namespace nicxlive::core::d_runtime {
#define NG_AUTOMESH_CONFIG_KEY_ORIGIN(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::commands::automesh::config::Type> { \
    inline static constexpr bool isNativeObject = false; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.automesh.config." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.automesh.config." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
};
NG_AUTOMESH_CONFIG_KEY_ORIGIN(AutoMeshGetConfigKey)
NG_AUTOMESH_CONFIG_KEY_ORIGIN(AutoMeshSetConfigKey)
NG_AUTOMESH_CONFIG_KEY_ORIGIN(AutoMeshSetSimpleKey)
NG_AUTOMESH_CONFIG_KEY_ORIGIN(AutoMeshSetAdvancedKey)
NG_AUTOMESH_CONFIG_KEY_ORIGIN(AutoMeshSetPresetKey)
#undef NG_AUTOMESH_CONFIG_KEY_ORIGIN
} // namespace nicxlive::core::d_runtime
