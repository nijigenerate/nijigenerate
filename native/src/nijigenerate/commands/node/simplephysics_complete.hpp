#pragma once
#include "simplephysics.hpp"
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include "simplephysics_float_commands.hpp"

#include "simplephysics_float_mixin.hpp"
#include "simplephysics_constructor_programs.hpp"

namespace nijigenerate::commands::node::simplephysics {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct NullParameter {
    static auto get() { return std::tuple{std::shared_ptr<nicxlive::core::param::Parameter>{}}; }
};
struct NoArguments { static auto get() { return std::tuple{}; } };
struct Model { static auto get() { return std::tuple{nicxlive::core::nodes::SourcePhysicsModel{
    nicxlive::core::nodes::SourcePhysicsModel::Pendulum}}; } };
struct MapMode { static auto get() { return std::tuple{nicxlive::core::nodes::SourceParamMapMode{
    nicxlive::core::nodes::SourceParamMapMode::AngleLength}}; } };
struct LocalOnly { static auto get() { return std::tuple{false}; } };
struct One { static auto get() { return std::tuple{1.0f}; } };
struct Length { static auto get() { return std::tuple{100.0f}; } };
struct Half { static auto get() { return std::tuple{0.5f}; } };
template<auto Id, class C, class Args>
using Assigned = ::nijigenerate::commands::detail::AssignedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsParameter, SetSimplePhysicsParameterCommand, NullParameter>,
    Assigned<NodeSimplePhysicsCommand::ClearSimplePhysicsParameter, ClearSimplePhysicsParameterCommand, NoArguments>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsModelType, SetSimplePhysicsModelTypeCommand, Model>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsMapMode, SetSimplePhysicsMapModeCommand, MapMode>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsLocalOnly, SetSimplePhysicsLocalOnlyCommand, LocalOnly>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsGravity, SetSimplePhysicsGravityCommand, One>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsLength, SetSimplePhysicsLengthCommand, Length>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsFrequency, SetSimplePhysicsFrequencyCommand, One>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsAngleDamping, SetSimplePhysicsAngleDampingCommand, Half>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsLengthDamping, SetSimplePhysicsLengthDampingCommand, Half>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsOutputScaleX, SetSimplePhysicsOutputScaleXCommand, One>,
    Assigned<NodeSimplePhysicsCommand::SetSimplePhysicsOutputScaleY, SetSimplePhysicsOutputScaleYCommand, One>>;
}
template<class T> requires (std::same_as<T, NodeSimplePhysicsCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitSimplePhysicsCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<node::simplephysics::NodeSimplePhysicsCommand> {
    static consteval bool supported() {
        return node::simplephysics::initialization::Program::supported<node::simplephysics::initialization::Table>();
    }
    template<class ActualKey = node::simplephysics::NodeSimplePhysicsCommand>
        requires (std::same_as<ActualKey, node::simplephysics::NodeSimplePhysicsCommand> && supported())
    static void invoke() { node::simplephysics::ngInitCommands<ActualKey>(); }
};
}
