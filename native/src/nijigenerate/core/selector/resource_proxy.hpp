#pragma once

#include "resource.hpp"
#include "tokenizer.hpp"
#include <nijigenerate/core/typed_format.hpp>
#include <serialization_fields.hpp>
#include <nijigenerate/compat/source_json_enums.hpp>
#include <nodes/node.hpp>
#include <param/parameter.hpp>
#include <param/binding.hpp>
#include <math/transform.hpp>
#include <array>
#include <concepts>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <type_traits>

namespace nijigenerate::core::selector {
using Node = nicxlive::core::nodes::Node;
using Parameter = nicxlive::core::param::Parameter;
using ParameterBinding = nicxlive::core::param::ParameterBinding;
template<class T> class Proxy;

namespace detail {
inline ResourceString ngSelectorDeclaredSpelling(nicxlive::core::d_runtime::SourceTypeSpelling context,
        std::string_view qualifiedArgument, std::string_view unqualifiedArgument,
        std::string_view qualifiedElement, std::string_view unqualifiedElement) {
    using Context = nicxlive::core::d_runtime::SourceTypeSpelling;
    // Every caller supplies static defining-declaration literals.
    switch (context) {
    case Context::QualifiedArgument: return ResourceString::fromStaticView(qualifiedArgument);
    case Context::UnqualifiedArgument: return ResourceString::fromStaticView(unqualifiedArgument);
    case Context::QualifiedElement: return ResourceString::fromStaticView(qualifiedElement);
    case Context::UnqualifiedElement: return ResourceString::fromStaticView(unqualifiedElement);
    }
    throw std::invalid_argument("Invalid Source type spelling context");
}

template<class Build> nicxlive::core::d_runtime::RetainedString ngBuildSelectorMetadata(Build build);

template<class T> ResourceString ngSelectorSourceArgument(
        nicxlive::core::d_runtime::SourceTypeSpelling context =
            nicxlive::core::d_runtime::SourceTypeSpelling::UnqualifiedArgument) {
    if constexpr (std::same_as<T, std::nullptr_t>)
        return ngSelectorDeclaredSpelling(context, "(typeof(null))", "(typeof(null))",
            "typeof(null)", "typeof(null)");
    else if constexpr (std::is_const_v<T> && !std::is_volatile_v<T> &&
            (std::is_arithmetic_v<std::remove_const_t<T>> || std::is_enum_v<std::remove_const_t<T>>)) {
        using Context = nicxlive::core::d_runtime::SourceTypeSpelling;
        const bool grouped = context == Context::QualifiedArgument || context == Context::UnqualifiedArgument;
        const bool qualified = context == Context::QualifiedArgument || context == Context::QualifiedElement;
        const auto element = ngSelectorSourceArgument<std::remove_const_t<T>>(
            qualified ? Context::QualifiedElement : Context::UnqualifiedElement);
        return ngBuildSelectorMetadata([&] {
            std::string name = grouped ? "(const(" : "const(";
            name.append(element.view());
            name += grouped ? "))" : ")";
            return name;
        }).toNullableString();
    } else if constexpr (std::same_as<T, nicxlive::core::math::Vec2Array>)
        return ngSelectorDeclaredSpelling(context, "(nijilive.math.veca.veca!(float, 2LU))",
            "(veca!(float, 2LU))", "nijilive.math.veca.veca!(float, 2LU)", "veca!(float, 2LU)");
    else if constexpr (std::same_as<T, nicxlive::core::math::Vec3Array>)
        return ngSelectorDeclaredSpelling(context, "(nijilive.math.veca.veca!(float, 3LU))",
            "(veca!(float, 3LU))", "nijilive.math.veca.veca!(float, 3LU)", "veca!(float, 3LU)");
    else if constexpr (std::same_as<T, nicxlive::core::math::Transform>)
        return ngSelectorDeclaredSpelling(context, "(nijilive.math.transform.Transform)", "(Transform)",
            "nijilive.math.transform.Transform", "Transform");
    else if constexpr (std::same_as<T, void>) return ResourceString::fromStaticLiteral("void");
    else return nicxlive::core::d_runtime::ngSourceTypeSpelling<T>(context).toNullableString();
}

template<class Build> nicxlive::core::d_runtime::RetainedString ngBuildSelectorMetadata(Build build) {
    try {
        return nicxlive::core::d_runtime::RetainedString(build());
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline nicxlive::core::d_runtime::RetainedString ngSelectorTemplateName(
        std::string_view declaration, ResourceString argument, bool typeArgument) {
    return ngBuildSelectorMetadata([&] {
        std::string name = typeArgument ? "(" : "";
        name += "nijigenerate.core.selector.resource.";
        name.append(declaration);
        name += '!';
        name.append(argument.view());
        if (typeArgument) name += ')';
        else { name += '.'; name.append(declaration); }
        return name;
    });
}

inline nicxlive::core::d_runtime::SourceTypeSpellingNames ngSelectorTemplateTypeSpellings(
        std::string_view declaration, ResourceString argument) {
    const auto raw = [&](bool qualified) {
        return ngBuildSelectorMetadata([&] {
            std::string name = qualified ? "nijigenerate.core.selector.resource." : "";
            name.append(declaration);
            name += '!';
            name.append(argument.view());
            return name;
        });
    };
    const auto qualified = raw(true);
    const auto unqualified = raw(false);
    const auto grouped = [](const auto& element) {
        return ngBuildSelectorMetadata([&] {
            std::string name = "(";
            name.append(element.view());
            name += ')';
            return name;
        });
    };
    return {grouped(qualified), grouped(unqualified), qualified, unqualified};
}

template<class Class, class Argument> void ngRegisterSelectorClass(std::string_view declaration) {
    // One process-wide ClassInfo record per actual source specialization.
    // Subsequent objects do not rebuild its immutable name or reinsert it.
    static const bool registered = [declaration] {
        const auto argument = ngSelectorSourceArgument<Argument>();
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class),
            ngSelectorTemplateName(declaration, argument, false));
        return true;
    }();
    (void)registered;
}
} // namespace detail
} // namespace nijigenerate::core::selector

namespace nicxlive::core::d_runtime {
namespace ng_selector_resource = ::nijigenerate::core::selector;

template<> struct SourceObjectTypeNames<ng_selector_resource::Resource> {
    inline static constexpr char className[] = "nijigenerate.core.selector.resource.Resource";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.core.selector.resource.Resource)";
};
template<> struct SourceTypeSpellings<ng_selector_resource::Resource> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.resource.Resource)";
    inline static constexpr char unqualifiedArgument[] = "(Resource)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.resource.Resource";
    inline static constexpr char unqualifiedElement[] = "Resource";
};
template<> struct SourceTypeSpellings<ng_selector_resource::ResourceType> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.core.selector.resource.ResourceType)";
    inline static constexpr char unqualifiedArgument[] = "(ResourceType)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.core.selector.resource.ResourceType";
    inline static constexpr char unqualifiedElement[] = "ResourceType";
};

template<class T> struct SourceObjectTypeNames<ng_selector_resource::ResourceInfo<T>> {
    static RetainedString className() {
        return ng_selector_resource::detail::ngSelectorTemplateName("ResourceInfo",
            ng_selector_resource::detail::ngSelectorSourceArgument<T>(), false);
    }
    static RetainedString templateArgumentName() {
        return ng_selector_resource::detail::ngSelectorTemplateName("ResourceInfo",
            ng_selector_resource::detail::ngSelectorSourceArgument<T>(), true);
    }
};
template<class T> struct SourceTypeSpellings<ng_selector_resource::ResourceInfo<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return ng_selector_resource::detail::ngSelectorTemplateTypeSpellings("ResourceInfo",
            ng_selector_resource::detail::ngSelectorSourceArgument<T>());
    }
};
template<class T> struct SourceObjectTypeNames<ng_selector_resource::Proxy<T>> {
    static RetainedString className() {
        static_assert(requires { sizeof(ng_selector_resource::Proxy<T>); }, "Invalid Source Proxy argument");
        return ng_selector_resource::detail::ngSelectorTemplateName("Proxy",
            ng_selector_resource::detail::ngSelectorSourceArgument<T>(), false);
    }
    static RetainedString templateArgumentName() {
        static_assert(requires { sizeof(ng_selector_resource::Proxy<T>); }, "Invalid Source Proxy argument");
        return ng_selector_resource::detail::ngSelectorTemplateName("Proxy",
            ng_selector_resource::detail::ngSelectorSourceArgument<T>(), true);
    }
};
template<class T> struct SourceTypeSpellings<ng_selector_resource::Proxy<T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        static_assert(requires { sizeof(ng_selector_resource::Proxy<T>); }, "Invalid Source Proxy argument");
        return ng_selector_resource::detail::ngSelectorTemplateTypeSpellings("Proxy",
            ng_selector_resource::detail::ngSelectorSourceArgument<T>());
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::selector {

template<class T> requires std::derived_from<T, Node>
struct ResourceInfo<T> : ResourceInfoIdentity {
    ResourceInfo() { detail::ngRegisterSelectorClass<ResourceInfo<T>, T>("ResourceInfo"); }
    static constexpr ResourceType type() { return ResourceType::Node; }
};
template<class T> requires std::derived_from<T, Parameter>
struct ResourceInfo<T> : ResourceInfoIdentity {
    ResourceInfo() { detail::ngRegisterSelectorClass<ResourceInfo<T>, T>("ResourceInfo"); }
    static constexpr ResourceType type() { return ResourceType::Parameter; }
};
template<class T> requires std::derived_from<T, ParameterBinding>
struct ResourceInfo<T> : ResourceInfoIdentity {
    ResourceInfo() { detail::ngRegisterSelectorClass<ResourceInfo<T>, T>("ResourceInfo"); }
    static constexpr ResourceType type() { return ResourceType::Binding; }
};
template<> struct ResourceInfo<nicxlive::core::math::Vec2Array> : ResourceInfoIdentity {
    ResourceInfo() {
        detail::ngRegisterSelectorClass<ResourceInfo, nicxlive::core::math::Vec2Array>("ResourceInfo");
    }
    static constexpr ResourceType type() { return ResourceType::Vec2; }
};
template<> struct ResourceInfo<nicxlive::core::math::Vec3Array> : ResourceInfoIdentity {
    ResourceInfo() {
        detail::ngRegisterSelectorClass<ResourceInfo, nicxlive::core::math::Vec3Array>("ResourceInfo");
    }
    static constexpr ResourceType type() { return ResourceType::Vec3; }
};
template<> struct ResourceInfo<nicxlive::core::math::Transform> : ResourceInfoIdentity {
    ResourceInfo() {
        detail::ngRegisterSelectorClass<ResourceInfo, nicxlive::core::math::Transform>("ResourceInfo");
    }
    static constexpr ResourceType type() { return ResourceType::Transform; }
};
template<> struct ResourceInfo<SelectorArray<std::int32_t>> : ResourceInfoIdentity {
    ResourceInfo() { detail::ngRegisterSelectorClass<ResourceInfo, SelectorArray<std::int32_t>>("ResourceInfo"); }
    static constexpr ResourceType type() { return ResourceType::IntArray; }
};
template<> struct ResourceInfo<SelectorArray<float>> : ResourceInfoIdentity {
    ResourceInfo() { detail::ngRegisterSelectorClass<ResourceInfo, SelectorArray<float>>("ResourceInfo"); }
    static constexpr ResourceType type() { return ResourceType::FloatArray; }
};

template<class T> class Proxy;

template<class T> requires std::derived_from<T, Node>
class Proxy<T> : public Resource {
public:
    std::shared_ptr<T> target;
    explicit Proxy(std::shared_ptr<T> obj) : target(std::move(obj)) {
        type_ = ResourceInfo<T>::type();
        detail::ngRegisterSelectorClass<Proxy<T>, T>("Proxy");
    }
    ResourceString name() override {
        auto receiver = target;
        return compat::ngRequireObject(receiver)->getName();
    }
    ResourceString typeId() override {
        // Keep the evaluated D class receiver alive if its virtual call rebinds
        // the public target slot. Return its actual immutable descriptor.
        auto receiver = target;
        return compat::ngRequireObject(receiver)->typeId();
    }
    std::uint32_t uuid() override {
        auto receiver = target;
        return compat::ngRequireObject(receiver)->getUUID();
    }
    virtual std::shared_ptr<T>& obj() { return target; }
};

template<class T> requires std::derived_from<T, Parameter>
class Proxy<T> : public Resource {
public:
    std::shared_ptr<T> target;
    explicit Proxy(std::shared_ptr<T> obj) : target(std::move(obj)) {
        type_ = ResourceInfo<T>::type();
        detail::ngRegisterSelectorClass<Proxy<T>, T>("Proxy");
    }
    ResourceString name() override {
        auto receiver = target;
        return compat::ngRequireObject(receiver)->getName();
    }
    ResourceString typeId() override { return ResourceString::fromStaticLiteral("Parameter"); }
    std::uint32_t uuid() override {
        auto receiver = target;
        return compat::ngRequireObject(receiver)->getUUID();
    }
    virtual std::shared_ptr<T>& obj() { return target; }
};

template<class T> requires std::derived_from<T, ParameterBinding>
class Proxy<T> : public Resource {
public:
    std::shared_ptr<T> target;
    explicit Proxy(std::shared_ptr<T> obj) : target(std::move(obj)) {
        type_ = ResourceInfo<T>::type();
        detail::ngRegisterSelectorClass<Proxy<T>, T>("Proxy");
    }
    ResourceString name() override {
        auto receiver = target;
        return compat::ngRequireObject(receiver)->getTarget().name;
    }
    ResourceString typeId() override { return ResourceString::fromStaticLiteral("Binding"); }
    std::uint32_t uuid() override {
        // D casts the address of its target field, not the binding or node UUID.
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&target));
    }
    virtual std::shared_ptr<T>& obj() { return target; }
};

template<class T> requires (std::same_as<T, Node> || std::same_as<T, Parameter> || std::same_as<T, ParameterBinding>)
std::shared_ptr<T> to(const ResourcePtr& resource) {
    const auto proxy = std::dynamic_pointer_cast<Proxy<T>>(resource);
    return proxy ? proxy->obj() : std::shared_ptr<T>{};
}
}

namespace nijigenerate::core::translation {

template<>
struct FormatEnumTraits<::nijigenerate::core::selector::ResourceType> {
    using Enum = ::nijigenerate::core::selector::ResourceType;
    inline static constexpr std::string_view name = "ResourceType";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Node, "Node"},
        FormatEnumMember<Enum>{Enum::Parameter, "Parameter"},
        FormatEnumMember<Enum>{Enum::Binding, "Binding"},
        FormatEnumMember<Enum>{Enum::Vec2, "Vec2"},
        FormatEnumMember<Enum>{Enum::Vec3, "Vec3"},
        FormatEnumMember<Enum>{Enum::Transform, "Transform"},
        FormatEnumMember<Enum>{Enum::IntArray, "IntArray"},
        FormatEnumMember<Enum>{Enum::FloatArray, "FloatArray"}};
};

template<>
struct FormatObjectTraits<::nijigenerate::core::selector::ResourcePtr>
    : detail::SourceNoArgObjectFormat<::nijigenerate::core::selector::ResourcePtr> {};

template<class T>
struct FormatObjectTraits<std::shared_ptr<::nijigenerate::core::selector::ResourceInfo<T>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::core::selector::ResourceInfo<T>>> {};

template<class T>
struct FormatObjectTraits<std::shared_ptr<::nijigenerate::core::selector::Proxy<T>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::core::selector::Proxy<T>>> {};

} // namespace nijigenerate::core::translation

namespace nicxlive::core::serde {

template<> struct SourceJsonEnum<::nijigenerate::core::selector::ResourceType> {
    template<class Serializer>
    static void serialize(::nijigenerate::core::selector::ResourceType value, Serializer& serializer) {
        ::nijigenerate::compat::detail::serializePlainSourceEnum(value, serializer);
    }
};

template<> struct SourceJsonEnumKey<::nijigenerate::core::selector::ResourceType> {
    static d_runtime::RetainedString get(::nijigenerate::core::selector::ResourceType value) {
        return ::nijigenerate::compat::detail::enumJsonKey(value);
    }
};

// Every original ResourceInfo specialization has only its static type method.
template<class T>
struct SourceJsonFields<::nijigenerate::core::selector::ResourceInfo<T>> {
    template<class Serializer>
    static void serialize(const ::nijigenerate::core::selector::ResourceInfo<T>&, Serializer& serializer) {
        auto state = serializer.structBegin();
        serializer.structEnd(state);
    }
};

} // namespace nicxlive::core::serde
