#pragma once
#include "command_browser_argument_parser.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <array>
#include <new>

namespace nijigenerate::windows::command_browser::detail {
template<class C> struct BrowserSourceResultTraits;
template<class S> std::type_identity<S> browserStorage(const commands::ExCommand<S>*);
std::type_identity<void> browserStorage(...);
template<class C> using BrowserStorage = typename decltype(browserStorage(static_cast<C*>(nullptr)))::type;
template<class T> struct BrowserDefaultResource : std::bool_constant<isResourceArgument<T>> {};
template<class E> struct BrowserDefaultResource<std::shared_ptr<E>> : std::bool_constant<
    std::is_convertible_v<std::shared_ptr<E>,NodeRef> ||
    std::is_convertible_v<std::shared_ptr<E>,ParameterRef> ||
    std::is_convertible_v<std::shared_ptr<E>,BindingRef>> {};

template<class T> BrowserText browserArgDescription(BrowserText base) {
    if constexpr (!isBrowserEnum<T>) return base;
    else if constexpr (commands::SourceCommandEnum<T>) {
        return BrowserText(commands::enrichArgDesc<T>(base.retainedString()));
    } else {
        // Remaining Browser-only records describe numeric Source enums.
        // Ordinary foreach's __traits(identifier, mem) names its local symbol.
        Texts labels;
        for ([[maybe_unused]] const auto& member : BrowserEnumTraits<T>::members)
            labels.append(staticText("mem"));
        const auto choices = concat(concat(staticText("("), join(labels, "|")), staticText(")"));
        return base.empty() ? choices : concat(concat(base, staticText(" ")), choices);
    }
}

template<class T> BrowserText browserValueString(const T& value) {
    if constexpr (std::same_as<T, BrowserText>) return value;
    else if constexpr (std::same_as<T, compat::RetainedString>) return BrowserText(value);
    else if constexpr (std::same_as<T, std::string>) return text(value);
    else if constexpr (std::same_as<T, bool>) return text(value ? "true" : "false");
    else if constexpr (isBrowserEnum<T>) {
        for (const auto& [name, member] : BrowserEnumTraits<T>::members)
            if (member == value) return staticText(name);
        if constexpr (std::is_enum_v<T>) return format("cast(%s)%s", BrowserEnumTraits<T>::typeName,
            static_cast<std::underlying_type_t<T>>(value));
        else return format("cast(%s)%s", BrowserEnumTraits<T>::typeName,
            core::translation::FormatStringEnumTraits<T>::raw(value));
    } else if constexpr (std::is_arithmetic_v<T>) return format("%s", value);
    else if constexpr (std::same_as<T,core::settings::SettingsJsonValue>)
        return BrowserText(compat::ngJsonStringify(value));
    else if constexpr (requires { value.begin(); value.end(); }) {
        Texts items;
        for (const auto& item : value) items.append(browserValueString(item));
        return concat(concat(text("["),join(items,", ")),text("]"));
    }
    else return BrowserSourceResultTraits<T>::stringify(value);
}

template<class T> BrowserText browserEnumLiteral(std::size_t index) {
    using Traits = BrowserEnumTraits<T>;
    if constexpr (requires { Traits::sourceLiterals; }) {
        return staticText(Traits::sourceLiterals[index]);
    } else {
        // D's manifest foreach variable expands to its enum initializer.
        // hdrgen chooses the first declaration with the same enum value.
        static constexpr auto literals = [] {
            constexpr auto width = [] {
                std::size_t longest = 0;
                for (const auto& member : Traits::members)
                    if (member.first.size() > longest) longest = member.first.size();
                return Traits::typeName.size() + 1 + longest + 1;
            }();
            std::array<std::array<char,width>, Traits::members.size()> result{};
            for (std::size_t i = 0; i < Traits::members.size(); ++i) {
                auto name = Traits::members[i].first;
                for (const auto& member : Traits::members) {
                    if (member.second == Traits::members[i].second) {
                        name = member.first;
                        break;
                    }
                }
                std::size_t cursor = 0;
                for (char byte : Traits::typeName) result[i][cursor++] = byte;
                result[i][cursor++] = '.';
                for (char byte : name) result[i][cursor++] = byte;
            }
            return result;
        }();
        return staticText(std::string_view(literals[index].data()));
    }
}

template<class C, bool EnumMap> void applyBrowserArguments(CommandRef command, RawArguments values) {
    const auto instance = std::dynamic_pointer_cast<C>(command);
    if (!instance) return;
    using S = BrowserStorage<C>;
    std::apply([&](const auto&... fields) {
        ([&] {
            using F = std::remove_cvref_t<decltype(fields)>;
            using T = typename F::Type;
            // The enum-map source checks HideFromExpose UDAs, not TW's
            // fourth argument. None of the current declared argument types
            // carries that UDA; the dynamic-map branch checks TW's flag.
            if constexpr (EnumMap || !F::hidden) {
                const auto found = values.find(staticText(F::fieldName));
                if (found == values.end()) return;
                T value{};
                if (parseArgValue(found->second, value)) F::get(*instance) = value;
                else if constexpr (isResourceArgument<T>) {
                    clearResourceArgValue(value); F::get(*instance) = value;
                }
            }
        }(), ...);
    }, commands::CommandFields<S>::descriptors);
}

template<class C, bool EnumMap> void captureBrowserDefaults(CommandRef command, RawArguments& values) {
    const auto instance = std::dynamic_pointer_cast<C>(command);
    if (!instance) return;
    using S = BrowserStorage<C>;
    std::apply([&](const auto&... fields) {
        ([&] {
            using F = std::remove_cvref_t<decltype(fields)>;
            using T = typename F::Type;
            if constexpr ((!EnumMap || !F::hidden) && !BrowserDefaultResource<T>::value) {
                try { values[staticText(F::fieldName)] = trimAtNul(browserValueString(F::get(*instance))); }
                catch (const compat::ThrowableException&) {}
            }
        }(), ...);
    }, commands::CommandFields<S>::descriptors);
}

template<class C, bool EnumMap> void reflectBrowserCommand(CommandInfo& info) {
    if constexpr (!std::same_as<BrowserStorage<C>, void>) {
        using S = BrowserStorage<C>;
        info.applyArgs = &applyBrowserArguments<C, EnumMap>;
        info.captureDefaults = &captureBrowserDefaults<C, EnumMap>;
        std::apply([&](const auto&... fields) {
            ([&] {
                using F = std::remove_cvref_t<decltype(fields)>;
                using T = typename F::Type;
                CommandArgInfo arg;
                arg.name = staticText(F::fieldName);
                arg.typeName = staticText(F::typeName);
                arg.hidden = !EnumMap && F::hidden;
                if constexpr (EnumMap || F::named) {
                    arg.isIntegral = isBrowserIntegral<T>;
                    arg.isFloat = std::is_floating_point_v<T>;
                    arg.isBool = std::same_as<T, bool>;
                }
                if constexpr (F::named) {
                    arg.desc = browserArgDescription<T>(staticText(F::fieldDesc));
                    if constexpr (isBrowserEnum<T>) {
                        for (std::size_t i = 0; i < BrowserEnumTraits<T>::members.size(); ++i)
                            arg.enumValues.append(browserEnumLiteral<T>(i));
                    }
                    arg.isNodeResource = isNodeResource<T>;
                    arg.isNodeResourceArray = isNodeResourceArray<T>;
                    if constexpr (isNodeResource<T> || isNodeResourceArray<T>) {
                        using E = NodeResourceElement<T>;
                        arg.acceptsNodeResource = [](NodeRef node) {
                            return bool(std::dynamic_pointer_cast<typename E::element_type>(node));
                        };
                    }
                    if constexpr (isResourceArgument<T>) info.resourceArgs.append(arg.name);
                } else arg.desc = staticText("");
                if (!arg.hidden) info.inputs.append(arg);
            }(), ...);
        }, commands::CommandFields<S>::descriptors);
    }
    BrowserSourceResultTraits<C>::apply(info);
}
}
