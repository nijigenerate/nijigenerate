#pragma once

#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <array>
#include <functional>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::actions::binding::detail {

// Borrow the producer for one synchronous factory call. Its argument
// expressions run after the same complete action and shared owner exist.
template<class Producer> struct BindingArgumentsProducer { Producer&& produce; };
template<class T> inline constexpr bool isBindingArgumentsProducer=false;
template<class Producer>
inline constexpr bool isBindingArgumentsProducer<BindingArgumentsProducer<Producer>> = true;

template<class Initialize,class... Args>
void initializeBindingAction(Initialize&& initialize,Args&&... args) {
    if constexpr(sizeof...(Args)==1 &&
        (isBindingArgumentsProducer<std::remove_cvref_t<Args>> && ...)) {
        auto&& producer=std::get<0>(std::forward_as_tuple(args...));
        auto arguments=std::invoke(std::forward<decltype(producer.produce)>(producer.produce));
        std::apply(std::forward<Initialize>(initialize),std::move(arguments));
    } else {
        std::invoke(std::forward<Initialize>(initialize),std::forward<Args>(args)...);
    }
}

// D's private generic overload uses ordinary T value-copy semantics.
template<class T>
T cloneActionValue(T value) {
    return value;
}

inline compat::RetainedString retainBindingText(std::string text) {
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline void appendBindingText(std::string& text, std::string_view bytes) {
    try {
        text.append(bytes);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

// ClassInfo qualifies template arguments and retains the inner declaration.
// Resolve defining providers before translating passive text allocation errors.
template<class... Types> compat::RetainedString bindingActionClassName(const char* declaration) {
    using namespace nicxlive::core::d_runtime;
    constexpr auto context = sizeof...(Types) == 1 ? SourceTypeSpelling::QualifiedArgument :
        SourceTypeSpelling::QualifiedElement;
    const std::array<compat::RetainedString, sizeof...(Types)> arguments{
        ngSourceTypeSpelling<Types>(context)...};
    try {
        std::string text = "nijigenerate.actions.binding.";
        text += declaration;
        text += '!';
        if constexpr (sizeof...(Types) != 1) text += '(';
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            if (i) text += ", ";
            text.append(arguments[i].view());
        }
        if constexpr (sizeof...(Types) != 1) text += ')';
        text += '.';
        text += declaration;
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class Class, class... Types> void prepareBindingActionClass(const char* declaration) {
    static const bool registered = [declaration] {
        using namespace nicxlive::core::d_runtime;
        const auto name=bindingActionClassName<Types...>(declaration);
        ngPrepareSourceTypeSpellings<Class>();
        ngRegisterObjectTypeNames<Class>(name,SourceObjectTypeNames<Class>::templateArgumentName());
        ngRegisterObjectClassName(typeid(Class), name);
        return true;
    }();
    (void)registered;
}

// TypeClass's eponymous template printer uses unqualified inner arguments.
// These four type origins are independent of the ClassInfo name above.
template<class... Types> nicxlive::core::d_runtime::SourceTypeSpellingNames bindingActionTypeSpellings(
        const char* declaration) {
    using namespace nicxlive::core::d_runtime;
    constexpr auto context = sizeof...(Types) == 1 ? SourceTypeSpelling::UnqualifiedArgument :
        SourceTypeSpelling::UnqualifiedElement;
    const std::array<compat::RetainedString, sizeof...(Types)> arguments{
        ngSourceTypeSpelling<Types>(context)...};
    try {
        std::string unqualified = declaration;
        unqualified += '!';
        if constexpr (sizeof...(Types) != 1) unqualified += '(';
        for (std::size_t i = 0; i < arguments.size(); ++i) {
            if (i) unqualified += ", ";
            unqualified.append(arguments[i].view());
        }
        if constexpr (sizeof...(Types) != 1) unqualified += ')';
        auto qualified = std::string("nijigenerate.actions.binding.") + unqualified;
        return {compat::RetainedString("(" + qualified + ")"),
            compat::RetainedString("(" + unqualified + ")"),
            compat::RetainedString(std::move(qualified)), compat::RetainedString(std::move(unqualified))};
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

} // namespace nijigenerate::actions::binding::detail
