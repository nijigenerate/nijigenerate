#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include "editor_table.hpp"
#include <stdexcept>
#include <cstdint>
#include <concepts>
#include <memory>
#include <utility>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::depth::editor {
class AddEditorDepthOpCommand;
class UpdateEditorDepthOpCommand;
class RemoveEditorDepthOpCommand;
}
namespace nicxlive::core::d_runtime {
#define NG_DEPTH_EDITOR_SOURCE_NAMES(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::commands::depth::editor::Type> { \
    inline static constexpr bool isNativeObject = true; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.depth.editor." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.depth.editor." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
}; \
template<> struct SourceObjectTypeNames<::nijigenerate::commands::depth::editor::Type> { \
    inline static constexpr char className[] = "nijigenerate.commands.depth.editor." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.depth.editor." #Type ")"; \
};
NG_DEPTH_EDITOR_SOURCE_NAMES(AddEditorDepthOpCommand)
NG_DEPTH_EDITOR_SOURCE_NAMES(UpdateEditorDepthOpCommand)
NG_DEPTH_EDITOR_SOURCE_NAMES(RemoveEditorDepthOpCommand)
#undef NG_DEPTH_EDITOR_SOURCE_NAMES
}

namespace nijigenerate::viewport::depth {
namespace mesheditor::editor { class DepthMeshEditor; }
namespace mesheditor::node { class DepthMeshEditorOne; }
namespace tools::operation { class DepthOperation; }
}

namespace nijigenerate::commands::depth::editor {

namespace detail {
struct DepthCommandText {
    compat::RetainedString label;
    compat::RetainedString description;
};
}

void ngInitDepthEditorCommands();

struct AddEditorDepthOpArguments {
    std::shared_ptr<::nijigenerate::viewport::depth::mesheditor::editor::DepthMeshEditor> editorSet;
    std::shared_ptr<::nijigenerate::viewport::depth::mesheditor::node::DepthMeshEditorOne> target;
    std::shared_ptr<::nijigenerate::viewport::depth::tools::operation::DepthOperation> operation;
    std::int32_t index = 0;
};
struct UpdateEditorDepthOpArguments {
    std::shared_ptr<::nijigenerate::viewport::depth::mesheditor::editor::DepthMeshEditor> editorSet;
    std::shared_ptr<::nijigenerate::viewport::depth::mesheditor::node::DepthMeshEditorOne> target;
    std::int32_t index = 0;
    std::shared_ptr<::nijigenerate::viewport::depth::tools::operation::DepthOperation> operation;
};
struct RemoveEditorDepthOpArguments {
    std::shared_ptr<::nijigenerate::viewport::depth::mesheditor::editor::DepthMeshEditor> editorSet;
    std::shared_ptr<::nijigenerate::viewport::depth::mesheditor::node::DepthMeshEditorOne> target;
    std::int32_t index = 0;
};

} // namespace nijigenerate::commands::depth::editor

namespace nijigenerate::commands {

template<> struct CommandFields<depth::editor::AddEditorDepthOpArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::editor::AddEditorDepthOpArguments::editorSet, "editorSet",
            "Depth editor session", "DepthMeshEditor">{},
        FieldDescriptor<&depth::editor::AddEditorDepthOpArguments::target, "target",
            "Depth editor target", "DepthMeshEditorOne">{},
        FieldDescriptor<&depth::editor::AddEditorDepthOpArguments::operation, "operation",
            "Depth operation", "DepthOperation">{},
        FieldDescriptor<&depth::editor::AddEditorDepthOpArguments::index, "index",
            "Insertion index, or -1 to append", "int">{}};
};
template<> struct CommandFields<depth::editor::UpdateEditorDepthOpArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::editor::UpdateEditorDepthOpArguments::editorSet, "editorSet",
            "Depth editor session", "DepthMeshEditor">{},
        FieldDescriptor<&depth::editor::UpdateEditorDepthOpArguments::target, "target",
            "Depth editor target", "DepthMeshEditorOne">{},
        FieldDescriptor<&depth::editor::UpdateEditorDepthOpArguments::index, "index", "Operation index", "int">{},
        FieldDescriptor<&depth::editor::UpdateEditorDepthOpArguments::operation, "operation",
            "Replacement depth operation", "DepthOperation">{}};
};
template<> struct CommandFields<depth::editor::RemoveEditorDepthOpArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::editor::RemoveEditorDepthOpArguments::editorSet, "editorSet",
            "Depth editor session", "DepthMeshEditor">{},
        FieldDescriptor<&depth::editor::RemoveEditorDepthOpArguments::target, "target",
            "Depth editor target", "DepthMeshEditorOne">{},
        FieldDescriptor<&depth::editor::RemoveEditorDepthOpArguments::index, "index", "Operation index", "int">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::depth::editor {

class AddEditorDepthOpCommand : public ExCommand<AddEditorDepthOpArguments>,
    public std::enable_shared_from_this<AddEditorDepthOpCommand> {
    static detail::DepthCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AddEditorDepthOpCommand>;
    explicit AddEditorDepthOpCommand(detail::DepthCommandText text);
protected:
    void prepareSourceClassInfo() override;
public:
    AddEditorDepthOpCommand();
    AddEditorDepthOpCommand(const AddEditorDepthOpCommand&) = delete;
    AddEditorDepthOpCommand& operator=(const AddEditorDepthOpCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class UpdateEditorDepthOpCommand : public ExCommand<UpdateEditorDepthOpArguments>,
    public std::enable_shared_from_this<UpdateEditorDepthOpCommand> {
    static detail::DepthCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<UpdateEditorDepthOpCommand>;
    explicit UpdateEditorDepthOpCommand(detail::DepthCommandText text);
protected:
    void prepareSourceClassInfo() override;
public:
    UpdateEditorDepthOpCommand();
    UpdateEditorDepthOpCommand(const UpdateEditorDepthOpCommand&) = delete;
    UpdateEditorDepthOpCommand& operator=(const UpdateEditorDepthOpCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class RemoveEditorDepthOpCommand : public ExCommand<RemoveEditorDepthOpArguments>,
    public std::enable_shared_from_this<RemoveEditorDepthOpCommand> {
    static detail::DepthCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<RemoveEditorDepthOpCommand>;
    explicit RemoveEditorDepthOpCommand(detail::DepthCommandText text);
protected:
    void prepareSourceClassInfo() override;
public:
    RemoveEditorDepthOpCommand();
    RemoveEditorDepthOpCommand(const RemoveEditorDepthOpCommand&) = delete;
    RemoveEditorDepthOpCommand& operator=(const RemoveEditorDepthOpCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

template<DepthEditorOperationCommand id, class... Args>
std::shared_ptr<CommandResult> ngRunDepthEditorCommand(std::shared_ptr<Context> context, Args&&... args) {
    static_assert(id == DepthEditorOperationCommand::AddEditorDepthOp ||
        id == DepthEditorOperationCommand::UpdateEditorDepthOp ||
        id == DepthEditorOperationCommand::RemoveEditorDepthOp);
    const auto found = commands.find(id);
    if (found == commands.end()) throw std::runtime_error("No registered depth editor command");
    const auto base = found->second;
    if (!ngCommandAllowedInCurrentContext(base))
        return CommandResult::opCall(false, "Command is not available in the current command scope");
    using C = std::conditional_t<id == DepthEditorOperationCommand::AddEditorDepthOp, AddEditorDepthOpCommand,
        std::conditional_t<id == DepthEditorOperationCommand::UpdateEditorDepthOp, UpdateEditorDepthOpCommand,
            RemoveEditorDepthOpCommand>>;
    const auto command = std::dynamic_pointer_cast<C>(base);
    if (!command) throw std::runtime_error("Registered depth editor command instance type mismatch");
    if constexpr (id == DepthEditorOperationCommand::AddEditorDepthOp)
        ngAssignCommandFields(static_cast<AddEditorDepthOpArguments&>(*command), std::forward<Args>(args)...);
    else if constexpr (id == DepthEditorOperationCommand::UpdateEditorDepthOp)
        ngAssignCommandFields(static_cast<UpdateEditorDepthOpArguments&>(*command), std::forward<Args>(args)...);
    else
        ngAssignCommandFields(static_cast<RemoveEditorDepthOpArguments&>(*command), std::forward<Args>(args)...);
    return ngRunCommand(command, context);
}


} // namespace nijigenerate::commands::depth::editor

namespace nijigenerate::commands {

template<> struct CommandTraits<depth::editor::AddEditorDepthOpCommand> {
    static constexpr auto attributes = std::tuple{McpHidden};
    using BaseClasses = CommandTypeList<ExCommand<depth::editor::AddEditorDepthOpArguments>, Command>;
};
template<> struct CommandTraits<depth::editor::UpdateEditorDepthOpCommand> {
    static constexpr auto attributes = std::tuple{McpHidden};
    using BaseClasses = CommandTypeList<ExCommand<depth::editor::UpdateEditorDepthOpArguments>, Command>;
};
template<> struct CommandTraits<depth::editor::RemoveEditorDepthOpCommand> {
    static constexpr auto attributes = std::tuple{McpHidden};
    using BaseClasses = CommandTypeList<ExCommand<depth::editor::RemoveEditorDepthOpArguments>, Command>;
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands {

template<> struct SourceCommandConstructor<depth::editor::AddEditorDepthOpCommand> {
    template<class... Args> struct Program {
        using C = depth::editor::AddEditorDepthOpCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::sourceConstructorText() } -> std::same_as<depth::editor::detail::DepthCommandText>;
            } && SourceExCommandConstructorProgram<depth::editor::AddEditorDepthOpArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<depth::editor::UpdateEditorDepthOpCommand> {
    template<class... Args> struct Program {
        using C = depth::editor::UpdateEditorDepthOpCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::sourceConstructorText() } -> std::same_as<depth::editor::detail::DepthCommandText>;
            } && SourceExCommandConstructorProgram<depth::editor::UpdateEditorDepthOpArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<depth::editor::RemoveEditorDepthOpCommand> {
    template<class... Args> struct Program {
        using C = depth::editor::RemoveEditorDepthOpCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::sourceConstructorText() } -> std::same_as<depth::editor::detail::DepthCommandText>;
            } && SourceExCommandConstructorProgram<depth::editor::RemoveEditorDepthOpArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands

namespace nijigenerate::commands::depth::editor {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
template<auto Id, class C>
using Required = ::nijigenerate::commands::detail::DefaultCommandRegistration<Id, C>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Required<DepthEditorOperationCommand::AddEditorDepthOp, AddEditorDepthOpCommand>,
    Required<DepthEditorOperationCommand::UpdateEditorDepthOp, UpdateEditorDepthOpCommand>,
    Required<DepthEditorOperationCommand::RemoveEditorDepthOp, RemoveEditorDepthOpCommand>>;
}
template<class T> requires (std::same_as<T, DepthEditorOperationCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitDepthEditorCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<depth::editor::DepthEditorOperationCommand> {
    static consteval bool supported() {
        return depth::editor::initialization::Program::supported<depth::editor::initialization::Table>();
    }
    template<class ActualKey = depth::editor::DepthEditorOperationCommand>
        requires (std::same_as<ActualKey, depth::editor::DepthEditorOperationCommand> && supported())
    static void invoke() { depth::editor::ngInitCommands<ActualKey>(); }
};
}
