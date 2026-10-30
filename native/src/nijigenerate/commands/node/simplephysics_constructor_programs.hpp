#pragma once
#include "simplephysics_float_commands.hpp"
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>

namespace nijigenerate::commands {

// These are the twelve actual classes in the module's production registration
// sequence. Each factory executes the same typed initialization selected by its
// query, then the defining public constructor and its real text/base/body path.
#define NG_PHYSICS_VALUE_CONSTRUCTOR(Type, ParameterType, StorageType) \
template<> struct SourceCommandConstructor<node::simplephysics::Type> { \
    template<class... Args> struct Program { \
        using C = node::simplephysics::Type; \
        using Parameter = ParameterType; \
        using Storage = node::simplephysics::StorageType; \
        static consteval bool supported() { \
            if constexpr (sizeof...(Args) != 1) return false; \
            else return (ngCommandInitializationSupported<Parameter, Args&&>() && ...) && requires { \
                { C::sourceConstructorText() } -> std::same_as<node::simplephysics::detail::PhysicsCommandText>; \
            } && SourceExCommandConstructorProgram<Storage>::template preferredSupported<Parameter>() && \
                detail::declaredSourceParametersSupported<std::tuple<Parameter>, Args...>(); \
        } \
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) \
            requires(supported()) { \
            return detail::constructDeclaredSourceCommand<C, std::tuple<Parameter>>( \
                allocator, std::forward<Args>(args)...); \
        } \
    }; \
};

NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsParameterCommand,
    std::shared_ptr<nicxlive::core::param::Parameter>, SetSimplePhysicsParameterArguments)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsModelTypeCommand,
    nicxlive::core::nodes::SourcePhysicsModel, SetSimplePhysicsModelTypeArguments)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsMapModeCommand,
    nicxlive::core::nodes::SourceParamMapMode, SetSimplePhysicsMapModeArguments)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsLocalOnlyCommand, bool, SetSimplePhysicsLocalOnlyArguments)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsGravityCommand, float,
    PhysicsFloatArguments<"Set SimplePhysics gravity scale">)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsLengthCommand, float,
    PhysicsFloatArguments<"Set SimplePhysics pendulum or spring rest length">)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsFrequencyCommand, float,
    PhysicsFloatArguments<"Set SimplePhysics resonant frequency">)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsAngleDampingCommand, float,
    PhysicsFloatArguments<"Set SimplePhysics angular damping ratio">)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsLengthDampingCommand, float,
    PhysicsFloatArguments<"Set SimplePhysics length damping ratio">)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsOutputScaleXCommand, float,
    PhysicsFloatArguments<"Set SimplePhysics output scale X">)
NG_PHYSICS_VALUE_CONSTRUCTOR(SetSimplePhysicsOutputScaleYCommand, float,
    PhysicsFloatArguments<"Set SimplePhysics output scale Y">)
#undef NG_PHYSICS_VALUE_CONSTRUCTOR

template<> struct SourceCommandConstructor<node::simplephysics::ClearSimplePhysicsParameterCommand> {
    template<class... Args> struct Program {
        using C = node::simplephysics::ClearSimplePhysicsParameterCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::sourceConstructorText() } -> std::same_as<node::simplephysics::detail::PhysicsCommandText>;
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

} // namespace nijigenerate::commands
