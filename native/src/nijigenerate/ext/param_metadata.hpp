#pragma once
#include <compat/object_metadata.hpp>

namespace nijigenerate::ext {
class ExParameter;
class ExParameterGroup;
}

namespace nicxlive::core::d_runtime {
#define NG_EXT_PARAMETER_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::ext::Type> { \
        inline static constexpr char className[] = "nijigenerate.ext.param." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.ext.param." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::ext::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.param." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.ext.param." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral(qualifiedArgument), \
                RetainedString::fromStaticLiteral(unqualifiedArgument), \
                RetainedString::fromStaticLiteral(qualifiedElement), \
                RetainedString::fromStaticLiteral(unqualifiedElement)}; \
        } \
    };
NG_EXT_PARAMETER_SOURCE_NAMES(ExParameter)
NG_EXT_PARAMETER_SOURCE_NAMES(ExParameterGroup)
#undef NG_EXT_PARAMETER_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime
