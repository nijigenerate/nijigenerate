#pragma once

#include "../binding_commands.hpp"
#include "../binding_table.hpp"
#include "registration_diagnostic.hpp"
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>

namespace nijigenerate::commands::binding::detail {

template<CommandLiteral Identifier, auto... SourceArguments> struct RegistrationOperation {
    using Alias = typename BindingRegistrationDeclaration<Identifier>::SourceAlias;
    using Resolution = typename Alias::Resolution;
    using C = typename Resolution::ActualType;
    inline static constexpr auto id = Alias::value;
    static consteval bool supported() {
        if constexpr (!bindingCommandRegistrationCompiles<Alias>()) return false;
        else if constexpr (!requires { Resolution::construct(SourceArguments...); }) return false;
        else return ngCommandRegistrationSupported<id, C, CommandTable, decltype(SourceArguments)...>();
    }
    static void run() requires(supported()) {
        auto command = Resolution::construct(SourceArguments...);
        ngRegisterCommandMeta(command);
        // Select the current TLS map after the actual constructor/metadata calls.
        commands[id] = command;
    }
};

template<class... Operation> struct RegistrationProgram {
    static consteval bool supported() { return (Operation::supported() && ...); }
    static void run() requires(supported()) { (Operation::run(), ...); }
};

// These are the sixteen authored statements, not an enum-derived registration
// generator or a whitelist for the separate arbitrary-alias source template.
using Initialization = RegistrationProgram<
    RegistrationOperation<"UnsetKeyFrame">,
    RegistrationOperation<"SetKeyFrame">,
    RegistrationOperation<"ResetKeyFrame">,
    RegistrationOperation<"InvertKeyFrame">,
    RegistrationOperation<"MirrorKeyFrameHorizontally">,
    RegistrationOperation<"MirrorKeyFrameVertically">,
    RegistrationOperation<"FlipDeform">,
    RegistrationOperation<"SymmetrizeDeform">,
    RegistrationOperation<"SetFromHorizontalMirror">,
    RegistrationOperation<"SetFromVerticalMirror">,
    RegistrationOperation<"SetFromDiagonalMirror">,
    RegistrationOperation<"SetFrom1DMirror">,
    RegistrationOperation<"CopyBinding">,
    RegistrationOperation<"PasteBinding">,
    RegistrationOperation<"RemoveBinding">,
    RegistrationOperation<"SetInterpolation", nicxlive::core::common::InterpolateMode::Linear>>;

} // namespace nijigenerate::commands::binding::detail
