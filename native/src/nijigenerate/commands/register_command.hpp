#pragma once
#include "command.hpp"
#include "source_reflection.hpp"
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <concepts>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands {
template<auto Id> struct CommandClassFor;

// The defining constructor supplies its operation program. Its supported()
// describes those same operations, and construct() is the production creation
// path. A class signature alone cannot establish constructor-body semantics.
// Retain raw argument references until the actual class allocation; Source
// argument normalization and parameter initialization follow that allocation.
template<class C> struct SourceCommandConstructor;

namespace detail {

template<class...> inline constexpr bool missingSourceCommandConstructor = false;

template<class C, class... Args> consteval bool sourceCommandConstructorSupported() {
    if constexpr (requires { typename SourceCommandConstructor<C>::template Program<Args...>; }) {
        using Program = typename SourceCommandConstructor<C>::template Program<Args...>;
        if constexpr (!requires {
            { Program::supported() } -> std::same_as<bool>;
            typename std::bool_constant<Program::supported()>;
        }) {
            static_assert(missingSourceCommandConstructor<C, Args...>,
                "Source command constructor program has no operation-based supported query");
        } else if constexpr (!Program::supported()) return false;
        else {
            static_assert(requires(Args&&... args) {
                { Program::construct(nicxlive::core::d_runtime::SourceAllocator<C>{},
                    std::forward<Args>(args)...) } -> std::same_as<std::shared_ptr<C>>;
            }, "Source command constructor program has no matching production construction operation");
            return true;
        }
    } else {
        static_assert(missingSourceCommandConstructor<C, Args...>,
            "Missing defining Source constructor operation program for command construction");
    }
}

template<class C, class... Args> requires(sourceCommandConstructorSupported<C, Args...>())
std::shared_ptr<C> constructSourceCommand(Args&&... args) {
    using Program = typename SourceCommandConstructor<C>::template Program<Args...>;
    return Program::construct(nicxlive::core::d_runtime::SourceAllocator<C>{}, std::forward<Args>(args)...);
}
template<class... Args> compat::RetainedString commandArgsToSourceString(const Args&... args) {
    if constexpr (sizeof...(Args) == 0) return compat::RetainedString::emptyLiteral();
    else {
        std::string result;
        bool first = true;
        ([&] {
            if (!first) {
                try {
                    result += ", ";
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }
            first = false;
            const auto argument = toCodeString(args);
            try {
                result += argument.view();
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }(), ...);
        try {
            return compat::RetainedString(std::move(result));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
}
}

// The source registration mixin constructs the selected concrete class, records
// static-type attributes, then assigns the same class handle to the actual AA.
template<class C, class... Args>
consteval bool ngCommandConstructionSupported() {
    return detail::sourceCommandConstructorSupported<C, Args...>();
}

template<class C, class... Args>
consteval bool ngCommandFactorySupported() {
    if constexpr (!ngCommandConstructionSupported<C, Args...>()) return false;
    else return detail::metadataBuildSupported<C>();
}

template<class C, class Map, class Key, class... Args>
consteval bool ngRuntimeCommandRegistrationSupported() {
    if constexpr (!ngCommandFactorySupported<C, Args...>()) return false;
    else return requires(Map& commands, const Key& key, const std::shared_ptr<C>& command) {
            commands[key] = command;
        };
}

template<auto Id, class C, class Map, class... Args>
consteval bool ngCommandRegistrationSupported() {
    return ngRuntimeCommandRegistrationSupported<C, Map, decltype(Id), Args...>();
}

// Both enum registrations and processor-key registrations construct and record
// metadata through this same constrained production operation.
template<class C, class... Args> requires(ngCommandFactorySupported<C, Args...>())
std::shared_ptr<C> ngCreateRegisteredCommand(Args&&... args) {
    auto command = detail::constructSourceCommand<C>(std::forward<Args>(args)...);
    ngRegisterCommandMeta(command);
    return command;
}

template<auto Id, class C, class Map, class... Args>
requires(ngCommandRegistrationSupported<Id, C, Map, Args...>())
void ngRegisterConstructedCommand(Map& commands, Args&&... args) {
    auto command = ngCreateRegisteredCommand<C>(std::forward<Args>(args)...);
    commands[Id] = command;
}

template<auto Id, class C, class Map, class... Args>
bool ngRegisterCommandIfSupported(Map& commands, Args&&... args) {
    if constexpr (ngCommandRegistrationSupported<Id, C, Map, Args...>()) {
        ngRegisterConstructedCommand<Id, C>(commands, std::forward<Args>(args)...);
        return true;
    } else return false;
}

namespace detail {

template<auto Id, class C> struct DefaultCommandRegistration {
    template<class Binding> static consteval bool supported() {
        return ngCommandRegistrationSupported<Id, C, typename Binding::map_type>();
    }
    template<class Binding> requires(supported<Binding>())
    static void run() {
        auto command = ngCreateRegisteredCommand<C>();
        // Resolve the current TLS slot after construction/metadata callbacks.
        Binding::get()[Id] = command;
    }
};

template<auto Id, class C> struct OptionalCommandRegistration {
    template<class Binding> static consteval bool supported() {
        // The authored conditional can skip an unsupported registration. Force
        // that same real decision; do not report its false branch as a body error.
        return requires {
            typename std::integral_constant<bool,
                DefaultCommandRegistration<Id, C>::template supported<Binding>()>;
        };
    }
    template<class Binding> requires(supported<Binding>())
    static void run() {
        if constexpr (DefaultCommandRegistration<Id, C>::template supported<Binding>())
            DefaultCommandRegistration<Id, C>::template run<Binding>();
    }
};

template<auto Id, class C, class Arguments> struct ConstructedCommandRegistration {
    using Values = decltype(Arguments::get());
    template<class Binding> static consteval bool supported() {
        return []<std::size_t... Index>(std::index_sequence<Index...>) {
            return ngCommandRegistrationSupported<Id, C, typename Binding::map_type,
                decltype(std::get<Index>(std::declval<Values&>()))...>();
        }(std::make_index_sequence<std::tuple_size_v<Values>>{});
    }
    template<class Binding> requires(supported<Binding>())
    static void run() {
        auto values = Arguments::get();
        auto command = std::apply([](auto&... value) {
            return ngCreateRegisteredCommand<C>(value...);
        }, values);
        Binding::get()[Id] = command;
    }
};

// Some source initializers assign first, then read the AA's base-typed value
// for metadata registration. Do not replace that with metadata on concrete C.
template<auto Id, class C, class Arguments> struct AssignedCommandRegistration {
    using Values = decltype(Arguments::get());
    template<class Binding> static consteval bool supported() {
        using Map = typename Binding::map_type;
        if constexpr (!requires(Map& map, std::shared_ptr<C> command) { map[Id] = command; map.at(Id); })
            return false;
        else {
            using Handle = std::remove_cvref_t<decltype(std::declval<Map&>().at(Id))>;
            using Stored = typename Handle::element_type;
            return metadataBuildSupported<Stored>() && []<std::size_t... Index>(std::index_sequence<Index...>) {
                return ngCommandConstructionSupported<C,
                    decltype(std::get<Index>(std::declval<Values&>()))...>();
            }(std::make_index_sequence<std::tuple_size_v<Values>>{});
        }
    }
    template<class Binding> requires(supported<Binding>())
    static void run() {
        {
            auto values = Arguments::get();
            auto command = std::apply([](auto&... value) {
                return constructSourceCommand<C>(value...);
            }, values);
            Binding::get()[Id] = command;
        }
        const auto current = Binding::get().at(Id);
        ngRegisterCommandMeta(current);
    }
};

template<auto Id, class C, class Arguments> struct OptionalConstructedCommandRegistration {
    using Required = ConstructedCommandRegistration<Id, C, Arguments>;
    template<class Binding> static consteval bool supported() {
        return requires { typename std::integral_constant<bool, Required::template supported<Binding>()>; };
    }
    template<class Binding> requires(supported<Binding>())
    static void run() {
        if constexpr (Required::template supported<Binding>()) Required::template run<Binding>();
    }
};

template<class... Operation> struct CommandRegistrationSequence {
private:
    template<class Binding, std::size_t Index> static consteval bool supportedFrom() {
        if constexpr (Index == sizeof...(Operation)) return true;
        else if constexpr (!std::tuple_element_t<Index, std::tuple<Operation...>>::template supported<Binding>())
            return false;
        else return supportedFrom<Binding, Index + 1>();
    }
public:
    template<class Binding> static consteval bool supported() {
        return supportedFrom<Binding, 0>();
    }
    template<class Binding> requires(supported<Binding>())
    static void run() { (Operation::template run<Binding>(), ...); }
};

} // namespace detail

template<auto Id, class Map, class... Args> void registerCommand(Map& commands, Args&&... args) {
    using ActualClass = typename CommandClassFor<Id>::type;
    ngRegisterConstructedCommand<Id, ActualClass>(commands, std::forward<Args>(args)...);
}
}
