#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include "detail/simplephysics_constructor_text.hpp"
#include "detail/simplephysics_type_origins.hpp"

namespace nicxlive::core::param { class Parameter; }

namespace nijigenerate::commands::node::simplephysics {
namespace detail { struct PhysicsCommandText; }

struct SetSimplePhysicsParameterArguments {
    std::shared_ptr<nicxlive::core::param::Parameter> parameter;
};

} // namespace nijigenerate::commands::node::simplephysics

namespace nijigenerate::commands {

template<> struct CommandFields<node::simplephysics::SetSimplePhysicsParameterArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::simplephysics::SetSimplePhysicsParameterArguments::parameter,
            "parameter", "Target parameter driven by this SimplePhysics node", "Parameter">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::node::simplephysics {

class SetSimplePhysicsParameterCommand : public ExCommand<SetSimplePhysicsParameterArguments>,
    public std::enable_shared_from_this<SetSimplePhysicsParameterCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetSimplePhysicsParameterCommand>;
    SetSimplePhysicsParameterCommand(detail::PhysicsCommandText text,
        std::shared_ptr<nicxlive::core::param::Parameter> parameter);
public:
    explicit SetSimplePhysicsParameterCommand(std::shared_ptr<nicxlive::core::param::Parameter> parameter);
    SetSimplePhysicsParameterCommand(const SetSimplePhysicsParameterCommand&) = delete;
    SetSimplePhysicsParameterCommand& operator=(const SetSimplePhysicsParameterCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class ClearSimplePhysicsParameterCommand : public ExCommand<>,
    public std::enable_shared_from_this<ClearSimplePhysicsParameterCommand> {
    static detail::PhysicsCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ClearSimplePhysicsParameterCommand>;
    explicit ClearSimplePhysicsParameterCommand(detail::PhysicsCommandText text);
public:
    ClearSimplePhysicsParameterCommand();
    ClearSimplePhysicsParameterCommand(const ClearSimplePhysicsParameterCommand&) = delete;
    ClearSimplePhysicsParameterCommand& operator=(const ClearSimplePhysicsParameterCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

} // namespace nijigenerate::commands::node::simplephysics

namespace nijigenerate::commands {

template<> struct CommandTraits<node::simplephysics::SetSimplePhysicsParameterCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::simplephysics::SetSimplePhysicsParameterArguments>, Command>;
};
template<> struct CommandTraits<node::simplephysics::ClearSimplePhysicsParameterCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};

} // namespace nijigenerate::commands
