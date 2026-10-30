#include "hierarchy.hpp"
#include <nijigenerate/compat/phobos_index_sort.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <common/resource_key_map.hpp>
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <numeric>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace nijigenerate::autorig::deterministic::hierarchy {
namespace {
using Value = core::settings::SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
using String = core::settings::SettingsString;
using Id = std::uint64_t;
using Ids = compat::Slice<Id>;
using namespace contracts;
struct StringLess {
    bool operator()(const String& left, const String& right) const noexcept {
        return left.view() < right.view();
    }
};
struct IdHash {
    std::size_t operator()(Id value) const noexcept {
        // Selected core.internal.hash integral identity/fold, before AA mixing.
        constexpr auto shift = sizeof(std::size_t) < sizeof(value) ? sizeof(std::size_t) * 8 : 0;
        return static_cast<std::size_t>(value ^ (shift ? value >> shift : 0));
    }
};
struct StringHash {
    std::size_t operator()(const String& value) const noexcept {
        return compat::ngPhobosStringHash(value.view());
    }
};
template<class V> using IdMap = nicxlive::core::common::ResourceKeyMap<Id, V, IdHash, std::equal_to<Id>>;
template<class V> using StringMap = nicxlive::core::common::ResourceKeyMap<String, V,
    StringHash, std::equal_to<String>>;
template<std::size_t N> String literal(const char (&text)[N]) noexcept {
    return compat::RetainedString::fromStaticLiteral(text);
}
std::string concatenate(std::initializer_list<std::string_view> parts) {
    std::size_t length = 0;
    for (const auto part : parts) length += part.size();
    return contracts::detail::allocateContractStorage([&] {
        std::string result;
        result.reserve(length);
        for (const auto part : parts) if (!part.empty()) result.append(part);
        return result;
    });
}
String retainString(std::string value) {
    return contracts::detail::allocateContractStorage([&] {
        return String(compat::RetainedString(std::move(value)));
    });
}
Object& object(Value& value) {
    return value.sourceObjectRef();
}
Value get(Value value, std::string_view key) {
    return value.sourceIndexValue(key);
}
Array array(Value value, std::string_view key) { return get(value, key).as_array(); }
String text(Value value, std::string_view key) { return get(value, key).as_string(); }
Object duplicate(Value value) {
    Object result;
    for (const auto& entry : object(value)) result[entry.name] = entry.item;
    return result;
}
Value row(std::initializer_list<std::pair<String, Value>> entries) {
    Object result;
    for (const auto& [key, value] : entries) result[key] = value;
    return Value(result);
}
bool contains(Ids ids, Id id) {
    if (ids.empty()) return false;
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}
template<std::size_t N> void require(bool condition, const char (&message)[N], std::size_t sourceLine) {
    if (!condition) throw compat::ThrowableException(contracts::detail::copyContractMessage(std::string_view(message, N - 1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/hierarchy.d"), sourceLine);
}
template<class Map, class Key> auto aaRead(const Map& values, const Key& key,
    std::size_t sourceLine) -> std::remove_cvref_t<decltype(values.begin()->second)> {
    const auto entry = values.find(key);
    if (entry == values.end())
        throw compat::ngStaticError<compat::RangeError>(
            literal("source/nijigenerate/autorig/deterministic/hierarchy.d"), sourceLine);
    return entry->second;
}
} // namespace

Value ngRigCompileHierarchy(Value observation, Array carriers, Value scaffold) {
    IdMap<Value> nodes;
    const Id root = ngRigUnsigned(get(observation, "rootId"));
    nodes[root] = row({{literal("uuid"), get(observation, "rootId")},
        {literal("parent"), Value{}}, {literal("type"), Value(literal("Node"))}});
    const auto hierarchyNodes = ngRigGet(observation, literal("hierarchy_nodes"), Value(Array{})).as_array();
    for (auto node : hierarchyNodes) nodes[ngRigUnsigned(get(node, "uuid"))] = node;
    const auto sourceGroups = ngRigGet(observation, literal("groups"), Value(Array{})).as_array();
    for (auto node : sourceGroups) {
        const auto id = ngRigUnsigned(get(node, "uuid"));
        if (!nodes.contains(id)) {
            auto copy = duplicate(node);
            copy[literal("type")] = Value(literal("DynamicComposite"));
            nodes[id] = Value(copy);
        }
    }
    IdMap<Value> materials;
    const auto sourceMaterials = array(observation, "materials");
    for (std::size_t order = 0; order < sourceMaterials.size(); ++order) {
        auto material = sourceMaterials[order];
        const auto id = ngRigUnsigned(get(material, "uuid"));
        materials[id] = material;
        if (!nodes.contains(id)) nodes[id] = row({{literal("uuid"), get(material, "uuid")},
            {literal("parent"), ngRigGet(material, literal("parent"), get(observation, "rootId"))},
            {literal("type"), Value(literal("Part"))}, {literal("source_order"), Value(order)}});
    }
    IdMap<Ids> children;
    for (const auto [id, node] : nodes) {
        auto parent = get(node, "parent");
        if (!parent.is_null()) children[ngRigUnsigned(parent)].append(id);
    }
    auto chain = [&](Id id) {
        Ids result;
        for (;;) {
            require(!contains(result, id), "Cyclic source hierarchy", 36);
            result.append(id);
            const auto node = nodes.find(id);
            require(node != nodes.end(), "Missing source ancestor", 37);
            auto parent = get(node->second, "parent");
            if (parent.is_null()) return result;
            id = ngRigUnsigned(parent);
        }
    };
    IdMap<String> assigned;
    StringMap<Ids> domains;
    StringMap<Value> definitions;
    for (auto carrier : carriers) {
        const auto id = ngRigUnsigned(get(carrier, "part"));
        const auto domain = text(carrier, "domain_id");
        assigned[id] = domain;
        domains[domain].append(id);
        definitions[domain] = carrier;
    }
    if (carriers.empty())
        throw compat::ngStaticError<compat::ArrayIndexError>(0, carriers.size(),
            literal("source/nijigenerate/autorig/deterministic/hierarchy.d"), 49);
    const auto first = chain(ngRigUnsigned(get(carriers[0], "part")));
    Id scopeId = 0;
    bool found = false;
    for (std::size_t i = 1; i < first.size(); ++i) {
        const auto candidate = first[i];
        bool common = true;
        for (const auto [id, domain] : assigned) {
            const auto ancestry = chain(id);
            if (std::find(ancestry.begin() + 1, ancestry.end(), candidate) == ancestry.end()) common = false;
        }
        if (common) { scopeId = candidate; found = true; break; }
    }
    require(found, "No common material render scope", 55);
    IdMap<Ids> descendants;
    std::function<Ids(Id)> partsBelow = [&](Id id) -> Ids {
        if (auto cached = descendants.find(id); cached != descendants.end()) return cached->second;
        Ids result;
        if (text(aaRead(nodes, id, 60), "type").view() == "Part") result.append(id);
        if (auto list = children.find(id); list != children.end()) for (const auto child : list->second) {
            auto nested = partsBelow(child);
            result.append(nested);
        }
        descendants[id] = result;
        return result;
    };
    IdMap<Id> receivers;
    for (auto [id, material] : materials)
        if (auto* receiver = object(material).if_contains("receiver")) receivers[id] = ngRigUnsigned(*receiver);
    IdMap<bool> clipScopes;
    for (const auto [id, node] : nodes) {
        const auto type = text(node, "type");
        if (type.view() != "Composite" && type.view() != "DynamicComposite") continue;
        if (ngRigString(node, literal("blend_mode"), literal("Normal")).view() != "Normal" ||
            ngRigScalar(node, literal("opacity"), 1) != 1) continue;
        const auto parts = partsBelow(id);
        Ids bases;
        for (auto part : parts) if (!receivers.contains(part)) bases.append(part);
        if (parts.size() <= 1 || bases.size() != 1) continue;
        bool clipped = true;
        for (auto part : parts) if (part != bases[0])
            if (!receivers.contains(part) || aaRead(receivers, part, 77) != bases[0]) clipped = false;
        if (clipped) clipScopes[id] = true;
    }
    auto anchor = [&](String domain) {
        Ids candidates;
        const auto domainParts = aaRead(domains, domain, 81);
        for (auto id : domainParts) if (!receivers.contains(id)) candidates.append(id);
        // enforce evaluates its message lazily, only when the condition fails.
        if (candidates.empty()) {
            const auto message = concatenate({"Material domain has no unclipped origin: ", domain.view()});
            throw compat::ThrowableException(
                contracts::detail::copyContractMessage(std::string_view(message)),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/hierarchy.d"), 83);
        }
        const auto definition = aaRead(definitions, domain, 84);
        if (text(definition, "owner").view() == "head" && text(definition, "chart").view() == "face") {
            Ids face;
            for (auto id : candidates) {
                const auto material = materials.find(id);
                if (material == materials.end())
                    throw compat::ngStaticError<compat::RangeError>(
                        literal("source/nijigenerate/autorig/deterministic/hierarchy.d"), 86);
                if (text(material->second, "role").view() == "face") face.append(id);
            }
            if (!face.empty()) candidates = std::move(face);
        }
        double best = -1;
        Id selected = 0;
        for (auto id : candidates) {
            const auto entry = materials.find(id);
            if (entry == materials.end())
                throw compat::ngStaticError<compat::RangeError>(
                    literal("source/nijigenerate/autorig/deterministic/hierarchy.d"), 91);
            const auto material = entry->second;
            const auto fallback = get(aaRead(definitions, domain, 91), "bounds");
            const auto b = ngRigNumbers(ngRigGet(material, literal("bounds"), fallback));
            const auto component = [&](std::size_t index) {
                if (index >= b.size())
                    throw compat::ngStaticError<compat::ArrayIndexError>(index, b.size(),
                        literal("source/nijigenerate/autorig/deterministic/hierarchy.d"), 92);
                return b[index];
            };
            const auto right = component(2);
            const auto left = component(0);
            const auto width = right - left;
            const auto bottom = component(3);
            const auto top = component(1);
            const auto height = bottom - top;
            const double area = width * height;
            if (area > best) { best = area; selected = id; }
        }
        return selected;
    };
    // Source captures domains.keys.sort.array once; other AAs retain bucket order.
    compat::Slice<String> domainIds;
    for (const auto& [id, ids] : domains) domainIds.append(id);
    if (domainIds.size() > 1) std::sort(domainIds.begin(), domainIds.end(), StringLess{});
    String body;
    for (const auto& id : domainIds)
        if (text(aaRead(definitions, id, 99), "owner").view() == "torso" &&
            text(aaRead(definitions, id, 99), "chart").view() == "body") body = id;
    if (body.empty()) for (const auto& id : domainIds) {
        const auto definition = aaRead(definitions, id, 101);
        const auto chart = text(definition, "chart");
        if (text(definition, "owner").view() == "torso" &&
            (chart.view() == "topwear_front" || chart.view() == "topwear_waist"))
            if (body.empty()) body = id;
            else {
                const auto currentParts = aaRead(domains, id, 103);
                const auto bodyParts = aaRead(domains, body, 103);
                if (currentParts.size() > bodyParts.size()) body = id;
            }
    }
    require(!body.empty(), "No torso origin material", 105);
    const auto bodyOrigin = anchor(body);
    StringMap<Value> bones;
    for (auto bone : array(scaffold, "bones")) bones[text(bone, "id")] = bone;
    Array groups;
    auto group = [&](String id, Value parent, String bone) {
        groups.append(row({{literal("id"), Value(id)}, {literal("parent"), parent},
            {literal("bone"), Value(bone)},
            {literal("origin"), Value([&]() -> Array {
                const auto entry = bones.find(bone);
                if (entry == bones.end())
                    throw compat::ngStaticError<compat::RangeError>(
                        literal("source/nijigenerate/autorig/deterministic/hierarchy.d"), 111);
                const auto head = array(entry->second, "head");
                if (head.size() < 2)
                    throw compat::ngStaticError<compat::ArraySliceError>(0, 2, head.size(),
                        literal("source/nijigenerate/autorig/deterministic/hierarchy.d"), 111);
                return head.slice(0, 2);
            }())}}));
    };
    group(literal("Body::Root"), row({{literal("node"), Value(scopeId)}}), literal("Pelvis"));
    group(literal("Head::Root"), row({{literal("node"), Value(bodyOrigin)}}), literal("Head"));
    const compat::Slice<String> families{literal("arm"), literal("leg")};
    for (const auto familyDescriptor : families)
        for (const auto sideDescriptor : compat::Slice<String>{literal("L"), literal("R")}) {
        const auto family = familyDescriptor.view();
        const auto side = sideDescriptor.view();
        bool exists = false;
        for (const auto& id : domainIds) {
            const auto owner = text(aaRead(definitions, id, 117), "owner");
            const auto expected = concatenate({family, ":", side == "R" ? "r" : "l"});
            if (owner.view() == std::string_view(expected.data(), expected.size())) exists = true;
        }
        if (exists) {
            const String groupId = retainString(concatenate({family == "arm" ? "Arm" : "Leg", "::Root::", side}));
            const auto parent = row({{literal("node"), Value(family == "arm" ? bodyOrigin : scopeId)}});
            const String bone = retainString(concatenate({family == "arm" ? "UpperArm." : "Thigh.", side}));
            group(groupId, parent, bone);
        }
    }
    Object parents, units, origins;
    for (const auto& id : domainIds) {
        const auto definition = aaRead(definitions, id, 124);
        const auto owner = text(definition, "owner"), chart = text(definition, "chart");
        Value parent;
        if (id == body) parent = row({{literal("group"), Value(literal("Body::Root"))}});
        else if (owner.view() == "head") parent = row({{literal("group"), Value(literal("Head::Root"))}});
        else if (owner.view() == "torso") parent = row({{literal("node"), Value(bodyOrigin)}});
        else {
            const String skin = retainString(concatenate({owner.view(), "/skin@semantic"}));
            if (chart.view() != "skin" && domains.contains(skin))
                parent = row({{literal("node"), Value(anchor(skin))}});
            else if (text(definition, "side").view() == "Both")
                parent = row({{literal("node"), Value(owner.view().starts_with("arm:") ? bodyOrigin : scopeId)}});
            else {
                const auto prefix = concatenate({owner.view().starts_with("arm:") ? "Arm" : "Leg", "::Root::"});
                const auto side = text(definition, "side");
                parent = row({{literal("group"), Value(retainString(concatenate({prefix, side.view()})))}});
            }
        }
        parents[id] = parent;
        origins[id] = Value(anchor(id));
        Ids topUnits;
        const auto ids = aaRead(domains, id, 137);
        for (auto part : ids) {
            auto top = part;
            const auto ancestry = chain(part);
            for (std::size_t i = 1; i < ancestry.size(); ++i) {
                const auto candidate = ancestry[i];
                if (candidate == scopeId || clipScopes.contains(candidate)) break;
                const auto parts = partsBelow(candidate);
                bool complete = !parts.empty();
                for (auto descendant : parts) if (!contains(ids, descendant)) complete = false;
                if (!complete) break;
                top = candidate;
            }
            if (!contains(topUnits, top)) topUnits.append(top);
        }
        compat::Slice<std::size_t> order;
        order.resize(topUnits.size(), std::size_t{0});
        std::iota(order.begin(), order.end(), std::size_t{0});
        // Retain Phobos's ulong sort permutation when source_order keys tie.
        compat::phobos_index_sort::sort<sizeof(Id)>({order.data(), order.size()}, [&](std::size_t a, std::size_t b) {
            const auto left = ngRigScalar(aaRead(nodes, topUnits[a], 149), literal("source_order"), 0);
            const auto right = ngRigScalar(aaRead(nodes, topUnits[b], 149), literal("source_order"), 0);
            return left < right;
        }, "Failed to sort range of type ulong[]");
        Array sorted;
        for (auto i : order) sorted.append(Value(topUnits[i]));
        units[id] = Value(sorted);
    }
    Value faceOrigin;
    for (const auto& id : domainIds)
        if (text(aaRead(definitions, id, 153), "owner").view() == "head" &&
            text(aaRead(definitions, id, 153), "chart").view() == "face")
            faceOrigin = Value(anchor(id));
    Object clipping;
    for (const auto [part, receiver] : receivers)
        if (assigned.contains(part) && assigned.contains(receiver)) {
            const auto partDomain = aaRead(assigned, part, 157);
            const auto receiverDomain = aaRead(assigned, receiver, 157);
            if (partDomain == receiverDomain) {
                auto key = contracts::detail::allocateContractStorage([&] { return std::to_string(part); });
                clipping[retainString(std::move(key))] = Value(receiver);
            }
        }
    return row({{literal("render_scope"), Value(scopeId)}, {literal("body_domain"), Value(body)},
        {literal("face_origin"), faceOrigin}, {literal("clipping_receivers"), Value(clipping)},
        {literal("body_origin"), Value(bodyOrigin)}, {literal("groups"), Value(groups)},
        {literal("surface_parents"), Value(parents)}, {literal("render_units"), Value(units)},
        {literal("origin_parts"), Value(origins)}});
}
} // namespace nijigenerate::autorig::deterministic::hierarchy
