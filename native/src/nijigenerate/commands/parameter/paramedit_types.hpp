#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::parameter::paramedit {
enum class ParameditCommand : std::int32_t {
    ConvertTo2DParam = 0,
    FlipX = 1,
    FlipY = 2,
    Flip1D = 3,
    MirrorHorizontally = 4,
    MirrorVertically = 5,
    MirroredAutoFillDir1 = 6,
    MirroredAutoFillDir2 = 7,
    MirroredAutoFillDir3 = 8,
    MirroredAutoFillDir4 = 9,
    CopyParameter = 10,
    PasteParameter = 11,
    PasteParameterWithFlip = 12,
    DuplicateParameter = 13,
    DuplicateParameterWithFlip = 14,
    DeleteParameter = 15,
    LinkTo = 16,
    ToggleParameterArm = 17,
    SetParameterKeypoint = 18,
    SetArmedParameterAndKeypoint = 19,
    SetStartingKeyFrame = 20
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::parameter::paramedit::ParameditCommand> {
    using Enum = commands::parameter::paramedit::ParameditCommand;
    inline static constexpr std::string_view name = "ParameditCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::ConvertTo2DParam, "ConvertTo2DParam"},
        FormatEnumMember<Enum>{Enum::FlipX, "FlipX"},
        FormatEnumMember<Enum>{Enum::FlipY, "FlipY"},
        FormatEnumMember<Enum>{Enum::Flip1D, "Flip1D"},
        FormatEnumMember<Enum>{Enum::MirrorHorizontally, "MirrorHorizontally"},
        FormatEnumMember<Enum>{Enum::MirrorVertically, "MirrorVertically"},
        FormatEnumMember<Enum>{Enum::MirroredAutoFillDir1, "MirroredAutoFillDir1"},
        FormatEnumMember<Enum>{Enum::MirroredAutoFillDir2, "MirroredAutoFillDir2"},
        FormatEnumMember<Enum>{Enum::MirroredAutoFillDir3, "MirroredAutoFillDir3"},
        FormatEnumMember<Enum>{Enum::MirroredAutoFillDir4, "MirroredAutoFillDir4"},
        FormatEnumMember<Enum>{Enum::CopyParameter, "CopyParameter"},
        FormatEnumMember<Enum>{Enum::PasteParameter, "PasteParameter"},
        FormatEnumMember<Enum>{Enum::PasteParameterWithFlip, "PasteParameterWithFlip"},
        FormatEnumMember<Enum>{Enum::DuplicateParameter, "DuplicateParameter"},
        FormatEnumMember<Enum>{Enum::DuplicateParameterWithFlip, "DuplicateParameterWithFlip"},
        FormatEnumMember<Enum>{Enum::DeleteParameter, "DeleteParameter"},
        FormatEnumMember<Enum>{Enum::LinkTo, "LinkTo"},
        FormatEnumMember<Enum>{Enum::ToggleParameterArm, "ToggleParameterArm"},
        FormatEnumMember<Enum>{Enum::SetParameterKeypoint, "SetParameterKeypoint"},
        FormatEnumMember<Enum>{Enum::SetArmedParameterAndKeypoint, "SetArmedParameterAndKeypoint"},
        FormatEnumMember<Enum>{Enum::SetStartingKeyFrame, "SetStartingKeyFrame"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::paramedit::ParameditCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.parameter.paramedit.ParameditCommand)";
    inline static constexpr char unqualifiedArgument[] = "(ParameditCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.paramedit.ParameditCommand";
    inline static constexpr char unqualifiedElement[] = "ParameditCommand";
};
} // namespace nicxlive::core::d_runtime
