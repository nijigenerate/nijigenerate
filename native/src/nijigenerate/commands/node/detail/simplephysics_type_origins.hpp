#pragma once
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::node::simplephysics {
class SetSimplePhysicsParameterCommand;
class ClearSimplePhysicsParameterCommand;
class SetSimplePhysicsModelTypeCommand;
class SetSimplePhysicsMapModeCommand;
class SetSimplePhysicsLocalOnlyCommand;
class SetSimplePhysicsGravityCommand;
class SetSimplePhysicsLengthCommand;
class SetSimplePhysicsFrequencyCommand;
class SetSimplePhysicsAngleDampingCommand;
class SetSimplePhysicsLengthDampingCommand;
class SetSimplePhysicsOutputScaleXCommand;
class SetSimplePhysicsOutputScaleYCommand;
}

namespace nicxlive::core::d_runtime {
// Independent nongeneric declarations in commands/node/simplephysics.d.
// The seven string-mixin calls declare their actual named classes in that same
// module. These records describe those declarations, not their C++ recipe type.
#define NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(C) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::node::simplephysics::C> { \
        inline static constexpr char className[] = "nijigenerate.commands.node.simplephysics." #C; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.node.simplephysics." #C ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::node::simplephysics::C> { \
        inline static constexpr bool isNativeObject = true; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(nijigenerate.commands.node.simplephysics." #C ")"), \
                RetainedString::fromStaticLiteral("(" #C ")"), \
                RetainedString::fromStaticLiteral("nijigenerate.commands.node.simplephysics." #C), \
                RetainedString::fromStaticLiteral(#C)}; \
        } \
    }

NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsParameterCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(ClearSimplePhysicsParameterCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsModelTypeCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsMapModeCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsLocalOnlyCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsGravityCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsLengthCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsFrequencyCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsAngleDampingCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsLengthDampingCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsOutputScaleXCommand);
NG_SIMPLE_PHYSICS_COMMAND_ORIGIN(SetSimplePhysicsOutputScaleYCommand);
#undef NG_SIMPLE_PHYSICS_COMMAND_ORIGIN
}
