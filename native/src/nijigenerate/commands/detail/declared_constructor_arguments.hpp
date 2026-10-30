#pragma once
#include <nijigenerate/commands/fields.hpp>
#include <compat/source_allocator.hpp>
#include <cstddef>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands::detail {

template<class Targets, class Arguments> struct DeclaredConstructorParameterChecks;

template<class... Target, class... Argument>
struct DeclaredConstructorParameterChecks<std::tuple<Target...>, std::tuple<Argument...>> {
    static consteval bool supported() {
        static_assert(sizeof...(Target) == sizeof...(Argument));
        static_assert((CommandInitializationValue<Target>::value && ...),
            "A custom Source constructor value needs its defining lifetime program");
        return (ngCommandInitializationSupported<Target, Argument>() && ...) &&
            (ngCommandParameterCopySupported<Target, Target&>() && ...);
    }
};

template<class Targets, class... Args> consteval bool declaredSourceParametersSupported() {
    return DeclaredConstructorParameterChecks<Targets, std::tuple<Args&&...>>::supported();
}

// Used by defining command programs with actual passive declared parameter
// carriers. Raw argument references, not Source values, cross the allocation
// boundary. Custom value/postblit lifetimes require their defining program.
template<class Targets, class Arguments> class DeclaredConstructorArguments;

template<class... Target, class... Argument>
class DeclaredConstructorArguments<std::tuple<Target...>, std::tuple<Argument...>> {
    static_assert(sizeof...(Target) == sizeof...(Argument));
    static_assert((std::is_reference_v<Argument> && ...));
    static_assert((CommandInitializationValue<Target>::value && ...),
        "A custom Source constructor value needs its defining lifetime program");
    static_assert(DeclaredConstructorParameterChecks<std::tuple<Target...>, std::tuple<Argument...>>::supported());
    std::tuple<Argument...> arguments_;
    std::tuple<std::optional<Target>...> values_;

    template<std::size_t Index> void prepare() {
        if constexpr (Index != 0) prepare<Index - 1>();
        auto& value = std::get<Index>(values_);
        if (!value) {
            using T = std::tuple_element_t<Index, std::tuple<Target...>>;
            using A = std::tuple_element_t<Index, std::tuple<Argument...>>;
            value.emplace();
            SourceCommandInitialization<T, A>::construct(*value,
                std::get<Index>(std::move(arguments_)));
        }
    }

public:
    explicit DeclaredConstructorArguments(std::tuple<Argument...> arguments) : arguments_(std::move(arguments)) {}

    template<std::size_t Index> struct Parameter {
        DeclaredConstructorArguments& frame;
        using Value = std::tuple_element_t<Index, std::tuple<Target...>>;
        operator Value() && {
            frame.template prepare<Index>();
            return SourceCommandParameterCopy<Value, Value&>::copy(*std::get<Index>(frame.values_));
        }
    };
};

template<class C, class Targets, class... Args>
std::shared_ptr<C> constructDeclaredSourceCommand(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
    Args&&... args) {
    using Frame = DeclaredConstructorArguments<Targets, std::tuple<Args&&...>>;
    Frame frame(std::forward_as_tuple(std::forward<Args>(args)...));
    return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
        return std::allocate_shared<C>(allocator, typename Frame::template Parameter<Index>{frame}...);
    }(std::index_sequence_for<Args...>{});
}

} // namespace nijigenerate::commands::detail
