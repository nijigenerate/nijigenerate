#pragma once
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include "node.hpp"
#include "base.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <nijigenerate/commands/context.hpp>
#include <memory>
#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>
namespace nijigenerate::commands::node::node {
namespace detail {
struct NodeCommandText { compat::RetainedString label; compat::RetainedString description; };
struct NodeCommandDescription { compat::RetainedString description; };
}
template<bool expose> struct AddArguments { compat::RetainedString className; compat::RetainedString _suffix; };
template<bool expose> struct ConvertArguments { compat::RetainedString className; };
struct MoveArguments { base::NodeRef newParent; std::uint64_t index = 0; };
struct RenameArguments { compat::Slice<compat::RetainedString> newNames; };
}
namespace nijigenerate::commands {
template<bool E> struct CommandFields<node::node::AddArguments<E>> {
    using A = node::node::AddArguments<E>;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&A::className,"className","class name of new node.","string",true,!E>{},
        FieldDescriptor<&A::_suffix,"_suffix","suffix pattern for new node","string",true,false>{}};
};
template<bool E> struct CommandFields<node::node::ConvertArguments<E>> {
    using A = node::node::ConvertArguments<E>;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&A::className,"className","new class name for node","string",true,!E>{}};
};
template<> struct CommandFields<node::node::MoveArguments> {
    using A = node::node::MoveArguments;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&A::newParent,"newParent","new parent node","Node">{},
        FieldDescriptor<&A::index,"index","index in new parent node","ulong">{}};
};
template<> struct CommandFields<node::node::RenameArguments> {
    using A = node::node::RenameArguments;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&A::newNames,"newNames","New node names. Must have the same length as context.nodes.","string[]">{}};
};
}
namespace nijigenerate::commands::node::node {
template<bool expose> class AddNodeCommandT : public ::nijigenerate::commands::detail::CovariantCommand<
        AddNodeCommandT<expose>, AddArguments<expose>> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<
        AddNodeCommandT<expose>, AddArguments<expose>>;
    static detail::NodeCommandDescription sourceConstructorDescription(const compat::RetainedString& name);
    AddNodeCommandT(detail::NodeCommandDescription text, compat::RetainedString name, compat::RetainedString suffix);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AddNodeCommandT<expose>>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = CreateResult<base::NodeRef>;
    AddNodeCommandT(const AddNodeCommandT&) = delete;
    AddNodeCommandT& operator=(const AddNodeCommandT&) = delete;
    AddNodeCommandT(AddNodeCommandT&&) = delete;
    AddNodeCommandT& operator=(AddNodeCommandT&&) = delete;
    AddNodeCommandT(compat::RetainedString className, compat::RetainedString suffix = {});
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& ctx) { return runTyped(ctx); }
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> ctx);
};
template<bool expose> class InsertNodeCommandT : public ::nijigenerate::commands::detail::CovariantCommand<
        InsertNodeCommandT<expose>, AddArguments<expose>> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<
        InsertNodeCommandT<expose>, AddArguments<expose>>;
    static detail::NodeCommandDescription sourceConstructorDescription(const compat::RetainedString& name);
    InsertNodeCommandT(detail::NodeCommandDescription text, compat::RetainedString name, compat::RetainedString suffix);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<InsertNodeCommandT<expose>>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = CreateResult<base::NodeRef>;
    InsertNodeCommandT(const InsertNodeCommandT&) = delete;
    InsertNodeCommandT& operator=(const InsertNodeCommandT&) = delete;
    InsertNodeCommandT(InsertNodeCommandT&&) = delete;
    InsertNodeCommandT& operator=(InsertNodeCommandT&&) = delete;
    InsertNodeCommandT(compat::RetainedString className, compat::RetainedString suffix = {});
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& ctx) { return runTyped(ctx); }
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> ctx);
};
template<bool expose> class ConvertToCommandT : public ::nijigenerate::commands::detail::CovariantCommand<
        ConvertToCommandT<expose>, ConvertArguments<expose>> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<
        ConvertToCommandT<expose>, ConvertArguments<expose>>;
    static detail::NodeCommandDescription sourceConstructorDescription(const compat::RetainedString& name);
    ConvertToCommandT(detail::NodeCommandDescription text, compat::RetainedString name);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ConvertToCommandT<expose>>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = CreateResult<base::NodeRef>;
    ConvertToCommandT(compat::RetainedString className);
    ConvertToCommandT(const ConvertToCommandT&) = delete;
    ConvertToCommandT& operator=(const ConvertToCommandT&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& ctx) { return runTyped(ctx); }
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> ctx);
};
class MoveNodeCommand : public ExCommand<MoveArguments>,
    public std::enable_shared_from_this<MoveNodeCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    bool explicitNodes = false;
    static detail::NodeCommandDescription sourceConstructorDescription();
    MoveNodeCommand(detail::NodeCommandDescription text, base::NodeRef parent, std::uint64_t index, bool explicitNodes);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<MoveNodeCommand>;
    static consteval bool sourceFlagSupported() { return ngCommandInitializationSupported<bool, bool&>(); }
    void initializeSourceFlag(bool& value) { SourceCommandInitialization<bool, bool&>::construct(explicitNodes, value); }
public:
    MoveNodeCommand(base::NodeRef parent, std::uint64_t index, bool explicitNodes = false);
    MoveNodeCommand(const MoveNodeCommand&) = delete;
    MoveNodeCommand& operator=(const MoveNodeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class SetNodeNameCommand : public ExCommand<RenameArguments>,
    public std::enable_shared_from_this<SetNodeNameCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    SetNodeNameCommand(detail::NodeCommandText text, compat::Slice<compat::RetainedString> names);
    static detail::NodeCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetNodeNameCommand>;
public:
    SetNodeNameCommand(compat::Slice<compat::RetainedString> names = {});
    SetNodeNameCommand(const SetNodeNameCommand&) = delete;
    SetNodeNameCommand& operator=(const SetNodeNameCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class DeleteNodeCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<DeleteNodeCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    bool explicitNodes = false;
    static detail::NodeCommandDescription sourceConstructorDescription();
    DeleteNodeCommand(detail::NodeCommandDescription text, bool explicitNodes);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<DeleteNodeCommand>;
    static consteval bool sourceFlagSupported() { return ngCommandInitializationSupported<bool, bool&>(); }
    void initializeSourceFlag(bool& value) { SourceCommandInitialization<bool, bool&>::construct(explicitNodes, value); }
public:
    DeleteNodeCommand(bool explicitNodes = false);
    DeleteNodeCommand(const DeleteNodeCommand&) = delete;
    DeleteNodeCommand& operator=(const DeleteNodeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class CutNodeCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<CutNodeCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    explicit CutNodeCommand(detail::NodeCommandText text);
    static detail::NodeCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<CutNodeCommand>;
public:
    CutNodeCommand();
    CutNodeCommand(const CutNodeCommand&) = delete;
    CutNodeCommand& operator=(const CutNodeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
    bool runnable(std::shared_ptr<Context> ctx) override;
};
class CopyNodeCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<CopyNodeCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    explicit CopyNodeCommand(detail::NodeCommandText text);
    static detail::NodeCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<CopyNodeCommand>;
public:
    CopyNodeCommand();
    CopyNodeCommand(const CopyNodeCommand&) = delete;
    CopyNodeCommand& operator=(const CopyNodeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
    bool runnable(std::shared_ptr<Context> ctx) override;
};
class PasteNodeCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<PasteNodeCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    explicit PasteNodeCommand(detail::NodeCommandText text);
    static detail::NodeCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<PasteNodeCommand>;
public:
    PasteNodeCommand();
    PasteNodeCommand(const PasteNodeCommand&) = delete;
    PasteNodeCommand& operator=(const PasteNodeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
    bool runnable(std::shared_ptr<Context> ctx) override;
};
class ReloadNodeCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<ReloadNodeCommand> {
    static detail::NodeCommandDescription sourceConstructorDescription();
    explicit ReloadNodeCommand(detail::NodeCommandDescription text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ReloadNodeCommand>;
protected:
    void prepareSourceClassInfo() override;
private:
public:
    ReloadNodeCommand();
    ReloadNodeCommand(const ReloadNodeCommand&) = delete;
    ReloadNodeCommand& operator=(const ReloadNodeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class VertexModeCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<VertexModeCommand> {
    static detail::NodeCommandDescription sourceConstructorDescription();
    explicit VertexModeCommand(detail::NodeCommandDescription text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<VertexModeCommand>;
protected:
    void prepareSourceClassInfo() override;
private:
public:
    VertexModeCommand();
    VertexModeCommand(const VertexModeCommand&) = delete;
    VertexModeCommand& operator=(const VertexModeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ToggleVisibilityCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<ToggleVisibilityCommand> {
    static detail::NodeCommandDescription sourceConstructorDescription();
    explicit ToggleVisibilityCommand(detail::NodeCommandDescription text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ToggleVisibilityCommand>;
protected:
    void prepareSourceClassInfo() override;
private:
public:
    ToggleVisibilityCommand();
    ToggleVisibilityCommand(const ToggleVisibilityCommand&) = delete;
    ToggleVisibilityCommand& operator=(const ToggleVisibilityCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class CentralizeNodeCommand : public ExCommand<CommandNoArguments>,
    public std::enable_shared_from_this<CentralizeNodeCommand> {
    static detail::NodeCommandDescription sourceConstructorDescription();
    explicit CentralizeNodeCommand(detail::NodeCommandDescription text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<CentralizeNodeCommand>;
protected:
    void prepareSourceClassInfo() override;
private:
public:
    CentralizeNodeCommand();
    CentralizeNodeCommand(const CentralizeNodeCommand&) = delete;
    CentralizeNodeCommand& operator=(const CentralizeNodeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
void ngInitNodeCommands();
template<NodeCommand id, class... Args>
std::shared_ptr<CommandResult> ngRunNodeCommand(std::shared_ptr<Context> ctx, Args&&... args) {
    auto found = commands.find(id);
    if (found == commands.end()) throw std::runtime_error("No registered node command");
    const auto command = found->second;
    if (!ngCommandAllowedInCurrentContext(command))
        return CommandResult::opCall(false,"Command is not available in the current command scope");
    if constexpr (id == NodeCommand::MoveNode) {
        auto value = std::dynamic_pointer_cast<MoveNodeCommand>(command);
        if (!value) throw std::runtime_error("Registered node command instance type mismatch");
        ngAssignCommandFields(static_cast<MoveArguments&>(*value),std::forward<Args>(args)...);
    } else if constexpr (id == NodeCommand::AddNode || id == NodeCommand::InsertNode) {
        using C = std::conditional_t<id == NodeCommand::AddNode, AddNodeCommand, InsertNodeCommand>;
        auto value = std::dynamic_pointer_cast<C>(command);
        if (!value) throw std::runtime_error("Registered node command instance type mismatch");
        ngAssignCommandFields(static_cast<AddArguments<true>&>(*value),std::forward<Args>(args)...);
    } else if constexpr (id == NodeCommand::ConvertTo) {
        auto value = std::dynamic_pointer_cast<ConvertToCommand>(command);
        if (!value) throw std::runtime_error("Registered node command instance type mismatch");
        ngAssignCommandFields(static_cast<ConvertArguments<true>&>(*value),std::forward<Args>(args)...);
    } else if constexpr (id == NodeCommand::SetNodeName) {
        auto value = std::dynamic_pointer_cast<SetNodeNameCommand>(command);
        if (!value) throw std::runtime_error("Registered node command instance type mismatch");
        ngAssignCommandFields(static_cast<RenameArguments&>(*value),std::forward<Args>(args)...);
    } else static_assert(sizeof...(Args) == 0);
    return ngRunCommand(command,ctx);
}
}
namespace nijigenerate::commands {
template<bool E> struct CommandTraits<node::node::AddNodeCommandT<E>> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<node::node::AddArguments<E>>,Command>;
};
template<bool E> struct CommandTraits<node::node::InsertNodeCommandT<E>> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<node::node::AddArguments<E>>,Command>;
};
template<bool E> struct CommandTraits<node::node::ConvertToCommandT<E>> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::node::ConvertArguments<E>>,Command>;
};
template<> struct CommandTraits<node::node::MoveNodeCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::node::MoveArguments>,Command>;
};
template<> struct CommandTraits<node::node::SetNodeNameCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden,EffectRename};
    using BaseClasses = CommandTypeList<ExCommand<node::node::RenameArguments>,Command>;
};
template<> struct CommandTraits<node::node::DeleteNodeCommand> {
    static constexpr auto attributes = std::tuple{EffectDelete};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<node::node::CutNodeCommand> {
    static constexpr auto attributes = std::tuple{EffectDelete};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<node::node::CopyNodeCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<node::node::PasteNodeCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<node::node::ReloadNodeCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<node::node::VertexModeCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<node::node::ToggleVisibilityCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<node::node::CentralizeNodeCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
}

namespace nijigenerate::commands::node::node::constructor_detail {
template<class T> struct DynamicArray : std::false_type {};
template<class T> struct DynamicArray<compat::Slice<T>> : std::true_type {};
template<class...> inline constexpr bool missingParameter = false;
template<class Target, class Argument> consteval bool parameterSupported() {
    using Value = std::remove_cvref_t<Argument>;
    if constexpr (std::same_as<Target, compat::RetainedString>) {
        if constexpr (std::same_as<Value, std::nullptr_t>) return true;
        else if constexpr (::nijigenerate::commands::detail::commandStringDescriptor<Value>)
            return ngCommandInitializationSupported<Target, Argument&&>();
        else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value>) return false;
        else static_assert(missingParameter<Argument>, "Missing defining Source string constructor parameter");
    } else if constexpr (DynamicArray<Target>::value) {
        if constexpr (std::same_as<Value, std::nullptr_t>) return true;
        else if constexpr (std::same_as<Value, Target>) return ngCommandInitializationSupported<Target, Argument&&>();
        else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value>) return false;
        else static_assert(missingParameter<Argument>, "Missing defining Source array constructor parameter");
    } else return ngCommandInitializationSupported<Target, Argument&&>();
}
template<class Targets, class... Args> consteval bool parametersSupported() {
    return []<std::size_t... I>(std::index_sequence<I...>) {
        using Values = std::tuple<Args...>;
        return (parameterSupported<std::tuple_element_t<I, Targets>, std::tuple_element_t<I, Values>>() && ...) &&
            ::nijigenerate::commands::detail::declaredSourceParametersSupported<
                std::tuple<std::tuple_element_t<I, Targets>...>, Args...>();
    }(std::index_sequence_for<Args...>{});
}
template<class C, class Targets, class... Args>
std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) {
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return ::nijigenerate::commands::detail::constructDeclaredSourceCommand<C,
            std::tuple<std::tuple_element_t<I, Targets>...>>(allocator, std::forward<Args>(args)...);
    }(std::index_sequence_for<Args...>{});
}
} // namespace nijigenerate::commands::node::node::constructor_detail

namespace nijigenerate::commands {

template<bool E> struct SourceCommandConstructor<node::node::AddNodeCommandT<E>> {
    template<class... Args> struct Program {
        using C = node::node::AddNodeCommandT<E>;
        using Targets = std::tuple<compat::RetainedString, compat::RetainedString>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) < 1 || sizeof...(Args) > 2) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires(compat::RetainedString& name) { { C::sourceConstructorDescription(name) } -> std::same_as<node::node::detail::NodeCommandDescription>; } &&
                SourceExCommandConstructorProgram<node::node::AddArguments<E>>::template preferredSupported<compat::RetainedString&, compat::RetainedString&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<bool E> struct SourceCommandConstructor<node::node::InsertNodeCommandT<E>> {
    template<class... Args> struct Program {
        using C = node::node::InsertNodeCommandT<E>;
        using Targets = std::tuple<compat::RetainedString, compat::RetainedString>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) < 1 || sizeof...(Args) > 2) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires(compat::RetainedString& name) { { C::sourceConstructorDescription(name) } -> std::same_as<node::node::detail::NodeCommandDescription>; } &&
                SourceExCommandConstructorProgram<node::node::AddArguments<E>>::template preferredSupported<compat::RetainedString&, compat::RetainedString&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<bool E> struct SourceCommandConstructor<node::node::ConvertToCommandT<E>> {
    template<class... Args> struct Program {
        using C = node::node::ConvertToCommandT<E>;
        using Targets = std::tuple<compat::RetainedString>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) < 1 || sizeof...(Args) > 1) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires(compat::RetainedString& name) { { C::sourceConstructorDescription(name) } -> std::same_as<node::node::detail::NodeCommandDescription>; } &&
                SourceExCommandConstructorProgram<node::node::ConvertArguments<E>>::template preferredSupported<compat::RetainedString&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::MoveNodeCommand> {
    template<class... Args> struct Program {
        using C = node::node::MoveNodeCommand;
        using Targets = std::tuple<node::base::NodeRef, std::uint64_t, bool>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) < 2 || sizeof...(Args) > 3) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorDescription() } -> std::same_as<node::node::detail::NodeCommandDescription>; } &&
                SourceExCommandConstructorProgram<node::node::MoveArguments>::template preferredSupported<node::base::NodeRef&, std::uint64_t&>() && C::sourceFlagSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::SetNodeNameCommand> {
    template<class... Args> struct Program {
        using C = node::node::SetNodeNameCommand;
        using Targets = std::tuple<compat::Slice<compat::RetainedString>>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 1) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorText() } -> std::same_as<node::node::detail::NodeCommandText>; } &&
                SourceExCommandConstructorProgram<node::node::RenameArguments>::template preferredSupported<compat::Slice<compat::RetainedString>&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::DeleteNodeCommand> {
    template<class... Args> struct Program {
        using C = node::node::DeleteNodeCommand;
        using Targets = std::tuple<bool>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 1) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorDescription() } -> std::same_as<node::node::detail::NodeCommandDescription>; } &&
                SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported() && C::sourceFlagSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::CutNodeCommand> {
    template<class... Args> struct Program {
        using C = node::node::CutNodeCommand;
        using Targets = std::tuple<>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 0) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorText() } -> std::same_as<node::node::detail::NodeCommandText>; } &&
                SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::CopyNodeCommand> {
    template<class... Args> struct Program {
        using C = node::node::CopyNodeCommand;
        using Targets = std::tuple<>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 0) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorText() } -> std::same_as<node::node::detail::NodeCommandText>; } &&
                SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::PasteNodeCommand> {
    template<class... Args> struct Program {
        using C = node::node::PasteNodeCommand;
        using Targets = std::tuple<>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 0) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorText() } -> std::same_as<node::node::detail::NodeCommandText>; } &&
                SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::ReloadNodeCommand> {
    template<class... Args> struct Program {
        using C = node::node::ReloadNodeCommand;
        using Targets = std::tuple<>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 0) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorDescription() } -> std::same_as<node::node::detail::NodeCommandDescription>; } &&
                SourceExCommandConstructorProgram<CommandNoArguments>::descriptionSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::VertexModeCommand> {
    template<class... Args> struct Program {
        using C = node::node::VertexModeCommand;
        using Targets = std::tuple<>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 0) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorDescription() } -> std::same_as<node::node::detail::NodeCommandDescription>; } &&
                SourceExCommandConstructorProgram<CommandNoArguments>::descriptionSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::ToggleVisibilityCommand> {
    template<class... Args> struct Program {
        using C = node::node::ToggleVisibilityCommand;
        using Targets = std::tuple<>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 0) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorDescription() } -> std::same_as<node::node::detail::NodeCommandDescription>; } &&
                SourceExCommandConstructorProgram<CommandNoArguments>::descriptionSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::node::CentralizeNodeCommand> {
    template<class... Args> struct Program {
        using C = node::node::CentralizeNodeCommand;
        using Targets = std::tuple<>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 0) return false;
            else return node::node::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires { { C::sourceConstructorDescription() } -> std::same_as<node::node::detail::NodeCommandDescription>; } &&
                SourceExCommandConstructorProgram<CommandNoArguments>::descriptionSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return node::node::constructor_detail::construct<C, Targets>(allocator, std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands

namespace nijigenerate::commands::node::node {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct NullNames { static auto get() { return std::tuple{compat::RetainedString{}, compat::RetainedString{}}; } };
struct NullType { static auto get() { return std::tuple{compat::RetainedString{}}; } };
struct MoveDefaults { static auto get() { return std::tuple{nullptr, 0}; } };
template<auto Id, class C>
using Optional = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
template<auto Id, class C, class Args>
using Required = ::nijigenerate::commands::detail::ConstructedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Optional<NodeCommand::AddNode, AddNodeCommand>,
    Optional<NodeCommand::InsertNode, InsertNodeCommand>,
    Optional<NodeCommand::SetNodeName, SetNodeNameCommand>,
    Optional<NodeCommand::DeleteNode, DeleteNodeCommand>,
    Optional<NodeCommand::MoveNode, MoveNodeCommand>,
    Optional<NodeCommand::ConvertTo, ConvertToCommand>,
    Optional<NodeCommand::CutNode, CutNodeCommand>,
    Optional<NodeCommand::CopyNode, CopyNodeCommand>,
    Optional<NodeCommand::PasteNode, PasteNodeCommand>,
    Optional<NodeCommand::ReloadNode, ReloadNodeCommand>,
    Optional<NodeCommand::VertexMode, VertexModeCommand>,
    Optional<NodeCommand::ToggleVisibility, ToggleVisibilityCommand>,
    Optional<NodeCommand::CentralizeNode, CentralizeNodeCommand>,
    Required<NodeCommand::AddNode, AddNodeCommand, NullNames>,
    Required<NodeCommand::InsertNode, InsertNodeCommand, NullNames>,
    Required<NodeCommand::ConvertTo, ConvertToCommand, NullType>,
    Required<NodeCommand::MoveNode, MoveNodeCommand, MoveDefaults>>;
}
template<class T> requires (std::same_as<T, NodeCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitNodeCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<node::node::NodeCommand> {
    static consteval bool supported() {
        return node::node::initialization::Program::supported<node::node::initialization::Table>();
    }
    template<class ActualKey = node::node::NodeCommand>
        requires (std::same_as<ActualKey, node::node::NodeCommand> && supported())
    static void invoke() { node::node::ngInitCommands<ActualKey>(); }
};
}

namespace nicxlive::core::d_runtime {
#define NG_NODE_COMMAND_ORIGIN(Type, ClassName, Qualified, Unqualified) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::node::node::Type> { \
        inline static constexpr char className[] = ClassName; \
        inline static constexpr char templateArgumentName[] = "(" Qualified ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::node::node::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(" Qualified ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" Unqualified ")"; \
        inline static constexpr char qualifiedElement[] = Qualified; \
        inline static constexpr char unqualifiedElement[] = Unqualified; \
    }
NG_NODE_COMMAND_ORIGIN(MoveNodeCommand,
    "nijigenerate.commands.node.node.MoveNodeCommand",
    "nijigenerate.commands.node.node.MoveNodeCommand", "MoveNodeCommand");
NG_NODE_COMMAND_ORIGIN(SetNodeNameCommand,
    "nijigenerate.commands.node.node.SetNodeNameCommand",
    "nijigenerate.commands.node.node.SetNodeNameCommand", "SetNodeNameCommand");
NG_NODE_COMMAND_ORIGIN(DeleteNodeCommand,
    "nijigenerate.commands.node.node.DeleteNodeCommand",
    "nijigenerate.commands.node.node.DeleteNodeCommand", "DeleteNodeCommand");
NG_NODE_COMMAND_ORIGIN(CutNodeCommand,
    "nijigenerate.commands.node.node.CutNodeCommand",
    "nijigenerate.commands.node.node.CutNodeCommand", "CutNodeCommand");
NG_NODE_COMMAND_ORIGIN(CopyNodeCommand,
    "nijigenerate.commands.node.node.CopyNodeCommand",
    "nijigenerate.commands.node.node.CopyNodeCommand", "CopyNodeCommand");
NG_NODE_COMMAND_ORIGIN(PasteNodeCommand,
    "nijigenerate.commands.node.node.PasteNodeCommand",
    "nijigenerate.commands.node.node.PasteNodeCommand", "PasteNodeCommand");
NG_NODE_COMMAND_ORIGIN(ReloadNodeCommand,
    "nijigenerate.commands.node.node.ReloadNodeCommand",
    "nijigenerate.commands.node.node.ReloadNodeCommand", "ReloadNodeCommand");
NG_NODE_COMMAND_ORIGIN(VertexModeCommand,
    "nijigenerate.commands.node.node.VertexModeCommand",
    "nijigenerate.commands.node.node.VertexModeCommand", "VertexModeCommand");
NG_NODE_COMMAND_ORIGIN(ToggleVisibilityCommand,
    "nijigenerate.commands.node.node.ToggleVisibilityCommand",
    "nijigenerate.commands.node.node.ToggleVisibilityCommand", "ToggleVisibilityCommand");
NG_NODE_COMMAND_ORIGIN(CentralizeNodeCommand,
    "nijigenerate.commands.node.node.CentralizeNodeCommand",
    "nijigenerate.commands.node.node.CentralizeNodeCommand", "CentralizeNodeCommand");
NG_NODE_COMMAND_ORIGIN(AddNodeCommandT<true>,
    "nijigenerate.commands.node.node.AddNodeCommandT!true.AddNodeCommandT",
    "nijigenerate.commands.node.node.AddNodeCommandT!true", "AddNodeCommandT!true");
NG_NODE_COMMAND_ORIGIN(AddNodeCommandT<false>,
    "nijigenerate.commands.node.node.AddNodeCommandT!false.AddNodeCommandT",
    "nijigenerate.commands.node.node.AddNodeCommandT!false", "AddNodeCommandT!false");
NG_NODE_COMMAND_ORIGIN(InsertNodeCommandT<true>,
    "nijigenerate.commands.node.node.InsertNodeCommandT!true.InsertNodeCommandT",
    "nijigenerate.commands.node.node.InsertNodeCommandT!true", "InsertNodeCommandT!true");
NG_NODE_COMMAND_ORIGIN(InsertNodeCommandT<false>,
    "nijigenerate.commands.node.node.InsertNodeCommandT!false.InsertNodeCommandT",
    "nijigenerate.commands.node.node.InsertNodeCommandT!false", "InsertNodeCommandT!false");
NG_NODE_COMMAND_ORIGIN(ConvertToCommandT<true>,
    "nijigenerate.commands.node.node.ConvertToCommandT!true.ConvertToCommandT",
    "nijigenerate.commands.node.node.ConvertToCommandT!true", "ConvertToCommandT!true");
NG_NODE_COMMAND_ORIGIN(ConvertToCommandT<false>,
    "nijigenerate.commands.node.node.ConvertToCommandT!false.ConvertToCommandT",
    "nijigenerate.commands.node.node.ConvertToCommandT!false", "ConvertToCommandT!false");
#undef NG_NODE_COMMAND_ORIGIN
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::node::node {
inline void MoveNodeCommand::prepareSourceClassInfo() {
    ExCommand<MoveArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<MoveNodeCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<MoveNodeCommand>();
}
inline void SetNodeNameCommand::prepareSourceClassInfo() {
    ExCommand<RenameArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetNodeNameCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<SetNodeNameCommand>();
}
inline void DeleteNodeCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<DeleteNodeCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<DeleteNodeCommand>();
}
inline void CutNodeCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<CutNodeCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<CutNodeCommand>();
}
inline void CopyNodeCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<CopyNodeCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<CopyNodeCommand>();
}
inline void PasteNodeCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<PasteNodeCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<PasteNodeCommand>();
}
inline void ReloadNodeCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ReloadNodeCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<ReloadNodeCommand>();
}
inline void VertexModeCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<VertexModeCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<VertexModeCommand>();
}
inline void ToggleVisibilityCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ToggleVisibilityCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<ToggleVisibilityCommand>();
}
inline void CentralizeNodeCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<CentralizeNodeCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<CentralizeNodeCommand>();
}
template<bool expose> void AddNodeCommandT<expose>::prepareSourceClassInfo() {
    NativeBase::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AddNodeCommandT>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<AddNodeCommandT>();
}
template<bool expose> void InsertNodeCommandT<expose>::prepareSourceClassInfo() {
    NativeBase::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<InsertNodeCommandT>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<InsertNodeCommandT>();
}
template<bool expose> void ConvertToCommandT<expose>::prepareSourceClassInfo() {
    NativeBase::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<ConvertToCommandT>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<ConvertToCommandT>();
}
} // namespace nijigenerate::commands::node::node
