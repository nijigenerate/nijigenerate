#pragma once
#include <nijigenerate/commands/command.hpp>
#include "simplephysics_parameter_commands.hpp"
#include "simplephysics_config_commands.hpp"
#include <limits>
namespace nijigenerate::commands::node::simplephysics {
template<CommandLiteral Description> struct PhysicsFloatArguments {
    float value = std::numeric_limits<float>::quiet_NaN();
};
}
namespace nijigenerate::commands {
template<CommandLiteral Description> struct CommandFields<node::simplephysics::PhysicsFloatArguments<Description>> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::simplephysics::PhysicsFloatArguments<Description>::value,
            "value", Description, "float">{}};
};
}
namespace nijigenerate::commands::node::simplephysics {
class SetSimplePhysicsGravityCommand : public ExCommand<PhysicsFloatArguments<"Set SimplePhysics gravity scale">>,
    public std::enable_shared_from_this<SetSimplePhysicsGravityCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    SetSimplePhysicsGravityCommand(detail::PhysicsCommandText text, float value);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsGravityCommand>;
public:
    explicit SetSimplePhysicsGravityCommand(float value);
    SetSimplePhysicsGravityCommand(const SetSimplePhysicsGravityCommand&) = delete;
    SetSimplePhysicsGravityCommand& operator=(const SetSimplePhysicsGravityCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetSimplePhysicsLengthCommand : public ExCommand<PhysicsFloatArguments<"Set SimplePhysics pendulum or spring rest length">>,
    public std::enable_shared_from_this<SetSimplePhysicsLengthCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    SetSimplePhysicsLengthCommand(detail::PhysicsCommandText text, float value);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsLengthCommand>;
public:
    explicit SetSimplePhysicsLengthCommand(float value);
    SetSimplePhysicsLengthCommand(const SetSimplePhysicsLengthCommand&) = delete;
    SetSimplePhysicsLengthCommand& operator=(const SetSimplePhysicsLengthCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetSimplePhysicsFrequencyCommand : public ExCommand<PhysicsFloatArguments<"Set SimplePhysics resonant frequency">>,
    public std::enable_shared_from_this<SetSimplePhysicsFrequencyCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    SetSimplePhysicsFrequencyCommand(detail::PhysicsCommandText text, float value);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsFrequencyCommand>;
public:
    explicit SetSimplePhysicsFrequencyCommand(float value);
    SetSimplePhysicsFrequencyCommand(const SetSimplePhysicsFrequencyCommand&) = delete;
    SetSimplePhysicsFrequencyCommand& operator=(const SetSimplePhysicsFrequencyCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetSimplePhysicsAngleDampingCommand : public ExCommand<PhysicsFloatArguments<"Set SimplePhysics angular damping ratio">>,
    public std::enable_shared_from_this<SetSimplePhysicsAngleDampingCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    SetSimplePhysicsAngleDampingCommand(detail::PhysicsCommandText text, float value);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsAngleDampingCommand>;
public:
    explicit SetSimplePhysicsAngleDampingCommand(float value);
    SetSimplePhysicsAngleDampingCommand(const SetSimplePhysicsAngleDampingCommand&) = delete;
    SetSimplePhysicsAngleDampingCommand& operator=(const SetSimplePhysicsAngleDampingCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetSimplePhysicsLengthDampingCommand : public ExCommand<PhysicsFloatArguments<"Set SimplePhysics length damping ratio">>,
    public std::enable_shared_from_this<SetSimplePhysicsLengthDampingCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    SetSimplePhysicsLengthDampingCommand(detail::PhysicsCommandText text, float value);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsLengthDampingCommand>;
public:
    explicit SetSimplePhysicsLengthDampingCommand(float value);
    SetSimplePhysicsLengthDampingCommand(const SetSimplePhysicsLengthDampingCommand&) = delete;
    SetSimplePhysicsLengthDampingCommand& operator=(const SetSimplePhysicsLengthDampingCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetSimplePhysicsOutputScaleXCommand : public ExCommand<PhysicsFloatArguments<"Set SimplePhysics output scale X">>,
    public std::enable_shared_from_this<SetSimplePhysicsOutputScaleXCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    SetSimplePhysicsOutputScaleXCommand(detail::PhysicsCommandText text, float value);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsOutputScaleXCommand>;
public:
    explicit SetSimplePhysicsOutputScaleXCommand(float value);
    SetSimplePhysicsOutputScaleXCommand(const SetSimplePhysicsOutputScaleXCommand&) = delete;
    SetSimplePhysicsOutputScaleXCommand& operator=(const SetSimplePhysicsOutputScaleXCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetSimplePhysicsOutputScaleYCommand : public ExCommand<PhysicsFloatArguments<"Set SimplePhysics output scale Y">>,
    public std::enable_shared_from_this<SetSimplePhysicsOutputScaleYCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    SetSimplePhysicsOutputScaleYCommand(detail::PhysicsCommandText text, float value);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsOutputScaleYCommand>;
public:
    explicit SetSimplePhysicsOutputScaleYCommand(float value);
    SetSimplePhysicsOutputScaleYCommand(const SetSimplePhysicsOutputScaleYCommand&) = delete;
    SetSimplePhysicsOutputScaleYCommand& operator=(const SetSimplePhysicsOutputScaleYCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
void ngInitSimplePhysicsCommands();
}
namespace nijigenerate::commands {
template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsGravityCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::PhysicsFloatArguments<"Set SimplePhysics gravity scale">>, Command>;
};
template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsLengthCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::PhysicsFloatArguments<"Set SimplePhysics pendulum or spring rest length">>, Command>;
};
template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsFrequencyCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::PhysicsFloatArguments<"Set SimplePhysics resonant frequency">>, Command>;
};
template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsAngleDampingCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::PhysicsFloatArguments<"Set SimplePhysics angular damping ratio">>, Command>;
};
template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsLengthDampingCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::PhysicsFloatArguments<"Set SimplePhysics length damping ratio">>, Command>;
};
template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsOutputScaleXCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::PhysicsFloatArguments<"Set SimplePhysics output scale X">>, Command>;
};
template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsOutputScaleYCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::PhysicsFloatArguments<"Set SimplePhysics output scale Y">>, Command>;
};
}
