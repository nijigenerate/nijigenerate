#pragma once

#include "fields.hpp"
#include "metadata.hpp"
#include "source_reflection.hpp"
#include "source_enum_metadata.hpp"
#include "source_diagnostic_text.hpp"
#include <optional>
#include <array>
#include <cstddef>
#include <cstdint>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/compat/source_call_arguments.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <nijigenerate/core/settings_string.hpp>
#include <nijigenerate/core/settings_value_metadata.hpp>
#include <nijigenerate/ext/param_metadata.hpp>
#include <common/nullable_string.hpp>
#include <common/resource_key_map.hpp>
#include <common/shared_array.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <math/types.hpp>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace nijigenerate::core::settings { class SettingsJsonValue; }
namespace nijigenerate::ext { class ExParameterGroup; }
namespace nicxlive::core { class Puppet; }
namespace nicxlive::core::nodes { class Node; }
namespace nicxlive::core::param { class Parameter; class ParameterBinding; }

namespace nijigenerate::commands {

namespace detail {
inline compat::RetainedString commandTypeArgumentGroup(const compat::RetainedString& element) {
    try {
        return compat::RetainedString("(" + std::string(element.view()) + ')');
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline compat::RetainedString commandSourceTypeSuffix(const compat::RetainedString& element, std::string_view suffix) {
    try {
        std::string name(element.view());
        name.append(suffix);
        return compat::RetainedString(std::move(name));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline compat::RetainedString commandConstSourceType(const compat::RetainedString& element) {
    try {
        return compat::RetainedString("const(" + std::string(element.view()) + ')');
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline compat::RetainedString commandStaticArrayElement(const compat::RetainedString& element, std::size_t length) {
    try {
        std::string name(element.view());
        name += '[';
        if (length > 0xffff) name += "cast(size_t)";
        name += std::to_string(length);
        name += ']';
        return compat::RetainedString(std::move(name));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class T> struct ResultArrayArgument { static constexpr bool value = false; };
template<class T> struct ResultArrayArgument<compat::Slice<T>> {
    static constexpr bool value = true, fixed = false;
    using Element = T;
};
template<class T> struct ResultArrayArgument<nicxlive::core::common::SharedArray<T>> {
    static constexpr bool value = true, fixed = false;
    using Element = T;
};
template<class T, std::size_t N> struct ResultArrayArgument<std::array<T, N>> {
    static constexpr bool value = true, fixed = true;
    static constexpr std::size_t length = N;
    using Element = T;
};

template<class T> struct ResultConstScalarShape {
    static constexpr bool scalar = std::is_arithmetic_v<std::remove_const_t<T>> && !std::is_volatile_v<T>;
    static constexpr bool qualified = scalar && std::is_const_v<T>;
    using Mutable = std::remove_const_t<T>;
};
template<class T, std::size_t N> struct ResultConstScalarShape<std::array<T, N>> {
    using Element = ResultConstScalarShape<T>;
    static constexpr bool scalar = Element::scalar;
    static constexpr bool qualified = Element::qualified;
    using Mutable = std::array<typename Element::Mutable, N>;
};
template<class T, std::size_t N> struct ResultConstScalarShape<const std::array<T, N>>
    : ResultConstScalarShape<std::array<T, N>> {
    static constexpr bool qualified = ResultConstScalarShape<std::array<T, N>>::scalar;
};
template<class T> void prepareCommandResultTypeSpellings();

template<class T> void prepareCommandResultScalarOrigin() {
    if constexpr (std::is_arithmetic_v<T>) prepareCommandResultTypeSpellings<T>();
    else if constexpr (ResultArrayArgument<T>::value)
        prepareCommandResultScalarOrigin<typename ResultArrayArgument<T>::Element>();
    else if constexpr (std::is_pointer_v<T> && !std::is_function_v<std::remove_pointer_t<T>>)
        prepareCommandResultScalarOrigin<std::remove_pointer_t<T>>();
}

template<class T> void prepareCommandResultTypeSpellings() {
    using namespace nicxlive::core::d_runtime;
    const auto& type = typeid(SourceObjectTypeIdentity<T>);
    if (ngFindSourceTypeSpelling(type, SourceTypeSpelling::QualifiedArgument) &&
            ngFindSourceTypeSpelling(type, SourceTypeSpelling::UnqualifiedArgument) &&
            ngFindSourceTypeSpelling(type, SourceTypeSpelling::QualifiedElement) &&
            ngFindSourceTypeSpelling(type, SourceTypeSpelling::UnqualifiedElement)) return;
    if constexpr (ResultArrayArgument<T>::value) {
        prepareCommandResultScalarOrigin<typename ResultArrayArgument<T>::Element>();
    } else if constexpr (std::is_pointer_v<T> && !std::is_function_v<std::remove_pointer_t<T>>)
        prepareCommandResultScalarOrigin<std::remove_pointer_t<T>>();
    if constexpr (requires { SourceTypeSpellings<T>::get(); } || requires {
            SourceTypeSpellings<T>::qualifiedArgument;
            SourceTypeSpellings<T>::unqualifiedArgument;
            SourceTypeSpellings<T>::qualifiedElement;
            SourceTypeSpellings<T>::unqualifiedElement;
        }) {
        ngPrepareSourceTypeSpellings<T>();
    } else if constexpr (std::is_arithmetic_v<T> && !std::is_volatile_v<T>) {
        const auto basic = core::translation::FormatSourceTypeName<std::remove_const_t<T>>::get();
        if (basic.empty()) return;
        try {
            const auto element = std::is_const_v<T> ? "const(" + basic + ')' : basic;
            const auto argument = std::is_const_v<T> ? '(' + element + ')' : element;
            const compat::RetainedString elementName(element), argumentName(argument);
            ngRegisterSourceTypeSpellings<T>({argumentName, argumentName, elementName, elementName});
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
}

template<class T> compat::RetainedString commandResultSourceElement(bool qualified) {
    using namespace nicxlive::core::d_runtime;
    const auto context = qualified ? SourceTypeSpelling::QualifiedElement : SourceTypeSpelling::UnqualifiedElement;
    prepareCommandResultTypeSpellings<T>();
    if (const auto registered = ngFindSourceTypeSpelling(typeid(SourceObjectTypeIdentity<T>), context))
        return *registered;
    if constexpr (!std::same_as<T, std::remove_cv_t<T>>) return ngSourceTypeSpelling<T>(context);
    else if constexpr (std::is_arithmetic_v<T> && std::same_as<T, std::remove_cv_t<T>>) {
        const auto name = core::translation::FormatSourceTypeName<T>::get();
        if (!name.empty()) return compat::RetainedString(name);
    } else if constexpr (std::same_as<T, void>)
        return compat::RetainedString::fromStaticLiteral("void");
    else if constexpr (std::same_as<T, std::nullptr_t>)
        return compat::RetainedString::fromStaticLiteral("typeof(null)");
    else if constexpr (std::is_pointer_v<T> && !std::is_function_v<std::remove_pointer_t<T>>) {
        const auto element = commandResultSourceElement<std::remove_pointer_t<T>>(qualified);
        return commandSourceTypeSuffix(element, "*");
    } else if constexpr (ResultArrayArgument<T>::value) {
        using Shape = ResultArrayArgument<T>;
        if constexpr (!Shape::fixed) {
            const auto element = commandResultSourceElement<typename ResultArrayArgument<T>::Element>(qualified);
            return commandSourceTypeSuffix(element, "[]");
        } else if constexpr (Shape::length <= std::numeric_limits<std::int64_t>::max()) {
            using Element = typename Shape::Element;
            const auto element = commandResultSourceElement<typename Shape::Element>(qualified);
            if constexpr (ResultConstScalarShape<Element>::qualified) {
                using MutableElement = typename ResultConstScalarShape<Element>::Mutable;
                const auto basic = commandResultSourceElement<MutableElement>(qualified);
                if (element.view() != commandConstSourceType(basic).view()) return ngSourceTypeSpelling<T>(context);
                return commandConstSourceType(commandStaticArrayElement(basic, Shape::length));
            } else return commandStaticArrayElement(element, Shape::length);
        }
    }
    return ngSourceTypeSpelling<T>(context);
}

template<class T> compat::RetainedString commandResultSourceArgument(bool qualified = true) {
    using namespace nicxlive::core::d_runtime;
    prepareCommandResultTypeSpellings<T>();
    if (!qualified) {
        if (const auto registered = ngFindSourceTypeSpelling(typeid(SourceObjectTypeIdentity<T>),
                SourceTypeSpelling::UnqualifiedArgument)) return *registered;
        if constexpr (!std::same_as<T, std::remove_cv_t<T>>)
            return ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
        else if constexpr (std::is_arithmetic_v<T> && std::same_as<T, std::remove_cv_t<T>>) {
            const auto name = core::translation::FormatSourceTypeName<T>::get();
            if (!name.empty()) return compat::RetainedString(name);
        } else if constexpr (std::same_as<T, void>)
            return compat::RetainedString::fromStaticLiteral("void");
        else if constexpr (std::same_as<T, std::nullptr_t>)
            return compat::RetainedString::fromStaticLiteral("(typeof(null))");
        else if constexpr ((std::is_pointer_v<T> && !std::is_function_v<std::remove_pointer_t<T>>) ||
                ResultArrayArgument<T>::value) {
            const auto element = commandResultSourceElement<T>(false);
            return commandTypeArgumentGroup(element);
        }
        return ngSourceTypeSpelling<T>(SourceTypeSpelling::UnqualifiedArgument);
    }
    if (const auto registered = ngFindObjectTemplateArgumentName(typeid(SourceObjectTypeIdentity<T>)))
        return *registered;
    if constexpr (requires { SourceObjectTypeNames<T>::templateArgumentName(); } ||
            requires { SourceObjectTypeNames<T>::templateArgumentName; } ||
            requires { SourceTypeArgumentName<T>::argumentName(); } ||
            requires { SourceTypeArgumentName<T>::argumentName; })
        return ngObjectTemplateArgumentName<T>();
    else if constexpr (!std::same_as<T, std::remove_cv_t<T>>)
        return ngObjectTemplateArgumentName<T>();
    else if constexpr (std::same_as<T, void>)
        return compat::RetainedString::fromStaticLiteral("void");
    else if constexpr (std::same_as<T, std::nullptr_t>)
        return compat::RetainedString::fromStaticLiteral("(typeof(null))");
    else if constexpr (std::is_pointer_v<T> && !std::is_function_v<std::remove_pointer_t<T>>) {
        const auto element = commandResultSourceElement<T>(true);
        return commandTypeArgumentGroup(element);
    } else if constexpr (ResultArrayArgument<T>::value) {
        const auto element = commandResultSourceElement<T>(true);
        return commandTypeArgumentGroup(element);
    } else if constexpr (std::is_arithmetic_v<T>) {
        const auto name = core::translation::FormatSourceTypeName<T>::get();
        if (name.empty()) return ngObjectTemplateArgumentName<T>();
        return compat::RetainedString(name);
    } else if constexpr (std::same_as<T, compat::RetainedString> ||
            std::same_as<T, compat::RetainedStringSlice> ||
            std::same_as<T, core::settings::SettingsString> ||
            std::same_as<T, nicxlive::core::common::NullableString> || std::same_as<T, std::string>)
        return compat::RetainedString::fromStaticLiteral("string");
    else if constexpr (std::same_as<T, core::settings::SettingsJsonValue>)
        return compat::RetainedString::fromStaticLiteral("(std.json.JSONValue)");
    else if constexpr (std::same_as<T, nicxlive::core::nodes::Node>)
        return compat::RetainedString::fromStaticLiteral("(nijilive.core.nodes.node.Node)");
    else if constexpr (std::same_as<T, nicxlive::core::param::Parameter>)
        return compat::RetainedString::fromStaticLiteral("(nijilive.core.param.Parameter)");
    else if constexpr (std::same_as<T, nicxlive::core::param::ParameterBinding>)
        return compat::RetainedString::fromStaticLiteral("(nijilive.core.param.binding.ParameterBinding)");
    else if constexpr (std::same_as<T, nijigenerate::ext::ExParameterGroup>)
        return compat::RetainedString::fromStaticLiteral("(nijigenerate.ext.param.ExParameterGroup)");
    else if constexpr (std::same_as<T, nicxlive::core::Puppet>)
        return compat::RetainedString::fromStaticLiteral("(nijilive.core.puppet.Puppet)");
    else return ngObjectTemplateArgumentName<T>();
}

template<class... Args> compat::RetainedString commandTemplateSourceArguments(bool qualified) {
    if constexpr (sizeof...(Args) == 0) return compat::RetainedString::fromStaticLiteral("()");
    else if constexpr (sizeof...(Args) == 1) return (commandResultSourceArgument<Args>(qualified), ...);
    else {
        const std::array elements{commandResultSourceElement<Args>(qualified)...};
        try {
            std::string arguments = "(";
            bool first = true;
            for (const auto& element : elements) {
                if (!first) arguments += ", ";
                first = false;
                arguments.append(element.view());
            }
            arguments += ')';
            return compat::RetainedString(std::move(arguments));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }
}

inline compat::RetainedString composeCommandTemplateName(std::string_view module, std::string_view declaration,
        const compat::RetainedString& arguments, bool classLeaf) {
    try {
        std::string name(module);
        if (!module.empty()) name += '.';
        name.append(declaration);
        name += '!';
        name.append(arguments.view());
        if (classLeaf) { name += '.'; name.append(declaration); }
        return compat::RetainedString(std::move(name));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class... Args> compat::RetainedString commandTemplateClassName(
        std::string_view module, std::string_view declaration) {
    const auto arguments = commandTemplateSourceArguments<Args...>(true);
    return composeCommandTemplateName(module, declaration, arguments, true);
}

template<class... Args> nicxlive::core::d_runtime::SourceTypeSpellingNames commandTemplateSourceSpellings(
        std::string_view module, std::string_view declaration) {
    const auto arguments = commandTemplateSourceArguments<Args...>(false);
    const auto qualified = composeCommandTemplateName(module, declaration, arguments, false);
    const auto unqualified = composeCommandTemplateName({}, declaration, arguments, false);
    return {commandTypeArgumentGroup(qualified), commandTypeArgumentGroup(unqualified), qualified, unqualified};
}

template<class T> compat::RetainedString commandResultSourceName(std::string_view declaration, bool argument) {
    constexpr std::string_view module = "nijigenerate.commands.base";
    if (!argument) return commandTemplateClassName<T>(module, declaration);
    const auto arguments = commandTemplateSourceArguments<T>(false);
    return commandTypeArgumentGroup(composeCommandTemplateName(module, declaration, arguments, false));
}

template<class T> auto commandResultCachedSourceSpellings()
        -> std::optional<nicxlive::core::d_runtime::SourceTypeSpellingNames> {
    using namespace nicxlive::core::d_runtime;
    const auto& type = typeid(SourceObjectTypeIdentity<T>);
    const auto qa = ngFindSourceTypeSpelling(type, SourceTypeSpelling::QualifiedArgument);
    const auto ua = ngFindSourceTypeSpelling(type, SourceTypeSpelling::UnqualifiedArgument);
    const auto qe = ngFindSourceTypeSpelling(type, SourceTypeSpelling::QualifiedElement);
    const auto ue = ngFindSourceTypeSpelling(type, SourceTypeSpelling::UnqualifiedElement);
    if (qa && ua && qe && ue) return SourceTypeSpellingNames{*qa, *ua, *qe, *ue};
    return std::nullopt;
}

template<class T> nicxlive::core::d_runtime::SourceTypeSpellingNames commandResultSourceSpellings(
        std::string_view declaration) {
    return commandTemplateSourceSpellings<T>("nijigenerate.commands.base", declaration);
}

template<class T> void prepareCommandResultClass() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names = SourceObjectTypeNames<T>;
        const auto name = [] {
            if constexpr (requires { Names::className(); }) return compat::RetainedString(Names::className());
            else return compat::RetainedString::fromStaticLiteral(Names::className);
        }();
        const auto argument = [] {
            if constexpr (requires { Names::templateArgumentName(); })
                return compat::RetainedString(Names::templateArgumentName());
            else return compat::RetainedString::fromStaticLiteral(Names::templateArgumentName);
        }();
        try {
            ngRegisterObjectTypeNames<T>(name, argument);
            ngRegisterObjectClassName(typeid(T), name);
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)registered;
}

template<class Result, class... Args> std::shared_ptr<Result> allocateCommandResult(Args&&... args) {
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        std::forward<Args>(args)...);
}
}

class Context;
class CommandResult : public std::enable_shared_from_this<CommandResult> {
public:
    bool succeeded;
    compat::RetainedString message;
    explicit CommandResult(bool success, compat::RetainedString text = compat::RetainedString::emptyLiteral())
        : succeeded(success), message(std::move(text)) {}
    virtual ~CommandResult() = default;
    CommandResult(const CommandResult&) = delete;
    CommandResult& operator=(const CommandResult&) = delete;
    static std::shared_ptr<CommandResult> opCall(
        bool success, compat::RetainedString text = compat::RetainedString::emptyLiteral()) {
        return detail::allocateCommandResult<CommandResult>(success, std::move(text));
    }
    virtual std::shared_ptr<CommandResult> waitForCompletion() { return shared_from_this(); }
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<CommandResult> other);
    virtual int opCmp(std::shared_ptr<CommandResult> other);
    compat::RetainedString sourceClassInfoName();
protected:
    virtual void prepareSourceClassInfo();
};

bool ngCommandResultObjectEquals(std::shared_ptr<CommandResult> left, std::shared_ptr<CommandResult> right);

namespace detail {
// Unsupported source .init mappings must be supplied explicitly by their real
// type provider. Native value-initialization is not a general D .init adapter.
template<class T> struct CommandResultInitialValue;
template<class T, std::size_t N> struct CommandResultInitialValue<std::array<T, N>> {
    static std::array<T, N> get() {
        if constexpr (N == 0) return {};
        else {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return std::array<T, N>{(static_cast<void>(I), CommandResultInitialValue<T>::get())...};
            }(std::make_index_sequence<N>{});
        }
    }
};
template<std::integral T> struct CommandResultInitialValue<T> {
    static T get() {
        using Value = std::remove_cv_t<T>;
        if constexpr (std::same_as<Value, char> || std::same_as<Value, char8_t>) return static_cast<T>(0xff);
        else if constexpr (std::same_as<Value, char16_t> || std::same_as<Value, char32_t> ||
                std::same_as<Value, wchar_t>) return static_cast<T>(0xffff);
        else return T{0};
    }
};
template<std::floating_point T> struct CommandResultInitialValue<T> {
    static T get() { return std::numeric_limits<T>::quiet_NaN(); }
};
template<class T> requires (std::is_enum_v<T> &&
    requires { core::translation::FormatEnumTraits<std::remove_cv_t<T>>::members; })
struct CommandResultInitialValue<T> {
    static T get() {
        constexpr auto& members = core::translation::FormatEnumTraits<std::remove_cv_t<T>>::members;
        static_assert(std::size(members) != 0, "A source enum initial value requires its first declared member");
        return members[0].value;
    }
};
template<class T> struct CommandResultInitialValue<T*> {
    static T* get() { return nullptr; }
};
template<> struct CommandResultInitialValue<std::nullptr_t> {
    static std::nullptr_t get() { return nullptr; }
};
template<> struct CommandResultInitialValue<nicxlive::core::math::Vec2> {
    static nicxlive::core::math::Vec2 get() {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan};
    }
};
template<> struct CommandResultInitialValue<nicxlive::core::math::Vec3> {
    static nicxlive::core::math::Vec3 get() {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan};
    }
};
template<> struct CommandResultInitialValue<nicxlive::core::math::Vec4> {
    static nicxlive::core::math::Vec4 get() {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        return {nan, nan, nan, nan};
    }
};
template<> struct CommandResultInitialValue<nicxlive::core::math::Vec2i> {
    static nicxlive::core::math::Vec2i get() { return {}; }
};
template<> struct CommandResultInitialValue<nicxlive::core::math::Vec4i> {
    static nicxlive::core::math::Vec4i get() { return {}; }
};
template<class T> struct CommandResultInitialValue<std::shared_ptr<T>> {
    static std::shared_ptr<T> get() { return {}; }
};
template<class T> struct CommandResultInitialValue<compat::Slice<T>> {
    static compat::Slice<T> get() { return {}; }
};
template<class T> struct CommandResultInitialValue<nicxlive::core::common::SharedArray<T>> {
    static nicxlive::core::common::SharedArray<T> get() { return {}; }
};
template<class Key, class Value, class Hash, class Equal>
struct CommandResultInitialValue<nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, Equal>> {
    static nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, Equal> get() { return {}; }
};
template<class Result, class... Args> struct CommandResultInitialValue<compat::SharedDelegate<Result(Args...)>> {
    static compat::SharedDelegate<Result(Args...)> get() { return {}; }
};
template<> struct CommandResultInitialValue<compat::RetainedString> {
    static compat::RetainedString get() { return {}; }
};
template<> struct CommandResultInitialValue<compat::RetainedStringSlice> {
    static compat::RetainedStringSlice get() { return {}; }
};
template<> struct CommandResultInitialValue<core::settings::SettingsString> {
    static core::settings::SettingsString get() { return {}; }
};
template<> struct CommandResultInitialValue<nicxlive::core::common::NullableString> {
    static nicxlive::core::common::NullableString get() { return {}; }
};
template<> struct CommandResultInitialValue<core::settings::SettingsJsonValue> {
    static core::settings::SettingsJsonValue get();
};
}

template<class T> class ExCommandResultImpl : public CommandResult {
public:
    T result;
    explicit ExCommandResultImpl(bool success, T payload = detail::CommandResultInitialValue<T>::get(),
        compat::RetainedString text = compat::RetainedString::emptyLiteral())
        : CommandResult(success, std::move(text)), result(payload) {}
    static std::shared_ptr<ExCommandResultImpl> opCall(bool success,
        T payload = detail::CommandResultInitialValue<T>::get(),
        compat::RetainedString text = compat::RetainedString::emptyLiteral()) {
        return detail::allocateCommandResult<ExCommandResultImpl>(success, payload, std::move(text));
    }
protected:
    void prepareSourceClassInfo() override {
        CommandResult::prepareSourceClassInfo();
        detail::prepareCommandResultClass<ExCommandResultImpl>();
    }
};

template<class T> using ExCommandResult =
    std::conditional_t<std::derived_from<T, CommandResult>, T, ExCommandResultImpl<T>>;

template<class R> class CreateResult : public CommandResult {
public:
    compat::Slice<R> created;
    explicit CreateResult(bool success, compat::Slice<R> values = {},
                          compat::RetainedString text = compat::RetainedString::emptyLiteral())
        : CommandResult(success, std::move(text)) {
        if (!values.isNull()) created = values;
    }
    static std::shared_ptr<CreateResult> opCall(bool success, compat::Slice<R> values = {},
        compat::RetainedString text = compat::RetainedString::emptyLiteral()) {
        return detail::allocateCommandResult<CreateResult>(success, std::move(values), std::move(text));
    }
protected:
    void prepareSourceClassInfo() override {
        CommandResult::prepareSourceClassInfo();
        detail::prepareCommandResultClass<CreateResult>();
    }
};

template<class R> class DeleteResult : public CommandResult {
public:
    compat::Slice<R> deleted;
    explicit DeleteResult(bool success, compat::Slice<R> values = {},
                          compat::RetainedString text = compat::RetainedString::emptyLiteral())
        : CommandResult(success, std::move(text)) {
        if (!values.isNull()) deleted = values;
    }
    static std::shared_ptr<DeleteResult> opCall(bool success, compat::Slice<R> values = {},
        compat::RetainedString text = compat::RetainedString::emptyLiteral()) {
        return detail::allocateCommandResult<DeleteResult>(success, std::move(values), std::move(text));
    }
protected:
    void prepareSourceClassInfo() override {
        CommandResult::prepareSourceClassInfo();
        detail::prepareCommandResultClass<DeleteResult>();
    }
};

template<class R> class LoadResult : public CommandResult {
public:
    compat::Slice<R> loaded;
    explicit LoadResult(bool success, compat::Slice<R> values = {},
                        compat::RetainedString text = compat::RetainedString::emptyLiteral())
        : CommandResult(success, std::move(text)) {
        if (!values.isNull()) loaded = values;
    }
    static std::shared_ptr<LoadResult> opCall(bool success, compat::Slice<R> values = {},
        compat::RetainedString text = compat::RetainedString::emptyLiteral()) {
        return detail::allocateCommandResult<LoadResult>(success, std::move(values), std::move(text));
    }
protected:
    void prepareSourceClassInfo() override {
        CommandResult::prepareSourceClassInfo();
        detail::prepareCommandResultClass<LoadResult>();
    }
};

class Command {
public:
    virtual ~Command() = default;
    virtual std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) = 0;
    virtual compat::RetainedString label() = 0;
    virtual compat::RetainedString description() = 0;
    virtual bool runnable(std::shared_ptr<Context> context) = 0;
    virtual bool shortcutRunnable() = 0;
    virtual bool mcpExposed() = 0;
    virtual CommandGuiDisplay guiDisplay() = 0;
    virtual CommandIrreversibleEffect irreversibleEffect() = 0;

    // Inherited Source Object methods follow the existing interface slots.
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Command> other);
    virtual int opCmp(std::shared_ptr<Command> other);
    compat::RetainedString sourceClassInfoName();
protected:
    virtual void prepareSourceClassInfo();
};

bool ngCommandObjectEquals(std::shared_ptr<Command> left, std::shared_ptr<Command> right);

namespace detail {
void registerCommandMeta(std::type_index key, CommandMetadata metadata);
CommandMetadata lookupCommandMeta(const Command& command);
}
template<std::derived_from<Command> C> void ngRegisterCommandMeta(const std::shared_ptr<C>& command) {
    // Internal type identity only; RTTI names never cross a persistence/API boundary.
    const auto receiver = compat::ngRequireObject(command);
    const auto key = std::type_index(typeid(*receiver));
    auto metadata = ngBuildCommandMetadata<C>();
    detail::registerCommandMeta(key, std::move(metadata));
}
bool ngIsPresentationCommand(const std::shared_ptr<Command>& command);
bool ngCommandAllowedInCurrentContext(std::shared_ptr<Command> command);
std::shared_ptr<CommandResult> ngRunCommand(std::shared_ptr<Command> command, std::shared_ptr<Context> context);

// Generated fields live in Storage, constructed before the ExCommand body.
// Retained descriptors preserve D null, empty and returned byte alias identity.
// The original zero-parameter ExCommand!() generates no argument members.
struct CommandNoArguments {};
template<> struct CommandFields<CommandNoArguments> {
    static constexpr auto descriptors = std::tuple{};
};

template<class Storage> class ExCommand;

// Authored constructor bodies only. The defining Storage remains responsible
// for its field declarations and their Source .init values before these bodies.
template<class Storage> struct SourceExCommandConstructorProgram {
    using String = compat::RetainedString;
    static constexpr auto argumentCount = std::tuple_size_v<decltype(CommandFields<Storage>::descriptors)>;

    static consteval bool defaultSupported() { return argumentCount != 0; }
    static consteval bool pairSupported() { return ngCommandInitializationSupported<String, String&>(); }
    static consteval bool descriptionSupported() { return argumentCount == 0 && pairSupported(); }
    template<class... Args> static consteval bool preferredSupported() {
        if constexpr (argumentCount == 0 || sizeof...(Args) != argumentCount) return false;
        else return pairSupported() &&
            (ngCommandParameterCopySupported<std::decay_t<Args>, Args&&>() && ...) &&
            ngCommandFieldsInitializationSupported<Storage, std::decay_t<Args>&...>();
    }
    static void defaultInitialize(ExCommand<Storage>&) requires(defaultSupported()) {}
    static void description(ExCommand<Storage>& target, String& desc) requires(descriptionSupported()) {
        SourceCommandInitialization<String, String&>::construct(target._desc, desc);
        auto empty = String::emptyLiteral();
        SourceCommandInitialization<String, String&>::construct(target._label, empty);
    }
    static void pair(ExCommand<Storage>& target, String& label, String& desc) requires(pairSupported()) {
        auto& actual = [&]() -> String& {
            if constexpr (argumentCount == 0) return label.isNull() ? desc : label;
            else return label;
        }();
        SourceCommandInitialization<String, String&>::construct(target._label, actual);
        SourceCommandInitialization<String, String&>::construct(target._desc, desc);
    }
    template<class... Args> requires(pairSupported() &&
        ngCommandFieldsInitializationSupported<Storage, Args&...>())
    static void preferred(ExCommand<Storage>& target, String& label, String& desc, Args&... args) {
        auto& actual = label.isNull() ? desc : label;
        SourceCommandInitialization<String, String&>::construct(target._label, actual);
        SourceCommandInitialization<String, String&>::construct(target._desc, desc);
        ngInitializeCommandFields(static_cast<Storage&>(target), args...);
    }
};

template<class Storage = CommandNoArguments> class ExCommand : public Storage, public Command {
    static constexpr auto argumentCount = std::tuple_size_v<decltype(CommandFields<Storage>::descriptors)>;
public:
    compat::RetainedString _label;
    compat::RetainedString _desc;
    template<class W> using _unwrapType = typename detail::UnwrapCommandType<W>::type;
    template<class... W> using unwrapTypes = std::tuple<_unwrapType<W>...>;
    using SourceConstructorProgram = SourceExCommandConstructorProgram<Storage>;
    ~ExCommand() override = 0;
    ExCommand() requires(SourceConstructorProgram::defaultSupported()) {
        SourceConstructorProgram::defaultInitialize(*this);
    }
    explicit ExCommand(compat::RetainedString desc) requires(SourceConstructorProgram::descriptionSupported()) {
        SourceConstructorProgram::description(*this, desc);
    }
    ExCommand(compat::RetainedString label, compat::RetainedString desc)
        requires(SourceConstructorProgram::pairSupported()) {
        SourceConstructorProgram::pair(*this, label, desc);
    }
    template<class... Args> requires(SourceConstructorProgram::template preferredSupported<Args...>())
    ExCommand(compat::RetainedString label, compat::RetainedString desc, Args&&... args) {
        compat::SourceCallArguments<std::decay_t<Args>...> parameters{
            {SourceCommandParameterCopy<std::decay_t<Args>, Args&&>::copy(std::forward<Args>(args))}...};
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            // Bind the actual frame values as named lvalues in the Source body.
            auto named = std::forward_as_tuple(compat::ngSourceCallArgument<I>(parameters)...);
            SourceConstructorProgram::preferred(*this, label, desc, std::get<I>(named)...);
        }(std::index_sequence_for<Args...>{});
    }
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override { return CommandResult::opCall(true); }
    compat::RetainedString label() override { return !_label.empty() ? _label : _desc; }
    compat::RetainedString description() override { return _desc; }
    bool runnable(std::shared_ptr<Context>) override { return true; }
    bool shortcutRunnable() override { return detail::lookupCommandMeta(*this).shortcutRunnable; }
    bool mcpExposed() override { return detail::lookupCommandMeta(*this).mcpExposed; }
    CommandGuiDisplay guiDisplay() override { return detail::lookupCommandMeta(*this).guiDisplay; }
    CommandIrreversibleEffect irreversibleEffect() override { return detail::lookupCommandMeta(*this).irreversibleEffect; }
    using ArgMeta = CommandArgMeta;
    static compat::Slice<ArgMeta> reflectArgMeta() { return ngReflectCommandArgMeta<Storage>(); }
protected:
    void prepareSourceClassInfo() override {
        Command::prepareSourceClassInfo();
        using Self = ExCommand<Storage>;
        using Names = nicxlive::core::d_runtime::SourceObjectTypeNames<Self>;
        if constexpr ((requires { Names::className(); } || requires { Names::className; }) &&
                (requires { Names::templateArgumentName(); } || requires { Names::templateArgumentName; })) {
            detail::prepareCommandResultClass<Self>();
        } else if (const auto name = nicxlive::core::d_runtime::ngFindObjectClassName(
                typeid(nicxlive::core::d_runtime::SourceObjectTypeIdentity<Self>))) {
            try {
                nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Self), *name);
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }
    }
};

template<class Storage> ExCommand<Storage>::~ExCommand() = default;

} // namespace nijigenerate::commands

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::Command> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.base.Command)";
    inline static constexpr char unqualifiedArgument[] = "(Command)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.base.Command";
    inline static constexpr char unqualifiedElement[] = "Command";
    NG_SOURCE_TYPE_DIAGNOSTIC_MEMBERS("Command")
};

template<> struct SourceTypeSpellings<std::shared_ptr<::nijigenerate::commands::Command>>
    : SourceTypeSpellings<::nijigenerate::commands::Command> {};

template<> struct SourceObjectTypeNames<::nijigenerate::commands::ExCommand<
        ::nijigenerate::commands::CommandNoArguments>> {
    inline static constexpr char className[] = "nijigenerate.commands.base.ExCommand!().ExCommand";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.base.ExCommand!())";
};

template<> struct SourceTypeSpellings<::nijigenerate::commands::ExCommand<
        ::nijigenerate::commands::CommandNoArguments>> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.base.ExCommand!())";
    inline static constexpr char unqualifiedArgument[] = "(ExCommand!())";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.base.ExCommand!()";
    inline static constexpr char unqualifiedElement[] = "ExCommand!()";
    NG_SOURCE_TYPE_DIAGNOSTIC_MEMBERS("ExCommand!()")
};

template<> struct SourceTypeSpellings<::nijigenerate::commands::CommandResult> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.commands.base.CommandResult)"),
            RetainedString::fromStaticLiteral("(CommandResult)"),
            RetainedString::fromStaticLiteral("nijigenerate.commands.base.CommandResult"),
            RetainedString::fromStaticLiteral("CommandResult")};
    }
};

template<class T> struct SourceTypeSpellings<::nijigenerate::commands::ExCommandResultImpl<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        if (const auto names = ::nijigenerate::commands::detail::commandResultCachedSourceSpellings<
                ::nijigenerate::commands::ExCommandResultImpl<T>>()) return *names;
        return ::nijigenerate::commands::detail::commandResultSourceSpellings<T>("ExCommandResultImpl");
    }
};

template<class R> struct SourceTypeSpellings<::nijigenerate::commands::CreateResult<R>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        if (const auto names = ::nijigenerate::commands::detail::commandResultCachedSourceSpellings<
                ::nijigenerate::commands::CreateResult<R>>()) return *names;
        return ::nijigenerate::commands::detail::commandResultSourceSpellings<R>("CreateResult");
    }
};

template<class R> struct SourceTypeSpellings<::nijigenerate::commands::DeleteResult<R>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        if (const auto names = ::nijigenerate::commands::detail::commandResultCachedSourceSpellings<
                ::nijigenerate::commands::DeleteResult<R>>()) return *names;
        return ::nijigenerate::commands::detail::commandResultSourceSpellings<R>("DeleteResult");
    }
};

template<class R> struct SourceTypeSpellings<::nijigenerate::commands::LoadResult<R>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        if (const auto names = ::nijigenerate::commands::detail::commandResultCachedSourceSpellings<
                ::nijigenerate::commands::LoadResult<R>>()) return *names;
        return ::nijigenerate::commands::detail::commandResultSourceSpellings<R>("LoadResult");
    }
};

template<> struct SourceObjectTypeNames<::nijigenerate::commands::CommandResult> {
    inline static constexpr char className[] = "nijigenerate.commands.base.CommandResult";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.base.CommandResult)";
};

template<class T> struct SourceObjectTypeNames<::nijigenerate::commands::ExCommandResultImpl<T>> {
    static RetainedString className() {
        using Self = ::nijigenerate::commands::ExCommandResultImpl<T>;
        if (const auto name = ngFindObjectClassName(typeid(SourceObjectTypeIdentity<Self>))) return *name;
        return ::nijigenerate::commands::detail::commandResultSourceName<T>("ExCommandResultImpl", false);
    }
    static RetainedString templateArgumentName() {
        using Self = ::nijigenerate::commands::ExCommandResultImpl<T>;
        if (const auto name = ngFindObjectTemplateArgumentName(typeid(SourceObjectTypeIdentity<Self>))) return *name;
        return ::nijigenerate::commands::detail::commandResultSourceName<T>("ExCommandResultImpl", true);
    }
};
template<class R> struct SourceObjectTypeNames<::nijigenerate::commands::CreateResult<R>> {
    static RetainedString className() {
        using Self = ::nijigenerate::commands::CreateResult<R>;
        if (const auto name = ngFindObjectClassName(typeid(SourceObjectTypeIdentity<Self>))) return *name;
        return ::nijigenerate::commands::detail::commandResultSourceName<R>("CreateResult", false);
    }
    static RetainedString templateArgumentName() {
        using Self = ::nijigenerate::commands::CreateResult<R>;
        if (const auto name = ngFindObjectTemplateArgumentName(typeid(SourceObjectTypeIdentity<Self>))) return *name;
        return ::nijigenerate::commands::detail::commandResultSourceName<R>("CreateResult", true);
    }
};
template<class R> struct SourceObjectTypeNames<::nijigenerate::commands::DeleteResult<R>> {
    static RetainedString className() {
        using Self = ::nijigenerate::commands::DeleteResult<R>;
        if (const auto name = ngFindObjectClassName(typeid(SourceObjectTypeIdentity<Self>))) return *name;
        return ::nijigenerate::commands::detail::commandResultSourceName<R>("DeleteResult", false);
    }
    static RetainedString templateArgumentName() {
        using Self = ::nijigenerate::commands::DeleteResult<R>;
        if (const auto name = ngFindObjectTemplateArgumentName(typeid(SourceObjectTypeIdentity<Self>))) return *name;
        return ::nijigenerate::commands::detail::commandResultSourceName<R>("DeleteResult", true);
    }
};
template<class R> struct SourceObjectTypeNames<::nijigenerate::commands::LoadResult<R>> {
    static RetainedString className() {
        using Self = ::nijigenerate::commands::LoadResult<R>;
        if (const auto name = ngFindObjectClassName(typeid(SourceObjectTypeIdentity<Self>))) return *name;
        return ::nijigenerate::commands::detail::commandResultSourceName<R>("LoadResult", false);
    }
    static RetainedString templateArgumentName() {
        using Self = ::nijigenerate::commands::LoadResult<R>;
        if (const auto name = ngFindObjectTemplateArgumentName(typeid(SourceObjectTypeIdentity<Self>))) return *name;
        return ::nijigenerate::commands::detail::commandResultSourceName<R>("LoadResult", true);
    }
};
}
