#pragma once
#include "base.hpp"
#include "source_reflection.hpp"
#include <compat/object_metadata.hpp>

namespace nicxlive::core::d_runtime {
#define NG_COMMAND_ENUM_SOURCE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.base." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.base." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_COMMAND_ENUM_SOURCE_ORIGIN(CommandJsonSchema);
NG_COMMAND_ENUM_SOURCE_ORIGIN(CommandShortcutPolicy);
NG_COMMAND_ENUM_SOURCE_ORIGIN(CommandMcpExposure);
NG_COMMAND_ENUM_SOURCE_ORIGIN(CommandGuiDisplay);
NG_COMMAND_ENUM_SOURCE_ORIGIN(CommandIrreversibleEffect);
#undef NG_COMMAND_ENUM_SOURCE_ORIGIN
}
