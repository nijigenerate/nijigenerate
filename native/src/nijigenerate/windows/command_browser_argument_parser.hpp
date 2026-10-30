#pragma once
// Enum conversion adapted from Phobos std.conv/std.algorithm.searching.
// Copyright The D Language Foundation 2007-; Walter Bright, Andrei Alexandrescu,
// Shin Fujishiro, Adam D. Ruppe, Kenji Hara. Boost Software License 1.0;
// full license: native/port/licenses/phobos-unicode.txt.
#include "command_browser_helpers.hpp"
#include "command_browser_resource_traits.hpp"
#include <nijigenerate/project.hpp>
#include <new>

namespace nijigenerate::windows::command_browser::detail {
// Explicit specializations enumerate the original D enum, including aliases.
// Unknown enum metadata is a source-closure error, never an empty choice list.
template<class E> struct BrowserEnumTraits;

template<class T> void clearResourceArgValue(T& value) {
    static_assert(isResourceArgument<T>);
    if constexpr (ResourceArrayType<T>::isDynamicArray) {
        value = T();
    } else value = {};
}

template<class T> bool parseArgValue(BrowserText raw, T& value) {
    if constexpr (std::same_as<T, BrowserText>) { value = raw; return true; }
    else if constexpr (std::same_as<T, compat::RetainedString>) {
        value = raw.retainedString(); return true;
    } else if constexpr (std::same_as<T, std::string>) {
        value = std::string(raw.view()); return true;
    } else if constexpr (isBrowserEnum<T>) {
        try {
            T parsed{};
            std::size_t longest = 0;
            for (const auto& [name, member] : BrowserEnumTraits<T>::members) {
                // Phobos startsWith's same-code-unit string path compares bytes.
                if (longest < name.size() && raw.view().starts_with(name)) {
                    parsed = member;
                    longest = name.size();
                }
            }
            if (longest == 0) {
                const auto message = allocateBrowserMemory([&] {
                    std::string bytes(BrowserEnumTraits<T>::typeName);
                    bytes += " does not have a member named '";
                    bytes.append(raw.view());
                    bytes += '\'';
                    return compat::RetainedString(std::move(bytes));
                });
                // The source's to!string(raw) is identity, including invalid UTF.
                throw compat::PhobosConvException(message,
                    compat::RetainedString::fromStaticLiteral("std/conv.d"), 2866);
            }
            if (longest != raw.size()) {
                const auto suffix = raw.view().substr(longest);
                const auto scalarLength = compat::phobos::ngUnexpectedScalarLength(suffix);
                const auto message = allocateBrowserMemory([&] {
                    std::string bytes = "Unexpected '";
                    if (suffix.front() == '\n') bytes += "\\n";
                    else bytes.append(suffix.data(), scalarLength);
                    bytes += "' when converting from type string to type ";
                    bytes += BrowserEnumTraits<T>::typeName;
                    return compat::RetainedString(std::move(bytes));
                });
                throw compat::PhobosConvException(message,
                    compat::RetainedString::fromStaticLiteral("std/conv.d"), 1898);
            }
            value = parsed;
            return true;
        } catch (const compat::ThrowableException&) { return false; }
    } else if constexpr (std::same_as<T, bool>) return parseBool(raw, value);
    else if constexpr (isResourceArgument<T>) {
        try {
            using E = typename ResourceArrayType<T>::Element;
            std::uint32_t scalarId;
            if constexpr (!ResourceArrayType<T>::isDynamicArray)
                if (!parseInteger(raw.view(), scalarId)) return false;
            decltype(project::incActivePuppet()) puppet;
            // Binding IDs are process-local and resolved independently of a puppet.
            if constexpr (!isExactBindingResource<E>) {
                puppet = project::incActivePuppet();
                if (!puppet) return false;
            }
            auto lookup = [&](std::uint32_t id) -> E {
                if constexpr (isNodeResource<E>)
                    return std::dynamic_pointer_cast<typename E::element_type>(puppet->findNodeById(id));
                else if constexpr (isExactParameterResource<E>) return puppet->findParameter(id);
                else return findBindingByCommandBrowserId(id);
            };
            if constexpr (ResourceArrayType<T>::isDynamicArray) {
                T result;
                for (auto part : split(raw)) {
                    const auto stripped = compat::ngStripUnicode(part.view());
                    if (stripped.empty()) continue;
                    std::uint32_t id;
                    if (!parseInteger(stripped, id)) return false;
                    if (auto found = lookup(id)) result.append(found);
                }
                value = result;
                return true;
            } else {
                auto found = lookup(scalarId);
                if (!found) return false;
                value = found; return true;
            }
        } catch (const compat::ThrowableException&) { return false; }
    } else if constexpr (isBrowserIntegral<T>) return parseInteger(raw.view(), value);
    else if constexpr (std::floating_point<T>) return parseFloat(raw, value);
    else return false;
}
}
