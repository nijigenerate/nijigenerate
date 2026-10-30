#pragma once
#include "meta.hpp"
#include "automesh.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::viewport::vertex::automesh::meta::source_binding {
// The actual translated UDA text has static compiler-metadata lifetime.
inline AMParam ngParam(AutoMeshLevel level, std::string_view id, std::string_view label,
        std::string_view description, std::string_view widget,
        float min = std::numeric_limits<float>::quiet_NaN(),
        float max = std::numeric_limits<float>::quiet_NaN(),
        float step = std::numeric_limits<float>::quiet_NaN()) {
    using String = compat::RetainedString;
    return {level, String::fromStaticView(id), String::fromStaticView(label),
        String::fromStaticView(description), String::fromStaticView(widget), min, max, step, 0};
}
template<auto Member, class... Attributes> auto ngMember(Attributes... attributes) {
    return AMReflectionMember{Member, std::tuple{std::move(attributes)...}};
}

// Declaration metadata is constant-expression input, unlike the retained
// strings of the actual runtime AMParam value. These views refer only to
// the translated declaration's static text, not to a runtime host instance.
struct AMParamDeclaration {
    AutoMeshLevel level;
    std::string_view id, label, desc, widget;
    float min = std::numeric_limits<float>::quiet_NaN();
    float max = std::numeric_limits<float>::quiet_NaN();
    float step = std::numeric_limits<float>::quiet_NaN();
    std::int32_t order = 0;
};

// Native declaration input for the existing AMEnum UDA, not a Source type.
// As with AMParamDeclaration, every view refers to actual static metadata.
template<std::size_t Count> struct AMEnumDeclaration {
    std::array<std::string_view, Count> values;
};

template<std::size_t Count>
constexpr AMEnumDeclaration<Count> ngEnumDeclaration(std::array<std::string_view, Count> values) {
    return {values};
}

constexpr AMParamDeclaration ngParamDeclaration(AutoMeshLevel level, std::string_view id,
        std::string_view label, std::string_view description, std::string_view widget,
        float min = std::numeric_limits<float>::quiet_NaN(),
        float max = std::numeric_limits<float>::quiet_NaN(),
        float step = std::numeric_limits<float>::quiet_NaN(), std::int32_t order = 0) {
    return {level, id, label, description, widget, min, max, step, order};
}

inline AMParam ngRuntimeAttribute(const AMParamDeclaration& source) {
    auto result = ngParam(source.level, source.id, source.label, source.desc,
        source.widget, source.min, source.max, source.step);
    result.order = source.order;
    return result;
}

template<std::size_t Count>
AMEnum ngRuntimeAttribute(const AMEnumDeclaration<Count>& source) {
    AMEnum result;
    if constexpr (Count != 0) {
        std::array<compat::RetainedString, Count> values;
        for (std::size_t i = 0; i < Count; ++i)
            values[i] = compat::RetainedString::fromStaticView(source.values[i]);
        result.values = compat::Slice<compat::RetainedString>(values.begin(), values.end());
    }
    return result;
}

template<class Attribute> Attribute ngRuntimeAttribute(const Attribute& source) { return source; }

template<class Accessor, class Attributes> struct AMMemberDeclaration {
    Accessor accessor;
    std::string_view identifier;
    Attributes attributes;

    template<class Host> constexpr decltype(auto) get(Host& host) const {
        return std::invoke(accessor, host);
    }
    auto runtime() const {
        return std::apply([&](const auto&... attribute) {
            return AMReflectionMember{accessor, std::tuple{ngRuntimeAttribute(attribute)...}};
        }, attributes);
    }
};

template<auto Member, class... Attributes>
constexpr auto ngMemberDeclaration(std::string_view identifier, Attributes... attributes) {
    return AMMemberDeclaration<decltype(Member), std::tuple<Attributes...>>{
        Member, identifier, std::tuple{std::move(attributes)...}};
}

template<class Declarations> auto ngMembers(const Declarations& declarations) {
    return std::apply([](const auto&... declaration) {
        return std::tuple{declaration.runtime()...};
    }, declarations);
}
template<class Invocation> auto ngPreset(std::string_view name, Invocation invocation) {
    return AMReflectionPreset{std::move(invocation),
        std::tuple{AMPreset{compat::RetainedString::fromStaticView(name)}}};
}
template<class Host> auto ngOwner(Host& receiver) {
    // This is the real source class handle; never fabricate a borrowed owner.
    return std::static_pointer_cast<Host>(receiver.shared_from_this());
}
template<class Host> compat::RetainedString ngTypeName(Host& receiver) {
    // The authored M277 base operation already implements the identical
    // dynamic typename/final-dot suffix expression used by M281 schema().
    // Qualified dispatch deliberately bypasses the UDA procId override.
    return receiver.AutoMeshProcessor::procId();
}
} // namespace nijigenerate::viewport::vertex::automesh::meta::source_binding
