#pragma once
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/command_table.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/enums.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/base.hpp>
#include <compat/object_metadata.hpp>
#include <concepts>
#include <utility>

namespace nijigenerate::commands::mesheditor::tool {

using VertexToolMode = ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode;

struct SelectToolModeArguments {
    ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode mode =
        ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::Points;
};

} // namespace nijigenerate::commands::mesheditor::tool

namespace nijigenerate::commands {

template<> struct CommandFields<mesheditor::tool::SelectToolModeArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&mesheditor::tool::SelectToolModeArguments::mode,
            "mode", "Tool mode", "VertexToolMode">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::mesheditor::tool {

class SelectToolModeCommand : public ExCommand<SelectToolModeArguments>,
    public std::enable_shared_from_this<SelectToolModeCommand> {
    struct SourcePresentation { compat::RetainedString description; };
    static SourcePresentation sourceConstructorPresentation();
    SelectToolModeCommand(SourcePresentation presentation, VertexToolMode mode);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SelectToolModeCommand>;
    struct SourceInitializationProgram {
        using Info = ::nijigenerate::viewport::common::mesheditor::tools::base::ToolInfo;
        using InfoRef = std::shared_ptr<Info>;
        static consteval bool supported() {
            return ngCommandAssignmentSupported<compat::RetainedString, compat::RetainedString&>() &&
                ngCommandAssignmentSupported<InfoRef, InfoRef&>() && requires(Info& value) {
                    { value.mode() } -> std::same_as<VertexToolMode>;
                    { value.description() } -> std::same_as<compat::RetainedString>;
                };
        }
        static void initialize(SelectToolModeCommand& target);
    };
    struct SourceAfterBaseProgram {
        static consteval bool supported() {
            return SourceInitializationProgram::supported() &&
                requires(SelectToolModeCommand& target) { target._init(); };
        }
        static void initialize(SelectToolModeCommand& target) { target._init(); }
    };
protected:
    void prepareSourceClassInfo() override;
public:
    std::shared_ptr<::nijigenerate::viewport::common::mesheditor::tools::base::ToolInfo> info;
    explicit SelectToolModeCommand(::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode mode);
    SelectToolModeCommand(const SelectToolModeCommand&) = delete;
    SelectToolModeCommand& operator=(const SelectToolModeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
    bool runnable(std::shared_ptr<Context> context) override;
    virtual void _init();
};

std::shared_ptr<Command> ngEnsureSelectToolModeCommand(
    ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode mode);
void ngInitSelectToolModeCommands();
using SelectToolModeCommandMap = ::nijigenerate::commands::CommandTableStorage<VertexToolMode>;
extern thread_local SelectToolModeCommandMap selectToolModeCommands;

} // namespace nijigenerate::commands::mesheditor::tool

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::commands::mesheditor::tool::SelectToolModeCommand> {
    inline static constexpr char className[] =
        "nijigenerate.commands.mesheditor.tool.SelectToolModeCommand";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.mesheditor.tool.SelectToolModeCommand)";
};
template<> struct SourceTypeSpellings<::nijigenerate::commands::mesheditor::tool::SelectToolModeCommand> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.commands.mesheditor.tool.SelectToolModeCommand)"),
            RetainedString::fromStaticLiteral("(SelectToolModeCommand)"),
            RetainedString::fromStaticLiteral("nijigenerate.commands.mesheditor.tool.SelectToolModeCommand"),
            RetainedString::fromStaticLiteral("SelectToolModeCommand")};
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands {

template<> struct CommandTraits<mesheditor::tool::SelectToolModeCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<ExCommand<mesheditor::tool::SelectToolModeArguments>, Command>;
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands {
template<> struct SourceCommandConstructor<mesheditor::tool::SelectToolModeCommand> {
    template<class... Args> struct Program {
        using C = mesheditor::tool::SelectToolModeCommand;
        using Mode = mesheditor::tool::VertexToolMode;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else return detail::declaredSourceParametersSupported<std::tuple<Mode>, Args...>() &&
                requires {
                    { C::sourceConstructorPresentation() } -> std::same_as<typename C::SourcePresentation>;
                } && SourceExCommandConstructorProgram<mesheditor::tool::SelectToolModeArguments>::
                    preferredSupported<Mode&>() && C::SourceAfterBaseProgram::supported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<Mode>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands

namespace nijigenerate::commands::mesheditor::tool {
namespace initialization {
consteval bool supported() {
    return ngRuntimeCommandRegistrationSupported<SelectToolModeCommand,
        SelectToolModeCommandMap, VertexToolMode, VertexToolMode>();
}
}
template<class T> requires (std::same_as<T, VertexToolMode> && initialization::supported())
void ngInitCommands() { ngInitSelectToolModeCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<mesheditor::tool::VertexToolMode> {
    static consteval bool supported() { return mesheditor::tool::initialization::supported(); }
    template<class ActualKey = mesheditor::tool::VertexToolMode>
        requires (std::same_as<ActualKey, mesheditor::tool::VertexToolMode> && supported())
    static void invoke() { mesheditor::tool::ngInitCommands<ActualKey>(); }
};
}

