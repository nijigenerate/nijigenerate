#pragma once
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include "prop_table.hpp"
#include <nijigenerate/commands/parameter/prop_axis_normalization.hpp>
#include <nijigenerate/commands/parameter/prop_axis_snapshots.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <array>
#include <concepts>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands::parameter::prop {

struct SetParameterNameArguments {
    compat::RetainedStringSlice newName;
};
struct ApplyParameterPropsAxesArguments {
    std::array<float, 2> min{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    std::array<float, 2> max{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
    compat::Slice<float> axisX;
    compat::Slice<float> axisY;
};

} // namespace nijigenerate::commands::parameter::prop

namespace nijigenerate::commands {

template<> struct CommandFields<parameter::prop::SetParameterNameArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&parameter::prop::SetParameterNameArguments::newName,
            "newName", "New parameter name", "string">{}};
};
template<> struct CommandFields<parameter::prop::ApplyParameterPropsAxesArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&parameter::prop::ApplyParameterPropsAxesArguments::min,
            "min", "New min (x,y) as [2]", "float[2]">{},
        FieldDescriptor<&parameter::prop::ApplyParameterPropsAxesArguments::max,
            "max", "New max (x,y) as [2]", "float[2]">{},
        FieldDescriptor<&parameter::prop::ApplyParameterPropsAxesArguments::axisX,
            "axisX", "Normalized X breakpoints as ratios in [0, 1], including endpoints 0 and 1", "float[]">{},
        FieldDescriptor<&parameter::prop::ApplyParameterPropsAxesArguments::axisY,
            "axisY", "Normalized Y breakpoints as ratios in [0, 1], including endpoints 0 and 1; empty for 1D",
            "float[]">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::prop {

class SetParameterNameCommand : public ExCommand<SetParameterNameArguments>,
    public std::enable_shared_from_this<SetParameterNameCommand> {
    struct SourceConstructorArguments {
        compat::RetainedString description;
        compat::RetainedStringSlice newName;
    };
    static SourceConstructorArguments sourceConstructorArguments(const compat::RetainedStringSlice& newName);
    explicit SetParameterNameCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetParameterNameCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    explicit SetParameterNameCommand(compat::RetainedStringSlice newName);
    SetParameterNameCommand(const SetParameterNameCommand&) = delete;
    SetParameterNameCommand& operator=(const SetParameterNameCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ApplyParameterPropsAxesCommand : public ExCommand<ApplyParameterPropsAxesArguments>,
    public std::enable_shared_from_this<ApplyParameterPropsAxesCommand> {
    struct SourceConstructorArguments {
        compat::RetainedString description;
        std::array<float, 2> min;
        std::array<float, 2> max;
        compat::Slice<float> axisX;
        compat::Slice<float> axisY;
    };
    static SourceConstructorArguments sourceConstructorArguments(const std::array<float, 2>& min,
        const std::array<float, 2>& max, const compat::Slice<float>& axisX, const compat::Slice<float>& axisY);
    explicit ApplyParameterPropsAxesCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ApplyParameterPropsAxesCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ApplyParameterPropsAxesCommand(std::array<float, 2> min, std::array<float, 2> max,
        compat::Slice<float> axisX, compat::Slice<float> axisY);
    ApplyParameterPropsAxesCommand(const ApplyParameterPropsAxesCommand&) = delete;
    ApplyParameterPropsAxesCommand& operator=(const ApplyParameterPropsAxesCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

void ngInitParameterPropCommands();

} // namespace nijigenerate::commands::parameter::prop

namespace nicxlive::core::d_runtime {
#define NG_PARAMETER_PROP_COMMAND_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::parameter::prop::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.parameter.prop." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.parameter.prop." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::prop::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.parameter.prop." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.prop." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_PARAMETER_PROP_COMMAND_SOURCE_NAMES(SetParameterNameCommand)
NG_PARAMETER_PROP_COMMAND_SOURCE_NAMES(ApplyParameterPropsAxesCommand)
#undef NG_PARAMETER_PROP_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::parameter::prop {

template<ParamPropCommand id, class... Args>
std::shared_ptr<CommandResult> ngRunParameterPropCommand(std::shared_ptr<Context> context, Args&&... args) {
    static_assert(id == ParamPropCommand::SetParameterName || id == ParamPropCommand::ApplyParameterPropsAxes);
    using C = std::conditional_t<id == ParamPropCommand::SetParameterName,
        SetParameterNameCommand, ApplyParameterPropsAxesCommand>;
    constexpr CommandDispatchSource source = id == ParamPropCommand::SetParameterName
        ? CommandDispatchSource{"ParamPropCommand.SetParameterName", "ParamPropCommand", "SetParameterNameCommand"}
        : CommandDispatchSource{"ParamPropCommand.ApplyParameterPropsAxes", "ParamPropCommand", "ApplyParameterPropsAxesCommand"};
    return ngDispatchRegisteredCommand<C>(commands, id, context, source, std::forward<Args>(args)...);
}

} // namespace nijigenerate::commands::parameter::prop

namespace nijigenerate::commands {

template<> struct CommandTraits<parameter::prop::SetParameterNameCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectRename};
    using BaseClasses = CommandTypeList<ExCommand<parameter::prop::SetParameterNameArguments>, Command>;
};
template<> struct CommandTraits<parameter::prop::ApplyParameterPropsAxesCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<parameter::prop::ApplyParameterPropsAxesArguments>, Command>;
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::prop::constructor_detail {
template<class...> inline constexpr bool missingParameter = false;

template<class Target, class Argument> consteval bool parameterSupported() {
    using Value = std::remove_cvref_t<Argument>;
    if constexpr (std::same_as<Target, compat::RetainedStringSlice>) {
        if constexpr (std::same_as<Value, std::nullptr_t>) return true;
        else if constexpr (::nijigenerate::commands::detail::commandStringDescriptor<Value>)
            return ngCommandInitializationSupported<Target, Argument&&>();
        else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value>) return false;
        else static_assert(missingParameter<Argument>, "Missing defining Source string parameter initialization");
    } else if constexpr (std::same_as<Target, compat::Slice<float>>) {
        if constexpr (std::same_as<Value, std::nullptr_t>) return true;
        else if constexpr (std::same_as<Value, Target>)
            return ngCommandInitializationSupported<Target, Argument&&>();
        else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value>) return false;
        else static_assert(missingParameter<Argument>, "Missing defining Source dynamic-array parameter initialization");
    } else {
        if constexpr (std::same_as<Value, Target>)
            return ngCommandInitializationSupported<Target, Argument&&>();
        else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value> ||
            std::same_as<Value, std::nullptr_t>) return false;
        else static_assert(missingParameter<Argument>, "Missing defining Source static-array parameter initialization");
    }
}

} // namespace nijigenerate::commands::parameter::prop::constructor_detail

namespace nijigenerate::commands {
template<> struct SourceCommandConstructor<parameter::prop::SetParameterNameCommand> {
    template<class... Args> struct Program {
        using C = parameter::prop::SetParameterNameCommand;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else return (parameter::prop::constructor_detail::parameterSupported<compat::RetainedStringSlice, Args>() && ...)
                && detail::declaredSourceParametersSupported<std::tuple<compat::RetainedStringSlice>, Args...>()
                && requires(compat::RetainedStringSlice& name) {
                    { C::sourceConstructorArguments(name) } -> std::same_as<typename C::SourceConstructorArguments>;
                } && SourceExCommandConstructorProgram<parameter::prop::SetParameterNameArguments>::
                    preferredSupported<compat::RetainedStringSlice&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<compat::RetainedStringSlice>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};
template<> struct SourceCommandConstructor<parameter::prop::ApplyParameterPropsAxesCommand> {
    template<class... Args> struct Program {
        using C = parameter::prop::ApplyParameterPropsAxesCommand;
        using Pair = std::array<float, 2>;
        using Array = compat::Slice<float>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 4) return false;
            else return []<class Min, class Max, class X, class Y>(CommandTypeList<Min, Max, X, Y>) {
                return parameter::prop::constructor_detail::parameterSupported<Pair, Min>() &&
                    parameter::prop::constructor_detail::parameterSupported<Pair, Max>() &&
                    parameter::prop::constructor_detail::parameterSupported<Array, X>() &&
                    parameter::prop::constructor_detail::parameterSupported<Array, Y>() &&
                    detail::declaredSourceParametersSupported<std::tuple<Pair, Pair, Array, Array>, Min, Max, X, Y>() &&
                    requires(Pair& lo, Pair& hi, Array& x, Array& y) {
                        { C::sourceConstructorArguments(lo, hi, x, y) } ->
                            std::same_as<typename C::SourceConstructorArguments>;
                    } && SourceExCommandConstructorProgram<parameter::prop::ApplyParameterPropsAxesArguments>::
                        preferredSupported<Pair&, Pair&, Array&, Array&>();
            }(CommandTypeList<Args...>{});
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<Pair, Pair, Array, Array>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::prop {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct NameArguments {
    static auto get() {
        return std::tuple{compat::RetainedStringSlice{compat::RetainedString::emptyLiteral()}};
    }
};
struct AxesArguments {
    static auto get() {
        return std::tuple{std::array<float, 2>{0, 0}, std::array<float, 2>{1, 1},
            compat::Slice<float>{}, compat::Slice<float>{}};
    }
};
template<auto Id, class C, class Args>
using Required = ::nijigenerate::commands::detail::ConstructedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Required<ParamPropCommand::SetParameterName, SetParameterNameCommand, NameArguments>,
    Required<ParamPropCommand::ApplyParameterPropsAxes, ApplyParameterPropsAxesCommand, AxesArguments>>;
}
template<class T> requires (std::same_as<T, ParamPropCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitParameterPropCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<parameter::prop::ParamPropCommand> {
    static consteval bool supported() {
        return parameter::prop::initialization::Program::supported<parameter::prop::initialization::Table>();
    }
    template<class ActualKey = parameter::prop::ParamPropCommand>
        requires (std::same_as<ActualKey, parameter::prop::ParamPropCommand> && supported())
    static void invoke() { parameter::prop::ngInitCommands<ActualKey>(); }
};
}
