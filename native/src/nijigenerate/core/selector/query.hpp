#pragma once

#include "parser.hpp"
#include "parse_uint.hpp"
#include "resource_proxy.hpp"
#include "nijigenerate/compat/retained_string_slice.hpp"
#include "nijigenerate/compat/source_c_string.hpp"
#include "nijigenerate/compat/source_comparison_operator.hpp"
#include "nijigenerate/compat/translated_source_expression.hpp"
#include "nijigenerate/core/typed_format.hpp"
#include <serialization_fields.hpp>
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include "nijigenerate/compat/assert_error.hpp"
#include "nijigenerate/compat/range_error.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include "nijigenerate/compat/shared_delegate.hpp"
#include <resource.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <typeinfo>
#include <functional>
#include <memory>
#include <new>
#include <source_location>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace nijigenerate::ext { class ExParameterGroup; }

namespace nijigenerate::core::selector {
class ResourceCache : public std::enable_shared_from_this<ResourceCache> {
    struct UuidHash {
        std::size_t operator()(std::uint32_t value) const noexcept { return value; }
    };
public:
    using CacheMap = nicxlive::core::common::ResourceKeyMap<std::uint32_t, ResourcePtr,
        UuidHash, std::equal_to<std::uint32_t>>;
    ResourceCache() = default;
    virtual ~ResourceCache() = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(std::shared_ptr<ResourceCache> other);
    virtual bool opEquals(std::shared_ptr<ResourceCache> other);
    ResourceCache(const ResourceCache&) = delete;
    ResourceCache& operator=(const ResourceCache&) = delete;
    CacheMap cache;
    using Callback = nijigenerate::compat::SharedDelegate<void(ResourcePtr)>;

    template<class T> ResourcePtr create(std::shared_ptr<T> obj, Callback callback = {}) {
        [[maybe_unused]] const auto owner = weak_from_this().lock();
        return createOn(this, std::move(obj), std::move(callback));
    }
    // Direct Source template methods may receive null without dereferencing it.
    // Keep that receiver alive while invoking the same actual method body.
    template<class T> static ResourcePtr createSource(std::shared_ptr<ResourceCache> receiver,
            std::shared_ptr<T> obj, Callback callback = {}) {
        return createOn(receiver.get(), std::move(obj), std::move(callback));
    }
private:
    template<std::derived_from<Node> T>
    static ResourcePtr createOn(ResourceCache* receiver, std::shared_ptr<T> obj, Callback callback) {
        const auto firstKey = compat::ngRequireObject(obj)->getUUID();
        auto& firstTable = compat::ngRequireObject(receiver)->cache;
        if (firstTable.contains(firstKey))
            return cachedValue(receiver->cache, [&] { return obj->getUUID(); }, 28);
        auto& insertionTable = receiver->cache;
        const auto insertionKey = obj->getUUID();
        const auto insertionValue = std::allocate_shared<Proxy<T>>(
            nicxlive::core::d_runtime::SourceAllocator<Proxy<T>>{}, obj);
        cacheSlot(insertionTable, insertionKey) = insertionValue;
        if (callback) callback(cachedValue(receiver->cache, [&] { return obj->getUUID(); }, 31));
        return cachedValue(receiver->cache, [&] { return obj->getUUID(); }, 32);
    }
    template<std::derived_from<Parameter> T>
    static ResourcePtr createOn(ResourceCache*, std::shared_ptr<T> obj, Callback callback) {
        const auto result = std::allocate_shared<Proxy<Parameter>>(
            nicxlive::core::d_runtime::SourceAllocator<Proxy<Parameter>>{}, obj);
        if (callback) callback(result);
        return result;
    }
    template<std::derived_from<ParameterBinding> T>
    static ResourcePtr createOn(ResourceCache*, std::shared_ptr<T> obj, Callback callback) {
        const auto result = std::allocate_shared<Proxy<ParameterBinding>>(
            nicxlive::core::d_runtime::SourceAllocator<Proxy<ParameterBinding>>{}, obj);
        if (callback) callback(result);
        return result;
    }
    template<std::derived_from<Resource> T>
    static ResourcePtr createOn(ResourceCache* receiver, std::shared_ptr<T> obj, Callback callback) {
        (void)callback;
        compat::ngRequireObject(obj);
        if (obj->type() == ResourceType::Parameter || obj->type() == ResourceType::Binding) return obj;
        const auto firstKey = obj->uuid();
        auto& firstTable = compat::ngRequireObject(receiver)->cache;
        if (firstTable.contains(firstKey)) return cachedValue(receiver->cache, [&] { return obj->uuid(); }, 49);
        auto& insertionTable = receiver->cache;
        const auto insertionKey = obj->uuid();
        cacheSlot(insertionTable, insertionKey) = obj;
        return obj;
    }
private:
    static CacheMap::MappedReference cacheSlot(CacheMap& table, std::uint32_t key,
            std::source_location source = std::source_location::current()) {
        try {
            return table[key];
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(source);
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(source);
        }
    }
    template<class ReadKey>
    static ResourcePtr cachedValue(CacheMap& table, ReadKey readKey,
            std::size_t line) {
        // Retain the actual AA field address; its value is read after the key.
        const auto key = readKey();
        const auto found = table.find(key);
        if (found == table.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/core/selector/query.d"), line);
        return found->second;
    }
};

template<class T> class Processor;

namespace detail {
std::shared_ptr<ext::ExParameterGroup>& ngQueryDummyRoot();
template<class T> std::shared_ptr<T> ngAllocateQueryFrame() {
    try {
        return std::make_shared<T>();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline void ngAppendQueryResource(SelectorArray<ResourcePtr>& result, const ResourcePtr& value) {
    try {
        result.push_back(value);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
}
compat::RetainedString ngProcessorObjectClassName(const std::type_info& type);
[[noreturn]] void ngFailProcessorObjectComparison(const std::type_info& type);
void ngRegisterQueryTemplateClass(const std::type_info& type, std::string_view declaration,
    compat::RetainedString arguments);
compat::RetainedString ngQueryTemplateName(std::string_view declaration,
    compat::RetainedString arguments, bool typeArgument);
compat::RetainedString ngQueryStringArgument(std::string_view text);
compat::RetainedString ngQueryConstBasicArgument(compat::RetainedString element, bool argument);
nicxlive::core::d_runtime::SourceTypeSpellingNames ngQueryClassTypeSpellings(
    std::string_view qualifiedElement, std::string_view unqualifiedElement);
nicxlive::core::d_runtime::SourceTypeSpellingNames ngQueryTemplateTypeSpellings(
    std::string_view declaration, compat::RetainedString arguments);
compat::RetainedString ngQueryFilterArguments(std::string_view name, compat::RetainedString type,
    compat::RetainedString op);
compat::RetainedString ngQueryWalkerArguments(compat::RetainedString source,
    compat::RetainedString target, bool direct);

template<class T> struct QuerySourceType { using Type = T; };
// The declaring origin proves that this is a class reference. Unwrapping its
// native handle must retain const on the pointee for the qualified producer.
template<class T> requires (!std::is_volatile_v<T> &&
    nicxlive::core::d_runtime::detail::sourceNativeObjectOrigin<std::remove_const_t<T>>)
struct QuerySourceType<std::shared_ptr<T>> { using Type = T; };
template<class T> using QuerySourceValue = std::conditional_t<!std::is_volatile_v<T> &&
    (std::derived_from<std::remove_const_t<T>, Resource> ||
        nicxlive::core::d_runtime::detail::sourceNativeObjectOrigin<std::remove_const_t<T>>),
    std::shared_ptr<T>, T>;

template<class Key, class Value, class Hash, class Equal, class Actual>
bool ngQueryAssociativeContains(
        const nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, Equal>& table,
        const Actual& actual) {
    constexpr bool stringKey = std::is_same_v<Key, ResourceString> ||
        std::is_same_v<Key, compat::RetainedString> || std::is_same_v<Key, compat::RetainedStringSlice>;
    constexpr bool stringActual = std::is_same_v<Actual, ResourceString> ||
        std::is_same_v<Actual, compat::RetainedString> || std::is_same_v<Actual, compat::RetainedStringSlice>;
    if constexpr (stringKey && stringActual) {
        const auto text = [&] {
            if constexpr (std::is_same_v<Actual, compat::RetainedStringSlice>) return actual.retainedString();
            else return compat::RetainedString(actual);
        }();
        if constexpr (std::is_same_v<Key, ResourceString>) return table.contains(text.toNullableString());
        else if constexpr (std::is_same_v<Key, compat::RetainedStringSlice>)
            return table.contains(compat::RetainedStringSlice(text));
        else return table.contains(text);
    } else return table.contains(actual);
}

template<class L, class R> bool ngQueryScalarIdentity(const L& left, R&& right) {
    if constexpr (std::is_arithmetic_v<L> && std::is_arithmetic_v<std::remove_cvref_t<R>>) {
        using Common = std::common_type_t<L, std::remove_cvref_t<R>>;
        if constexpr (std::is_same_v<Common, float> || std::is_same_v<Common, double>) {
            using Bits = std::conditional_t<sizeof(Common) == sizeof(std::uint32_t),
                std::uint32_t, std::uint64_t>;
            return std::bit_cast<Bits>(static_cast<Common>(left)) ==
                std::bit_cast<Bits>(static_cast<Common>(right));
        } else if constexpr (std::is_floating_point_v<Common>) {
            static_assert(sizeof(Common) == 0, "Source real identity requires its compiled numeric binding");
        } else return left == right;
    } else static_assert(sizeof(L) == 0, "Source identity requires a ResourceFilterExpression binding");
}

template<class T> compat::RetainedString ngQueryTypeArgument(
        nicxlive::core::d_runtime::SourceTypeSpelling context =
            nicxlive::core::d_runtime::SourceTypeSpelling::UnqualifiedElement) {
    using Context = nicxlive::core::d_runtime::SourceTypeSpelling;
    using Source = typename QuerySourceType<T>::Type;
    const bool argument = context == Context::QualifiedArgument || context == Context::UnqualifiedArgument;
    const bool qualified = context == Context::QualifiedArgument || context == Context::QualifiedElement;
    if constexpr (std::is_same_v<Source, std::nullptr_t>)
        return argument ? compat::RetainedString::fromStaticLiteral("(typeof(null))")
            : compat::RetainedString::fromStaticLiteral("typeof(null)");
    else if constexpr (std::is_const_v<Source> && !std::is_volatile_v<Source> &&
            (std::is_arithmetic_v<std::remove_const_t<Source>> || std::is_void_v<std::remove_const_t<Source>> ||
                std::is_enum_v<std::remove_const_t<Source>>))
        return ngQueryConstBasicArgument(ngQueryTypeArgument<std::remove_const_t<Source>>(
            qualified ? Context::QualifiedElement : Context::UnqualifiedElement), argument);
    else return nicxlive::core::d_runtime::ngSourceTypeSpelling<Source>(context);
}

template<class Class, class BuildArguments>
void ngPrepareQueryTemplateClass(std::string_view declaration, BuildArguments buildArguments) {
    static const bool registered = [&] {
        using Names = nicxlive::core::d_runtime::SourceObjectTypeNames<Class>;
        if constexpr (requires { Names::className(); Names::templateArgumentName(); }) {
            const auto name = compat::RetainedString(Names::className());
            nicxlive::core::d_runtime::ngRegisterObjectTypeNames<Class>(name,
                compat::RetainedString(Names::templateArgumentName()));
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class), name);
        } else if constexpr (requires { Names::className(); })
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class),
                compat::RetainedString(Names::className()));
        else if constexpr (requires { Names::className; })
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class),
                compat::RetainedString::fromStaticLiteral(Names::className));
        else ngRegisterQueryTemplateClass(typeid(Class), declaration, buildArguments());
        return true;
    }();
    (void)registered;
}

}

} // namespace nijigenerate::core::selector

namespace nicxlive::core::d_runtime {
namespace ng_query_source = ::nijigenerate::core::selector;
template<class T> struct SourceObjectTypeNames<ng_query_source::Processor<T>> {
    static RetainedString className();
    static RetainedString templateArgumentName();
};
}

namespace nijigenerate::core::selector {

template<class T> class Processor : public std::enable_shared_from_this<Processor<T>> {
    // A genuine Source class denotes a reference; value types retain their
    // actual element type. Native shared handles are already reference values.
    using Element = detail::QuerySourceValue<T>;
protected:
    virtual void prepareSourceClassInfo() {
        detail::ngPrepareQueryTemplateClass<Processor<T>>("Processor", [] {
            return detail::ngQueryTypeArgument<T>(
                nicxlive::core::d_runtime::SourceTypeSpelling::UnqualifiedArgument);
        });
    }
public:
    Processor() = default;
    virtual ~Processor() = default;
    virtual compat::RetainedString toString() {
        [[maybe_unused]] const auto owner = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return detail::ngProcessorObjectClassName(typeid(*this));
    }
    virtual std::size_t toHash() {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<Processor<T>> other) {
        return dynamic_cast<const void*>(this) == nicxlive::core::common::ngClassObjectIdentity(other);
    }
    virtual int opCmp(std::shared_ptr<Processor<T>>) {
        [[maybe_unused]] const auto owner = this->weak_from_this().lock();
        prepareSourceClassInfo();
        detail::ngFailProcessorObjectComparison(typeid(*this));
    }
    Processor(const Processor&) = delete;
    Processor& operator=(const Processor&) = delete;
    virtual SelectorArray<Element> process(SelectorArray<Element> targets) {
        (void)targets;
        SelectorArray<Element> result;
        return result;
    }
};
using ResourceProcessor = Processor<Resource>;
using ResourceArray = SelectorArray<ResourcePtr>;

class MarkerProcessor : public ResourceProcessor {
public:
    ResourceArray process(ResourceArray targets) override {
        for (auto target : targets) {
            if (!target) compat::ngFailNullAccess(true);
            target->explicitSelection = true;
        }
        return targets;
    }
};

inline compat::RetainedStringSlice AttrComparison(compat::RetainedStringSlice name,
    compat::RetainedStringSlice op) {
    try {
        std::string result = "target.";
        if (!name.empty()) result.append(name.view());
        if (!op.empty()) result.append(op.view());
        result.append(" value");
        return compat::RetainedStringSlice(compat::RetainedString(std::move(result)));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<std::size_t N, class Identity = void> struct SelectorLiteral {
    using SourceIdentity = Identity;
    char text[N];
    constexpr SelectorLiteral(const char (&value)[N]) { std::copy_n(value, N, text); }
    constexpr std::string_view view() const { return {text, N - 1}; }
};
template<class Identity> struct SelectorAliasArgument;

namespace detail {
template<SelectorLiteral op> constexpr std::string_view ngQueryComparisonToken() {
    const auto parsed = compat::ngParseSourceComparisonFragment<sizeof(op.text) * 4>(op.view());
    if (parsed.kind == compat::SourceComparisonOperator::None) return op.view();
    return compat::ngSourceComparisonOperatorSpelling(parsed.kind);
}
template<SelectorLiteral op> compat::RetainedString ngQueryOperatorArgument() {
    using Identity = typename std::remove_cvref_t<decltype(op)>::SourceIdentity;
    if constexpr (std::is_void_v<Identity>) return ngQueryStringArgument(op.view());
    else {
        using Argument = SelectorAliasArgument<Identity>;
        if constexpr (requires { Argument::get(); }) return compat::RetainedString(Argument::get());
        else return compat::RetainedString::fromStaticLiteral(Argument::value);
    }
}
}
template<SelectorLiteral name, class T, SelectorLiteral op> class ResourceAttrFilter;

namespace detail {
template<SelectorLiteral name, SelectorLiteral op> consteval auto ngQueryFilterExpressionText() {
    constexpr bool nameSpecialization = name.view() == "name";
    constexpr std::string_view prefix = nameSpecialization ?
        "(target.name.toStringz).fromStringz " : "target.";
    constexpr std::string_view suffix = " value";
    constexpr std::size_t nameLength = nameSpecialization ? 0 : name.view().size();
    char text[prefix.size() + nameLength + op.view().size() + suffix.size() + 1]{};
    std::size_t offset = 0;
    for (const char byte : prefix) text[offset++] = byte;
    if constexpr (!nameSpecialization) for (const char byte : name.view()) text[offset++] = byte;
    for (const char byte : op.view()) text[offset++] = byte;
    for (const char byte : suffix) text[offset++] = byte;
    return SelectorLiteral{text};
}
}

// Declaration-side translation of the actual Source method. The native body
// is an implementation detail, never another Source class-template argument.
// A complete process body can provide genuine closure cells/owners or avoid
// evaluating an operand in an unevaluated Source context. Metadata alone and
// a bool feasibility flag cannot instantiate this descriptor as a body.
template<SelectorLiteral name, class T, SelectorLiteral op, auto NativeProcess>
struct TranslatedResourceFilterExpression {
    using Filter = ResourceAttrFilter<name, T, op>;
    using SourceValueType = T;
    inline static constexpr auto sourceAttribute = name;
    inline static constexpr auto sourceOperation = op;
    inline static constexpr auto sourceExpression = detail::ngQueryFilterExpressionText<name, op>();
    inline static constexpr std::string_view sourceModule = "nijigenerate.core.selector.query";

    static ResourceArray process(Filter& filter, ResourceArray targets) {
        static_assert(std::is_invocable_v<decltype(NativeProcess), Filter&, ResourceArray>,
            "A translated filter body must accept its real receiver and Source array value");
        if constexpr (std::is_invocable_v<decltype(NativeProcess), Filter&, ResourceArray>) {
            static_assert(std::same_as<std::invoke_result_t<decltype(NativeProcess), Filter&, ResourceArray>, ResourceArray>,
                "A translated filter body must return the actual Source Resource array");
            using Body = compat::TranslatedSourceExpression<TranslatedResourceFilterExpression, NativeProcess>;
            return Body::invoke(filter, std::move(targets));
        }
    }
};

// A translated Source mixin can bind any compiled expression to these exact
// arguments. Bindings use the real filter and foreach local, including writes.
template<SelectorLiteral name, class T, SelectorLiteral op> struct ResourceFilterExpression {
    inline static constexpr auto sourceExpression = detail::ngQueryFilterExpressionText<name, op>();
    static ResourceArray process(ResourceAttrFilter<name, T, op>& filter, ResourceArray targets) {
        return filter.processBuiltin(std::move(targets));
    }
    static bool evaluate(ResourceAttrFilter<name, T, op>& filter, ResourcePtr& target,
        ResourceArray&, ResourceArray&)
        requires (name.view() != "name") {
        return filter.matchesBuiltin(target);
    }
    static bool evaluate(ResourceAttrFilter<name, T, op>& filter, ResourcePtr&,
        ResourceArray&, ResourceArray&, compat::RetainedStringSlice&,
        compat::RetainedStringSlice convertedName) requires (name.view() == "name") {
        return filter.compare(convertedName);
    }
};

template<SelectorLiteral name, class T, SelectorLiteral op>
class ResourceAttrFilter : public ResourceProcessor {
    friend struct ResourceFilterExpression<name, T, op>;
    static constexpr auto operation = detail::ngQueryComparisonToken<op>();
    static_assert(std::is_void_v<typename std::remove_cvref_t<decltype(name)>::SourceIdentity>,
        "Source attribute name is a string value, not a symbol alias");
public:
    using Value = detail::QuerySourceValue<T>;
    Value value;
protected:
    void prepareSourceClassInfo() override {
        ResourceProcessor::prepareSourceClassInfo();
        detail::ngPrepareQueryTemplateClass<ResourceAttrFilter>("ResourceAttrFilter", [] {
            return detail::ngQueryFilterArguments(name.view(), detail::ngQueryTypeArgument<T>(),
                detail::ngQueryOperatorArgument<op>());
        });
    }
private:
    template<class U> bool compare(const U& actual) {
        // Keep the source string descriptor in the filter; compare only its bytes.
        auto&& expected = [&]() -> decltype(auto) {
            using Value = std::remove_const_t<T>;
            if constexpr (std::is_same_v<Value, compat::RetainedStringSlice> ||
                    std::is_same_v<Value, compat::RetainedString> || std::is_same_v<Value, ResourceString>)
                return value.view();
            else return (value);
        }();
        using Expected = std::remove_cvref_t<decltype(expected)>;
        if constexpr (operation == "==" || operation == "!=") {
            static_assert(!std::is_enum_v<U> || !std::is_enum_v<Expected> || std::is_same_v<U, Expected>,
                "Source equality does not compare distinct enum declaration types");
        }
        const auto& scalarLeft = [&]() -> decltype(auto) {
            if constexpr (std::is_enum_v<U> && (std::is_arithmetic_v<Expected> || std::is_enum_v<Expected>))
                return static_cast<std::underlying_type_t<U>>(actual);
            else if constexpr (std::is_same_v<U, ResourceString> ||
                    std::is_same_v<U, compat::RetainedStringSlice> || std::is_same_v<U, compat::RetainedString>)
                return actual.view();
            else return (actual);
        }();
        auto&& scalarRight = [&]() -> decltype(auto) {
            if constexpr (std::is_enum_v<Expected> && (std::is_arithmetic_v<U> || std::is_enum_v<U>))
                return static_cast<std::underlying_type_t<Expected>>(expected);
            else return (expected);
        }();
        if constexpr (operation == "in" || operation == "!in") {
            const bool present = detail::ngQueryAssociativeContains(value, actual);
            if constexpr (operation == "in") return present;
            else return !present;
        } else if constexpr ((operation == "==" || operation == "!=") && std::is_same_v<U, ResourcePtr>) {
            const bool equal = ngSelectorResourceObjectEquals(actual, expected);
            if constexpr (operation == "==") return equal;
            else return !equal;
        } else if constexpr (std::is_same_v<U, ResourcePtr> && (operation == "<" || operation == "<=" ||
                operation == ">" || operation == ">=")) {
            const auto left = actual;
            const auto right = expected;
            const auto order = [&] {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(right)>, std::nullptr_t>)
                    return nicxlive::core::common::ngClassObjectCompare(left, nullptr);
                else return nicxlive::core::common::ngClassObjectCompare(left, ResourcePtr(right));
            }();
            if constexpr (operation == "<") return order < 0;
            else if constexpr (operation == "<=") return order <= 0;
            else if constexpr (operation == ">") return order > 0;
            else return order >= 0;
        } else if constexpr (operation == "is" || operation == "!is") {
            const bool identical = [&] {
                if constexpr (std::is_same_v<U, ResourcePtr>) {
                    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(expected)>, std::nullptr_t>)
                        return !actual;
                    else return nicxlive::core::common::ngClassObjectIdentity(actual) ==
                        nicxlive::core::common::ngClassObjectIdentity(expected);
                } else if constexpr (requires { actual.data(); actual.size(); }) {
                    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(expected)>, std::nullptr_t>)
                        return actual.data() == nullptr && actual.size() == 0;
                    else return actual.data() == expected.data() && actual.size() == expected.size();
                } else return detail::ngQueryScalarIdentity(scalarLeft, scalarRight);
            }();
            if constexpr (operation == "is") return identical;
            else return !identical;
        } else if constexpr (operation == "==") return scalarLeft == scalarRight;
        else if constexpr (operation == "!=") return scalarLeft != scalarRight;
        else if constexpr (operation == "<") return scalarLeft < scalarRight;
        else if constexpr (operation == "<=") return scalarLeft <= scalarRight;
        else if constexpr (operation == ">") return scalarLeft > scalarRight;
        else if constexpr (operation == ">=") return scalarLeft >= scalarRight;
        else static_assert(sizeof(U) == 0, "Unsupported D selector comparison operator");
    }
    template<SelectorLiteral attribute, std::size_t skip>
    static consteval auto attributeTail() {
        char text[sizeof(attribute.text) - skip]{};
        for (std::size_t i = 0; i < sizeof(text); ++i) text[i] = attribute.text[i + skip];
        return SelectorLiteral{text};
    }
    template<SelectorLiteral attribute> static auto readAttribute(ResourcePtr target) {
        const auto receiver = compat::ngRequireObject(target);
        if constexpr (attribute.view().starts_with("source."))
            return readAttribute<attributeTail<attribute, 7>()>(receiver->source);
        else if constexpr (attribute.view() == "name") return receiver->name();
        else if constexpr (attribute.view() == "uuid") return receiver->uuid();
        else if constexpr (attribute.view() == "typeId") return receiver->typeId();
        else if constexpr (attribute.view() == "type") return receiver->type();
        else if constexpr (attribute.view() == "index") return receiver->index;
        else if constexpr (attribute.view() == "explicit") return receiver->explicitSelection;
        else if constexpr (attribute.view() == "source") return receiver->source;
        else if constexpr (attribute.view() == "toString") return receiver->toString();
        else if constexpr (attribute.view() == "toHash") return receiver->toHash();
        else if constexpr (attribute.view() == "classinfo.name")
            return nicxlive::core::d_runtime::ngObjectClassName(typeid(*receiver));
        else static_assert(sizeof(T) == 0, "Source expression requires a ResourceFilterExpression binding");
    }
    bool matchesBuiltin(const ResourcePtr& target) {
        static_assert(op.view().empty() || static_cast<unsigned char>(op.view().front()) < 0x80,
            "Source identifier continuation must be resolved before builtin operator dispatch");
        static_assert((operation != "is" && operation != "in") ||
            compat::ngScanSourceLexicalTrivia(op.view()).consumed,
            "Source word operator needs a separator after the builtin attribute identifier");
        return compare(readAttribute<name>(target));
    }
public:
    // D rewrites this first constructor-field assignment to initialization.
    explicit ResourceAttrFilter(Value value_) : value(value_) {}
    ResourceArray process(ResourceArray targets) override {
        [[maybe_unused]] const auto owner = this->weak_from_this().lock();
        using Expression = ResourceFilterExpression<name, T, op>;
        using Body = TranslatedResourceFilterExpression<name, T, op, &Expression::process>;
        return Body::process(*this, std::move(targets));
    }
private:
    ResourceArray processBuiltin(ResourceArray targets) {
        ResourceArray result;
        const auto capturedTargets = targets;
        for (std::size_t index = 0; index < capturedTargets.size(); ++index) {
            auto target = capturedTargets[index];
            if (ResourceFilterExpression<name, T, op>::evaluate(*this, target, targets, result)) {
                compat::ngRequireObject(target)->index = result.size();
                try {
                    result.push_back(target);
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
                }
            }
        }
        return result;
    }
};

template<class T, SelectorLiteral op>
class ResourceAttrFilter<"name", T, op> : public ResourceProcessor {
    friend struct ResourceFilterExpression<"name", T, op>;
    static constexpr auto operation = detail::ngQueryComparisonToken<op>();
public:
    using Value = detail::QuerySourceValue<T>;
    Value value;
protected:
    void prepareSourceClassInfo() override {
        ResourceProcessor::prepareSourceClassInfo();
        detail::ngPrepareQueryTemplateClass<ResourceAttrFilter>("ResourceAttrFilter", [] {
            return detail::ngQueryFilterArguments("name", detail::ngQueryTypeArgument<T>(),
                detail::ngQueryOperatorArgument<op>());
        });
    }
private:
    static compat::RetainedStringSlice nameCString(const ResourceString& text) {
        const compat::SourceCString converted{compat::RetainedStringSlice{compat::RetainedString(text)}};
        return converted.retainedString();
    }
    bool compare(compat::RetainedStringSlice convertedName) {
        const auto actual = convertedName.view();
        auto&& expected = [&]() -> decltype(auto) {
            using Value = std::remove_const_t<T>;
            if constexpr (std::is_same_v<Value, compat::RetainedStringSlice>
                    || std::is_same_v<Value, compat::RetainedString>
                    || std::is_same_v<Value, ResourceString>) return value.view();
            else return (value);
        }();
        if constexpr (operation == "in" || operation == "!in") {
            const bool present = detail::ngQueryAssociativeContains(value, convertedName);
            if constexpr (operation == "in") return present;
            else return !present;
        } else if constexpr (operation == "==") return actual == expected;
        else if constexpr (operation == "!=") return actual != expected;
        else if constexpr (operation == "<") return actual < expected;
        else if constexpr (operation == "<=") return actual <= expected;
        else if constexpr (operation == ">") return actual > expected;
        else if constexpr (operation == ">=") return actual >= expected;
        else if constexpr (operation == "is" || operation == "!is") {
            const bool identical = [&] {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(expected)>, std::nullptr_t>)
                    return actual.data() == nullptr && actual.size() == 0;
                else return actual.data() == expected.data() && actual.size() == expected.size();
            }();
            if constexpr (operation == "is") return identical;
            else return !identical;
        } else static_assert(sizeof(T) == 0, "Unsupported D selector comparison operator");
    }
public:
    // D rewrites this first constructor-field assignment to initialization.
    explicit ResourceAttrFilter(Value value_) : value(value_) {}
    ResourceArray process(ResourceArray targets) override {
        [[maybe_unused]] const auto owner = this->weak_from_this().lock();
        using Expression = ResourceFilterExpression<"name", T, op>;
        using Body = TranslatedResourceFilterExpression<"name", T, op, &Expression::process>;
        return Body::process(*this, std::move(targets));
    }
private:
    ResourceArray processBuiltin(ResourceArray targets) {
        ResourceArray result;
        const auto capturedTargets = targets;
        for (std::size_t index = 0; index < capturedTargets.size(); ++index) {
            auto target = capturedTargets[index];
            // The unused first conversion remains alive through the second getter.
            auto tname = nameCString(compat::ngRequireObject(target)->name());
            const auto text = target->name();
            if (ResourceFilterExpression<"name", T, op>::evaluate(
                    *this, target, targets, result, tname, nameCString(text))) {
                compat::ngRequireObject(target)->index = result.size();
                try {
                    result.push_back(target);
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(
                        compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
                }
            }
        }
        return result;
    }
};
using TypeIdFilter = ResourceAttrFilter<"typeId", nijigenerate::compat::RetainedStringSlice, "==">;
using UUIDFilter = ResourceAttrFilter<"uuid", std::uint32_t, "==">;
using NameFilter = ResourceAttrFilter<"name", nijigenerate::compat::RetainedStringSlice, "==">;

// Keep the misspelled primary declaration from D; concrete walkers use ResourceWalker.
template<class S, class T, bool direct = true> class ResrouceWalker : public ResourceProcessor {
protected:
    void prepareSourceClassInfo() override {
        ResourceProcessor::prepareSourceClassInfo();
        detail::ngPrepareQueryTemplateClass<ResrouceWalker>("ResrouceWalker", [] {
            return detail::ngQueryWalkerArguments(detail::ngQueryTypeArgument<S>(),
                detail::ngQueryTypeArgument<T>(), direct);
        });
    }
};
template<class S, class T, bool direct> class ResourceWalker;

template<class S, class T, bool direct>
    requires (std::derived_from<S, Node> && std::derived_from<T, Node>)
class ResourceWalker<S, T, direct> : public ResourceProcessor {
    struct UuidHash {
        std::size_t operator()(std::uint32_t value) const noexcept { return value; }
    };
    using UuidMap = nicxlive::core::common::ResourceKeyMap<std::uint32_t, bool,
        UuidHash, std::equal_to<std::uint32_t>>;
protected:
    void prepareSourceClassInfo() override {
        ResourceProcessor::prepareSourceClassInfo();
        detail::ngPrepareQueryTemplateClass<ResourceWalker>("ResourceWalker", [] {
            return detail::ngQueryWalkerArguments(detail::ngQueryTypeArgument<S>(),
                detail::ngQueryTypeArgument<T>(), direct);
        });
    }
public:
    std::shared_ptr<ResourceCache> cache;
    explicit ResourceWalker(std::shared_ptr<ResourceCache> value) : cache(std::move(value)) {}
    ResourceArray process(ResourceArray targets) override {
        const auto owner = shared_from_this();
        const auto result = detail::ngAllocateQueryFrame<ResourceArray>();
        if constexpr (direct) {
            for (const auto t : targets) {
                if (!std::dynamic_pointer_cast<Proxy<Node>>(t)) continue;
                const auto target = to<Node>(t);
                const auto children = compat::ngRequireObject(target)->children;
                for (const auto child : children) {
                    const auto sourceCache = cache;
                    auto proxy = ResourceCache::createSource(sourceCache, child, [result, t](ResourcePtr proxy) {
                        if (!proxy) compat::ngFailNullAccess(true);
                        proxy->source = t;
                        proxy->index = result->size();
                    });
                    detail::ngAppendQueryResource(*result, proxy);
                }
            }
        } else {
            const auto uuidMap = detail::ngAllocateQueryFrame<UuidMap>();
            // The recursive function itself needs no additional erased-callable allocation.
            const auto traverse = [&](auto&& self, std::shared_ptr<Node> node, ResourcePtr source) -> void {
                if (!uuidMap->contains(compat::ngRequireObject(node)->getUUID())) {
                    const auto sourceCache = cache;
                    auto proxy = ResourceCache::createSource(sourceCache, node, [result, uuidMap, source](ResourcePtr proxy) {
                        if (!proxy) compat::ngFailNullAccess(true);
                        proxy->source = source;
                        proxy->index = result->size();
                        // Evaluate the virtual key outside the backing-allocation catch.
                        const auto key = compat::ngRequireObject(proxy)->uuid();
                        try {
                            (*uuidMap)[key] = true;
                        } catch (const std::bad_alloc&) {
                            throw compat::ngStaticError<compat::OutOfMemoryError>(
                                std::source_location::current());
                        } catch (const std::length_error&) {
                            throw compat::ngStaticError<compat::OutOfMemoryError>(
                                std::source_location::current());
                        }
                    });
                    detail::ngAppendQueryResource(*result, proxy);
                    const auto children = compat::ngRequireObject(node)->children;
                    for (const auto child : children) self(self, child, proxy);
                }
            };
            for (const auto t : targets) {
                if (!std::dynamic_pointer_cast<Proxy<Node>>(t)) continue;
                const auto target = to<Node>(t);
                const auto children = compat::ngRequireObject(target)->children;
                for (const auto child : children) traverse(traverse, child, t);
            }
        }
        return *result;
    }
};
using NodeChildWalker = ResourceWalker<Node, Node, true>;
using NodeDescendantsWalker = ResourceWalker<Node, Node, false>;

template<std::derived_from<Parameter> S, std::derived_from<ParameterBinding> T>
class ResourceWalker<S, T, true> : public ResourceProcessor {
protected:
    void prepareSourceClassInfo() override {
        ResourceProcessor::prepareSourceClassInfo();
        detail::ngPrepareQueryTemplateClass<ResourceWalker>("ResourceWalker", [] {
            return detail::ngQueryWalkerArguments(detail::ngQueryTypeArgument<S>(),
                detail::ngQueryTypeArgument<T>(), true);
        });
    }
public:
    std::shared_ptr<ResourceCache> cache;
    explicit ResourceWalker(std::shared_ptr<ResourceCache> value) : cache(std::move(value)) {}
    ResourceArray process(ResourceArray targets) override {
        [[maybe_unused]] const auto owner = weak_from_this().lock();
        ResourceArray result;
        for (auto t : targets) {
            auto source = compat::ngRequireObject(t)->source;
            std::shared_ptr<Node> targetNode;
            while (source) {
                if (source->type() == ResourceType::Node) {
                    targetNode = to<Node>(source);
                    break;
                }
                source = source->source;
            }
            if (!std::dynamic_pointer_cast<Proxy<Parameter>>(t)) continue;
            const auto target = to<Parameter>(t);
            const auto bindings = compat::ngRequireObject(target)->bindings;
            for (auto child : bindings) {
                if (!targetNode || nicxlive::core::ngResourceObjectEquals(
                        compat::ngRequireObject(child)->getTarget().target.lock(), targetNode)) {
                    const auto sourceCache = cache;
                    auto proxy = ResourceCache::createSource(sourceCache, child, [&](ResourcePtr proxy) {
                        if (!proxy) compat::ngFailNullAccess(true);
                        proxy->source = t;
                        proxy->index = result.size();
                    });
                    try {
                        result.push_back(std::move(proxy));
                    } catch (const std::bad_alloc&) {
                        throw compat::ngStaticError<compat::OutOfMemoryError>(
                            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
                    } catch (const std::length_error&) {
                        throw compat::ngStaticError<compat::OutOfMemoryError>(
                            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
                    }
                }
            }
        }
        return result;
    }
};
using ParameterChildWalker = ResourceWalker<Parameter, ParameterBinding, true>;

namespace detail {
// Live field references preserve receiver mutations during real model callbacks.
ResourceArray ngProcessNodeBindingWalker(ResourceArray targets,
    std::shared_ptr<ResourceCache>& cache, bool& armedOnly);
ResourceArray ngProcessNodeParameterWalker(ResourceArray targets,
    std::shared_ptr<ResourceCache>& cache);
}

template<std::derived_from<Node> S, std::derived_from<ParameterBinding> T>
class ResourceWalker<S, T, true> : public ResourceProcessor {
protected:
    void prepareSourceClassInfo() override {
        ResourceProcessor::prepareSourceClassInfo();
        detail::ngPrepareQueryTemplateClass<ResourceWalker>("ResourceWalker", [] {
            return detail::ngQueryWalkerArguments(detail::ngQueryTypeArgument<S>(),
                detail::ngQueryTypeArgument<T>(), true);
        });
    }
public:
    std::shared_ptr<ResourceCache> cache;
    bool armedOnly = false;
    ResourceWalker(std::shared_ptr<ResourceCache> value, bool armed)
        : cache(std::move(value)), armedOnly(armed) {}
    ResourceArray process(ResourceArray targets) override {
        [[maybe_unused]] const auto owner = weak_from_this().lock();
        return detail::ngProcessNodeBindingWalker(std::move(targets), cache, armedOnly);
    }
};
template<std::derived_from<Node> S, std::derived_from<Parameter> T>
class ResourceWalker<S, T, true> : public ResourceProcessor {
protected:
    void prepareSourceClassInfo() override {
        ResourceProcessor::prepareSourceClassInfo();
        detail::ngPrepareQueryTemplateClass<ResourceWalker>("ResourceWalker", [] {
            return detail::ngQueryWalkerArguments(detail::ngQueryTypeArgument<S>(),
                detail::ngQueryTypeArgument<T>(), true);
        });
    }
public:
    std::shared_ptr<ResourceCache> cache;
    explicit ResourceWalker(std::shared_ptr<ResourceCache> value) : cache(std::move(value)) {}
    ResourceArray process(ResourceArray targets) override {
        [[maybe_unused]] const auto owner = weak_from_this().lock();
        return detail::ngProcessNodeParameterWalker(std::move(targets), cache);
    }
};
} // namespace nijigenerate::core::selector

namespace nicxlive::core::d_runtime {
template<class S, class T, bool direct>
struct SourceObjectTypeNames<ng_query_source::ResourceWalker<S, T, direct>> {
    static RetainedString arguments();
    static RetainedString className();
    static RetainedString templateArgumentName();
};
}

namespace nijigenerate::core::selector {
class PuppetWalker : public NodeDescendantsWalker {
public:
    explicit PuppetWalker(std::shared_ptr<ResourceCache> value);
    ResourceArray process(ResourceArray targets) override;
};
class Selector : public std::enable_shared_from_this<Selector> {
public:
    SelectorArray<SelectorArray<std::shared_ptr<ResourceProcessor>>> processors;
    std::shared_ptr<Tokenizer> tokenizer;
    std::shared_ptr<SelectorParser> parser;
    Selector();
    virtual ~Selector() = default;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual int opCmp(std::shared_ptr<Selector> other);
    virtual bool opEquals(std::shared_ptr<Selector> other);
    Selector(const Selector&) = delete;
    Selector& operator=(const Selector&) = delete;
    virtual void build(SelectorString text);
    void buildNativeBytes(std::string_view text) {
        [[maybe_unused]] const auto owner = weak_from_this().lock();
        build(SelectorString(compat::RetainedString(text)));
    }
    virtual ResourceArray run();
};
}

namespace nicxlive::core::d_runtime {

template<> struct SourceObjectTypeNames<ng_query_source::ResourceCache> {
    inline static constexpr char className[] = "nijigenerate.core.selector.query.ResourceCache";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.core.selector.query.ResourceCache)";
};
template<> struct SourceObjectTypeNames<ng_query_source::MarkerProcessor> {
    inline static constexpr char className[] = "nijigenerate.core.selector.query.MarkerProcessor";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.core.selector.query.MarkerProcessor)";
};
template<> struct SourceObjectTypeNames<ng_query_source::PuppetWalker> {
    inline static constexpr char className[] = "nijigenerate.core.selector.query.PuppetWalker";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.core.selector.query.PuppetWalker)";
};
template<> struct SourceObjectTypeNames<ng_query_source::Selector> {
    inline static constexpr char className[] = "nijigenerate.core.selector.query.Selector";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.core.selector.query.Selector)";
};

template<class T> RetainedString SourceObjectTypeNames<ng_query_source::Processor<T>>::className() {
    return ng_query_source::detail::ngQueryTemplateName("Processor",
        ng_query_source::detail::ngQueryTypeArgument<T>(SourceTypeSpelling::UnqualifiedArgument), false);
}
template<class T> RetainedString SourceObjectTypeNames<ng_query_source::Processor<T>>::templateArgumentName() {
    return ng_query_source::detail::ngQueryTemplateName("Processor",
        ng_query_source::detail::ngQueryTypeArgument<T>(SourceTypeSpelling::UnqualifiedArgument), true);
}

template<ng_query_source::SelectorLiteral name, class T, ng_query_source::SelectorLiteral op>
struct SourceObjectTypeNames<ng_query_source::ResourceAttrFilter<name, T, op>> {
    static RetainedString arguments() {
        return ng_query_source::detail::ngQueryFilterArguments(name.view(),
            ng_query_source::detail::ngQueryTypeArgument<T>(),
            ng_query_source::detail::ngQueryOperatorArgument<op>());
    }
    static RetainedString className() {
        return ng_query_source::detail::ngQueryTemplateName("ResourceAttrFilter", arguments(), false);
    }
    static RetainedString templateArgumentName() {
        return ng_query_source::detail::ngQueryTemplateName("ResourceAttrFilter", arguments(), true);
    }
};

template<class S, class T, bool direct>
struct SourceObjectTypeNames<ng_query_source::ResrouceWalker<S, T, direct>> {
    static RetainedString arguments() {
        return ng_query_source::detail::ngQueryWalkerArguments(
            ng_query_source::detail::ngQueryTypeArgument<S>(),
            ng_query_source::detail::ngQueryTypeArgument<T>(), direct);
    }
    static RetainedString className() {
        return ng_query_source::detail::ngQueryTemplateName("ResrouceWalker", arguments(), false);
    }
    static RetainedString templateArgumentName() {
        return ng_query_source::detail::ngQueryTemplateName("ResrouceWalker", arguments(), true);
    }
};

template<class S, class T, bool direct>
RetainedString SourceObjectTypeNames<ng_query_source::ResourceWalker<S, T, direct>>::arguments() {
    static_assert(requires { sizeof(ng_query_source::ResourceWalker<S, T, direct>); },
        "Invalid Source ResourceWalker arguments");
    return ng_query_source::detail::ngQueryWalkerArguments(
        ng_query_source::detail::ngQueryTypeArgument<S>(),
        ng_query_source::detail::ngQueryTypeArgument<T>(), direct);
}
template<class S, class T, bool direct>
RetainedString SourceObjectTypeNames<ng_query_source::ResourceWalker<S, T, direct>>::className() {
    return ng_query_source::detail::ngQueryTemplateName("ResourceWalker", arguments(), false);
}
template<class S, class T, bool direct>
RetainedString SourceObjectTypeNames<ng_query_source::ResourceWalker<S, T, direct>>::templateArgumentName() {
    return ng_query_source::detail::ngQueryTemplateName("ResourceWalker", arguments(), true);
}

template<> struct SourceTypeSpellings<ng_query_source::ResourceCache> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.query.ResourceCache)";
    inline static constexpr char unqualifiedArgument[] = "(ResourceCache)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.query.ResourceCache";
    inline static constexpr char unqualifiedElement[] = "ResourceCache";
};
template<> struct SourceTypeSpellings<ng_query_source::MarkerProcessor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.query.MarkerProcessor)";
    inline static constexpr char unqualifiedArgument[] = "(MarkerProcessor)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.query.MarkerProcessor";
    inline static constexpr char unqualifiedElement[] = "MarkerProcessor";
};
template<> struct SourceTypeSpellings<ng_query_source::PuppetWalker> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.query.PuppetWalker)";
    inline static constexpr char unqualifiedArgument[] = "(PuppetWalker)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.query.PuppetWalker";
    inline static constexpr char unqualifiedElement[] = "PuppetWalker";
};
template<> struct SourceTypeSpellings<ng_query_source::Selector> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.query.Selector)";
    inline static constexpr char unqualifiedArgument[] = "(Selector)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.query.Selector";
    inline static constexpr char unqualifiedElement[] = "Selector";
};
template<class T> struct SourceTypeSpellings<ng_query_source::Processor<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ng_query_source::detail::ngQueryTemplateTypeSpellings("Processor",
            ng_query_source::detail::ngQueryTypeArgument<T>(SourceTypeSpelling::UnqualifiedArgument));
    }
};
template<ng_query_source::SelectorLiteral name, class T, ng_query_source::SelectorLiteral op>
struct SourceTypeSpellings<ng_query_source::ResourceAttrFilter<name, T, op>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ng_query_source::detail::ngQueryTemplateTypeSpellings("ResourceAttrFilter",
            SourceObjectTypeNames<ng_query_source::ResourceAttrFilter<name, T, op>>::arguments());
    }
};
template<class S, class T, bool direct>
struct SourceTypeSpellings<ng_query_source::ResrouceWalker<S, T, direct>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ng_query_source::detail::ngQueryTemplateTypeSpellings("ResrouceWalker",
            SourceObjectTypeNames<ng_query_source::ResrouceWalker<S, T, direct>>::arguments());
    }
};
template<class S, class T, bool direct>
struct SourceTypeSpellings<ng_query_source::ResourceWalker<S, T, direct>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ng_query_source::detail::ngQueryTemplateTypeSpellings("ResourceWalker",
            SourceObjectTypeNames<ng_query_source::ResourceWalker<S, T, direct>>::arguments());
    }
};
}

namespace nijigenerate::core::translation {
namespace query_format_source = ::nijigenerate::core::selector;

template<> struct FormatObjectTraits<std::shared_ptr<query_format_source::ResourceCache>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<query_format_source::ResourceCache>> {};

template<class T> struct FormatObjectTraits<std::shared_ptr<query_format_source::Processor<T>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<query_format_source::Processor<T>>> {};

template<> struct FormatObjectTraits<std::shared_ptr<query_format_source::MarkerProcessor>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<query_format_source::MarkerProcessor>> {};

template<query_format_source::SelectorLiteral name, class T, query_format_source::SelectorLiteral op>
struct FormatObjectTraits<std::shared_ptr<query_format_source::ResourceAttrFilter<name, T, op>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<query_format_source::ResourceAttrFilter<name, T, op>>> {};

template<class S, class T, bool direct>
struct FormatObjectTraits<std::shared_ptr<query_format_source::ResrouceWalker<S, T, direct>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<query_format_source::ResrouceWalker<S, T, direct>>> {};

template<class S, class T, bool direct>
struct FormatObjectTraits<std::shared_ptr<query_format_source::ResourceWalker<S, T, direct>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<query_format_source::ResourceWalker<S, T, direct>>> {};

template<> struct FormatObjectTraits<std::shared_ptr<query_format_source::PuppetWalker>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<query_format_source::PuppetWalker>> {};

template<> struct FormatObjectTraits<std::shared_ptr<query_format_source::Selector>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<query_format_source::Selector>> {};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::serde {

// These exact Source classes declare no runtime fields. Native ESFT state and
// method implementations are not fields in the Source serialization schema.
template<class T> struct SourceJsonFields<::nijigenerate::core::selector::Processor<T>> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::core::selector::Processor<T>& value, Serializer& serializer) {
        [[maybe_unused]] const auto owner = value.weak_from_this().lock();
        auto state = serializer.structBegin();
        serializer.structEnd(state);
    }
};

template<> struct SourceJsonFields<::nijigenerate::core::selector::MarkerProcessor> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::core::selector::MarkerProcessor& value, Serializer& serializer) {
        [[maybe_unused]] const auto owner = value.weak_from_this().lock();
        auto state = serializer.structBegin();
        serializer.structEnd(state);
    }
};

template<class Source, class Target, bool direct>
struct SourceJsonFields<::nijigenerate::core::selector::ResrouceWalker<Source, Target, direct>> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::core::selector::ResrouceWalker<Source, Target, direct>& value,
            Serializer& serializer) {
        [[maybe_unused]] const auto owner = value.weak_from_this().lock();
        auto state = serializer.structBegin();
        serializer.structEnd(state);
    }
};

} // namespace nicxlive::core::serde
