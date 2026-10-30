#pragma once
#include <nijigenerate/commands/fields.hpp>
#include <nijigenerate/commands/command_table.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include "detail/simplephysics_type_origins.hpp"
#include "simplephysics_alias.hpp"
#include <concepts>
#include <cstdint>
#include <memory>
#include <utility>

namespace nijigenerate::commands { class Command; }

namespace nijigenerate::commands::node::simplephysics {

enum class NodeSimplePhysicsCommand : std::int32_t {
    SetSimplePhysicsParameter = 0,
    ClearSimplePhysicsParameter = 1,
    SetSimplePhysicsModelType = 2,
    SetSimplePhysicsMapMode = 3,
    SetSimplePhysicsLocalOnly = 4,
    SetSimplePhysicsGravity = 5,
    SetSimplePhysicsLength = 6,
    SetSimplePhysicsFrequency = 7,
    SetSimplePhysicsAngleDamping = 8,
    SetSimplePhysicsLengthDamping = 9,
    SetSimplePhysicsOutputScaleX = 10,
    SetSimplePhysicsOutputScaleY = 11
};

using CommandTable = ::nijigenerate::commands::CommandTableStorage<NodeSimplePhysicsCommand>;
extern thread_local CommandTable commands;

template<class T, auto Getter, auto Setter, bool Rescan = false>
class SimplePhysicsValueChangeAction;

template<class I, class... Args>
std::shared_ptr<I> ngCreateSimplePhysicsValueAction(Args&&... args);

// Available before the cyclic public action/complete/mixin facade includes.
// The producer is borrowed synchronously for one owned factory invocation.
template<class Producer> struct SimplePhysicsActionArguments { Producer&& produce; };
template<class Producer> auto ngSimplePhysicsActionArguments(Producer&& producer) {
    return SimplePhysicsActionArguments<Producer>{std::forward<Producer>(producer)};
}

class SetSimplePhysicsParameterCommand;
class ClearSimplePhysicsParameterCommand;
class SetSimplePhysicsModelTypeCommand;
class SetSimplePhysicsMapModeCommand;
class SetSimplePhysicsLocalOnlyCommand;

template<::nijigenerate::commands::CommandLiteral ClassName,
         ::nijigenerate::commands::CommandLiteral Label,
         ::nijigenerate::commands::CommandLiteral Description,
         ::nijigenerate::commands::CommandLiteral PropertyName,
         auto Getter, auto Setter>
struct SimplePhysicsFloatCommand;

class SetSimplePhysicsGravityCommand;
class SetSimplePhysicsLengthCommand;
class SetSimplePhysicsFrequencyCommand;
class SetSimplePhysicsAngleDampingCommand;
class SetSimplePhysicsLengthDampingCommand;
class SetSimplePhysicsOutputScaleXCommand;
class SetSimplePhysicsOutputScaleYCommand;

void ngInitSimplePhysicsCommands();

}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::commands::node::simplephysics::NodeSimplePhysicsCommand> {
    using Enum = ::nijigenerate::commands::node::simplephysics::NodeSimplePhysicsCommand;
    inline static constexpr std::string_view name = "NodeSimplePhysicsCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsParameter, "SetSimplePhysicsParameter"},
        FormatEnumMember<Enum>{Enum::ClearSimplePhysicsParameter, "ClearSimplePhysicsParameter"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsModelType, "SetSimplePhysicsModelType"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsMapMode, "SetSimplePhysicsMapMode"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsLocalOnly, "SetSimplePhysicsLocalOnly"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsGravity, "SetSimplePhysicsGravity"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsLength, "SetSimplePhysicsLength"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsFrequency, "SetSimplePhysicsFrequency"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsAngleDamping, "SetSimplePhysicsAngleDamping"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsLengthDamping, "SetSimplePhysicsLengthDamping"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsOutputScaleX, "SetSimplePhysicsOutputScaleX"},
        FormatEnumMember<Enum>{Enum::SetSimplePhysicsOutputScaleY, "SetSimplePhysicsOutputScaleY"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::node::simplephysics::NodeSimplePhysicsCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.node.simplephysics.NodeSimplePhysicsCommand)";
    inline static constexpr char unqualifiedArgument[] = "(NodeSimplePhysicsCommand)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.node.simplephysics.NodeSimplePhysicsCommand";
    inline static constexpr char unqualifiedElement[] = "NodeSimplePhysicsCommand";
};
}

#include "simplephysics_complete.hpp"
#include "simplephysics_action.hpp"
