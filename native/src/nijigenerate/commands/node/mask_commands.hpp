#pragma once
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nodes/part.hpp>
#include "mask.hpp"
#include <compat/object_metadata.hpp>
#include <concepts>
#include <memory>
#include <utility>

namespace nicxlive::core::d_runtime {
#define NG_MASK_SOURCE_NAMES(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::commands::node::mask::Type> { \
    inline static constexpr bool isNativeObject = true; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.node.mask." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.node.mask." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
}; \
template<> struct SourceObjectTypeNames<::nijigenerate::commands::node::mask::Type> { \
    inline static constexpr char className[] = "nijigenerate.commands.node.mask." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.node.mask." #Type ")"; \
};
NG_MASK_SOURCE_NAMES(AddMaskCommand)
NG_MASK_SOURCE_NAMES(RemoveMaskCommand)
NG_MASK_SOURCE_NAMES(ChangeMaskModeCommand)
#undef NG_MASK_SOURCE_NAMES
}

namespace nijigenerate::commands::node::mask {

namespace detail {
struct MaskCommandText {
    compat::RetainedString label;
    compat::RetainedString description;
};
}

struct AddMaskArguments {
    std::shared_ptr<nicxlive::core::nodes::Drawable> maskSrc;
    nicxlive::core::nodes::MaskingMode mode = nicxlive::core::nodes::MaskingMode::Mask;
};
struct RemoveMaskArguments {
    std::shared_ptr<nicxlive::core::nodes::Drawable> maskSrc;
};
struct ChangeMaskModeArguments {
    std::shared_ptr<nicxlive::core::nodes::Drawable> maskSrc;
    nicxlive::core::nodes::MaskingMode mode = nicxlive::core::nodes::MaskingMode::Mask;
};

} // namespace nijigenerate::commands::node::mask

namespace nijigenerate::commands {

template<> struct CommandFields<node::mask::AddMaskArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::mask::AddMaskArguments::maskSrc, "maskSrc",
            "Mask source drawable", "Drawable">{},
        FieldDescriptor<&node::mask::AddMaskArguments::mode, "mode", "Masking mode", "MaskingMode">{}};
};
template<> struct CommandFields<node::mask::RemoveMaskArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::mask::RemoveMaskArguments::maskSrc, "maskSrc",
            "Mask source drawable", "Drawable">{}};
};
template<> struct CommandFields<node::mask::ChangeMaskModeArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::mask::ChangeMaskModeArguments::maskSrc, "maskSrc",
            "Mask source drawable", "Drawable">{},
        FieldDescriptor<&node::mask::ChangeMaskModeArguments::mode, "mode",
            "New masking mode", "MaskingMode">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::node::mask {

class AddMaskCommand : public ExCommand<AddMaskArguments>, public std::enable_shared_from_this<AddMaskCommand> {
    static detail::MaskCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AddMaskCommand>;
    AddMaskCommand(detail::MaskCommandText text, std::shared_ptr<nicxlive::core::nodes::Drawable> source,
        nicxlive::core::nodes::MaskingMode mode);
protected:
    void prepareSourceClassInfo() override;
public:
    AddMaskCommand(std::shared_ptr<nicxlive::core::nodes::Drawable> maskSrc,
        nicxlive::core::nodes::MaskingMode mode);
    AddMaskCommand(const AddMaskCommand&) = delete;
    AddMaskCommand& operator=(const AddMaskCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class RemoveMaskCommand : public ExCommand<RemoveMaskArguments>, public std::enable_shared_from_this<RemoveMaskCommand> {
    static detail::MaskCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<RemoveMaskCommand>;
    RemoveMaskCommand(detail::MaskCommandText text, std::shared_ptr<nicxlive::core::nodes::Drawable> source);
protected:
    void prepareSourceClassInfo() override;
public:
    explicit RemoveMaskCommand(std::shared_ptr<nicxlive::core::nodes::Drawable> maskSrc);
    RemoveMaskCommand(const RemoveMaskCommand&) = delete;
    RemoveMaskCommand& operator=(const RemoveMaskCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ChangeMaskModeCommand : public ExCommand<ChangeMaskModeArguments>,
    public std::enable_shared_from_this<ChangeMaskModeCommand> {
    static detail::MaskCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ChangeMaskModeCommand>;
    ChangeMaskModeCommand(detail::MaskCommandText text, std::shared_ptr<nicxlive::core::nodes::Drawable> source,
        nicxlive::core::nodes::MaskingMode mode);
protected:
    void prepareSourceClassInfo() override;
public:
    ChangeMaskModeCommand(std::shared_ptr<nicxlive::core::nodes::Drawable> maskSrc,
        nicxlive::core::nodes::MaskingMode mode);
    ChangeMaskModeCommand(const ChangeMaskModeCommand&) = delete;
    ChangeMaskModeCommand& operator=(const ChangeMaskModeCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

template<NodeMaskCommand id, class... Args>
std::shared_ptr<CommandResult> ngRunNodeMaskCommand(std::shared_ptr<Context> context, Args&&... args) {
    static_assert(id == NodeMaskCommand::AddMask || id == NodeMaskCommand::RemoveMask ||
                  id == NodeMaskCommand::ChangeMaskMode);
    const auto found = commands.find(id);
    if (found == commands.end()) throw std::runtime_error("No registered mask command");
    const auto base = found->second;
    if (!ngCommandAllowedInCurrentContext(base))
        return CommandResult::opCall(false, "Command is not available in the current command scope");
    using C = std::conditional_t<id == NodeMaskCommand::AddMask, AddMaskCommand,
        std::conditional_t<id == NodeMaskCommand::RemoveMask, RemoveMaskCommand, ChangeMaskModeCommand>>;
    using A = std::conditional_t<id == NodeMaskCommand::AddMask, AddMaskArguments,
        std::conditional_t<id == NodeMaskCommand::RemoveMask, RemoveMaskArguments, ChangeMaskModeArguments>>;
    const auto command = std::dynamic_pointer_cast<C>(base);
    if (!command) throw std::runtime_error("Registered mask command instance type mismatch");
    ngAssignCommandFields(static_cast<A&>(*command), std::forward<Args>(args)...);
    return ngRunCommand(command, context);
}

} // namespace nijigenerate::commands::node::mask

namespace nijigenerate::commands {

template<> struct CommandTraits<node::mask::AddMaskCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::mask::AddMaskArguments>, Command>;
};
template<> struct CommandTraits<node::mask::RemoveMaskCommand> {
    static constexpr auto attributes = std::tuple{EffectDelete};
    using BaseClasses = CommandTypeList<ExCommand<node::mask::RemoveMaskArguments>, Command>;
};
template<> struct CommandTraits<node::mask::ChangeMaskModeCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::mask::ChangeMaskModeArguments>, Command>;
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands {

template<> struct SourceCommandConstructor<node::mask::AddMaskCommand> {
    template<class... Args> struct Program {
        using C = node::mask::AddMaskCommand;
        using Target = std::shared_ptr<nicxlive::core::nodes::Drawable>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 2) return false;
            else return []<class T, class W>(CommandTypeList<T, W>) {
                return detail::declaredSourceParametersSupported<std::tuple<Target, nicxlive::core::nodes::MaskingMode>, T, W>() &&
                    requires {
                        { C::sourceConstructorText() } -> std::same_as<node::mask::detail::MaskCommandText>;
                    } && SourceExCommandConstructorProgram<node::mask::AddMaskArguments>::
                        preferredSupported<Target&, nicxlive::core::nodes::MaskingMode&>();
            }(CommandTypeList<Args...>{});
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return [&]<class T, class W>(T&& target, W&& mode) {
                return detail::constructDeclaredSourceCommand<C, std::tuple<Target, nicxlive::core::nodes::MaskingMode>>(allocator,
                    std::forward<T>(target), std::forward<W>(mode));
            }(std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::mask::ChangeMaskModeCommand> {
    template<class... Args> struct Program {
        using C = node::mask::ChangeMaskModeCommand;
        using Target = std::shared_ptr<nicxlive::core::nodes::Drawable>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 2) return false;
            else return []<class T, class W>(CommandTypeList<T, W>) {
                return detail::declaredSourceParametersSupported<std::tuple<Target, nicxlive::core::nodes::MaskingMode>, T, W>() &&
                    requires {
                        { C::sourceConstructorText() } -> std::same_as<node::mask::detail::MaskCommandText>;
                    } && SourceExCommandConstructorProgram<node::mask::ChangeMaskModeArguments>::
                        preferredSupported<Target&, nicxlive::core::nodes::MaskingMode&>();
            }(CommandTypeList<Args...>{});
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return [&]<class T, class W>(T&& target, W&& mode) {
                return detail::constructDeclaredSourceCommand<C, std::tuple<Target, nicxlive::core::nodes::MaskingMode>>(allocator,
                    std::forward<T>(target), std::forward<W>(mode));
            }(std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::mask::RemoveMaskCommand> {
    template<class... Args> struct Program {
        using C = node::mask::RemoveMaskCommand;
        using Target = std::shared_ptr<nicxlive::core::nodes::Drawable>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else return detail::declaredSourceParametersSupported<std::tuple<Target>, Args...>() &&
                requires {
                    { C::sourceConstructorText() } -> std::same_as<node::mask::detail::MaskCommandText>;
                } && SourceExCommandConstructorProgram<node::mask::RemoveMaskArguments>::
                    preferredSupported<Target&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<Target>>(allocator,
                std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands


namespace nijigenerate::commands::node::mask {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct NullMask {
    static auto get() { return std::tuple{nullptr, nicxlive::core::nodes::MaskingMode::Mask}; }
};
struct NullSource { static auto get() { return std::tuple{nullptr}; } };
template<auto Id, class C, class Args>
using Required = ::nijigenerate::commands::detail::ConstructedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Required<NodeMaskCommand::AddMask, AddMaskCommand, NullMask>,
    Required<NodeMaskCommand::RemoveMask, RemoveMaskCommand, NullSource>,
    Required<NodeMaskCommand::ChangeMaskMode, ChangeMaskModeCommand, NullMask>>;
}
template<class T> requires (std::same_as<T, NodeMaskCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitNodeMaskCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<node::mask::NodeMaskCommand> {
    static consteval bool supported() {
        return node::mask::initialization::Program::supported<node::mask::initialization::Table>();
    }
    template<class ActualKey = node::mask::NodeMaskCommand>
        requires (std::same_as<ActualKey, node::mask::NodeMaskCommand> && supported())
    static void invoke() { node::mask::ngInitCommands<ActualKey>(); }
};
}
