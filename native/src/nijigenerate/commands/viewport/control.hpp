#pragma once
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include "control_types.hpp"
#include "control_commands.hpp"
#include "control_pair_commands.hpp"
#include "control_constructor_programs.hpp"
#include "control_table.hpp"
#include <concepts>

namespace nijigenerate::commands::viewport::control {

void ngInitViewportControlCommands();
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
template<auto Id, class C>
using Step = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Step<ViewportCommand::ToggleMirrorView, ToggleMirrorViewCommand>,
    Step<ViewportCommand::ToggleOnionSlice, ToggleOnionSliceCommand>,
    Step<ViewportCommand::TogglePhysics, TogglePhysicsCommand>,
    Step<ViewportCommand::TogglePostProcess, TogglePostProcessCommand>,
    Step<ViewportCommand::ResetPhysics, ResetPhysicsCommand>,
    Step<ViewportCommand::ResetParameters, ResetParametersCommand>,
    Step<ViewportCommand::ListFlipPairs, ListFlipPairsCommand>,
    Step<ViewportCommand::AddFlipPair, AddFlipPairCommand>,
    Step<ViewportCommand::AutoAddFlipPairs, AutoAddFlipPairsCommand>,
    Step<ViewportCommand::RemoveFlipPair, RemoveFlipPairCommand>,
    Step<ViewportCommand::OpenFlipPairWindow, OpenFlipPairWindowCommand>,
    Step<ViewportCommand::OpenAutomeshBatching, OpenAutomeshBatchingCommand>,
    Step<ViewportCommand::ResetViewportZoom, ResetViewportZoomCommand>,
    Step<ViewportCommand::ResetViewportPosition, ResetViewportPositionCommand>,
    Step<ViewportCommand::FitViewportToModel, FitViewportToModelCommand>>;
}

template<class T> requires (std::same_as<T, ViewportCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitViewportControlCommands(); }

}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<viewport::control::ViewportCommand> {
    static consteval bool supported() {
        return viewport::control::initialization::Program::supported<viewport::control::initialization::Table>();
    }
    template<class ActualKey = viewport::control::ViewportCommand>
        requires (std::same_as<ActualKey, viewport::control::ViewportCommand> && supported())
    static void invoke() { viewport::control::ngInitCommands<ActualKey>(); }
};
}
