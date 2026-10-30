#pragma once

#include <nijigenerate/core/typed_format_inspector_enums.hpp>
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include "detail/simplephysics_constructor_text.hpp"
#include "detail/simplephysics_type_origins.hpp"
#include <nodes/simple_physics_driver.hpp>

namespace nijigenerate::commands::node::simplephysics {
namespace detail { struct PhysicsCommandText; }

struct SetSimplePhysicsModelTypeArguments {
    nicxlive::core::nodes::SourcePhysicsModel modelType = nicxlive::core::nodes::SourcePhysicsModel::Pendulum;
};
struct SetSimplePhysicsMapModeArguments {
    nicxlive::core::nodes::SourceParamMapMode mapMode = nicxlive::core::nodes::SourceParamMapMode::AngleLength;
};
struct SetSimplePhysicsLocalOnlyArguments {
    bool localOnly = false;
};

} // namespace nijigenerate::commands::node::simplephysics

namespace nijigenerate::commands {

template<> struct CommandFields<node::simplephysics::SetSimplePhysicsModelTypeArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::simplephysics::SetSimplePhysicsModelTypeArguments::modelType,
            "modelType", "Physics model type", "PhysicsModel">{}};
};
template<> struct CommandFields<node::simplephysics::SetSimplePhysicsMapModeArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::simplephysics::SetSimplePhysicsMapModeArguments::mapMode,
            "mapMode", "Parameter mapping mode", "ParamMapMode">{}};
};
template<> struct CommandFields<node::simplephysics::SetSimplePhysicsLocalOnlyArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::simplephysics::SetSimplePhysicsLocalOnlyArguments::localOnly,
            "localOnly", "Whether physics listens only to local transform", "bool">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::node::simplephysics {

class SetSimplePhysicsModelTypeCommand : public ExCommand<SetSimplePhysicsModelTypeArguments>,
    public std::enable_shared_from_this<SetSimplePhysicsModelTypeCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsModelTypeCommand>;
    SetSimplePhysicsModelTypeCommand(detail::PhysicsCommandText text, nicxlive::core::nodes::SourcePhysicsModel value);
public:
    explicit SetSimplePhysicsModelTypeCommand(nicxlive::core::nodes::SourcePhysicsModel modelType);
    SetSimplePhysicsModelTypeCommand(const SetSimplePhysicsModelTypeCommand&) = delete;
    SetSimplePhysicsModelTypeCommand& operator=(const SetSimplePhysicsModelTypeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetSimplePhysicsMapModeCommand : public ExCommand<SetSimplePhysicsMapModeArguments>,
    public std::enable_shared_from_this<SetSimplePhysicsMapModeCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsMapModeCommand>;
    SetSimplePhysicsMapModeCommand(detail::PhysicsCommandText text, nicxlive::core::nodes::SourceParamMapMode value);
public:
    explicit SetSimplePhysicsMapModeCommand(nicxlive::core::nodes::SourceParamMapMode mapMode);
    SetSimplePhysicsMapModeCommand(const SetSimplePhysicsMapModeCommand&) = delete;
    SetSimplePhysicsMapModeCommand& operator=(const SetSimplePhysicsMapModeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetSimplePhysicsLocalOnlyCommand : public ExCommand<SetSimplePhysicsLocalOnlyArguments>,
    public std::enable_shared_from_this<SetSimplePhysicsLocalOnlyCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsLocalOnlyCommand>;
    SetSimplePhysicsLocalOnlyCommand(detail::PhysicsCommandText text, bool value);
public:
    explicit SetSimplePhysicsLocalOnlyCommand(bool localOnly);
    SetSimplePhysicsLocalOnlyCommand(const SetSimplePhysicsLocalOnlyCommand&) = delete;
    SetSimplePhysicsLocalOnlyCommand& operator=(const SetSimplePhysicsLocalOnlyCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

} // namespace nijigenerate::commands::node::simplephysics

namespace nijigenerate::commands {

template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsModelTypeCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::SetSimplePhysicsModelTypeArguments>, Command>;
};
template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsMapModeCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::SetSimplePhysicsMapModeArguments>, Command>;
};
template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsLocalOnlyCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::SetSimplePhysicsLocalOnlyArguments>, Command>;
};

} // namespace nijigenerate::commands
