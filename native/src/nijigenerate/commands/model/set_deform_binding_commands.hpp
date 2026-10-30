#pragma once
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include "set_deform_binding.hpp"
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <limits>
#include <memory>
#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands::model::set_deform_binding {

struct SetDeformBindingArguments {
    compat::RetainedStringSlice bindingName;
    compat::Slice<float> values;
};
struct SetTRSBindingArguments {
    compat::Slice<float> translation;
    compat::Slice<float> scale;
    float rotationDegrees = std::numeric_limits<float>::quiet_NaN();
    bool applyRotation = false;
};
struct SetRotationXBindingArguments {
    float rotationRadians = std::numeric_limits<float>::quiet_NaN();
};
struct SetRotationYBindingArguments {
    float rotationRadians = std::numeric_limits<float>::quiet_NaN();
};

} // namespace nijigenerate::commands::model::set_deform_binding

namespace nijigenerate::commands {

template<> struct CommandFields<model::set_deform_binding::SetDeformBindingArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&model::set_deform_binding::SetDeformBindingArguments::bindingName,
            "bindingName", "Binding name. Must be 'deform'.", "string">{},
        FieldDescriptor<&model::set_deform_binding::SetDeformBindingArguments::values,
            "values", "Flattened deformation offsets: [dx0,dy0, dx1,dy1, ...]", "float[]">{}};
};
template<> struct CommandFields<model::set_deform_binding::SetTRSBindingArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&model::set_deform_binding::SetTRSBindingArguments::translation,
            "translation", "Node transform translation binding [x,y]. Omit to leave unchanged.", "float[]">{},
        FieldDescriptor<&model::set_deform_binding::SetTRSBindingArguments::scale,
            "scale", "Node transform scale binding [x,y]. Omit to leave unchanged.", "float[]">{},
        FieldDescriptor<&model::set_deform_binding::SetTRSBindingArguments::rotationDegrees,
            "rotationDegrees", "Node transform Z rotation binding in degrees. Set applyRotation=true to write zero.",
            "float">{},
        FieldDescriptor<&model::set_deform_binding::SetTRSBindingArguments::applyRotation,
            "applyRotation", "Whether to write rotationDegrees even when it is zero.", "bool">{}};
};
template<> struct CommandFields<model::set_deform_binding::SetRotationXBindingArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&model::set_deform_binding::SetRotationXBindingArguments::rotationRadians,
            "rotationRadians", "Node transform X rotation binding in radians.", "float">{}};
};
template<> struct CommandFields<model::set_deform_binding::SetRotationYBindingArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&model::set_deform_binding::SetRotationYBindingArguments::rotationRadians,
            "rotationRadians", "Node transform Y rotation binding in radians.", "float">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::model::set_deform_binding {

namespace detail {
struct CommandLabels {
    compat::RetainedString label;
    compat::RetainedString description;
};
}

// Child bodies can retain the actual command allocation across reentrant calls.
class SetDeformBindingCommand : public ExCommand<SetDeformBindingArguments>,
    public std::enable_shared_from_this<SetDeformBindingCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    bool createZeroBinding = false;
    bool notifyDepthBone = false;
    static detail::CommandLabels sourceConstructorLabels();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetDeformBindingCommand>;
    struct SourceFlagProgram {
        static consteval bool supported() { return ngCommandInitializationSupported<bool, bool&>(); }
        static void initialize(SetDeformBindingCommand& target, bool& createZero, bool& notify) {
            SourceCommandInitialization<bool, bool&>::construct(target.createZeroBinding, createZero);
            SourceCommandInitialization<bool, bool&>::construct(target.notifyDepthBone, notify);
        }
    };
    SetDeformBindingCommand(detail::CommandLabels labels, compat::RetainedStringSlice bname,
        compat::Slice<float> vals, bool createZeroBinding, bool notifyDepthBone);
public:
    SetDeformBindingCommand(compat::RetainedStringSlice bname, compat::Slice<float> vals,
        bool createZeroBinding = false, bool notifyDepthBone = true);
    SetDeformBindingCommand(const SetDeformBindingCommand&) = delete;
    SetDeformBindingCommand& operator=(const SetDeformBindingCommand&) = delete;
    bool runnable(std::shared_ptr<Context> context) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetTRSBindingCommand : public ExCommand<SetTRSBindingArguments>,
    public std::enable_shared_from_this<SetTRSBindingCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    SetTRSBindingCommand(detail::CommandLabels labels, compat::Slice<float> translation,
        compat::Slice<float> scale, float rotationDegrees, bool applyRotation);
    static detail::CommandLabels sourceConstructorLabels();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetTRSBindingCommand>;
public:
    SetTRSBindingCommand(compat::Slice<float> translation = {}, compat::Slice<float> scale = {},
        float rotationDegrees = 0.0f, bool applyRotation = false);
    SetTRSBindingCommand(const SetTRSBindingCommand&) = delete;
    SetTRSBindingCommand& operator=(const SetTRSBindingCommand&) = delete;
    bool runnable(std::shared_ptr<Context> context) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetRotationXBindingCommand : public ExCommand<SetRotationXBindingArguments>,
    public std::enable_shared_from_this<SetRotationXBindingCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    SetRotationXBindingCommand(detail::CommandLabels labels, float rotationRadians);
    static detail::CommandLabels sourceConstructorLabels();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetRotationXBindingCommand>;
public:
    explicit SetRotationXBindingCommand(float rotationRadians = 0.0f);
    SetRotationXBindingCommand(const SetRotationXBindingCommand&) = delete;
    SetRotationXBindingCommand& operator=(const SetRotationXBindingCommand&) = delete;
    bool runnable(std::shared_ptr<Context> context) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetRotationYBindingCommand : public ExCommand<SetRotationYBindingArguments>,
    public std::enable_shared_from_this<SetRotationYBindingCommand> {
protected:
    void prepareSourceClassInfo() override;
private:
    SetRotationYBindingCommand(detail::CommandLabels labels, float rotationRadians);
    static detail::CommandLabels sourceConstructorLabels();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetRotationYBindingCommand>;
public:
    explicit SetRotationYBindingCommand(float rotationRadians = 0.0f);
    SetRotationYBindingCommand(const SetRotationYBindingCommand&) = delete;
    SetRotationYBindingCommand& operator=(const SetRotationYBindingCommand&) = delete;
    bool runnable(std::shared_ptr<Context> context) override;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

} // namespace nijigenerate::commands::model::set_deform_binding

namespace nijigenerate::commands {

template<> struct CommandTraits<model::set_deform_binding::SetDeformBindingCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectBindingEdit};
    using BaseClasses = CommandTypeList<ExCommand<model::set_deform_binding::SetDeformBindingArguments>, Command>;
};
template<> struct CommandTraits<model::set_deform_binding::SetTRSBindingCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectBindingEdit};
    using BaseClasses = CommandTypeList<ExCommand<model::set_deform_binding::SetTRSBindingArguments>, Command>;
};
template<> struct CommandTraits<model::set_deform_binding::SetRotationXBindingCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectBindingEdit};
    using BaseClasses = CommandTypeList<ExCommand<model::set_deform_binding::SetRotationXBindingArguments>, Command>;
};
template<> struct CommandTraits<model::set_deform_binding::SetRotationYBindingCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectBindingEdit};
    using BaseClasses = CommandTypeList<ExCommand<model::set_deform_binding::SetRotationYBindingArguments>, Command>;
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::model::set_deform_binding::constructor_detail {
template<class...> inline constexpr bool missingParameter = false;
template<class Target, class Argument> consteval bool parameterSupported() {
    using Value = std::remove_cvref_t<Argument>;
    if constexpr (std::same_as<Target, compat::RetainedStringSlice>) {
        if constexpr (std::same_as<Value, std::nullptr_t>) return true;
        else if constexpr (::nijigenerate::commands::detail::commandStringDescriptor<Value>)
            return ngCommandInitializationSupported<Target, Argument&&>();
        else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value>) return false;
        else static_assert(missingParameter<Argument>, "Missing defining Source string parameter program");
    } else if constexpr (std::same_as<Target, compat::Slice<float>>) {
        if constexpr (std::same_as<Value, std::nullptr_t>) return true;
        else if constexpr (std::same_as<Value, Target>) return ngCommandInitializationSupported<Target, Argument&&>();
        else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value>) return false;
        else static_assert(missingParameter<Argument>, "Missing defining Source dynamic-array parameter program");
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
} // namespace nijigenerate::commands::model::set_deform_binding::constructor_detail

namespace nijigenerate::commands {

template<> struct SourceCommandConstructor<model::set_deform_binding::SetDeformBindingCommand> {
    template<class... Args> struct Program {
        using C = model::set_deform_binding::SetDeformBindingCommand;
        using Targets = std::tuple<compat::RetainedStringSlice, compat::Slice<float>, bool, bool>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) < 2 || sizeof...(Args) > 4) return false;
            else return model::set_deform_binding::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires {
                    { C::sourceConstructorLabels() } -> std::same_as<model::set_deform_binding::detail::CommandLabels>;
                } && SourceExCommandConstructorProgram<model::set_deform_binding::SetDeformBindingArguments>::
                    preferredSupported<compat::RetainedStringSlice&, compat::Slice<float>&>() && C::SourceFlagProgram::supported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return model::set_deform_binding::constructor_detail::construct<C, Targets>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<model::set_deform_binding::SetTRSBindingCommand> {
    template<class... Args> struct Program {
        using C = model::set_deform_binding::SetTRSBindingCommand;
        using Targets = std::tuple<compat::Slice<float>, compat::Slice<float>, float, bool>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 4) return false;
            else return model::set_deform_binding::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires {
                    { C::sourceConstructorLabels() } -> std::same_as<model::set_deform_binding::detail::CommandLabels>;
                } && SourceExCommandConstructorProgram<model::set_deform_binding::SetTRSBindingArguments>::
                    preferredSupported<compat::Slice<float>&, compat::Slice<float>&, float&, bool&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return model::set_deform_binding::constructor_detail::construct<C, Targets>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<model::set_deform_binding::SetRotationXBindingCommand> {
    template<class... Args> struct Program {
        using C = model::set_deform_binding::SetRotationXBindingCommand;
        using Targets = std::tuple<float>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 1) return false;
            else return model::set_deform_binding::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires {
                    { C::sourceConstructorLabels() } -> std::same_as<model::set_deform_binding::detail::CommandLabels>;
                } && SourceExCommandConstructorProgram<model::set_deform_binding::SetRotationXBindingArguments>::
                    preferredSupported<float&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return model::set_deform_binding::constructor_detail::construct<C, Targets>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<model::set_deform_binding::SetRotationYBindingCommand> {
    template<class... Args> struct Program {
        using C = model::set_deform_binding::SetRotationYBindingCommand;
        using Targets = std::tuple<float>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) > 1) return false;
            else return model::set_deform_binding::constructor_detail::parametersSupported<Targets, Args...>() &&
                requires {
                    { C::sourceConstructorLabels() } -> std::same_as<model::set_deform_binding::detail::CommandLabels>;
                } && SourceExCommandConstructorProgram<model::set_deform_binding::SetRotationYBindingArguments>::
                    preferredSupported<float&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return model::set_deform_binding::constructor_detail::construct<C, Targets>(
                allocator, std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands

namespace nijigenerate::commands::model::set_deform_binding {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct DeformArguments {
    static auto get() { return std::tuple{compat::RetainedString::fromStaticLiteral("deform"), compat::Slice<float>{}}; }
};
template<auto Id, class C>
using Default = ::nijigenerate::commands::detail::DefaultCommandRegistration<Id, C>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    ::nijigenerate::commands::detail::ConstructedCommandRegistration<
        ModelCommand::SetDeformBinding, SetDeformBindingCommand, DeformArguments>,
    Default<ModelCommand::SetTRSBinding, SetTRSBindingCommand>,
    Default<ModelCommand::SetRotationYBinding, SetRotationYBindingCommand>,
    Default<ModelCommand::SetRotationXBinding, SetRotationXBindingCommand>>;
}
template<class T> requires (std::same_as<T, ModelCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitModelBindingCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<model::set_deform_binding::ModelCommand> {
    static consteval bool supported() {
        return model::set_deform_binding::initialization::Program::supported<model::set_deform_binding::initialization::Table>();
    }
    template<class ActualKey = model::set_deform_binding::ModelCommand>
        requires (std::same_as<ActualKey, model::set_deform_binding::ModelCommand> && supported())
    static void invoke() { model::set_deform_binding::ngInitCommands<ActualKey>(); }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<
        ::nijigenerate::commands::model::set_deform_binding::SetDeformBindingCommand> {
    inline static constexpr char className[] =
        "nijigenerate.commands.model.set_deform_binding.SetDeformBindingCommand";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.model.set_deform_binding.SetDeformBindingCommand)";
};
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::model::set_deform_binding::SetDeformBindingCommand> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.model.set_deform_binding.SetDeformBindingCommand)";
    inline static constexpr char unqualifiedArgument[] = "(SetDeformBindingCommand)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.model.set_deform_binding.SetDeformBindingCommand";
    inline static constexpr char unqualifiedElement[] = "SetDeformBindingCommand";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::commands::model::set_deform_binding::SetTRSBindingCommand> {
    inline static constexpr char className[] =
        "nijigenerate.commands.model.set_deform_binding.SetTRSBindingCommand";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.model.set_deform_binding.SetTRSBindingCommand)";
};
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::model::set_deform_binding::SetTRSBindingCommand> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.model.set_deform_binding.SetTRSBindingCommand)";
    inline static constexpr char unqualifiedArgument[] = "(SetTRSBindingCommand)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.model.set_deform_binding.SetTRSBindingCommand";
    inline static constexpr char unqualifiedElement[] = "SetTRSBindingCommand";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::commands::model::set_deform_binding::SetRotationXBindingCommand> {
    inline static constexpr char className[] =
        "nijigenerate.commands.model.set_deform_binding.SetRotationXBindingCommand";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.model.set_deform_binding.SetRotationXBindingCommand)";
};
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::model::set_deform_binding::SetRotationXBindingCommand> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.model.set_deform_binding.SetRotationXBindingCommand)";
    inline static constexpr char unqualifiedArgument[] = "(SetRotationXBindingCommand)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.model.set_deform_binding.SetRotationXBindingCommand";
    inline static constexpr char unqualifiedElement[] = "SetRotationXBindingCommand";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::commands::model::set_deform_binding::SetRotationYBindingCommand> {
    inline static constexpr char className[] =
        "nijigenerate.commands.model.set_deform_binding.SetRotationYBindingCommand";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.model.set_deform_binding.SetRotationYBindingCommand)";
};
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::model::set_deform_binding::SetRotationYBindingCommand> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.model.set_deform_binding.SetRotationYBindingCommand)";
    inline static constexpr char unqualifiedArgument[] = "(SetRotationYBindingCommand)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.model.set_deform_binding.SetRotationYBindingCommand";
    inline static constexpr char unqualifiedElement[] = "SetRotationYBindingCommand";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::model::set_deform_binding {
inline void SetDeformBindingCommand::prepareSourceClassInfo() {
    ExCommand<SetDeformBindingArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetDeformBindingCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<SetDeformBindingCommand>();
}
inline void SetTRSBindingCommand::prepareSourceClassInfo() {
    ExCommand<SetTRSBindingArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetTRSBindingCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<SetTRSBindingCommand>();
}
inline void SetRotationXBindingCommand::prepareSourceClassInfo() {
    ExCommand<SetRotationXBindingArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetRotationXBindingCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<SetRotationXBindingCommand>();
}
inline void SetRotationYBindingCommand::prepareSourceClassInfo() {
    ExCommand<SetRotationYBindingArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetRotationYBindingCommand>();
    nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<SetRotationYBindingCommand>();
}
} // namespace nijigenerate::commands::model::set_deform_binding
