#pragma once

#include <cstdint>
#include <concepts>
#include <array>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/commands/command_table.hpp>
#include <nijigenerate/commands/source_diagnostic_text.hpp>

namespace nijigenerate::commands::depth::psd_dialog {

enum class PsdDepthDialogCommand : std::int32_t {
    InspectPsdDepthDialog,
    GetPsdDepthDialogPartData,
    SetPsdDepthDialogColorSource,
    SetPsdDepthDialogInvert,
    SetPsdDepthDialogBackDepth,
    SetPsdDepthDialogFrontDepth,
    SetPsdDepthDialogDepthScale,
    SetPsdDepthDialogChannel,
    SetPsdDepthDialogSampling,
    SetPsdDepthDialogCustomRadius,
    SetPsdDepthDialogAlphaThreshold,
    SetPsdDepthDialogMissingPolicy,
    SetPsdDepthDialogContourRepair,
    SetPsdDepthDialogSurfaceSmoothing,
    FillPsdDepthDialogAlphaDepthGaps,
    SetPsdDepthDialogGpuComposition,
    SetPsdDepthDialogDirectGridMatch,
    SetPsdDepthDialogProblemFilter,
    SetPsdDepthDialogLayerMappingAuto,
    SetPsdDepthDialogLayerMappingIgnored,
    SetPsdDepthDialogLayerMappingTarget,
    SetPsdDepthDialogTargetEnabled,
    SetPsdDepthDialogLayerEnabled,
    SetPsdDepthDialogLayerVisible,
    SetPsdDepthDialogLayerDepthEnabled,
    SetPsdDepthDialogLayerDepthInverted,
    SetPsdDepthDialogLayerDepthScale,
    SetPsdDepthDialogLayerDepthOffset,
    ResetPsdDepthDialogLayerDepthTransform,
    ApplyPsdDepthDialog,
    CancelPsdDepthDialog,
};

namespace detail {
// Actual Source expressions in the two scope-transition array literals.
inline constexpr auto applyPsdDepthDialogIdOrigin = NG_SOURCE_COMMAND_ID(
    PsdDepthDialogCommand::ApplyPsdDepthDialog, "ApplyPsdDepthDialog");
inline constexpr auto cancelPsdDepthDialogIdOrigin = NG_SOURCE_COMMAND_ID(
    PsdDepthDialogCommand::CancelPsdDepthDialog, "CancelPsdDepthDialog");
}

class PsdDepthDialogCommandScope;
class InspectPsdDepthDialogCommand;
class GetPsdDepthDialogPartDataCommand;
class SetPsdDepthDialogColorSourceCommand;
class SetPsdDepthDialogInvertCommand;
class SetPsdDepthDialogBackDepthCommand;
class SetPsdDepthDialogFrontDepthCommand;
class SetPsdDepthDialogDepthScaleCommand;
class SetPsdDepthDialogChannelCommand;
class SetPsdDepthDialogSamplingCommand;
class SetPsdDepthDialogCustomRadiusCommand;
class SetPsdDepthDialogAlphaThresholdCommand;
class SetPsdDepthDialogMissingPolicyCommand;
class SetPsdDepthDialogContourRepairCommand;
class SetPsdDepthDialogSurfaceSmoothingCommand;
class SetPsdDepthDialogGpuCompositionCommand;
class SetPsdDepthDialogDirectGridMatchCommand;
class SetPsdDepthDialogProblemFilterCommand;
class SetPsdDepthDialogLayerMappingAutoCommand;
class SetPsdDepthDialogLayerMappingIgnoredCommand;
class SetPsdDepthDialogLayerMappingTargetCommand;
class SetPsdDepthDialogTargetEnabledCommand;
class SetPsdDepthDialogLayerEnabledCommand;
class SetPsdDepthDialogLayerVisibleCommand;
class SetPsdDepthDialogLayerDepthEnabledCommand;
class SetPsdDepthDialogLayerDepthInvertedCommand;
class SetPsdDepthDialogLayerDepthScaleCommand;
class SetPsdDepthDialogLayerDepthOffsetCommand;
class ResetPsdDepthDialogLayerDepthTransformCommand;
class FillPsdDepthDialogAlphaDepthGapsCommand;
class ApplyPsdDepthDialogCommand;
class CancelPsdDepthDialogCommand;

using CommandTable = ::nijigenerate::commands::CommandTableStorage<PsdDepthDialogCommand>;
extern thread_local CommandTable commands;

} // namespace nijigenerate::commands::depth::psd_dialog

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::depth::psd_dialog::PsdDepthDialogCommand> {
    using Value = commands::depth::psd_dialog::PsdDepthDialogCommand;
    inline static constexpr std::string_view name = "PsdDepthDialogCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::InspectPsdDepthDialog, "InspectPsdDepthDialog"},
        FormatEnumMember<Value>{Value::GetPsdDepthDialogPartData, "GetPsdDepthDialogPartData"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogColorSource, "SetPsdDepthDialogColorSource"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogInvert, "SetPsdDepthDialogInvert"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogBackDepth, "SetPsdDepthDialogBackDepth"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogFrontDepth, "SetPsdDepthDialogFrontDepth"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogDepthScale, "SetPsdDepthDialogDepthScale"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogChannel, "SetPsdDepthDialogChannel"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogSampling, "SetPsdDepthDialogSampling"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogCustomRadius, "SetPsdDepthDialogCustomRadius"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogAlphaThreshold, "SetPsdDepthDialogAlphaThreshold"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogMissingPolicy, "SetPsdDepthDialogMissingPolicy"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogContourRepair, "SetPsdDepthDialogContourRepair"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogSurfaceSmoothing, "SetPsdDepthDialogSurfaceSmoothing"},
        FormatEnumMember<Value>{Value::FillPsdDepthDialogAlphaDepthGaps, "FillPsdDepthDialogAlphaDepthGaps"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogGpuComposition, "SetPsdDepthDialogGpuComposition"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogDirectGridMatch, "SetPsdDepthDialogDirectGridMatch"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogProblemFilter, "SetPsdDepthDialogProblemFilter"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogLayerMappingAuto, "SetPsdDepthDialogLayerMappingAuto"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogLayerMappingIgnored, "SetPsdDepthDialogLayerMappingIgnored"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogLayerMappingTarget, "SetPsdDepthDialogLayerMappingTarget"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogTargetEnabled, "SetPsdDepthDialogTargetEnabled"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogLayerEnabled, "SetPsdDepthDialogLayerEnabled"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogLayerVisible, "SetPsdDepthDialogLayerVisible"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogLayerDepthEnabled, "SetPsdDepthDialogLayerDepthEnabled"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogLayerDepthInverted, "SetPsdDepthDialogLayerDepthInverted"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogLayerDepthScale, "SetPsdDepthDialogLayerDepthScale"},
        FormatEnumMember<Value>{Value::SetPsdDepthDialogLayerDepthOffset, "SetPsdDepthDialogLayerDepthOffset"},
        FormatEnumMember<Value>{Value::ResetPsdDepthDialogLayerDepthTransform, "ResetPsdDepthDialogLayerDepthTransform"},
        FormatEnumMember<Value>{Value::ApplyPsdDepthDialog, "ApplyPsdDepthDialog"},
        FormatEnumMember<Value>{Value::CancelPsdDepthDialog, "CancelPsdDepthDialog"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::depth::psd_dialog::PsdDepthDialogCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.depth.psd_dialog.PsdDepthDialogCommand)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthDialogCommand)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.depth.psd_dialog.PsdDepthDialogCommand";
    inline static constexpr char unqualifiedElement[] = "PsdDepthDialogCommand";
    NG_SOURCE_TYPE_DIAGNOSTIC_MEMBERS("PsdDepthDialogCommand")
};
} // namespace nicxlive::core::d_runtime

#include "psd_dialog_commands.hpp"
