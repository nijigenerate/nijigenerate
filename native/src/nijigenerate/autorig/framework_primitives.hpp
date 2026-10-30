#pragma once

#include <cstdint>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::autorig::framework {

enum class AutoRigValueKind : std::int32_t { FileName, Path, Json, Blob };
enum class AutoRigTaskState : std::int32_t { Pending, Running, Succeeded, Failed, Canceled, Stale };

// Marks extraction messages without translating or replacing their descriptor.
inline compat::RetainedStringSlice ngAutoRigMessage(compat::RetainedStringSlice message) {
    return message;
}

} // namespace nijigenerate::autorig::framework

namespace nicxlive::core::d_runtime {
#define NG_FRAMEWORK_ENUM_ORIGINS(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::autorig::framework::Type> { \
    inline static constexpr bool isNativeObject = false; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.autorig.framework." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.autorig.framework." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
};
NG_FRAMEWORK_ENUM_ORIGINS(AutoRigValueKind)
NG_FRAMEWORK_ENUM_ORIGINS(AutoRigTaskState)
#undef NG_FRAMEWORK_ENUM_ORIGINS
}
