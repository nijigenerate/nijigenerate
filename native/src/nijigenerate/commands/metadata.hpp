#pragma once

#include "base.hpp"
#include <tuple>
#include <type_traits>

namespace nijigenerate::commands {

template<class... T> struct CommandTypeList {};
// Specializations list direct attributes and all D base classes in D order.
// An empty CommandScopes<> suppresses default normal scope, preserving inherited scopes.
template<class C> struct CommandTraits {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<>;
};

namespace detail {
template<class T> struct ScopeCount : std::integral_constant<std::size_t, 0> {};
template<class... S> struct ScopeCount<CommandScopes<S...>> : std::integral_constant<std::size_t, sizeof...(S)> {};
template<class C> constexpr std::size_t typeScopeCount() {
    return std::apply([](const auto&... attr) {
        return (std::size_t{0} + ... + ScopeCount<std::remove_cvref_t<decltype(attr)>>::value);
    }, CommandTraits<C>::attributes);
}
template<class... Bases> constexpr std::size_t baseScopeCount(CommandTypeList<Bases...>) {
    return (std::size_t{0} + ... + typeScopeCount<Bases>());
}

template<class S> consteval bool scopeBuildSupported() {
    if constexpr (!requires { sizeof(S); }) return false;
    else if constexpr (!std::derived_from<S, CommandScope>) return false;
    else return std::is_default_constructible_v<S> && std::is_destructible_v<S>
        && requires(std::shared_ptr<S> scope, CommandScopeMap& scopes) {
            { CommandScopeTypeName<S>::value } -> std::convertible_to<std::string_view>;
            scopes[scope->typeName()] = scope;
        };
}

template<class Attribute> struct ScopeBuildDomain {
    static constexpr bool declared = false;
    static constexpr bool supported = true;
};
template<class... S> struct ScopeBuildDomain<CommandScopes<S...>> {
    static constexpr bool declared = true;
    static constexpr bool supported = (scopeBuildSupported<S>() && ...);
};

template<class C> consteval bool typeScopesSupported() {
    return std::apply([](const auto&... attr) {
        return (ScopeBuildDomain<std::remove_cvref_t<decltype(attr)>>::supported && ...);
    }, CommandTraits<C>::attributes);
}
template<class C> consteval bool typeDeclaresScope() {
    return std::apply([](const auto&... attr) {
        return (ScopeBuildDomain<std::remove_cvref_t<decltype(attr)>>::declared || ...);
    }, CommandTraits<C>::attributes);
}
template<class... Bases> consteval bool baseScopesSupported(CommandTypeList<Bases...>) {
    return (typeScopesSupported<Bases>() && ...);
}
template<class... Bases> consteval bool basesDeclareScope(CommandTypeList<Bases...>) {
    return (typeDeclaresScope<Bases>() || ...);
}

template<class C> consteval bool metadataBuildSupported() {
    using Bases = typename CommandTraits<C>::BaseClasses;
    if constexpr (typeScopeCount<C>() + baseScopeCount(Bases{}) > 1) return false;
    else if constexpr (!typeScopesSupported<C>() || !baseScopesSupported(Bases{})) return false;
    else if constexpr (!typeDeclaresScope<C>() && !basesDeclareScope(Bases{}))
        return scopeBuildSupported<NormalCommandScope>();
    else return true;
}
template<class Attribute, class C, std::size_t I = 0>
constexpr Attribute firstAttribute(Attribute fallback) {
    using Attributes = std::remove_cv_t<decltype(CommandTraits<C>::attributes)>;
    if constexpr (I == std::tuple_size_v<Attributes>) {
        return fallback;
    } else if constexpr (std::is_same_v<Attribute, std::tuple_element_t<I, Attributes>>) {
        return std::get<I>(CommandTraits<C>::attributes);
    } else {
        return firstAttribute<Attribute, C, I + 1>(fallback);
    }
}
template<class C, std::size_t I = 0> constexpr bool presentationOnlyOf() {
    using Attributes = std::remove_cv_t<decltype(CommandTraits<C>::attributes)>;
    if constexpr (I == std::tuple_size_v<Attributes>) return false;
    else if constexpr (std::is_same_v<PresentationOnly, std::tuple_element_t<I, Attributes>>) return true;
    else return presentationOnlyOf<C, I + 1>();
}
template<class Attribute> void addScopes(const Attribute&, CommandScopeMap&, bool&) {}
template<class... S> void addScopes(CommandScopes<S...>, CommandScopeMap& scopes, bool& found) {
    static_assert((std::derived_from<S, CommandScope> && ...));
    found = true;
    ([&] {
        auto scope = ngCommandScope<S>();
        scopes[scope->typeName()] = scope;
    }(), ...);
}
template<class C> void addTypeScopes(CommandScopeMap& scopes, bool& found) {
    std::apply([&](const auto&... attr) { (addScopes(attr, scopes, found), ...); },
        CommandTraits<C>::attributes);
}
template<class... Bases>
void addBaseScopes(CommandTypeList<Bases...>, CommandScopeMap& scopes, bool& found) {
    (addTypeScopes<Bases>(scopes, found), ...);
}
}

// Called with the static command type by the registration layer.
template<class C> requires(detail::metadataBuildSupported<C>())
CommandMetadata ngBuildCommandMetadata() {
    // D currently redeclares commandScope when static foreach expands twice.
    // Keep that unsupported instantiation explicit until the D contract changes.
    static_assert(detail::typeScopeCount<C>()
        + detail::baseScopeCount(typename CommandTraits<C>::BaseClasses{}) <= 1,
        "The D reference cannot instantiate more than one declared scope");
    CommandMetadata result;
    result.shortcutRunnable = detail::firstAttribute<ShortcutVisibility, C>(ShortcutVisible).value
        != CommandShortcutPolicy::hidden;
    result.mcpExposed = detail::firstAttribute<McpExposureMeta, C>(McpTool).value != CommandMcpExposure::hidden;
    result.guiDisplay = detail::firstAttribute<GuiDisplayMeta, C>({CommandGuiDisplay::none}).value;
    result.irreversibleEffect = detail::firstAttribute<IrreversibleEffectMeta, C>(
        {CommandIrreversibleEffect::none}).value;
    result.presentationOnly = detail::presentationOnlyOf<C>();
    bool found = false;
    detail::addTypeScopes<C>(result.scopes, found);
    detail::addBaseScopes(typename CommandTraits<C>::BaseClasses{}, result.scopes, found);
    if (!found) {
        auto normal = ngCommandScope<NormalCommandScope>();
        result.scopes[normal->typeName()] = normal;
    }
    return result;
}
} // namespace nijigenerate::commands
