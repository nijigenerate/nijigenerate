#pragma once

#include "apply_node_actions.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/typed_format_blend_mode.hpp>
#include <array>
#include <cstddef>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nijigenerate::commands {

// This is the real Source .init producer used by both generated fields and
// DefApply's default constructor. Custom values need their defining provider.
template<class Value, class Enable = void> struct SourceInspectorCommandInitialValue;

template<class Value>
struct SourceInspectorCommandInitialValue<Value, std::enable_if_t<
        detail::CommandInitializationValue<Value>::value ||
        requires { panels::inspector::InspectorValueInitialization<Value>::get(); }>> {
    static auto get() {
        if constexpr (requires { panels::inspector::InspectorValueInitialization<Value>::get(); })
            return panels::inspector::InspectorValueInitialization<Value>::get();
        else if constexpr (panels::inspector::InspectorStaticArray<Value>::value) {
            using Array = panels::inspector::InspectorStaticArray<Value>;
            return []<std::size_t... Index>(std::index_sequence<Index...>) {
                return Value{(static_cast<void>(Index),
                    SourceInspectorCommandInitialValue<typename Array::Element>::get())...};
            }(std::make_index_sequence<Array::size>{});
        } else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value> ||
                std::is_pointer_v<Value> || std::same_as<Value, std::nullptr_t>) {
            // Preserve Source NaN/character sentinels and the actual first enum
            // member; native value-initialization does not supply these rules.
            return detail::CommandResultInitialValue<Value>::get();
        } else return panels::inspector::ngInspectorInitialValue<Value>();
    }
};

// The declared parameter is initialized only when the actual allocated class
// consumes this operation. Unknown value families supply their own operation.
template<class Value, class From, class Enable = void> struct SourceInspectorCommandParameter;

template<class Value, class From>
struct SourceInspectorCommandParameter<Value, From,
        std::enable_if_t<detail::CommandInitializationValue<Value>::value>> {
    static consteval bool supported() {
        if constexpr (!ngCommandInitializationSupported<Value, From>()) return false;
        else if constexpr (std::same_as<Value, std::remove_cvref_t<From>>)
            return ngCommandParameterCopySupported<Value, From>();
        else return true;
    }
    static auto get(From value) requires(supported()) {
        if constexpr (std::same_as<Value, std::remove_cvref_t<From>>)
            return SourceCommandParameterCopy<Value, From>::copy(std::forward<From>(value));
        else {
            auto result = SourceInspectorCommandInitialValue<Value>::get();
            SourceCommandInitialization<Value, From>::construct(result, std::forward<From>(value));
            return result;
        }
    }
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::inspector::apply_node {

// This is the actual generated TW value field used by ExCommand; there is no
// second tuple of argument values or wrapper-side copy of the field.
template<class Access, CommandLiteral DTypeName> struct InspectorApplyFields {
    typename Access::Value value = SourceInspectorCommandInitialValue<typename Access::Value>::get();
};
struct InspectorToggleFields {};
namespace detail {
struct InspectorToggleText {
    compat::RetainedString label;
    compat::RetainedString description;
};
}

} // namespace nijigenerate::commands::inspector::apply_node

namespace nijigenerate::commands {

template<class Access, CommandLiteral DTypeName>
struct CommandFields<inspector::apply_node::InspectorApplyFields<Access, DTypeName>> {
    using Storage = inspector::apply_node::InspectorApplyFields<Access, DTypeName>;
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&Storage::value, "value", "Value to apply", DTypeName>{}};
};
template<> struct CommandFields<inspector::apply_node::InspectorToggleFields> {
    static constexpr auto descriptors = std::tuple{};
};

} // namespace nijigenerate::commands



namespace nijigenerate::commands::inspector::apply_node {

template<class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
class ApplyInspectorPropCommand : public ExCommand<InspectorApplyFields<Access, DTypeName>>,
    public std::enable_shared_from_this<ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>> {
    static_assert(std::derived_from<I, panels::inspector::Inspector>);
public:
    using ValT = typename Access::Value;
private:
    struct SourceConstructorFrame {
        compat::RetainedString description;
        ValT& value;
        static SourceConstructorFrame capture(ValT& value);
    };
    explicit ApplyInspectorPropCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ApplyInspectorPropCommand>;
public:
    ApplyInspectorPropCommand(const ApplyInspectorPropCommand&) = delete;
    ApplyInspectorPropCommand& operator=(const ApplyInspectorPropCommand&) = delete;
    ApplyInspectorPropCommand(ApplyInspectorPropCommand&&) = delete;
    ApplyInspectorPropCommand& operator=(ApplyInspectorPropCommand&&) = delete;
    // Generic constructor/run bodies are included after metadata specializations.
    explicit ApplyInspectorPropCommand(typename Access::Value value);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
protected:
    void prepareSourceClassInfo() override;
};

template<class I, class Access, CommandLiteral TargetName>
class ToggleInspectorPropCommand : public ExCommand<InspectorToggleFields>,
    public std::enable_shared_from_this<ToggleInspectorPropCommand<I, Access, TargetName>> {
    static_assert(std::derived_from<I, panels::inspector::Inspector>);
    static_assert(std::same_as<typename Access::Value, bool>,
        "Toggle command only works for boolean properties");
    explicit ToggleInspectorPropCommand(detail::InspectorToggleText text);
    static detail::InspectorToggleText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ToggleInspectorPropCommand>;
public:
    ToggleInspectorPropCommand(const ToggleInspectorPropCommand&) = delete;
    ToggleInspectorPropCommand& operator=(const ToggleInspectorPropCommand&) = delete;
    ToggleInspectorPropCommand(ToggleInspectorPropCommand&&) = delete;
    ToggleInspectorPropCommand& operator=(ToggleInspectorPropCommand&&) = delete;
    ToggleInspectorPropCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
protected:
    void prepareSourceClassInfo() override;
};

} // namespace nijigenerate::commands::inspector::apply_node

namespace nijigenerate::commands {

template<class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
struct CommandTraits<inspector::apply_node::ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<inspector::apply_node::InspectorApplyFields<Access, DTypeName>>>;
};
template<class I, class Access, CommandLiteral TargetName>
struct CommandTraits<inspector::apply_node::ToggleInspectorPropCommand<I, Access, TargetName>> {
    // The source's Toggle class has no Apply class attributes.
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<ExCommand<inspector::apply_node::InspectorToggleFields>>;
};

} // namespace nijigenerate::commands

#include "apply_node_command_bodies.hpp"

namespace nijigenerate::commands::inspector::apply_node {

// Typed production emission of the original DefApply mixin. Id keeps commands
// for distinct source names distinct even when they address the same property.
template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
class DefinedInspectorApplyCommand : public ApplyInspectorPropCommand<I, Access, TargetName, DTypeName> {
    using Base = ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>;
    static auto sourceInitialValue() {
        return SourceInspectorCommandInitialValue<typename Access::Value>::get();
    }
    friend struct ::nijigenerate::commands::SourceCommandConstructor<DefinedInspectorApplyCommand>;
public:
    DefinedInspectorApplyCommand() : Base(sourceInitialValue()) {}
protected:
    void prepareSourceClassInfo() override;
};

template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName>
class DefinedInspectorToggleCommand : public ToggleInspectorPropCommand<I, Access, TargetName> {
    using Base = ToggleInspectorPropCommand<I, Access, TargetName>;
public:
    DefinedInspectorToggleCommand() : Base() {}
protected:
    void prepareSourceClassInfo() override;
};

namespace detail {
template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName,
    bool Boolean = std::same_as<typename Access::Value, bool>>
struct DefApplyToggle {};

template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName>
struct DefApplyToggle<Id, I, Access, TargetName, true> {
    using Toggle = DefinedInspectorToggleCommand<Id, I, Access, TargetName>;
};
}

template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
struct DefApply : detail::DefApplyToggle<Id, I, Access, TargetName> {
    using ValT = typename Access::Value;
    using Apply = DefinedInspectorApplyCommand<Id, I, Access, TargetName, DTypeName>;
};

} // namespace nijigenerate::commands::inspector::apply_node

namespace nijigenerate::commands {
template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
struct CommandTraits<inspector::apply_node::DefinedInspectorApplyCommand<Id, I, Access, TargetName, DTypeName>> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<
        inspector::apply_node::ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>>;
};

template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName>
struct CommandTraits<inspector::apply_node::DefinedInspectorToggleCommand<Id, I, Access, TargetName>> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<inspector::apply_node::ToggleInspectorPropCommand<I, Access, TargetName>>;
};
} // namespace nijigenerate::commands

namespace nijigenerate::commands::inspector::apply_node::detail {


template<class I, class Access> compat::RetainedString inspectorPropertySourceClassName(
    std::string_view declaration) {
    const auto arguments = inspectorPropertySourceArguments<I, Access>(true);
    return ::nijigenerate::commands::detail::composeCommandTemplateName(
        "nijigenerate.commands.inspector.apply_node", declaration, arguments, true);
}

template<class I, class Access>
nicxlive::core::d_runtime::SourceTypeSpellingNames inspectorPropertySourceSpellings(std::string_view declaration) {
    const auto arguments = inspectorPropertySourceArguments<I, Access>(false);
    const auto qualified = ::nijigenerate::commands::detail::composeCommandTemplateName(
        "nijigenerate.commands.inspector.apply_node", declaration, arguments, false);
    const auto unqualified = ::nijigenerate::commands::detail::composeCommandTemplateName(
        {}, declaration, arguments, false);
    return {::nijigenerate::commands::detail::commandTypeArgumentGroup(qualified),
        ::nijigenerate::commands::detail::commandTypeArgumentGroup(unqualified), qualified, unqualified};
}

// DefApply emits these plain class identifiers into the module's lexical scope.
// Each Source printer context has its own compile-time descriptor.
template<CommandLiteral Id, bool Toggle, bool Qualified, bool Argument>
consteval auto definedInspectorTypeSpelling() {
    constexpr std::string_view module = Qualified ? "nijigenerate.commands.inspector.apply_node." : "";
    constexpr std::string_view prefix = Toggle ? "Toggle" : "";
    constexpr std::string_view suffix = "Command";
    constexpr auto idLength = sizeof(Id.value) - 1;
    std::array<char, module.size() + prefix.size() + idLength + suffix.size() + (Argument ? 2 : 0) + 1> text{};
    std::size_t offset = 0;
    if constexpr (Argument) text[offset++] = '(';
    for (const auto byte : module) text[offset++] = byte;
    for (const auto byte : prefix) text[offset++] = byte;
    for (std::size_t i = 0; i < idLength; ++i) text[offset++] = Id.value[i];
    for (const auto byte : suffix) text[offset++] = byte;
    if constexpr (Argument) text[offset++] = ')';
    return text;
}

template<CommandLiteral Id, bool Toggle> struct DefinedInspectorSourceNames {
    inline static constexpr auto qualifiedArgument = definedInspectorTypeSpelling<Id, Toggle, true, true>();
    inline static constexpr auto unqualifiedArgument = definedInspectorTypeSpelling<Id, Toggle, false, true>();
    inline static constexpr auto qualifiedElement = definedInspectorTypeSpelling<Id, Toggle, true, false>();
    inline static constexpr auto unqualifiedElement = definedInspectorTypeSpelling<Id, Toggle, false, false>();

    template<std::size_t N> static compat::RetainedString descriptor(const std::array<char, N>& bytes) {
        return compat::RetainedString::fromStaticView({bytes.data(), N - 1});
    }
    static compat::RetainedString className() { return descriptor(qualifiedElement); }
    static compat::RetainedString templateArgumentName() { return descriptor(qualifiedArgument); }
    static nicxlive::core::d_runtime::SourceTypeSpellingNames get() {
        return {descriptor(qualifiedArgument), descriptor(unqualifiedArgument),
            descriptor(qualifiedElement), descriptor(unqualifiedElement)};
    }
};

} // namespace nijigenerate::commands::inspector::apply_node::detail

namespace nicxlive::core::d_runtime {

template<class I, class Access, ::nijigenerate::commands::CommandLiteral TargetName,
    ::nijigenerate::commands::CommandLiteral DTypeName>
struct SourceObjectTypeNames<::nijigenerate::commands::inspector::apply_node::
        ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>> {
    static RetainedString className() {
        return ::nijigenerate::commands::inspector::apply_node::detail::
            inspectorPropertySourceClassName<I, Access>("ApplyInspectorPropCommand");
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::commands::inspector::apply_node::detail::
            inspectorPropertySourceSpellings<I, Access>("ApplyInspectorPropCommand").qualifiedArgument;
    }
};

template<class I, class Access, ::nijigenerate::commands::CommandLiteral TargetName,
    ::nijigenerate::commands::CommandLiteral DTypeName>
struct SourceTypeSpellings<::nijigenerate::commands::inspector::apply_node::
        ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::commands::inspector::apply_node::detail::
            inspectorPropertySourceSpellings<I, Access>("ApplyInspectorPropCommand");
    }
};

template<class I, class Access, ::nijigenerate::commands::CommandLiteral TargetName>
struct SourceObjectTypeNames<::nijigenerate::commands::inspector::apply_node::
        ToggleInspectorPropCommand<I, Access, TargetName>> {
    static RetainedString className() {
        return ::nijigenerate::commands::inspector::apply_node::detail::
            inspectorPropertySourceClassName<I, Access>("ToggleInspectorPropCommand");
    }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::commands::inspector::apply_node::detail::
            inspectorPropertySourceSpellings<I, Access>("ToggleInspectorPropCommand").qualifiedArgument;
    }
};

template<class I, class Access, ::nijigenerate::commands::CommandLiteral TargetName>
struct SourceTypeSpellings<::nijigenerate::commands::inspector::apply_node::
        ToggleInspectorPropCommand<I, Access, TargetName>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ::nijigenerate::commands::inspector::apply_node::detail::
            inspectorPropertySourceSpellings<I, Access>("ToggleInspectorPropCommand");
    }
};

template<::nijigenerate::commands::CommandLiteral Id, class I, class Access,
    ::nijigenerate::commands::CommandLiteral TargetName, ::nijigenerate::commands::CommandLiteral DTypeName>
struct SourceObjectTypeNames<::nijigenerate::commands::inspector::apply_node::
        DefinedInspectorApplyCommand<Id, I, Access, TargetName, DTypeName>>
    : ::nijigenerate::commands::inspector::apply_node::detail::DefinedInspectorSourceNames<Id, false> {};

template<::nijigenerate::commands::CommandLiteral Id, class I, class Access,
    ::nijigenerate::commands::CommandLiteral TargetName, ::nijigenerate::commands::CommandLiteral DTypeName>
struct SourceTypeSpellings<::nijigenerate::commands::inspector::apply_node::
        DefinedInspectorApplyCommand<Id, I, Access, TargetName, DTypeName>>
    : ::nijigenerate::commands::inspector::apply_node::detail::DefinedInspectorSourceNames<Id, false> {
    inline static constexpr bool isNativeObject = true;
};

template<::nijigenerate::commands::CommandLiteral Id, class I, class Access,
    ::nijigenerate::commands::CommandLiteral TargetName>
struct SourceObjectTypeNames<::nijigenerate::commands::inspector::apply_node::
        DefinedInspectorToggleCommand<Id, I, Access, TargetName>>
    : ::nijigenerate::commands::inspector::apply_node::detail::DefinedInspectorSourceNames<Id, true> {};

template<::nijigenerate::commands::CommandLiteral Id, class I, class Access,
    ::nijigenerate::commands::CommandLiteral TargetName>
struct SourceTypeSpellings<::nijigenerate::commands::inspector::apply_node::
        DefinedInspectorToggleCommand<Id, I, Access, TargetName>>
    : ::nijigenerate::commands::inspector::apply_node::detail::DefinedInspectorSourceNames<Id, true> {
    inline static constexpr bool isNativeObject = true;
};

} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::inspector::apply_node {

template<class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
void ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>::prepareSourceClassInfo() {
    ExCommand<InspectorApplyFields<Access, DTypeName>>::prepareSourceClassInfo();
    using Self = ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>;
    static const bool prepared = [] {
        ::nijigenerate::commands::detail::prepareCommandResultClass<Self>();
        detail::prepareInspectorTypeSpellings<Self>();
        return true;
    }();
    (void)prepared;
}

template<class I, class Access, CommandLiteral TargetName>
void ToggleInspectorPropCommand<I, Access, TargetName>::prepareSourceClassInfo() {
    ExCommand<InspectorToggleFields>::prepareSourceClassInfo();
    using Self = ToggleInspectorPropCommand<I, Access, TargetName>;
    static const bool prepared = [] {
        ::nijigenerate::commands::detail::prepareCommandResultClass<Self>();
        detail::prepareInspectorTypeSpellings<Self>();
        return true;
    }();
    (void)prepared;
}

template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
void DefinedInspectorApplyCommand<Id, I, Access, TargetName, DTypeName>::prepareSourceClassInfo() {
    Base::prepareSourceClassInfo();
    using Self = DefinedInspectorApplyCommand<Id, I, Access, TargetName, DTypeName>;
    static const bool prepared = [] {
        ::nijigenerate::commands::detail::prepareCommandResultClass<Self>();
        detail::prepareInspectorTypeSpellings<Self>();
        return true;
    }();
    (void)prepared;
}

template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName>
void DefinedInspectorToggleCommand<Id, I, Access, TargetName>::prepareSourceClassInfo() {
    ToggleInspectorPropCommand<I, Access, TargetName>::prepareSourceClassInfo();
    using Self = DefinedInspectorToggleCommand<Id, I, Access, TargetName>;
    static const bool prepared = [] {
        ::nijigenerate::commands::detail::prepareCommandResultClass<Self>();
        detail::prepareInspectorTypeSpellings<Self>();
        return true;
    }();
    (void)prepared;
}

} // namespace nijigenerate::commands::inspector::apply_node

namespace nijigenerate::commands::inspector::apply_node::detail {

// The adapter contains only a reference to the caller's actual argument.
// allocate_shared invokes its conversion while constructing the allocated C.
template<class Value, class From> struct InspectorConstructorArgument {
    From value;
    operator Value() && {
        return ::nijigenerate::commands::SourceInspectorCommandParameter<Value, From>::get(
            std::forward<From>(value));
    }
};

} // namespace nijigenerate::commands::inspector::apply_node::detail

namespace nijigenerate::commands {

template<class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
struct SourceCommandConstructor<inspector::apply_node::ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>> {
    using C = inspector::apply_node::ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>;
    using Value = typename Access::Value;
    using Storage = inspector::apply_node::InspectorApplyFields<Access, DTypeName>;

    static consteval bool bodySupported() {
        static_assert(requires { SourceInspectorCommandInitialValue<Value>::get(); },
            "Missing defining Source inspector value initialization operation");
        return requires(Value& value) {
            { C::SourceConstructorFrame::capture(value) } -> std::same_as<typename C::SourceConstructorFrame>;
            { SourceInspectorCommandInitialValue<Value>::get() } -> std::same_as<Value>;
        } && SourceExCommandConstructorProgram<Storage>::template preferredSupported<Value&>();
    }

    template<class... Args> struct Program {
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else {
                using From = std::tuple_element_t<0, std::tuple<Args&&...>>;
                using Parameter = SourceInspectorCommandParameter<Value, From>;
                static_assert(requires { sizeof(Parameter); },
                    "Missing defining Source inspector constructor parameter operation");
                if constexpr (!Parameter::supported()) return false;
                else return requires {
                    { Parameter::get(std::declval<From>()) } -> std::same_as<Value>;
                } && bodySupported();
            }
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
                Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator,
                inspector::apply_node::detail::InspectorConstructorArgument<Value, Args&&>{
                    std::forward<Args>(args)}...);
        }
    };
};

template<class I, class Access, CommandLiteral TargetName>
struct SourceCommandConstructor<inspector::apply_node::ToggleInspectorPropCommand<I, Access, TargetName>> {
    using C = inspector::apply_node::ToggleInspectorPropCommand<I, Access, TargetName>;
    template<class... Args> struct Program {
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 0) return false;
            else return requires {
                { C::sourceConstructorText() } -> std::same_as<inspector::apply_node::detail::InspectorToggleText>;
            } && SourceExCommandConstructorProgram<inspector::apply_node::InspectorToggleFields>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
                Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName, CommandLiteral DTypeName>
struct SourceCommandConstructor<
        inspector::apply_node::DefinedInspectorApplyCommand<Id, I, Access, TargetName, DTypeName>> {
    using C = inspector::apply_node::DefinedInspectorApplyCommand<Id, I, Access, TargetName, DTypeName>;
    using Base = inspector::apply_node::ApplyInspectorPropCommand<I, Access, TargetName, DTypeName>;
    template<class... Args> struct Program {
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 0) return false;
            else return requires {
                { C::sourceInitialValue() } -> std::same_as<typename Access::Value>;
            } && SourceCommandConstructor<Base>::bodySupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
                Args&&... args) requires(supported()) {
            // The same production C() evaluates the actual ValT.init after
            // allocation, then runs the actual generic Apply constructor.
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<CommandLiteral Id, class I, class Access, CommandLiteral TargetName>
struct SourceCommandConstructor<inspector::apply_node::DefinedInspectorToggleCommand<Id, I, Access, TargetName>> {
    using C = inspector::apply_node::DefinedInspectorToggleCommand<Id, I, Access, TargetName>;
    using Base = inspector::apply_node::ToggleInspectorPropCommand<I, Access, TargetName>;
    template<class... Args> struct Program {
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 0) return false;
            else return SourceCommandConstructor<Base>::template Program<>::supported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
                Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

} // namespace nijigenerate::commands

