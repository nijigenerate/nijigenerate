#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::parameter::animedit {

enum class AnimeditCommand : std::int32_t {
    AddAnimationKeyFrame = 0,
    CopyAnimationKeyFrame = 1,
    PasteAnimationKeyFrame = 2
};

}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::parameter::animedit::AnimeditCommand> {
    using Enum = commands::parameter::animedit::AnimeditCommand;
    inline static constexpr std::string_view name = "AnimeditCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::AddAnimationKeyFrame, "AddAnimationKeyFrame"},
        FormatEnumMember<Enum>{Enum::CopyAnimationKeyFrame, "CopyAnimationKeyFrame"},
        FormatEnumMember<Enum>{Enum::PasteAnimationKeyFrame, "PasteAnimationKeyFrame"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::animedit::AnimeditCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.parameter.animedit.AnimeditCommand)";
    inline static constexpr char unqualifiedArgument[] = "(AnimeditCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.animedit.AnimeditCommand";
    inline static constexpr char unqualifiedElement[] = "AnimeditCommand";
};
} // namespace nicxlive::core::d_runtime

// The original module exports the concrete commands as well as its enum.
#include "animedit_commands.hpp"
